#include "core/securt_instance_registry.h"
#include <shared_mutex>

bool SecuRTInstanceRegistry::createInstance(const std::string &instanceId,
                                             const SecuRTInstance &instance) {
  std::unique_lock<std::shared_mutex> lock(mutex_);
  if (instances_.find(instanceId) != instances_.end()) {
    return false; // Instance already exists
  }
  instances_[instanceId] = instance;
  return true;
}

std::optional<SecuRTInstance>
SecuRTInstanceRegistry::getInstance(const std::string &instanceId) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  auto it = instances_.find(instanceId);
  if (it != instances_.end()) {
    return it->second;
  }
  return std::nullopt;
}

bool SecuRTInstanceRegistry::updateInstance(
    const std::string &instanceId, const SecuRTInstanceWrite &updates) {
  std::unique_lock<std::shared_mutex> lock(mutex_);
  auto it = instances_.find(instanceId);
  if (it == instances_.end()) {
    return false; // Instance not found
  }

  SecuRTInstance &instance = it->second;
  if (!updates.name.empty()) {
    instance.name = updates.name;
  }
  instance.detectorMode = updates.detectorMode;
  instance.detectionSensitivity = updates.detectionSensitivity;
  instance.movementSensitivity = updates.movementSensitivity;
  instance.sensorModality = updates.sensorModality;
  instance.frameRateLimit = updates.frameRateLimit;
  instance.metadataMode = updates.metadataMode;
  instance.statisticsMode = updates.statisticsMode;
  instance.diagnosticsMode = updates.diagnosticsMode;
  instance.debugMode = updates.debugMode;

  return true;
}

bool SecuRTInstanceRegistry::deleteInstance(const std::string &instanceId) {
  std::unique_lock<std::shared_mutex> lock(mutex_);
  return instances_.erase(instanceId) > 0;
}

bool SecuRTInstanceRegistry::hasInstance(const std::string &instanceId) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  return instances_.find(instanceId) != instances_.end();
}

std::vector<std::string> SecuRTInstanceRegistry::listInstances() const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  std::vector<std::string> result;
  result.reserve(instances_.size());
  for (const auto &pair : instances_) {
    result.push_back(pair.first);
  }
  return result;
}

size_t SecuRTInstanceRegistry::getInstanceCount() const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  return instances_.size();
}

