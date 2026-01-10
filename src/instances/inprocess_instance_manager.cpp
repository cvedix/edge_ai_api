#include "instances/inprocess_instance_manager.h"
#include "models/update_instance_request.h"
#include <algorithm>
#include <cctype>

// Helper function to trim whitespace
static std::string trim(const std::string &str) {
  if (str.empty())
    return str;
  size_t first = str.find_first_not_of(" \t\n\r\f\v");
  if (first == std::string::npos)
    return "";
  size_t last = str.find_last_not_of(" \t\n\r\f\v");
  return str.substr(first, (last - first + 1));
}

// Helper function to parse UpdateInstanceRequest from Json::Value
static bool parseUpdateRequestFromJson(const Json::Value &json,
                                       UpdateInstanceRequest &req,
                                       std::string &error) {
  // Support both camelCase and PascalCase field names
  // Basic fields - camelCase
  if (json.isMember("name") && json["name"].isString()) {
    req.name = json["name"].asString();
  }
  // PascalCase support
  if (json.isMember("DisplayName") && json["DisplayName"].isString()) {
    req.name = json["DisplayName"].asString();
  }

  if (json.isMember("group") && json["group"].isString()) {
    req.group = json["group"].asString();
  }
  if (json.isMember("Group") && json["Group"].isString()) {
    req.group = json["Group"].asString();
  }

  if (json.isMember("persistent") && json["persistent"].isBool()) {
    req.persistent = json["persistent"].asBool();
  }

  if (json.isMember("frameRateLimit") && json["frameRateLimit"].isNumeric()) {
    req.frameRateLimit = json["frameRateLimit"].asInt();
  }

  if (json.isMember("metadataMode") && json["metadataMode"].isBool()) {
    req.metadataMode = json["metadataMode"].asBool();
  }

  if (json.isMember("statisticsMode") && json["statisticsMode"].isBool()) {
    req.statisticsMode = json["statisticsMode"].asBool();
  }

  if (json.isMember("diagnosticsMode") && json["diagnosticsMode"].isBool()) {
    req.diagnosticsMode = json["diagnosticsMode"].asBool();
  }

  if (json.isMember("debugMode") && json["debugMode"].isBool()) {
    req.debugMode = json["debugMode"].asBool();
  }

  if (json.isMember("detectorMode") && json["detectorMode"].isString()) {
    req.detectorMode = json["detectorMode"].asString();
  }

  if (json.isMember("detectionSensitivity") &&
      json["detectionSensitivity"].isString()) {
    req.detectionSensitivity = json["detectionSensitivity"].asString();
  }

  if (json.isMember("movementSensitivity") &&
      json["movementSensitivity"].isString()) {
    req.movementSensitivity = json["movementSensitivity"].asString();
  }

  if (json.isMember("sensorModality") && json["sensorModality"].isString()) {
    req.sensorModality = json["sensorModality"].asString();
  }

  if (json.isMember("autoStart") && json["autoStart"].isBool()) {
    req.autoStart = json["autoStart"].asBool();
  }

  if (json.isMember("autoRestart") && json["autoRestart"].isBool()) {
    req.autoRestart = json["autoRestart"].asBool();
  }

  if (json.isMember("inputOrientation") &&
      json["inputOrientation"].isNumeric()) {
    req.inputOrientation = json["inputOrientation"].asInt();
  }

  if (json.isMember("inputPixelLimit") && json["inputPixelLimit"].isNumeric()) {
    req.inputPixelLimit = json["inputPixelLimit"].asInt();
  }

  // Parse additionalParams - support both new structure (input/output) and old
  // structure (flat)
  if (json.isMember("additionalParams") &&
      json["additionalParams"].isObject()) {
    // Check if using new structure (input/output)
    if (json["additionalParams"].isMember("input") &&
        json["additionalParams"]["input"].isObject()) {
      // New structure: parse input section
      for (const auto &key :
           json["additionalParams"]["input"].getMemberNames()) {
        if (json["additionalParams"]["input"][key].isString()) {
          std::string value = json["additionalParams"]["input"][key].asString();
          req.additionalParams[key] = value;
        }
      }
    }

    if (json["additionalParams"].isMember("output") &&
        json["additionalParams"]["output"].isObject()) {
      // New structure: parse output section
      // IMPORTANT: Output params should NOT be used for input
      // RTMP_URL in output should be stored as RTMP_DES_URL to avoid confusion with input
      for (const auto &key :
           json["additionalParams"]["output"].getMemberNames()) {
        if (json["additionalParams"]["output"][key].isString()) {
          std::string value =
              json["additionalParams"]["output"][key].asString();
          // Trim RTMP URLs to prevent GStreamer pipeline errors
          if (key == "RTMP_URL" || key == "RTMP_DES_URL") {
            value = trim(value);
            // If RTMP_URL is in output section, store as RTMP_DES_URL to avoid confusion with input
            if (key == "RTMP_URL") {
              req.additionalParams["RTMP_DES_URL"] = value;
            } else {
              req.additionalParams[key] = value;
            }
          } else {
            req.additionalParams[key] = value;
          }
        }
      }
    }

    // Backward compatibility: if no input/output sections, parse as flat
    // structure
    // Also parse top-level keys (like CrossingLines) even when input/output
    // sections exist
    if (!json["additionalParams"].isMember("input") &&
        !json["additionalParams"].isMember("output")) {
      for (const auto &key : json["additionalParams"].getMemberNames()) {
        if (json["additionalParams"][key].isString()) {
          std::string value = json["additionalParams"][key].asString();
          // Trim RTMP URLs to prevent GStreamer pipeline errors
          if (key == "RTMP_URL" || key == "RTMP_DES_URL") {
            value = trim(value);
          }
          req.additionalParams[key] = value;
        }
      }
    } else {
      // Parse top-level keys in additionalParams (like CrossingLines) even when
      // input/output exist
      for (const auto &key : json["additionalParams"].getMemberNames()) {
        // Skip input/output sections (already parsed above)
        if (key == "input" || key == "output") {
          continue;
        }
        if (json["additionalParams"][key].isString()) {
          std::string value = json["additionalParams"][key].asString();
          // Trim RTMP URLs to prevent GStreamer pipeline errors
          if (key == "RTMP_URL" || key == "RTMP_DES_URL") {
            value = trim(value);
          }
          req.additionalParams[key] = value;
        }
      }
    }
  }

  return true;
}

InProcessInstanceManager::InProcessInstanceManager(InstanceRegistry &registry)
    : registry_(registry) {}

std::string
InProcessInstanceManager::createInstance(const CreateInstanceRequest &req) {
  return registry_.createInstance(req);
}

bool InProcessInstanceManager::deleteInstance(const std::string &instanceId) {
  return registry_.deleteInstance(instanceId);
}

bool InProcessInstanceManager::startInstance(const std::string &instanceId,
                                             bool skipAutoStop) {
  return registry_.startInstance(instanceId, skipAutoStop);
}

bool InProcessInstanceManager::stopInstance(const std::string &instanceId) {
  return registry_.stopInstance(instanceId);
}

bool InProcessInstanceManager::updateInstance(const std::string &instanceId,
                                              const Json::Value &configJson) {
  // Check if this is a direct config update (PascalCase format)
  // If JSON has top-level fields like "InstanceId", "DisplayName", "Detector",
  // etc., it's a direct config update
  bool isDirectConfigUpdate =
      configJson.isMember("InstanceId") || configJson.isMember("DisplayName") ||
      configJson.isMember("Detector") || configJson.isMember("Input") ||
      configJson.isMember("Output") || configJson.isMember("Zone");

  if (isDirectConfigUpdate) {
    // Direct config update - merge JSON directly into storage
    return registry_.updateInstanceFromConfig(instanceId, configJson);
  }

  // Traditional update request (camelCase fields) - convert to
  // UpdateInstanceRequest
  UpdateInstanceRequest updateReq;
  std::string parseError;
  if (!parseUpdateRequestFromJson(configJson, updateReq, parseError)) {
    std::cerr << "[InProcessInstanceManager] Failed to parse update request: "
              << parseError << std::endl;
    return false;
  }

  // Validate request
  if (!updateReq.validate()) {
    std::cerr << "[InProcessInstanceManager] Update request validation failed: "
              << updateReq.getValidationError() << std::endl;
    return false;
  }

  // Check if request has any updates
  if (!updateReq.hasUpdates()) {
    std::cerr << "[InProcessInstanceManager] No fields to update" << std::endl;
    return false;
  }

  // Use the traditional updateInstance method
  return registry_.updateInstance(instanceId, updateReq);
}

std::optional<InstanceInfo>
InProcessInstanceManager::getInstance(const std::string &instanceId) const {
  return registry_.getInstance(instanceId);
}

std::vector<std::string> InProcessInstanceManager::listInstances() const {
  return registry_.listInstances();
}

std::vector<InstanceInfo> InProcessInstanceManager::getAllInstances() const {
  auto map = registry_.getAllInstances();
  std::vector<InstanceInfo> result;
  result.reserve(map.size());
  // Use getInstance() for each instance to get FPS calculated dynamically
  for (const auto &[instanceId, _] : map) {
    auto optInfo = registry_.getInstance(instanceId);
    if (optInfo.has_value()) {
      result.push_back(optInfo.value());
    }
  }
  return result;
}

bool InProcessInstanceManager::hasInstance(
    const std::string &instanceId) const {
  return registry_.hasInstance(instanceId);
}

int InProcessInstanceManager::getInstanceCount() const {
  return registry_.getInstanceCount();
}

std::optional<InstanceStatistics>
InProcessInstanceManager::getInstanceStatistics(const std::string &instanceId) {
  return registry_.getInstanceStatistics(instanceId);
}

std::string
InProcessInstanceManager::getLastFrame(const std::string &instanceId) const {
  return registry_.getLastFrame(instanceId);
}

Json::Value InProcessInstanceManager::getInstanceConfig(
    const std::string &instanceId) const {
  return registry_.getInstanceConfig(instanceId);
}

bool InProcessInstanceManager::updateInstanceFromConfig(
    const std::string &instanceId, const Json::Value &configJson) {
  return registry_.updateInstanceFromConfig(instanceId, configJson);
}

bool InProcessInstanceManager::hasRTMPOutput(
    const std::string &instanceId) const {
  return registry_.hasRTMPOutput(instanceId);
}

void InProcessInstanceManager::loadPersistentInstances() {
  registry_.loadPersistentInstances();
}

int InProcessInstanceManager::checkAndHandleRetryLimits() {
  return registry_.checkAndHandleRetryLimits();
}
