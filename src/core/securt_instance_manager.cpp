#include "core/securt_instance_manager.h"
#include "core/uuid_generator.h"
#include "instances/instance_info.h"
#include <iostream>

SecuRTInstanceManager::SecuRTInstanceManager(IInstanceManager *instanceManager)
    : instance_manager_(instanceManager) {}

std::string SecuRTInstanceManager::createInstance(
    const std::string &instanceId, const SecuRTInstanceWrite &write) {
  // Generate instance ID if not provided
  std::string finalInstanceId = instanceId.empty()
                                    ? generateInstanceId()
                                    : instanceId;

  // Check if instance already exists in SecuRT registry
  if (registry_.hasInstance(finalInstanceId)) {
    return ""; // Instance already exists
  }

  // Check if core instance already exists
  if (instance_manager_ && instance_manager_->hasInstance(finalInstanceId)) {
    return ""; // Core instance already exists
  }

  // Create core instance
  // Note: Core instance manager always generates its own UUID
  // For PUT with specific ID, we'll use the generated ID from core manager
  // and store the mapping in SecuRT registry
  if (instance_manager_) {
    CreateInstanceRequest coreReq = createCoreInstanceRequest(finalInstanceId, write);
    try {
      std::string createdId = instance_manager_->createInstance(coreReq);
      if (createdId.empty()) {
        std::cerr << "[SecuRTInstanceManager] Failed to create core instance"
                  << std::endl;
        return "";
      }
      // If core manager generated a different ID, use that instead
      // This handles the case where core manager always generates its own UUID
      if (createdId != finalInstanceId && !instanceId.empty()) {
        // For PUT with specific ID, we need to use the provided ID
        // But core manager generated a different one - this is a limitation
        // For now, we'll use the core manager's ID
        std::cerr << "[SecuRTInstanceManager] Warning: Core instance ID ("
                  << createdId << ") differs from requested ID ("
                  << finalInstanceId << "). Using core instance ID."
                  << std::endl;
        finalInstanceId = createdId;
      } else if (createdId != finalInstanceId) {
        // For POST (no specific ID), use the generated ID
        finalInstanceId = createdId;
      }
    } catch (const std::exception &e) {
      std::cerr << "[SecuRTInstanceManager] Exception creating core instance: "
                << e.what() << std::endl;
      return "";
    }
  }

  // Create SecuRT instance
  SecuRTInstance instance;
  instance.instanceId = finalInstanceId;
  instance.name = write.name;
  instance.detectorMode = write.detectorMode;
  instance.detectionSensitivity = write.detectionSensitivity;
  instance.movementSensitivity = write.movementSensitivity;
  instance.sensorModality = write.sensorModality;
  instance.frameRateLimit = write.frameRateLimit;
  instance.metadataMode = write.metadataMode;
  instance.statisticsMode = write.statisticsMode;
  instance.diagnosticsMode = write.diagnosticsMode;
  instance.debugMode = write.debugMode;

  if (!registry_.createInstance(finalInstanceId, instance)) {
    // Rollback: delete core instance if SecuRT creation fails
    if (instance_manager_) {
      instance_manager_->deleteInstance(finalInstanceId);
    }
    return "";
  }

  // Start statistics tracking
  statistics_collector_.startTracking(finalInstanceId);

  return finalInstanceId;
}

bool SecuRTInstanceManager::updateInstance(const std::string &instanceId,
                                            const SecuRTInstanceWrite &write) {
  if (!registry_.hasInstance(instanceId)) {
    return false;
  }

  // Update SecuRT registry
  if (!registry_.updateInstance(instanceId, write)) {
    return false;
  }

  // Update core instance if needed
  if (instance_manager_) {
    // Update core instance config
    Json::Value configJson;
    configJson["detectorMode"] = write.detectorMode;
    configJson["detectionSensitivity"] = write.detectionSensitivity;
    configJson["movementSensitivity"] = write.movementSensitivity;
    configJson["sensorModality"] = write.sensorModality;
    configJson["frameRateLimit"] = write.frameRateLimit;
    configJson["metadataMode"] = write.metadataMode;
    configJson["statisticsMode"] = write.statisticsMode;
    configJson["diagnosticsMode"] = write.diagnosticsMode;
    configJson["debugMode"] = write.debugMode;

    instance_manager_->updateInstance(instanceId, configJson);
  }

  return true;
}

bool SecuRTInstanceManager::deleteInstance(const std::string &instanceId) {
  if (!registry_.hasInstance(instanceId)) {
    return false;
  }

  // Stop statistics tracking
  statistics_collector_.stopTracking(instanceId);
  statistics_collector_.clearStatistics(instanceId);

  // Delete from SecuRT registry
  registry_.deleteInstance(instanceId);

  // Delete core instance
  if (instance_manager_) {
    instance_manager_->deleteInstance(instanceId);
  }

  return true;
}

std::optional<SecuRTInstance>
SecuRTInstanceManager::getInstance(const std::string &instanceId) const {
  return registry_.getInstance(instanceId);
}

SecuRTInstanceStats
SecuRTInstanceManager::getStatistics(const std::string &instanceId) const {
  // Get statistics from collector
  SecuRTInstanceStats stats = statistics_collector_.getStatistics(instanceId);

  // Try to enhance with core instance statistics if available
  if (instance_manager_) {
    auto optInfo = instance_manager_->getInstance(instanceId);
    if (optInfo.has_value()) {
      const InstanceInfo &info = optInfo.value();
      stats.isRunning = info.running;
      
      // Try to get core statistics
      try {
        auto coreStats = instance_manager_->getInstanceStatistics(instanceId);
        if (coreStats.has_value()) {
          stats.frameRate = coreStats->current_framerate;
          stats.latency = coreStats->latency;
          stats.framesProcessed = static_cast<int>(coreStats->frames_processed);
          
          // Convert start_time (seconds) to milliseconds
          if (coreStats->start_time > 0) {
            stats.startTime = static_cast<int64_t>(coreStats->start_time) * 1000;
          }
        }
      } catch (...) {
        // Ignore errors, use default stats
      }
    }
  }

  return stats;
}

bool SecuRTInstanceManager::hasInstance(const std::string &instanceId) const {
  return registry_.hasInstance(instanceId);
}

std::string SecuRTInstanceManager::generateInstanceId() const {
  return UUIDGenerator::generateUUID();
}

CreateInstanceRequest SecuRTInstanceManager::createCoreInstanceRequest(
    const std::string &instanceId, const SecuRTInstanceWrite &write) const {
  CreateInstanceRequest req;
  req.name = write.name;
  req.solution = "securt"; // Set solution to securt
  req.frameRateLimit = static_cast<int>(write.frameRateLimit);
  req.metadataMode = write.metadataMode;
  req.statisticsMode = write.statisticsMode;
  req.diagnosticsMode = write.diagnosticsMode;
  req.debugMode = write.debugMode;
  req.detectorMode = write.detectorMode;
  req.detectionSensitivity = write.detectionSensitivity;
  req.movementSensitivity = write.movementSensitivity;
  req.sensorModality = write.sensorModality;
  
  // Set instance ID in additional params if needed
  // Note: The core instance manager will use the instanceId from the request
  // if it's set in a specific way, but typically it generates its own.
  // We'll need to ensure the ID matches after creation.
  
  return req;
}

