#include "instances/instance_storage.h"
#include <json/json.h>
#include <fstream>
#include <iostream>
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

std::string InstanceStorage::getInstancesFilePath() const {
    return storage_dir_ + "/instances.json";
}

Json::Value InstanceStorage::loadInstancesFile() const {
    Json::Value root(Json::objectValue);
    
    try {
        std::string filepath = getInstancesFilePath();
        if (!std::filesystem::exists(filepath)) {
            return root; // Return empty object if file doesn't exist
        }
        
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return root;
        }
        
        Json::CharReaderBuilder builder;
        std::string errors;
        if (!Json::parseFromStream(builder, file, &root, &errors)) {
            return Json::Value(Json::objectValue); // Return empty on parse error
        }
    } catch (const std::exception& e) {
        return Json::Value(Json::objectValue);
    }
    
    return root;
}

bool InstanceStorage::saveInstancesFile(const Json::Value& instances) const {
    try {
        ensureStorageDir();
        
        std::string filepath = getInstancesFilePath();
        std::ofstream file(filepath);
        if (!file.is_open()) {
            return false;
        }
        
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "    "; // 4 spaces for indentation
        std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
        writer->write(instances, &file);
        
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool InstanceStorage::validateInstanceInfo(const InstanceInfo& info, std::string& error) const {
    if (info.instanceId.empty()) {
        error = "InstanceId cannot be empty";
        return false;
    }
    
    // Validate instanceId format (should be UUID-like)
    if (info.instanceId.length() < 10) {
        error = "InstanceId format appears invalid (too short)";
        return false;
    }
    
    // Validate displayName if provided
    if (!info.displayName.empty() && info.displayName.length() > 255) {
        error = "DisplayName too long (max 255 characters)";
        return false;
    }
    
    // Validate frameRateLimit
    if (info.frameRateLimit < 0 || info.frameRateLimit > 1000) {
        error = "frameRateLimit must be between 0 and 1000";
        return false;
    }
    
    // Validate inputOrientation
    if (info.inputOrientation < 0 || info.inputOrientation > 3) {
        error = "inputOrientation must be between 0 and 3";
        return false;
    }
    
    // Validate inputPixelLimit
    if (info.inputPixelLimit < 0) {
        error = "inputPixelLimit cannot be negative";
        return false;
    }
    
    return true;
}

bool InstanceStorage::validateConfigJson(const Json::Value& config, std::string& error) const {
    if (!config.isObject()) {
        error = "Config must be a JSON object";
        return false;
    }
    
    // InstanceId is required
    if (!config.isMember("InstanceId") || !config["InstanceId"].isString()) {
        error = "Config must contain 'InstanceId' as a string";
        return false;
    }
    
    std::string instanceId = config["InstanceId"].asString();
    if (instanceId.empty()) {
        error = "InstanceId cannot be empty";
        return false;
    }
    
    // Validate nested structures if present
    if (config.isMember("Input") && !config["Input"].isObject()) {
        error = "Input must be a JSON object";
        return false;
    }
    
    if (config.isMember("SolutionManager") && !config["SolutionManager"].isObject()) {
        error = "SolutionManager must be a JSON object";
        return false;
    }
    
    if (config.isMember("Detector") && !config["Detector"].isObject()) {
        error = "Detector must be a JSON object";
        return false;
    }
    
    return true;
}

bool InstanceStorage::mergeConfigs(Json::Value& existingConfig, const Json::Value& newConfig,
                                  const std::vector<std::string>& preserveKeys) const {
    if (!existingConfig.isObject() || !newConfig.isObject()) {
        return false;
    }
    
    // List of keys that should be completely replaced (not merged)
    std::vector<std::string> replaceKeys = {
        "InstanceId", "DisplayName", "Solution", "SolutionName", "Group",
        "ReadOnly", "SystemInstance", "AutoStart", "AutoRestart",
        "loaded", "running", "fps", "version"
    };
    
    // List of keys that should be merged (nested objects)
    std::vector<std::string> mergeKeys = {
        "Input", "SolutionManager", "Detector", "Movement", 
        "OriginatorInfo", "AdditionalParams", "Output"
    };
    
    // Replace simple fields
    for (const auto& key : replaceKeys) {
        if (newConfig.isMember(key)) {
            existingConfig[key] = newConfig[key];
        }
    }
    
    // Merge nested objects
    for (const auto& key : mergeKeys) {
        if (newConfig.isMember(key) && newConfig[key].isObject()) {
            if (!existingConfig.isMember(key)) {
                existingConfig[key] = Json::Value(Json::objectValue);
            }
            
            // Deep merge: copy all fields from newConfig to existingConfig
            for (const auto& nestedKey : newConfig[key].getMemberNames()) {
                existingConfig[key][nestedKey] = newConfig[key][nestedKey];
            }
        }
    }
    
    // Preserve special keys (TensorRT, Zone, Tripwire, etc.) from existing config
    for (const auto& preserveKey : preserveKeys) {
        if (existingConfig.isMember(preserveKey) && !newConfig.isMember(preserveKey)) {
            // Keep existing value
            continue;
        }
    }
    
    // Also preserve any UUID-like keys (TensorRT model IDs, Zone IDs, etc.)
    // These are typically UUIDs that map to complex config objects
    for (const auto& key : existingConfig.getMemberNames()) {
        // Check if key looks like a UUID (contains dashes and is long enough)
        if (key.length() >= 36 && key.find('-') != std::string::npos) {
            // This is likely a TensorRT model ID or similar - preserve it
            if (!newConfig.isMember(key)) {
                // Keep existing UUID-keyed configs
                continue;
            }
        }
        
        // Preserve special non-instance keys
        std::vector<std::string> specialKeys = {
            "AnimalTracker", "AutoRestart", "AutoStart", "Detector",
            "DetectorRegions", "DetectorThermal", "Global", "LicensePlateTracker",
            "ObjectAttributeExtraction", "ObjectMovementClassifier",
            "PersonTracker", "Tripwire", "VehicleTracker", "Zone"
        };
        
        for (const auto& specialKey : specialKeys) {
            if (key == specialKey && existingConfig.isMember(key) && !newConfig.isMember(key)) {
                // Preserve special key from existing config
                continue;
            }
        }
    }
    
    return true;
}

Json::Value InstanceStorage::instanceInfoToConfigJson(const InstanceInfo& info, std::string* error) const {
    Json::Value config(Json::objectValue);
    
    // Validate input
    std::string validationError;
    if (!validateInstanceInfo(info, validationError)) {
        if (error) {
            *error = "Validation failed: " + validationError;
        }
        return Json::Value(Json::objectValue); // Return empty object on error
    }
    
    // Store InstanceId
    config["InstanceId"] = info.instanceId;
    
    // Store DisplayName
    if (!info.displayName.empty()) {
        config["DisplayName"] = info.displayName;
    }
    
    // Store Solution
    if (!info.solutionId.empty()) {
        config["Solution"] = info.solutionId;
    }
    
    // Store SolutionName (if available)
    if (!info.solutionName.empty()) {
        config["SolutionName"] = info.solutionName;
    }
    
    // Store Group (if available)
    if (!info.group.empty()) {
        config["Group"] = info.group;
    }
    
    // Store ReadOnly
    config["ReadOnly"] = info.readOnly;
    
    // Store SystemInstance
    config["SystemInstance"] = info.systemInstance;
    
    // Store AutoStart
    config["AutoStart"] = info.autoStart;
    
    // Store AutoRestart
    config["AutoRestart"] = info.autoRestart;
    
    // Store Input configuration
    if (info.inputPixelLimit > 0 || info.inputOrientation > 0) {
        Json::Value input(Json::objectValue);
        if (info.inputPixelLimit > 0) {
            input["media_format"]["input_pixel_limit"] = info.inputPixelLimit;
        }
        if (info.inputOrientation > 0) {
            input["inputOrientation"] = info.inputOrientation;
        }
        if (!input.empty()) {
            config["Input"] = input;
        }
    }
    
    // Store Input URI if available
    if (!info.rtspUrl.empty()) {
        if (!config.isMember("Input")) {
            config["Input"] = Json::Value(Json::objectValue);
        }
        config["Input"]["media_type"] = "IP Camera";
        config["Input"]["uri"] = "gstreamer:///urisourcebin uri=" + info.rtspUrl + " ! decodebin ! videoconvert ! video/x-raw, format=NV12 ! appsink drop=true name=cvdsink";
    } else if (!info.filePath.empty()) {
        if (!config.isMember("Input")) {
            config["Input"] = Json::Value(Json::objectValue);
        }
        config["Input"]["media_type"] = "File";
        config["Input"]["uri"] = info.filePath;
    }
    
    // Store RTMP URL in Output section if available
    if (!info.rtmpUrl.empty()) {
        if (!config.isMember("Output")) {
            config["Output"] = Json::Value(Json::objectValue);
        }
        if (!config["Output"].isMember("handlers")) {
            config["Output"]["handlers"] = Json::Value(Json::objectValue);
        }
        // Store RTMP URL for reference (actual RTMP output is handled by pipeline)
        config["Output"]["rtmpUrl"] = info.rtmpUrl;
    }
    
    // Store OriginatorInfo
    if (!info.originator.address.empty()) {
        Json::Value originator(Json::objectValue);
        originator["address"] = info.originator.address;
        config["OriginatorInfo"] = originator;
    }
    
    // Store SolutionManager settings
    Json::Value solutionManager(Json::objectValue);
    solutionManager["frame_rate_limit"] = info.frameRateLimit;
    solutionManager["send_metadata"] = info.metadataMode;
    solutionManager["run_statistics"] = info.statisticsMode;
    solutionManager["send_diagnostics"] = info.diagnosticsMode;
    solutionManager["enable_debug"] = info.debugMode;
    if (info.inputPixelLimit > 0) {
        solutionManager["input_pixel_limit"] = info.inputPixelLimit;
    }
    config["SolutionManager"] = solutionManager;
    
    // Store Detector settings
    if (!info.detectorMode.empty() || !info.detectionSensitivity.empty()) {
        Json::Value detector(Json::objectValue);
        if (!info.detectorMode.empty()) {
            detector["current_preset"] = info.detectorMode;
        }
        if (!info.detectionSensitivity.empty()) {
            detector["current_sensitivity_preset"] = info.detectionSensitivity;
        }
        config["Detector"] = detector;
    }
    
    // Store Movement settings
    if (!info.movementSensitivity.empty()) {
        Json::Value movement(Json::objectValue);
        movement["current_sensitivity_preset"] = info.movementSensitivity;
        config["Movement"] = movement;
    }
    
    // Store additional parameters as nested config
    if (!info.additionalParams.empty()) {
        for (const auto& pair : info.additionalParams) {
            // Store model paths and other configs
            if (pair.first == "MODEL_PATH" || pair.first == "SFACE_MODEL_PATH") {
                // These might be stored in ObjectAttributeExtraction or Detector
                // For now, store in a generic AdditionalParams section
                if (!config.isMember("AdditionalParams")) {
                    config["AdditionalParams"] = Json::Value(Json::objectValue);
                }
                config["AdditionalParams"][pair.first] = pair.second;
            } else {
                // Store other params
                if (!config.isMember("AdditionalParams")) {
                    config["AdditionalParams"] = Json::Value(Json::objectValue);
                }
                config["AdditionalParams"][pair.first] = pair.second;
            }
        }
    }
    
    // Store runtime info (not persisted, but included for completeness)
    config["loaded"] = info.loaded;
    config["running"] = info.running;
    config["fps"] = info.fps;
    config["version"] = info.version;
    
    // Validate output config
    std::string configError;
    if (!validateConfigJson(config, configError)) {
        if (error) {
            *error = "Generated config validation failed: " + configError;
        }
        return Json::Value(Json::objectValue);
    }
    
    return config;
}

std::optional<InstanceInfo> InstanceStorage::configJsonToInstanceInfo(const Json::Value& config, std::string* error) const {
    try {
        // Validate input config
        std::string validationError;
        if (!validateConfigJson(config, validationError)) {
            if (error) {
                *error = "Config validation failed: " + validationError;
            }
            return std::nullopt;
        }
        
        InstanceInfo info;
        
        // Extract InstanceId
        if (config.isMember("InstanceId") && config["InstanceId"].isString()) {
            info.instanceId = config["InstanceId"].asString();
        } else {
            if (error) {
                *error = "InstanceId is required but missing or invalid";
            }
            return std::nullopt; // InstanceId is required
        }
        
        // Extract DisplayName
        if (config.isMember("DisplayName") && config["DisplayName"].isString()) {
            info.displayName = config["DisplayName"].asString();
        }
        
        // Extract Solution
        if (config.isMember("Solution") && config["Solution"].isString()) {
            info.solutionId = config["Solution"].asString();
        }
        
        // Extract SolutionName
        if (config.isMember("SolutionName") && config["SolutionName"].isString()) {
            info.solutionName = config["SolutionName"].asString();
        }
        
        // Extract Group
        if (config.isMember("Group") && config["Group"].isString()) {
            info.group = config["Group"].asString();
        }
        
        // Extract ReadOnly
        if (config.isMember("ReadOnly") && config["ReadOnly"].isBool()) {
            info.readOnly = config["ReadOnly"].asBool();
        }
        
        // Extract SystemInstance
        if (config.isMember("SystemInstance") && config["SystemInstance"].isBool()) {
            info.systemInstance = config["SystemInstance"].asBool();
        }
        
        // Extract AutoStart
        if (config.isMember("AutoStart") && config["AutoStart"].isBool()) {
            info.autoStart = config["AutoStart"].asBool();
        }
        
        // Extract AutoRestart
        if (config.isMember("AutoRestart") && config["AutoRestart"].isBool()) {
            info.autoRestart = config["AutoRestart"].asBool();
        }
        
        // Extract Input configuration
        if (config.isMember("Input") && config["Input"].isObject()) {
            const Json::Value& input = config["Input"];
            
            // Extract URI
            if (input.isMember("uri") && input["uri"].isString()) {
                std::string uri = input["uri"].asString();
                // Parse RTSP URL from gstreamer URI
                size_t rtspPos = uri.find("uri=");
                if (rtspPos != std::string::npos) {
                    size_t start = rtspPos + 4;
                    size_t end = uri.find(" !", start);
                    if (end == std::string::npos) {
                        end = uri.length();
                    }
                    info.rtspUrl = uri.substr(start, end - start);
                } else if (uri.find("://") == std::string::npos) {
                    // Direct file path (no protocol)
                    info.filePath = uri;
                } else {
                    // URL with protocol
                    info.filePath = uri;
                }
            }
            
            // Extract media_type
            if (input.isMember("media_type") && input["media_type"].isString()) {
                std::string mediaType = input["media_type"].asString();
                if (mediaType == "File" && info.filePath.empty() && input.isMember("uri")) {
                    info.filePath = input["uri"].asString();
                }
            }
        }
        
        // Extract RTMP URL from Output section if available
        if (config.isMember("Output") && config["Output"].isObject()) {
            const Json::Value& output = config["Output"];
            if (output.isMember("rtmpUrl") && output["rtmpUrl"].isString()) {
                info.rtmpUrl = output["rtmpUrl"].asString();
            }
        }
        
        // Extract OriginatorInfo
        if (config.isMember("OriginatorInfo") && config["OriginatorInfo"].isObject()) {
            const Json::Value& originator = config["OriginatorInfo"];
            if (originator.isMember("address") && originator["address"].isString()) {
                info.originator.address = originator["address"].asString();
            }
        }
        
        // Extract SolutionManager settings
        if (config.isMember("SolutionManager") && config["SolutionManager"].isObject()) {
            const Json::Value& sm = config["SolutionManager"];
            if (sm.isMember("frame_rate_limit") && sm["frame_rate_limit"].isInt()) {
                info.frameRateLimit = sm["frame_rate_limit"].asInt();
            }
            if (sm.isMember("send_metadata") && sm["send_metadata"].isBool()) {
                info.metadataMode = sm["send_metadata"].asBool();
            }
            if (sm.isMember("run_statistics") && sm["run_statistics"].isBool()) {
                info.statisticsMode = sm["run_statistics"].asBool();
            }
            if (sm.isMember("send_diagnostics") && sm["send_diagnostics"].isBool()) {
                info.diagnosticsMode = sm["send_diagnostics"].asBool();
            }
            if (sm.isMember("enable_debug") && sm["enable_debug"].isBool()) {
                info.debugMode = sm["enable_debug"].asBool();
            }
            if (sm.isMember("input_pixel_limit") && sm["input_pixel_limit"].isInt()) {
                info.inputPixelLimit = sm["input_pixel_limit"].asInt();
            }
        }
        
        // Extract Detector settings
        if (config.isMember("Detector") && config["Detector"].isObject()) {
            const Json::Value& detector = config["Detector"];
            if (detector.isMember("current_preset") && detector["current_preset"].isString()) {
                info.detectorMode = detector["current_preset"].asString();
            }
            if (detector.isMember("current_sensitivity_preset") && detector["current_sensitivity_preset"].isString()) {
                info.detectionSensitivity = detector["current_sensitivity_preset"].asString();
            }
        }
        
        // Extract Movement settings
        if (config.isMember("Movement") && config["Movement"].isObject()) {
            const Json::Value& movement = config["Movement"];
            if (movement.isMember("current_sensitivity_preset") && movement["current_sensitivity_preset"].isString()) {
                info.movementSensitivity = movement["current_sensitivity_preset"].asString();
            }
        }
        
        // Extract AdditionalParams
        if (config.isMember("AdditionalParams") && config["AdditionalParams"].isObject()) {
            const Json::Value& additionalParams = config["AdditionalParams"];
            for (const auto& key : additionalParams.getMemberNames()) {
                if (additionalParams[key].isString()) {
                    info.additionalParams[key] = additionalParams[key].asString();
                    
                    // Extract RTSP_URL from additionalParams if not already set
                    if (key == "RTSP_URL" && info.rtspUrl.empty()) {
                        info.rtspUrl = additionalParams[key].asString();
                    }
                    
                    // Extract FILE_PATH from additionalParams if not already set
                    if (key == "FILE_PATH" && info.filePath.empty()) {
                        info.filePath = additionalParams[key].asString();
                    }
                }
            }
        }
        
        // Extract runtime info
        if (config.isMember("loaded") && config["loaded"].isBool()) {
            info.loaded = config["loaded"].asBool();
        }
        if (config.isMember("running") && config["running"].isBool()) {
            info.running = config["running"].asBool();
        }
        if (config.isMember("fps") && config["fps"].isNumeric()) {
            info.fps = config["fps"].asDouble();
        }
        if (config.isMember("version") && config["version"].isString()) {
            info.version = config["version"].asString();
        }
        
        // Set defaults
        info.persistent = true; // All instances in instances.json are persistent
        info.loaded = true;
        info.running = false; // Will be set when started
        
        // Validate output InstanceInfo
        std::string infoError;
        if (!validateInstanceInfo(info, infoError)) {
            if (error) {
                *error = "Converted InstanceInfo validation failed: " + infoError;
            }
            return std::nullopt;
        }
        
        return info;
    } catch (const std::exception& e) {
        if (error) {
            *error = "Exception during conversion: " + std::string(e.what());
        }
        return std::nullopt;
    } catch (...) {
        if (error) {
            *error = "Unknown exception during conversion";
        }
        return std::nullopt;
    }
}

bool InstanceStorage::saveInstance(const std::string& instanceId, const InstanceInfo& info) {
    try {
        // Validate instanceId matches
        if (info.instanceId != instanceId) {
            std::cerr << "[InstanceStorage] Error: InstanceId mismatch. Expected: " << instanceId 
                      << ", Got: " << info.instanceId << std::endl;
            return false;
        }
        
        // Validate InstanceInfo
        std::string validationError;
        if (!validateInstanceInfo(info, validationError)) {
            std::cerr << "[InstanceStorage] Validation error: " << validationError << std::endl;
            return false;
        }
        
        // Load existing instances file
        Json::Value instances = loadInstancesFile();
        
        // Convert InstanceInfo to config JSON format
        std::string conversionError;
        Json::Value config = instanceInfoToConfigJson(info, &conversionError);
        if (config.isNull() || config.empty()) {
            std::cerr << "[InstanceStorage] Conversion error: " << conversionError << std::endl;
            return false;
        }
        
        // If instance already exists, merge with existing config to preserve TensorRT and other nested configs
        if (instances.isMember(instanceId) && instances[instanceId].isObject()) {
            Json::Value existingConfig = instances[instanceId];
            
            // List of keys to preserve (TensorRT model IDs, Zone IDs, etc.)
            std::vector<std::string> preserveKeys;
            
            // Collect UUID-like keys (TensorRT model IDs)
            for (const auto& key : existingConfig.getMemberNames()) {
                if (key.length() >= 36 && key.find('-') != std::string::npos) {
                    preserveKeys.push_back(key);
                }
            }
            
            // Add special keys to preserve
            std::vector<std::string> specialKeys = {
                "AnimalTracker", "DetectorRegions", "DetectorThermal", "Global",
                "LicensePlateTracker", "ObjectAttributeExtraction", 
                "ObjectMovementClassifier", "PersonTracker", "Tripwire",
                "VehicleTracker", "Zone"
            };
            preserveKeys.insert(preserveKeys.end(), specialKeys.begin(), specialKeys.end());
            
            // Merge configs
            if (!mergeConfigs(existingConfig, config, preserveKeys)) {
                std::cerr << "[InstanceStorage] Merge failed for instance " << instanceId << std::endl;
                return false;
            }
            
            instances[instanceId] = existingConfig;
        } else {
            // New instance, just store the config
            instances[instanceId] = config;
        }
        
        // Save updated instances file
        if (!saveInstancesFile(instances)) {
            std::cerr << "[InstanceStorage] Failed to save instances file" << std::endl;
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[InstanceStorage] Exception in saveInstance: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "[InstanceStorage] Unknown exception in saveInstance" << std::endl;
        return false;
    }
}

std::optional<InstanceInfo> InstanceStorage::loadInstance(const std::string& instanceId) {
    try {
        // Validate instanceId
        if (instanceId.empty()) {
            std::cerr << "[InstanceStorage] Error: Empty instanceId provided" << std::endl;
            return std::nullopt;
        }
        
        // Load instances file
        Json::Value instances = loadInstancesFile();
        
        // Check if instance exists
        if (!instances.isMember(instanceId) || !instances[instanceId].isObject()) {
            std::cerr << "[InstanceStorage] Instance " << instanceId << " not found in instances.json" << std::endl;
            return std::nullopt;
        }
        
        // Convert config JSON to InstanceInfo
        std::string conversionError;
        auto info = configJsonToInstanceInfo(instances[instanceId], &conversionError);
        if (!info.has_value()) {
            std::cerr << "[InstanceStorage] Conversion error for instance " << instanceId 
                      << ": " << conversionError << std::endl;
            return std::nullopt;
        }
        
        // Verify instanceId matches
        if (info->instanceId != instanceId) {
            std::cerr << "[InstanceStorage] Warning: InstanceId mismatch. Expected: " << instanceId 
                      << ", Got: " << info->instanceId << std::endl;
            // Fix it
            info->instanceId = instanceId;
        }
        
        return info;
    } catch (const std::exception& e) {
        std::cerr << "[InstanceStorage] Exception in loadInstance: " << e.what() << std::endl;
        return std::nullopt;
    } catch (...) {
        std::cerr << "[InstanceStorage] Unknown exception in loadInstance" << std::endl;
        return std::nullopt;
    }
}

std::vector<std::string> InstanceStorage::loadAllInstances() {
    std::vector<std::string> loaded;
    
    try {
        // Load instances file
        Json::Value instances = loadInstancesFile();
        
        // Iterate through all keys in instances object
        for (const auto& key : instances.getMemberNames()) {
            // Skip special keys that are not instance IDs (like "AutoRestart", "AnimalTracker", etc.)
            // Instance IDs are UUIDs, so we check if the key looks like a UUID or if it has an "InstanceId" field
            const Json::Value& value = instances[key];
            
            if (value.isObject()) {
                // Check if this is an instance config (has InstanceId field) or if key is a UUID-like string
                bool isInstance = false;
                
                // Check if it has InstanceId field
                if (value.isMember("InstanceId") && value["InstanceId"].isString()) {
                    isInstance = true;
                } else {
                    // Check if key looks like a UUID (contains dashes and is long enough)
                    // UUID format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
                    if (key.length() >= 36 && key.find('-') != std::string::npos) {
                        isInstance = true;
                    }
                }
                
                if (isInstance) {
                    // Try to load to validate it's a valid instance
                    auto info = loadInstance(key);
                if (info.has_value()) {
                        loaded.push_back(key);
                    }
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
        // Load instances file
        Json::Value instances = loadInstancesFile();
        
        // Check if instance exists
        if (!instances.isMember(instanceId)) {
            return true; // Already deleted
        }
        
        // Remove instance
        instances.removeMember(instanceId);
        
        // Save updated instances file
        return saveInstancesFile(instances);
    } catch (const std::exception& e) {
        return false;
    }
}

bool InstanceStorage::instanceExists(const std::string& instanceId) const {
    try {
        // Load instances file
        Json::Value instances = loadInstancesFile();
        
        // Check if instance exists
        return instances.isMember(instanceId) && instances[instanceId].isObject();
    } catch (const std::exception& e) {
        return false;
    }
}


