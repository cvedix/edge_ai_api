#include "instances/instance_storage.h"
#include <json/json.h>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iomanip>

InstanceStorage::InstanceStorage(const std::string& storage_dir)
    : storage_dir_(storage_dir) {
    ensureStorageDir();
}

void InstanceStorage::ensureStorageDir() const {
    if (!std::filesystem::exists(storage_dir_)) {
        std::filesystem::create_directories(storage_dir_);
    }
}

std::string InstanceStorage::getInstanceFilePath(const std::string& instanceId) const {
    return storage_dir_ + "/" + instanceId + ".json";
}

bool InstanceStorage::saveInstance(const std::string& instanceId, const InstanceInfo& info) {
    try {
        ensureStorageDir();
        
        Json::Value json;
        json["instanceId"] = info.instanceId;
        json["displayName"] = info.displayName;
        json["group"] = info.group;
        json["solutionId"] = info.solutionId;
        json["solutionName"] = info.solutionName;
        json["persistent"] = info.persistent;
        json["loaded"] = info.loaded;
        json["running"] = info.running;
        json["fps"] = info.fps;
        json["version"] = info.version;
        json["frameRateLimit"] = info.frameRateLimit;
        json["metadataMode"] = info.metadataMode;
        json["statisticsMode"] = info.statisticsMode;
        json["diagnosticsMode"] = info.diagnosticsMode;
        json["debugMode"] = info.debugMode;
        json["readOnly"] = info.readOnly;
        json["autoStart"] = info.autoStart;
        json["autoRestart"] = info.autoRestart;
        json["systemInstance"] = info.systemInstance;
        json["inputPixelLimit"] = info.inputPixelLimit;
        json["inputOrientation"] = info.inputOrientation;
    json["detectorMode"] = info.detectorMode;
    json["detectionSensitivity"] = info.detectionSensitivity;
    json["movementSensitivity"] = info.movementSensitivity;
    json["sensorModality"] = info.sensorModality;
    json["originator"]["address"] = info.originator.address;
    
    // Save streaming URLs if available
    if (!info.rtmpUrl.empty()) {
        json["rtmpUrl"] = info.rtmpUrl;
    }
    if (!info.rtspUrl.empty()) {
        json["rtspUrl"] = info.rtspUrl;
    }
    
    // Save file path if available
    if (!info.filePath.empty()) {
        json["filePath"] = info.filePath;
    }
        
        std::ofstream file(getInstanceFilePath(instanceId));
        if (!file.is_open()) {
            return false;
        }
        
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "  ";
        std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
        writer->write(json, &file);
        
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

std::optional<InstanceInfo> InstanceStorage::loadInstance(const std::string& instanceId) {
    try {
        std::ifstream file(getInstanceFilePath(instanceId));
        if (!file.is_open()) {
            return std::nullopt;
        }
        
        Json::Value json;
        Json::CharReaderBuilder builder;
        std::string errors;
        
        if (!Json::parseFromStream(builder, file, &json, &errors)) {
            return std::nullopt;
        }
        
        InstanceInfo info;
        info.instanceId = json.get("instanceId", "").asString();
        info.displayName = json.get("displayName", "").asString();
        info.group = json.get("group", "").asString();
        info.solutionId = json.get("solutionId", "").asString();
        info.solutionName = json.get("solutionName", "").asString();
        info.persistent = json.get("persistent", false).asBool();
        info.loaded = json.get("loaded", false).asBool();
        info.running = json.get("running", false).asBool();
        info.fps = json.get("fps", 0.0).asDouble();
        info.version = json.get("version", "").asString();
        info.frameRateLimit = json.get("frameRateLimit", 0).asInt();
        info.metadataMode = json.get("metadataMode", false).asBool();
        info.statisticsMode = json.get("statisticsMode", false).asBool();
        info.diagnosticsMode = json.get("diagnosticsMode", false).asBool();
        info.debugMode = json.get("debugMode", false).asBool();
        info.readOnly = json.get("readOnly", false).asBool();
        info.autoStart = json.get("autoStart", false).asBool();
        info.autoRestart = json.get("autoRestart", false).asBool();
        info.systemInstance = json.get("systemInstance", false).asBool();
        info.inputPixelLimit = json.get("inputPixelLimit", 0).asInt();
        info.inputOrientation = json.get("inputOrientation", 0).asInt();
        info.detectorMode = json.get("detectorMode", "SmartDetection").asString();
        info.detectionSensitivity = json.get("detectionSensitivity", "Low").asString();
        info.movementSensitivity = json.get("movementSensitivity", "Low").asString();
        info.sensorModality = json.get("sensorModality", "RGB").asString();
        
        if (json.isMember("originator") && json["originator"].isMember("address")) {
            info.originator.address = json["originator"]["address"].asString();
        }
        
        // Load streaming URLs if available
        if (json.isMember("rtmpUrl") && json["rtmpUrl"].isString()) {
            info.rtmpUrl = json["rtmpUrl"].asString();
        }
        if (json.isMember("rtspUrl") && json["rtspUrl"].isString()) {
            info.rtspUrl = json["rtspUrl"].asString();
        }
        
        // Load file path if available
        if (json.isMember("filePath") && json["filePath"].isString()) {
            info.filePath = json["filePath"].asString();
        }
        
        return info;
    } catch (const std::exception& e) {
        return std::nullopt;
    }
}

std::vector<std::string> InstanceStorage::loadAllInstances() {
    std::vector<std::string> loaded;
    
    try {
        ensureStorageDir();
        
        if (!std::filesystem::exists(storage_dir_)) {
            return loaded;
        }
        
        for (const auto& entry : std::filesystem::directory_iterator(storage_dir_)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                std::string filename = entry.path().stem().string();
                // Try to load to validate it's a valid instance file
                auto info = loadInstance(filename);
                if (info.has_value()) {
                    loaded.push_back(filename);
                }
            }
        }
    } catch (const std::exception& e) {
        // Ignore errors
    }
    
    return loaded;
}

bool InstanceStorage::deleteInstance(const std::string& instanceId) {
    try {
        std::string filepath = getInstanceFilePath(instanceId);
        if (std::filesystem::exists(filepath)) {
            return std::filesystem::remove(filepath);
        }
        return true; // Already deleted
    } catch (const std::exception& e) {
        return false;
    }
}

bool InstanceStorage::instanceExists(const std::string& instanceId) const {
    return std::filesystem::exists(getInstanceFilePath(instanceId));
}

std::string InstanceStorage::instanceInfoToJson(const InstanceInfo& info) const {
    // This is a helper that's not used, but kept for consistency
    Json::Value json;
    json["instanceId"] = info.instanceId;
    // ... (same as saveInstance)
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, json);
}

std::optional<InstanceInfo> InstanceStorage::jsonToInstanceInfo(const std::string& json) const {
    // This is a helper that's not used, but kept for consistency
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errors;
    std::istringstream stream(json);
    
    if (!Json::parseFromStream(builder, stream, &root, &errors)) {
        return std::nullopt;
    }
    
    InstanceInfo info;
    // ... (same as loadInstance)
    return info;
}

