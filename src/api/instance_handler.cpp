#include "api/instance_handler.h"
#include "models/update_instance_request.h"
#include "instances/instance_registry.h"
#include "instances/instance_info.h"
#include <drogon/HttpResponse.h>
#include <sstream>
#include <thread>
#include <chrono>
#include <future>
#include <vector>
#include <algorithm>
#include <atomic>
#include <experimental/filesystem>
#include <iomanip>
#include <ctime>
namespace fs = std::experimental::filesystem;

InstanceRegistry* InstanceHandler::instance_registry_ = nullptr;

void InstanceHandler::setInstanceRegistry(InstanceRegistry* registry) {
    instance_registry_ = registry;
}

std::string InstanceHandler::extractInstanceId(const HttpRequestPtr &req) const {
    // Try getParameter first (standard way)
    std::string instanceId = req->getParameter("instanceId");
    
    // Fallback: extract from path if getParameter doesn't work
    if (instanceId.empty()) {
        std::string path = req->getPath();
        size_t instancesPos = path.find("/instances/");
        if (instancesPos != std::string::npos) {
            size_t start = instancesPos + 11; // length of "/instances/"
            size_t end = path.find("/", start);
            if (end == std::string::npos) {
                end = path.length();
            }
            instanceId = path.substr(start, end - start);
        }
    }
    
    return instanceId;
}

HttpResponsePtr InstanceHandler::createSuccessResponse(const Json::Value& data, int statusCode) const {
    auto resp = HttpResponse::newHttpJsonResponse(data);
    resp->setStatusCode(static_cast<HttpStatusCode>(statusCode));
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    return resp;
}

void InstanceHandler::listInstances(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
    
    try {
        // Check if registry is set
        if (!instance_registry_) {
            callback(createErrorResponse(500, "Internal server error", "Instance registry not initialized"));
            return;
        }
        
        // Get all instances in one lock acquisition (optimized)
        auto allInstances = instance_registry_->getAllInstances();
        
        // Build response with summary information
        Json::Value response;
        Json::Value instances(Json::arrayValue);
        
        int totalCount = 0;
        int runningCount = 0;
        int stoppedCount = 0;
        
        // Process all instances without additional lock acquisitions
        for (const auto& [instanceId, info] : allInstances) {
            Json::Value instance;
            instance["instanceId"] = info.instanceId;
            instance["displayName"] = info.displayName;
            instance["group"] = info.group;
            instance["solutionId"] = info.solutionId;
            instance["solutionName"] = info.solutionName;
            instance["running"] = info.running;
            instance["loaded"] = info.loaded;
            instance["persistent"] = info.persistent;
            instance["fps"] = info.fps;
            
            instances.append(instance); // Use append instead of operator[] to avoid ambiguous overload
            totalCount++;
            if (info.running) {
                runningCount++;
            } else {
                stoppedCount++;
            }
        }
        
        response["instances"] = instances;
        response["total"] = totalCount;
        response["running"] = runningCount;
        response["stopped"] = stoppedCount;
        
        callback(createSuccessResponse(response));
        
    } catch (const std::exception& e) {
        std::cerr << "[InstanceHandler] Exception: " << e.what() << std::endl;
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        std::cerr << "[InstanceHandler] Unknown exception" << std::endl;
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void InstanceHandler::getInstance(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
    
    try {
        std::cerr << "[InstanceHandler] getInstance called" << std::endl;
        
        // Check if registry is set
        if (!instance_registry_) {
            std::cerr << "[InstanceHandler] Error: Instance registry not initialized" << std::endl;
            callback(createErrorResponse(500, "Internal server error", "Instance registry not initialized"));
            return;
        }
        
        // Get instance ID from path parameter
        std::string instanceId = extractInstanceId(req);
        std::cerr << "[InstanceHandler] Extracted instance ID: " << instanceId << std::endl;
        
        if (instanceId.empty()) {
            std::cerr << "[InstanceHandler] Error: Instance ID is empty" << std::endl;
            callback(createErrorResponse(400, "Invalid request", "Instance ID is required"));
            return;
        }
        
        // Get instance info
        std::cerr << "[InstanceHandler] Getting instance info for: " << instanceId << std::endl;
        auto optInfo = instance_registry_->getInstance(instanceId);
        if (!optInfo.has_value()) {
            std::cerr << "[InstanceHandler] Error: Instance not found: " << instanceId << std::endl;
            callback(createErrorResponse(404, "Not found", "Instance not found: " + instanceId));
            return;
        }
        
        // Build response
        std::cerr << "[InstanceHandler] Instance found, building response..." << std::endl;
        Json::Value response = instanceInfoToJson(optInfo.value());
        std::cerr << "[InstanceHandler] Response built successfully, sending callback..." << std::endl;
        callback(createSuccessResponse(response));
        std::cerr << "[InstanceHandler] getInstance completed successfully" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "[InstanceHandler] Exception in getInstance: " << e.what() << std::endl;
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        std::cerr << "[InstanceHandler] Unknown exception in getInstance" << std::endl;
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void InstanceHandler::startInstance(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
    
    try {
        // Check if registry is set
        if (!instance_registry_) {
            callback(createErrorResponse(500, "Internal server error", "Instance registry not initialized"));
            return;
        }
        
        // Get instance ID from path parameter
        std::string instanceId = extractInstanceId(req);
        
        if (instanceId.empty()) {
            callback(createErrorResponse(400, "Invalid request", "Instance ID is required"));
            return;
        }
        
        // Start instance
        if (instance_registry_->startInstance(instanceId)) {
            // Get updated instance info
            auto optInfo = instance_registry_->getInstance(instanceId);
            if (optInfo.has_value()) {
                Json::Value response = instanceInfoToJson(optInfo.value());
                response["message"] = "Instance started successfully";
                callback(createSuccessResponse(response));
            } else {
                callback(createErrorResponse(500, "Internal server error", "Instance started but could not retrieve info"));
            }
        } else {
            callback(createErrorResponse(400, "Failed to start", "Could not start instance. Check if instance exists and has a pipeline."));
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[InstanceHandler] Exception: " << e.what() << std::endl;
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        std::cerr << "[InstanceHandler] Unknown exception" << std::endl;
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void InstanceHandler::stopInstance(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
    
    try {
        // Check if registry is set
        if (!instance_registry_) {
            callback(createErrorResponse(500, "Internal server error", "Instance registry not initialized"));
            return;
        }
        
        // Get instance ID from path parameter
        std::string instanceId = extractInstanceId(req);
        
        if (instanceId.empty()) {
            callback(createErrorResponse(400, "Invalid request", "Instance ID is required"));
            return;
        }
        
        // Stop instance
        if (instance_registry_->stopInstance(instanceId)) {
            // Get updated instance info
            auto optInfo = instance_registry_->getInstance(instanceId);
            if (optInfo.has_value()) {
                Json::Value response = instanceInfoToJson(optInfo.value());
                response["message"] = "Instance stopped successfully";
                callback(createSuccessResponse(response));
            } else {
                callback(createErrorResponse(500, "Internal server error", "Instance stopped but could not retrieve info"));
            }
        } else {
            callback(createErrorResponse(400, "Failed to stop", "Could not stop instance. Check if instance exists."));
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[InstanceHandler] Exception: " << e.what() << std::endl;
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        std::cerr << "[InstanceHandler] Unknown exception" << std::endl;
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void InstanceHandler::restartInstance(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
    
    try {
        // Check if registry is set
        if (!instance_registry_) {
            callback(createErrorResponse(500, "Internal server error", "Instance registry not initialized"));
            return;
        }
        
        // Get instance ID from path parameter
        std::string instanceId = extractInstanceId(req);
        
        if (instanceId.empty()) {
            callback(createErrorResponse(400, "Invalid request", "Instance ID is required"));
            return;
        }
        
        // First, stop the instance if it's running
        auto optInfo = instance_registry_->getInstance(instanceId);
        if (optInfo.has_value() && optInfo.value().running) {
            if (!instance_registry_->stopInstance(instanceId)) {
                callback(createErrorResponse(500, "Failed to restart", "Could not stop instance before restart"));
                return;
            }
            // Give it a moment to fully stop and cleanup
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        // Now start the instance (skip auto-stop since we already stopped it)
        if (instance_registry_->startInstance(instanceId, true)) { // true = skipAutoStop
            // Get updated instance info
            optInfo = instance_registry_->getInstance(instanceId);
            if (optInfo.has_value()) {
                Json::Value response = instanceInfoToJson(optInfo.value());
                response["message"] = "Instance restarted successfully";
                callback(createSuccessResponse(response));
            } else {
                callback(createErrorResponse(500, "Internal server error", "Instance restarted but could not retrieve info"));
            }
        } else {
            callback(createErrorResponse(400, "Failed to restart", "Could not restart instance. Check if instance exists and has a pipeline."));
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[InstanceHandler] Exception: " << e.what() << std::endl;
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        std::cerr << "[InstanceHandler] Unknown exception" << std::endl;
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void InstanceHandler::updateInstance(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
    
    try {
        // Check if registry is set
        if (!instance_registry_) {
            callback(createErrorResponse(500, "Internal server error", "Instance registry not initialized"));
            return;
        }
        
        // Get instance ID from path parameter
        std::string instanceId = extractInstanceId(req);
        
        if (instanceId.empty()) {
            callback(createErrorResponse(400, "Invalid request", "Instance ID is required"));
            return;
        }
        
        // Parse JSON body
        auto json = req->getJsonObject();
        if (!json) {
            callback(createErrorResponse(400, "Invalid request", "Request body must be valid JSON"));
            return;
        }
        
        // Parse update request
        UpdateInstanceRequest updateReq;
        std::string parseError;
        if (!parseUpdateRequest(*json, updateReq, parseError)) {
            callback(createErrorResponse(400, "Invalid request", parseError));
            return;
        }
        
        // Validate request
        if (!updateReq.validate()) {
            callback(createErrorResponse(400, "Validation failed", updateReq.getValidationError()));
            return;
        }
        
        // Check if request has any updates
        if (!updateReq.hasUpdates()) {
            callback(createErrorResponse(400, "Invalid request", "No fields to update"));
            return;
        }
        
        // Update instance
        if (instance_registry_->updateInstance(instanceId, updateReq)) {
            // Get updated instance info
            auto optInfo = instance_registry_->getInstance(instanceId);
            if (optInfo.has_value()) {
                Json::Value response = instanceInfoToJson(optInfo.value());
                response["message"] = "Instance updated successfully";
                callback(createSuccessResponse(response));
            } else {
                callback(createErrorResponse(500, "Internal server error", "Instance updated but could not retrieve info"));
            }
        } else {
            callback(createErrorResponse(400, "Failed to update", "Could not update instance. Check if instance exists and is not read-only."));
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[InstanceHandler] Exception: " << e.what() << std::endl;
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        std::cerr << "[InstanceHandler] Unknown exception" << std::endl;
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void InstanceHandler::deleteInstance(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
    
    try {
        // Check if registry is set
        if (!instance_registry_) {
            callback(createErrorResponse(500, "Internal server error", "Instance registry not initialized"));
            return;
        }
        
        // Get instance ID from path parameter
        std::string instanceId = extractInstanceId(req);
        
        if (instanceId.empty()) {
            callback(createErrorResponse(400, "Invalid request", "Instance ID is required"));
            return;
        }
        
        // Delete instance
        if (instance_registry_->deleteInstance(instanceId)) {
            Json::Value response;
            response["success"] = true;
            response["message"] = "Instance deleted successfully";
            response["instanceId"] = instanceId;
            callback(createSuccessResponse(response));
        } else {
            callback(createErrorResponse(404, "Not found", "Instance not found: " + instanceId));
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[InstanceHandler] Exception: " << e.what() << std::endl;
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        std::cerr << "[InstanceHandler] Unknown exception" << std::endl;
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void InstanceHandler::handleOptions(
    const HttpRequestPtr & /*req*/,
    std::function<void(const HttpResponsePtr &)> &&callback) {
    
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    resp->addHeader("Access-Control-Max-Age", "3600");
    callback(resp);
}

bool InstanceHandler::parseUpdateRequest(
    const Json::Value& json,
    UpdateInstanceRequest& req,
    std::string& error) {
    
    // Optional fields - only parse if provided
    if (json.isMember("name") && json["name"].isString()) {
        req.name = json["name"].asString();
    }
    
    if (json.isMember("group") && json["group"].isString()) {
        req.group = json["group"].asString();
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
    
    if (json.isMember("detectionSensitivity") && json["detectionSensitivity"].isString()) {
        req.detectionSensitivity = json["detectionSensitivity"].asString();
    }
    
    if (json.isMember("movementSensitivity") && json["movementSensitivity"].isString()) {
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
    
    if (json.isMember("inputOrientation") && json["inputOrientation"].isNumeric()) {
        req.inputOrientation = json["inputOrientation"].asInt();
    }
    
    if (json.isMember("inputPixelLimit") && json["inputPixelLimit"].isNumeric()) {
        req.inputPixelLimit = json["inputPixelLimit"].asInt();
    }
    
    // Additional parameters (e.g., RTSP_URL, MODEL_PATH, FILE_PATH, RTMP_URL)
    if (json.isMember("additionalParams") && json["additionalParams"].isObject()) {
        for (const auto& key : json["additionalParams"].getMemberNames()) {
            if (json["additionalParams"][key].isString()) {
                req.additionalParams[key] = json["additionalParams"][key].asString();
            }
        }
    }
    
    // Also check for RTSP_URL at top level
    if (json.isMember("RTSP_URL") && json["RTSP_URL"].isString()) {
        req.additionalParams["RTSP_URL"] = json["RTSP_URL"].asString();
    }
    
    // Also check for MODEL_NAME at top level
    if (json.isMember("MODEL_NAME") && json["MODEL_NAME"].isString()) {
        req.additionalParams["MODEL_NAME"] = json["MODEL_NAME"].asString();
    }
    
    // Also check for MODEL_PATH at top level
    if (json.isMember("MODEL_PATH") && json["MODEL_PATH"].isString()) {
        req.additionalParams["MODEL_PATH"] = json["MODEL_PATH"].asString();
    }
    
    // Also check for FILE_PATH at top level (for file source)
    if (json.isMember("FILE_PATH") && json["FILE_PATH"].isString()) {
        req.additionalParams["FILE_PATH"] = json["FILE_PATH"].asString();
    }
    
    // Also check for RTMP_URL at top level (for RTMP destination)
    if (json.isMember("RTMP_URL") && json["RTMP_URL"].isString()) {
        req.additionalParams["RTMP_URL"] = json["RTMP_URL"].asString();
    }
    
    // Also check for SFACE_MODEL_PATH at top level (for SFace encoder)
    if (json.isMember("SFACE_MODEL_PATH") && json["SFACE_MODEL_PATH"].isString()) {
        req.additionalParams["SFACE_MODEL_PATH"] = json["SFACE_MODEL_PATH"].asString();
    }
    
    // Also check for SFACE_MODEL_NAME at top level (for SFace encoder by name)
    if (json.isMember("SFACE_MODEL_NAME") && json["SFACE_MODEL_NAME"].isString()) {
        req.additionalParams["SFACE_MODEL_NAME"] = json["SFACE_MODEL_NAME"].asString();
    }
    
    return true;
}

Json::Value InstanceHandler::instanceInfoToJson(const InstanceInfo& info) const {
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
    
    // Add streaming URLs if available
    if (!info.rtmpUrl.empty()) {
        json["rtmpUrl"] = info.rtmpUrl;
    }
    if (!info.rtspUrl.empty()) {
        json["rtspUrl"] = info.rtspUrl;
    }
    
    return json;
}

HttpResponsePtr InstanceHandler::createErrorResponse(
    int statusCode,
    const std::string& error,
    const std::string& message) const {
    
    Json::Value errorJson;
    errorJson["error"] = error;
    if (!message.empty()) {
        errorJson["message"] = message;
    }
    
    auto resp = HttpResponse::newHttpJsonResponse(errorJson);
    resp->setStatusCode(static_cast<HttpStatusCode>(statusCode));
    
    return resp;
}

void InstanceHandler::batchStartInstances(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
    
    try {
        // Check if registry is set
        if (!instance_registry_) {
            callback(createErrorResponse(500, "Internal server error", "Instance registry not initialized"));
            return;
        }
        
        // Parse JSON body
        auto json = req->getJsonObject();
        if (!json) {
            callback(createErrorResponse(400, "Invalid request", "Request body must be valid JSON"));
            return;
        }
        
        // Extract instance IDs from request
        if (!json->isMember("instanceIds") || !(*json)["instanceIds"].isArray()) {
            callback(createErrorResponse(400, "Invalid request", "Request body must contain 'instanceIds' array"));
            return;
        }
        
        std::vector<std::string> instanceIds;
        const Json::Value& idsArray = (*json)["instanceIds"];
        for (const auto& id : idsArray) {
            if (id.isString()) {
                instanceIds.push_back(id.asString());
            }
        }
        
        if (instanceIds.empty()) {
            callback(createErrorResponse(400, "Invalid request", "At least one instance ID is required"));
            return;
        }
        
        // Execute start operations concurrently using async
        std::vector<std::future<std::pair<std::string, bool>>> futures;
        for (const auto& instanceId : instanceIds) {
            futures.push_back(std::async(std::launch::async, [this, instanceId]() -> std::pair<std::string, bool> {
                try {
                    bool success = instance_registry_->startInstance(instanceId);
                    return {instanceId, success};
                } catch (...) {
                    return {instanceId, false};
                }
            }));
        }
        
        // Collect results
        Json::Value response;
        Json::Value results(Json::arrayValue);
        int successCount = 0;
        int failureCount = 0;
        
        for (auto& future : futures) {
            auto [instanceId, success] = future.get();
            
            Json::Value result;
            result["instanceId"] = instanceId;
            result["success"] = success;
            
            if (success) {
                auto optInfo = instance_registry_->getInstance(instanceId);
                if (optInfo.has_value()) {
                    result["status"] = "started";
                    result["running"] = optInfo.value().running;
                }
                successCount++;
            } else {
                result["status"] = "failed";
                result["error"] = "Could not start instance. Check if instance exists and has a pipeline.";
                failureCount++;
            }
            
            results.append(result);
        }
        
        response["results"] = results;
        response["total"] = static_cast<int>(instanceIds.size());
        response["success"] = successCount;
        response["failed"] = failureCount;
        response["message"] = "Batch start operation completed";
        
        callback(createSuccessResponse(response));
        
    } catch (const std::exception& e) {
        std::cerr << "[InstanceHandler] Exception in batchStartInstances: " << e.what() << std::endl;
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        std::cerr << "[InstanceHandler] Unknown exception in batchStartInstances" << std::endl;
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void InstanceHandler::batchStopInstances(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
    
    try {
        // Check if registry is set
        if (!instance_registry_) {
            callback(createErrorResponse(500, "Internal server error", "Instance registry not initialized"));
            return;
        }
        
        // Parse JSON body
        auto json = req->getJsonObject();
        if (!json) {
            callback(createErrorResponse(400, "Invalid request", "Request body must be valid JSON"));
            return;
        }
        
        // Extract instance IDs from request
        if (!json->isMember("instanceIds") || !(*json)["instanceIds"].isArray()) {
            callback(createErrorResponse(400, "Invalid request", "Request body must contain 'instanceIds' array"));
            return;
        }
        
        std::vector<std::string> instanceIds;
        const Json::Value& idsArray = (*json)["instanceIds"];
        for (const auto& id : idsArray) {
            if (id.isString()) {
                instanceIds.push_back(id.asString());
            }
        }
        
        if (instanceIds.empty()) {
            callback(createErrorResponse(400, "Invalid request", "At least one instance ID is required"));
            return;
        }
        
        // Execute stop operations concurrently using async
        std::vector<std::future<std::pair<std::string, bool>>> futures;
        for (const auto& instanceId : instanceIds) {
            futures.push_back(std::async(std::launch::async, [this, instanceId]() -> std::pair<std::string, bool> {
                try {
                    bool success = instance_registry_->stopInstance(instanceId);
                    return {instanceId, success};
                } catch (...) {
                    return {instanceId, false};
                }
            }));
        }
        
        // Collect results
        Json::Value response;
        Json::Value results(Json::arrayValue);
        int successCount = 0;
        int failureCount = 0;
        
        for (auto& future : futures) {
            auto [instanceId, success] = future.get();
            
            Json::Value result;
            result["instanceId"] = instanceId;
            result["success"] = success;
            
            if (success) {
                auto optInfo = instance_registry_->getInstance(instanceId);
                if (optInfo.has_value()) {
                    result["status"] = "stopped";
                    result["running"] = optInfo.value().running;
                }
                successCount++;
            } else {
                result["status"] = "failed";
                result["error"] = "Could not stop instance. Check if instance exists.";
                failureCount++;
            }
            
            results.append(result);
        }
        
        response["results"] = results;
        response["total"] = static_cast<int>(instanceIds.size());
        response["success"] = successCount;
        response["failed"] = failureCount;
        response["message"] = "Batch stop operation completed";
        
        callback(createSuccessResponse(response));
        
    } catch (const std::exception& e) {
        std::cerr << "[InstanceHandler] Exception in batchStopInstances: " << e.what() << std::endl;
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        std::cerr << "[InstanceHandler] Unknown exception in batchStopInstances" << std::endl;
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void InstanceHandler::batchRestartInstances(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
    
    try {
        // Check if registry is set
        if (!instance_registry_) {
            callback(createErrorResponse(500, "Internal server error", "Instance registry not initialized"));
            return;
        }
        
        // Parse JSON body
        auto json = req->getJsonObject();
        if (!json) {
            callback(createErrorResponse(400, "Invalid request", "Request body must be valid JSON"));
            return;
        }
        
        // Extract instance IDs from request
        if (!json->isMember("instanceIds") || !(*json)["instanceIds"].isArray()) {
            callback(createErrorResponse(400, "Invalid request", "Request body must contain 'instanceIds' array"));
            return;
        }
        
        std::vector<std::string> instanceIds;
        const Json::Value& idsArray = (*json)["instanceIds"];
        for (const auto& id : idsArray) {
            if (id.isString()) {
                instanceIds.push_back(id.asString());
            }
        }
        
        if (instanceIds.empty()) {
            callback(createErrorResponse(400, "Invalid request", "At least one instance ID is required"));
            return;
        }
        
        // Execute restart operations concurrently using async
        // Restart = stop then start
        std::vector<std::future<std::pair<std::string, bool>>> futures;
        for (const auto& instanceId : instanceIds) {
            futures.push_back(std::async(std::launch::async, [this, instanceId]() -> std::pair<std::string, bool> {
                try {
                    // First stop the instance
                    bool stopSuccess = instance_registry_->stopInstance(instanceId);
                    if (!stopSuccess) {
                        return {instanceId, false};
                    }
                    
                    // Wait a moment for cleanup
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    
                    // Then start the instance
                    bool startSuccess = instance_registry_->startInstance(instanceId, true); // skipAutoStop=true
                    return {instanceId, startSuccess};
                } catch (...) {
                    return {instanceId, false};
                }
            }));
        }
        
        // Collect results
        Json::Value response;
        Json::Value results(Json::arrayValue);
        int successCount = 0;
        int failureCount = 0;
        
        for (auto& future : futures) {
            auto [instanceId, success] = future.get();
            
            Json::Value result;
            result["instanceId"] = instanceId;
            result["success"] = success;
            
            if (success) {
                auto optInfo = instance_registry_->getInstance(instanceId);
                if (optInfo.has_value()) {
                    result["status"] = "restarted";
                    result["running"] = optInfo.value().running;
                }
                successCount++;
            } else {
                result["status"] = "failed";
                result["error"] = "Could not restart instance. Check if instance exists and has a pipeline.";
                failureCount++;
            }
            
            results.append(result);
        }
        
        response["results"] = results;
        response["total"] = static_cast<int>(instanceIds.size());
        response["success"] = successCount;
        response["failed"] = failureCount;
        response["message"] = "Batch restart operation completed";
        
        callback(createSuccessResponse(response));
        
    } catch (const std::exception& e) {
        std::cerr << "[InstanceHandler] Exception in batchRestartInstances: " << e.what() << std::endl;
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        std::cerr << "[InstanceHandler] Unknown exception in batchRestartInstances" << std::endl;
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void InstanceHandler::getInstanceOutput(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
    
    try {
        std::cerr << "[InstanceHandler] getInstanceOutput called" << std::endl;
        
        // Check if registry is set
        if (!instance_registry_) {
            std::cerr << "[InstanceHandler] Error: Instance registry not initialized" << std::endl;
            callback(createErrorResponse(500, "Internal server error", "Instance registry not initialized"));
            return;
        }
        
        // Get instance ID from path parameter
        std::string instanceId = extractInstanceId(req);
        std::cerr << "[InstanceHandler] Extracted instance ID: " << instanceId << std::endl;
        
        if (instanceId.empty()) {
            std::cerr << "[InstanceHandler] Error: Instance ID is empty" << std::endl;
            callback(createErrorResponse(400, "Invalid request", "Instance ID is required"));
            return;
        }
        
        // Get instance info
        std::cerr << "[InstanceHandler] Getting instance info for: " << instanceId << std::endl;
        auto optInfo = instance_registry_->getInstance(instanceId);
        if (!optInfo.has_value()) {
            std::cerr << "[InstanceHandler] Error: Instance not found: " << instanceId << std::endl;
            callback(createErrorResponse(404, "Not found", "Instance not found: " + instanceId));
            return;
        }
        
        std::cerr << "[InstanceHandler] Instance found, building response..." << std::endl;
        
        const InstanceInfo& info = optInfo.value();
        
        // Build output response
        Json::Value response;
        
        // Timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        response["timestamp"] = ss.str();
        
        // Basic instance info
        response["instanceId"] = info.instanceId;
        response["displayName"] = info.displayName;
        response["solutionId"] = info.solutionId;
        response["solutionName"] = info.solutionName;
        response["running"] = info.running;
        response["loaded"] = info.loaded;
        
        // Processing metrics
        Json::Value metrics;
        metrics["fps"] = info.fps;
        metrics["frameRateLimit"] = info.frameRateLimit;
        response["metrics"] = metrics;
        
        // Input source
        Json::Value input;
        if (!info.filePath.empty()) {
            input["type"] = "FILE";
            input["path"] = info.filePath;
        } else if (info.additionalParams.find("RTSP_URL") != info.additionalParams.end()) {
            input["type"] = "RTSP";
            input["url"] = info.additionalParams.at("RTSP_URL");
        } else if (info.additionalParams.find("FILE_PATH") != info.additionalParams.end()) {
            input["type"] = "FILE";
            input["path"] = info.additionalParams.at("FILE_PATH");
        } else {
            input["type"] = "UNKNOWN";
        }
        response["input"] = input;
        
        // Output information
        Json::Value output;
        bool hasRTMP = instance_registry_->hasRTMPOutput(instanceId);
        
        if (hasRTMP) {
            output["type"] = "RTMP_STREAM";
            if (!info.rtmpUrl.empty()) {
                output["rtmpUrl"] = info.rtmpUrl;
            } else if (info.additionalParams.find("RTMP_URL") != info.additionalParams.end()) {
                output["rtmpUrl"] = info.additionalParams.at("RTMP_URL");
            }
            if (!info.rtspUrl.empty()) {
                output["rtspUrl"] = info.rtspUrl;
            }
        } else {
            output["type"] = "FILE";
            // Get file output information
            Json::Value fileInfo = getOutputFileInfo(instanceId);
            output["files"] = fileInfo;
        }
        response["output"] = output;
        
        // Detection settings
        Json::Value detection;
        detection["sensitivity"] = info.detectionSensitivity;
        detection["mode"] = info.detectorMode;
        detection["movementSensitivity"] = info.movementSensitivity;
        detection["sensorModality"] = info.sensorModality;
        response["detection"] = detection;
        
        // Processing modes
        Json::Value modes;
        modes["statisticsMode"] = info.statisticsMode;
        modes["metadataMode"] = info.metadataMode;
        modes["debugMode"] = info.debugMode;
        modes["diagnosticsMode"] = info.diagnosticsMode;
        response["modes"] = modes;
        
        // Status summary
        Json::Value status;
        status["running"] = info.running;
        status["processing"] = (info.running && info.fps > 0);
        if (info.running) {
            if (info.fps > 0) {
                status["message"] = "Instance is running and processing frames";
            } else {
                status["message"] = "Instance is running but not processing frames (may be initializing)";
            }
        } else {
            status["message"] = "Instance is stopped";
        }
        response["status"] = status;
        
        std::cerr << "[InstanceHandler] Response built successfully, sending callback..." << std::endl;
        callback(createSuccessResponse(response));
        std::cerr << "[InstanceHandler] getInstanceOutput completed successfully" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "[InstanceHandler] Exception in getInstanceOutput: " << e.what() << std::endl;
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        std::cerr << "[InstanceHandler] Unknown exception in getInstanceOutput" << std::endl;
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

Json::Value InstanceHandler::getOutputFileInfo(const std::string& instanceId) const {
    Json::Value fileInfo;
    
    // Check common output directories
    std::vector<std::string> outputDirs = {
        "./output/" + instanceId,
        "./build/output/" + instanceId,
        "output/" + instanceId,
        "build/output/" + instanceId
    };
    
    fs::path outputDir;
    bool found = false;
    
    for (const auto& dir : outputDirs) {
        fs::path testPath(dir);
        if (fs::exists(testPath) && fs::is_directory(testPath)) {
            outputDir = testPath;
            found = true;
            break;
        }
    }
    
    if (!found) {
        fileInfo["exists"] = false;
        fileInfo["message"] = "Output directory not found";
        fileInfo["expectedPaths"] = Json::arrayValue;
        for (const auto& dir : outputDirs) {
            fileInfo["expectedPaths"].append(dir);
        }
        return fileInfo;
    }
    
    fileInfo["exists"] = true;
    fileInfo["directory"] = outputDir.string();
    
    // Count files - OPTIMIZED: Single pass through directory
    int fileCount = 0;
    int totalSize = 0;
    std::string latestFile;
    std::time_t latestTime = 0;
    int recentFileCount = 0;
    auto now = std::chrono::system_clock::now();
    
    try {
        // Single iteration to collect all file information
        for (const auto& entry : fs::directory_iterator(outputDir)) {
            if (fs::is_regular_file(entry)) {
                fileCount++;
                
                try {
                    // Get file size
                    auto fileSize = fs::file_size(entry);
                    totalSize += static_cast<int>(fileSize);
                    
                    // Get modification time
                    auto fileTime = fs::last_write_time(entry);
                    // Convert file_time_type to system_clock::time_point
                    auto fileTimeDuration = fileTime.time_since_epoch();
                    auto systemTimeDuration = std::chrono::duration_cast<std::chrono::system_clock::duration>(fileTimeDuration);
                    auto systemTimePoint = std::chrono::system_clock::time_point(systemTimeDuration);
                    auto timeT = std::chrono::system_clock::to_time_t(systemTimePoint);
                    
                    // Check if this is the latest file
                    if (timeT > latestTime) {
                        latestTime = timeT;
                        latestFile = entry.path().filename().string();
                    }
                    
                    // Check if file was created recently (within last minute)
                    auto age = std::chrono::duration_cast<std::chrono::seconds>(now - systemTimePoint).count();
                    if (age < 60) {
                        recentFileCount++;
                    }
                } catch (const std::exception& e) {
                    // Skip this file if we can't read its metadata
                    // Continue with next file
                }
            }
        }
    } catch (const std::exception& e) {
        fileInfo["error"] = std::string("Error reading directory: ") + e.what();
    }
    
    fileInfo["fileCount"] = fileCount;
    fileInfo["totalSizeBytes"] = totalSize;
    
    // Format total size
    std::string sizeStr;
    if (totalSize < 1024) {
        sizeStr = std::to_string(totalSize) + " B";
    } else if (totalSize < 1024 * 1024) {
        sizeStr = std::to_string(totalSize / 1024) + " KB";
    } else {
        sizeStr = std::to_string(totalSize / (1024 * 1024)) + " MB";
    }
    fileInfo["totalSize"] = sizeStr;
    
    if (!latestFile.empty()) {
        fileInfo["latestFile"] = latestFile;
        
        // Format latest file time
        std::stringstream ss;
        ss << std::put_time(std::localtime(&latestTime), "%Y-%m-%d %H:%M:%S");
        fileInfo["latestFileTime"] = ss.str();
    }
    
    fileInfo["recentFileCount"] = recentFileCount;
    fileInfo["isActive"] = (recentFileCount > 0);
    
    return fileInfo;
}

