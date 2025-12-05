#include "api/instance_handler.h"
#include "models/update_instance_request.h"
#include "instances/instance_registry.h"
#include "instances/instance_info.h"
#include "core/logging_flags.h"
#include "core/logger.h"
#include <drogon/HttpResponse.h>
#include <sstream>
#include <thread>
#include <chrono>
#include <future>
#include <vector>
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <iomanip>
#include <ctime>
namespace fs = std::filesystem;

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
    
    auto start_time = std::chrono::steady_clock::now();
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] GET /v1/core/instances - List instances";
        PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
    }
    
    try {
        // Check if registry is set
        if (!instance_registry_) {
            if (isApiLoggingEnabled()) {
                PLOG_ERROR << "[API] GET /v1/core/instances - Error: Instance registry not initialized";
            }
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
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_INFO << "[API] GET /v1/core/instances - Success: " << totalCount 
                      << " instances (running: " << runningCount << ", stopped: " << stoppedCount 
                      << ") - " << duration.count() << "ms";
        }
        
        callback(createSuccessResponse(response));
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] GET /v1/core/instances - Exception: " << e.what() << " - " << duration.count() << "ms";
        }
        std::cerr << "[InstanceHandler] Exception: " << e.what() << std::endl;
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] GET /v1/core/instances - Unknown exception - " << duration.count() << "ms";
        }
        std::cerr << "[InstanceHandler] Unknown exception" << std::endl;
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void InstanceHandler::getInstance(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
    
    auto start_time = std::chrono::steady_clock::now();
    
    // Get instance ID from path parameter
    std::string instanceId = extractInstanceId(req);
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] GET /v1/core/instances/" << instanceId << " - Get instance details";
        PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
    }
    
    try {
        // Check if registry is set
        if (!instance_registry_) {
            if (isApiLoggingEnabled()) {
                PLOG_ERROR << "[API] GET /v1/core/instances/" << instanceId << " - Error: Instance registry not initialized";
            }
            callback(createErrorResponse(500, "Internal server error", "Instance registry not initialized"));
            return;
        }
        
        if (instanceId.empty()) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] GET /v1/core/instances/{id} - Error: Instance ID is empty";
            }
            callback(createErrorResponse(400, "Invalid request", "Instance ID is required"));
            return;
        }
        
        // Get instance info
        auto optInfo = instance_registry_->getInstance(instanceId);
        if (!optInfo.has_value()) {
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] GET /v1/core/instances/" << instanceId << " - Not found - " << duration.count() << "ms";
            }
            callback(createErrorResponse(404, "Not found", "Instance not found: " + instanceId));
            return;
        }
        
        // Build response
        Json::Value response = instanceInfoToJson(optInfo.value());
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            const auto& info = optInfo.value();
            PLOG_INFO << "[API] GET /v1/core/instances/" << instanceId 
                      << " - Success: " << info.displayName 
                      << " (running: " << (info.running ? "true" : "false")
                      << ", fps: " << std::fixed << std::setprecision(2) << info.fps
                      << ") - " << duration.count() << "ms";
        }
        
        callback(createSuccessResponse(response));
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] GET /v1/core/instances/" << instanceId << " - Exception: " << e.what() << " - " << duration.count() << "ms";
        }
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] GET /v1/core/instances/" << instanceId << " - Unknown exception - " << duration.count() << "ms";
        }
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void InstanceHandler::startInstance(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
    
    auto start_time = std::chrono::steady_clock::now();
    
    // Get instance ID from path parameter
    std::string instanceId = extractInstanceId(req);
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] POST /v1/core/instances/" << instanceId << "/start - Start instance";
    }
    
    try {
        // Check if registry is set
        if (!instance_registry_) {
            if (isApiLoggingEnabled()) {
                PLOG_ERROR << "[API] POST /v1/core/instances/" << instanceId << "/start - Error: Instance registry not initialized";
            }
            callback(createErrorResponse(500, "Internal server error", "Instance registry not initialized"));
            return;
        }
        
        if (instanceId.empty()) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] POST /v1/core/instances/{id}/start - Error: Instance ID is empty";
            }
            callback(createErrorResponse(400, "Invalid request", "Instance ID is required"));
            return;
        }
        
        // Start instance
        if (instance_registry_->startInstance(instanceId)) {
            // Get updated instance info
            auto optInfo = instance_registry_->getInstance(instanceId);
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            if (optInfo.has_value()) {
                if (isApiLoggingEnabled()) {
                    PLOG_INFO << "[API] POST /v1/core/instances/" << instanceId << "/start - Success - " << duration.count() << "ms";
                }
                Json::Value response = instanceInfoToJson(optInfo.value());
                response["message"] = "Instance started successfully";
                callback(createSuccessResponse(response));
            } else {
                if (isApiLoggingEnabled()) {
                    PLOG_WARNING << "[API] POST /v1/core/instances/" << instanceId << "/start - Started but could not retrieve info - " << duration.count() << "ms";
                }
                callback(createErrorResponse(500, "Internal server error", "Instance started but could not retrieve info"));
            }
        } else {
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] POST /v1/core/instances/" << instanceId << "/start - Failed to start - " << duration.count() << "ms";
            }
            callback(createErrorResponse(400, "Failed to start", "Could not start instance. Check if instance exists and has a pipeline."));
        }
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] POST /v1/core/instances/" << instanceId << "/start - Exception: " << e.what() << " - " << duration.count() << "ms";
        }
        std::cerr << "[InstanceHandler] Exception: " << e.what() << std::endl;
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] POST /v1/core/instances/" << instanceId << "/start - Unknown exception - " << duration.count() << "ms";
        }
        std::cerr << "[InstanceHandler] Unknown exception" << std::endl;
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void InstanceHandler::stopInstance(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
    
    auto start_time = std::chrono::steady_clock::now();
    
    // Get instance ID from path parameter
    std::string instanceId = extractInstanceId(req);
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] POST /v1/core/instances/" << instanceId << "/stop - Stop instance";
    }
    
    try {
        // Check if registry is set
        if (!instance_registry_) {
            if (isApiLoggingEnabled()) {
                PLOG_ERROR << "[API] POST /v1/core/instances/" << instanceId << "/stop - Error: Instance registry not initialized";
            }
            callback(createErrorResponse(500, "Internal server error", "Instance registry not initialized"));
            return;
        }
        
        if (instanceId.empty()) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] POST /v1/core/instances/{id}/stop - Error: Instance ID is empty";
            }
            callback(createErrorResponse(400, "Invalid request", "Instance ID is required"));
            return;
        }
        
        // Stop instance
        if (instance_registry_->stopInstance(instanceId)) {
            // Get updated instance info
            auto optInfo = instance_registry_->getInstance(instanceId);
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            if (optInfo.has_value()) {
                if (isApiLoggingEnabled()) {
                    PLOG_INFO << "[API] POST /v1/core/instances/" << instanceId << "/stop - Success - " << duration.count() << "ms";
                }
                Json::Value response = instanceInfoToJson(optInfo.value());
                response["message"] = "Instance stopped successfully";
                callback(createSuccessResponse(response));
            } else {
                if (isApiLoggingEnabled()) {
                    PLOG_WARNING << "[API] POST /v1/core/instances/" << instanceId << "/stop - Stopped but could not retrieve info - " << duration.count() << "ms";
                }
                callback(createErrorResponse(500, "Internal server error", "Instance stopped but could not retrieve info"));
            }
        } else {
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] POST /v1/core/instances/" << instanceId << "/stop - Failed to stop - " << duration.count() << "ms";
            }
            callback(createErrorResponse(400, "Failed to stop", "Could not stop instance. Check if instance exists."));
        }
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] POST /v1/core/instances/" << instanceId << "/stop - Exception: " << e.what() << " - " << duration.count() << "ms";
        }
        std::cerr << "[InstanceHandler] Exception: " << e.what() << std::endl;
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] POST /v1/core/instances/" << instanceId << "/stop - Unknown exception - " << duration.count() << "ms";
        }
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
        
        // Check if this is a direct config update (PascalCase format matching instance_detail.txt)
        // If JSON has top-level fields like "InstanceId", "DisplayName", "Detector", etc., 
        // it's a direct config update
        bool isDirectConfigUpdate = json->isMember("InstanceId") || 
                                    json->isMember("DisplayName") || 
                                    json->isMember("Detector") ||
                                    json->isMember("Input") ||
                                    json->isMember("Output") ||
                                    json->isMember("Zone");
        
        if (isDirectConfigUpdate) {
            // Direct config update - merge JSON directly into storage
            if (instance_registry_->updateInstanceFromConfig(instanceId, *json)) {
                // Get updated instance info
                auto optInfo = instance_registry_->getInstance(instanceId);
                if (optInfo.has_value()) {
                    Json::Value response = instanceInfoToJson(optInfo.value());
                    response["message"] = "Instance updated successfully";
                    callback(createSuccessResponse(response));
                } else {
                    callback(createErrorResponse(500, "Internal server error", "Failed to retrieve updated instance"));
                }
            } else {
                callback(createErrorResponse(500, "Internal server error", "Failed to update instance"));
            }
            return;
        }
        
        // Traditional update request (camelCase fields)
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
    resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    resp->addHeader("Access-Control-Max-Age", "3600");
    callback(resp);
}

bool InstanceHandler::parseUpdateRequest(
    const Json::Value& json,
    UpdateInstanceRequest& req,
    std::string& error) {
    
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
    if (json.isMember("AutoStart") && json["AutoStart"].isBool()) {
        req.autoStart = json["AutoStart"].asBool();
    }
    
    if (json.isMember("autoRestart") && json["autoRestart"].isBool()) {
        req.autoRestart = json["autoRestart"].asBool();
    }
    
    // Parse Detector nested object
    if (json.isMember("Detector") && json["Detector"].isObject()) {
        const Json::Value& detector = json["Detector"];
        if (detector.isMember("current_preset") && detector["current_preset"].isString()) {
            req.detectorMode = detector["current_preset"].asString();
        }
        if (detector.isMember("current_sensitivity_preset") && detector["current_sensitivity_preset"].isString()) {
            req.detectionSensitivity = detector["current_sensitivity_preset"].asString();
        }
        if (detector.isMember("model_file") && detector["model_file"].isString()) {
            req.additionalParams["DETECTOR_MODEL_FILE"] = detector["model_file"].asString();
        }
        if (detector.isMember("animal_confidence_threshold") && detector["animal_confidence_threshold"].isNumeric()) {
            req.additionalParams["ANIMAL_CONFIDENCE_THRESHOLD"] = std::to_string(detector["animal_confidence_threshold"].asDouble());
        }
        if (detector.isMember("person_confidence_threshold") && detector["person_confidence_threshold"].isNumeric()) {
            req.additionalParams["PERSON_CONFIDENCE_THRESHOLD"] = std::to_string(detector["person_confidence_threshold"].asDouble());
        }
        if (detector.isMember("vehicle_confidence_threshold") && detector["vehicle_confidence_threshold"].isNumeric()) {
            req.additionalParams["VEHICLE_CONFIDENCE_THRESHOLD"] = std::to_string(detector["vehicle_confidence_threshold"].asDouble());
        }
        if (detector.isMember("face_confidence_threshold") && detector["face_confidence_threshold"].isNumeric()) {
            req.additionalParams["FACE_CONFIDENCE_THRESHOLD"] = std::to_string(detector["face_confidence_threshold"].asDouble());
        }
        if (detector.isMember("license_plate_confidence_threshold") && detector["license_plate_confidence_threshold"].isNumeric()) {
            req.additionalParams["LICENSE_PLATE_CONFIDENCE_THRESHOLD"] = std::to_string(detector["license_plate_confidence_threshold"].asDouble());
        }
        if (detector.isMember("conf_threshold") && detector["conf_threshold"].isNumeric()) {
            req.additionalParams["CONF_THRESHOLD"] = std::to_string(detector["conf_threshold"].asDouble());
        }
    }
    
    // Parse Input nested object
    if (json.isMember("Input") && json["Input"].isObject()) {
        const Json::Value& input = json["Input"];
        if (input.isMember("uri") && input["uri"].isString()) {
            std::string uri = input["uri"].asString();
            // Extract RTSP URL from GStreamer URI
            size_t rtspPos = uri.find("uri=");
            if (rtspPos != std::string::npos) {
                size_t start = rtspPos + 4;
                size_t end = uri.find(" !", start);
                if (end == std::string::npos) {
                    end = uri.length();
                }
                req.additionalParams["RTSP_URL"] = uri.substr(start, end - start);
            } else if (uri.find("://") == std::string::npos) {
                // Direct file path
                req.additionalParams["FILE_PATH"] = uri;
            }
        }
        if (input.isMember("media_type") && input["media_type"].isString()) {
            req.additionalParams["INPUT_MEDIA_TYPE"] = input["media_type"].asString();
        }
    }
    
    // Parse Output nested object
    if (json.isMember("Output") && json["Output"].isObject()) {
        const Json::Value& output = json["Output"];
        if (output.isMember("JSONExport") && output["JSONExport"].isObject()) {
            if (output["JSONExport"].isMember("enabled") && output["JSONExport"]["enabled"].isBool()) {
                req.metadataMode = output["JSONExport"]["enabled"].asBool();
            }
        }
        if (output.isMember("handlers") && output["handlers"].isObject()) {
            // Parse RTSP handler if available
            for (const auto& handlerKey : output["handlers"].getMemberNames()) {
                const Json::Value& handler = output["handlers"][handlerKey];
                if (handler.isMember("uri") && handler["uri"].isString()) {
                    req.additionalParams["OUTPUT_RTSP_URL"] = handler["uri"].asString();
                }
                if (handler.isMember("config") && handler["config"].isObject()) {
                    if (handler["config"].isMember("fps") && handler["config"]["fps"].isNumeric()) {
                        req.frameRateLimit = handler["config"]["fps"].asInt();
                    }
                }
            }
        }
    }
    
    // Parse SolutionManager nested object
    if (json.isMember("SolutionManager") && json["SolutionManager"].isObject()) {
        const Json::Value& sm = json["SolutionManager"];
        if (sm.isMember("frame_rate_limit") && sm["frame_rate_limit"].isNumeric()) {
            req.frameRateLimit = sm["frame_rate_limit"].asInt();
        }
        if (sm.isMember("send_metadata") && sm["send_metadata"].isBool()) {
            req.metadataMode = sm["send_metadata"].asBool();
        }
        if (sm.isMember("run_statistics") && sm["run_statistics"].isBool()) {
            req.statisticsMode = sm["run_statistics"].asBool();
        }
        if (sm.isMember("send_diagnostics") && sm["send_diagnostics"].isBool()) {
            req.diagnosticsMode = sm["send_diagnostics"].asBool();
        }
        if (sm.isMember("enable_debug") && sm["enable_debug"].isBool()) {
            req.debugMode = sm["enable_debug"].asBool();
        }
        if (sm.isMember("input_pixel_limit") && sm["input_pixel_limit"].isNumeric()) {
            req.inputPixelLimit = sm["input_pixel_limit"].asInt();
        }
    }
    
    // Parse PerformanceMode nested object
    if (json.isMember("PerformanceMode") && json["PerformanceMode"].isObject()) {
        const Json::Value& pm = json["PerformanceMode"];
        if (pm.isMember("current_preset") && pm["current_preset"].isString()) {
            req.additionalParams["PERFORMANCE_MODE"] = pm["current_preset"].asString();
        }
    }
    
    // Parse DetectorThermal nested object
    if (json.isMember("DetectorThermal") && json["DetectorThermal"].isObject()) {
        const Json::Value& dt = json["DetectorThermal"];
        if (dt.isMember("model_file") && dt["model_file"].isString()) {
            req.additionalParams["DETECTOR_THERMAL_MODEL_FILE"] = dt["model_file"].asString();
        }
    }
    
    // Parse Zone nested object (store as JSON string for later processing)
    if (json.isMember("Zone") && json["Zone"].isObject()) {
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string zoneJson = Json::writeString(builder, json["Zone"]);
        req.additionalParams["ZONE_CONFIG"] = zoneJson;
    }
    
    // Parse Tripwire nested object
    if (json.isMember("Tripwire") && json["Tripwire"].isObject()) {
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string tripwireJson = Json::writeString(builder, json["Tripwire"]);
        req.additionalParams["TRIPWIRE_CONFIG"] = tripwireJson;
    }
    
    // Parse DetectorRegions nested object
    if (json.isMember("DetectorRegions") && json["DetectorRegions"].isObject()) {
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        std::string regionsJson = Json::writeString(builder, json["DetectorRegions"]);
        req.additionalParams["DETECTOR_REGIONS_CONFIG"] = regionsJson;
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
    
    // Format matching task/instance_detail.txt
    // Basic fields
    json["InstanceId"] = info.instanceId;
    json["DisplayName"] = info.displayName;
    json["AutoStart"] = info.autoStart;
    json["Solution"] = info.solutionId;
    
    // OriginatorInfo
    Json::Value originator(Json::objectValue);
    originator["address"] = info.originator.address.empty() ? "127.0.0.1" : info.originator.address;
    json["OriginatorInfo"] = originator;
    
    // Input configuration
    Json::Value input(Json::objectValue);
    if (!info.rtspUrl.empty()) {
        input["media_type"] = "IP Camera";
        input["uri"] = "gstreamer:///urisourcebin uri=" + info.rtspUrl + " ! decodebin ! videoconvert ! video/x-raw, format=NV12 ! appsink drop=true name=cvdsink";
    } else if (!info.filePath.empty()) {
        input["media_type"] = "File";
        input["uri"] = info.filePath;
    } else {
        input["media_type"] = "IP Camera";
        input["uri"] = "";
    }
    
    // Input media_format
    Json::Value mediaFormat(Json::objectValue);
    mediaFormat["color_format"] = 0;
    mediaFormat["default_format"] = true;
    mediaFormat["height"] = 0;
    mediaFormat["is_software"] = false;
    mediaFormat["name"] = "Same as Source";
    input["media_format"] = mediaFormat;
    json["Input"] = input;
    
    // Output configuration
    Json::Value output(Json::objectValue);
    output["JSONExport"]["enabled"] = info.metadataMode;
    output["NXWitness"]["enabled"] = false;
    
    // Output handlers (RTSP output if available)
    Json::Value handlers(Json::objectValue);
    if (!info.rtspUrl.empty() || !info.rtmpUrl.empty()) {
        // Create RTSP handler for output stream
        Json::Value rtspHandler(Json::objectValue);
        Json::Value handlerConfig(Json::objectValue);
        handlerConfig["debug"] = info.debugMode ? "4" : "0";
        handlerConfig["fps"] = info.frameRateLimit > 0 ? info.frameRateLimit : 10;
        handlerConfig["pipeline"] = "( appsrc name=cvedia-rt ! videoconvert ! videoscale ! x264enc ! video/x-h264,profile=high ! rtph264pay name=pay0 pt=96 )";
        rtspHandler["config"] = handlerConfig;
        rtspHandler["enabled"] = info.running;
        rtspHandler["sink"] = "output-image";
        
        // Use RTSP URL if available, otherwise construct from RTMP
        std::string outputUrl = info.rtspUrl;
        if (outputUrl.empty() && !info.rtmpUrl.empty()) {
            // Extract port and stream name from RTMP URL if possible
            outputUrl = "rtsp://0.0.0.0:8554/stream1";
        }
        if (outputUrl.empty()) {
            outputUrl = "rtsp://0.0.0.0:8554/stream1";
        }
        rtspHandler["uri"] = outputUrl;
        handlers["rtsp:--0.0.0.0:8554-stream1"] = rtspHandler;
    }
    output["handlers"] = handlers;
    output["render_preset"] = "Default";
    json["Output"] = output;
    
    // Detector configuration
    Json::Value detector(Json::objectValue);
    detector["animal_confidence_threshold"] = info.animalConfidenceThreshold > 0.0 ? info.animalConfidenceThreshold : 0.3;
    detector["conf_threshold"] = info.confThreshold > 0.0 ? info.confThreshold : 0.2;
    detector["current_preset"] = info.detectorMode.empty() ? "FullRegionInference" : info.detectorMode;
    detector["current_sensitivity_preset"] = info.detectionSensitivity.empty() ? "High" : info.detectionSensitivity;
    detector["face_confidence_threshold"] = info.faceConfidenceThreshold > 0.0 ? info.faceConfidenceThreshold : 0.1;
    detector["license_plate_confidence_threshold"] = info.licensePlateConfidenceThreshold > 0.0 ? info.licensePlateConfidenceThreshold : 0.1;
    detector["model_file"] = info.detectorModelFile.empty() ? "pva_det_full_frame_512" : info.detectorModelFile;
    detector["person_confidence_threshold"] = info.personConfidenceThreshold > 0.0 ? info.personConfidenceThreshold : 0.3;
    detector["vehicle_confidence_threshold"] = info.vehicleConfidenceThreshold > 0.0 ? info.vehicleConfidenceThreshold : 0.3;
    
    // Preset values
    Json::Value presetValues(Json::objectValue);
    Json::Value mosaicInference(Json::objectValue);
    mosaicInference["Detector/model_file"] = "pva_det_mosaic_320";
    presetValues["MosaicInference"] = mosaicInference;
    detector["preset_values"] = presetValues;
    
    json["Detector"] = detector;
    
    // DetectorRegions (empty by default)
    json["DetectorRegions"] = Json::Value(Json::objectValue);
    
    // DetectorThermal (always include)
    Json::Value detectorThermal(Json::objectValue);
    detectorThermal["model_file"] = info.detectorThermalModelFile.empty() ? "pva_det_mosaic_320" : info.detectorThermalModelFile;
    json["DetectorThermal"] = detectorThermal;
    
    // PerformanceMode
    Json::Value performanceMode(Json::objectValue);
    performanceMode["current_preset"] = info.performanceMode.empty() ? "Balanced" : info.performanceMode;
    json["PerformanceMode"] = performanceMode;
    
    // SolutionManager
    Json::Value solutionManager(Json::objectValue);
    solutionManager["enable_debug"] = info.debugMode;
    solutionManager["frame_rate_limit"] = info.frameRateLimit > 0 ? info.frameRateLimit : 15;
    solutionManager["input_pixel_limit"] = info.inputPixelLimit > 0 ? info.inputPixelLimit : 2000000;
    solutionManager["recommended_frame_rate"] = info.recommendedFrameRate > 0 ? info.recommendedFrameRate : 5;
    solutionManager["run_statistics"] = info.statisticsMode;
    solutionManager["send_diagnostics"] = info.diagnosticsMode;
    solutionManager["send_metadata"] = info.metadataMode;
    json["SolutionManager"] = solutionManager;
    
    // Tripwire (empty by default)
    Json::Value tripwire(Json::objectValue);
    tripwire["Tripwires"] = Json::Value(Json::objectValue);
    json["Tripwire"] = tripwire;
    
    // Zone (empty by default, can be populated from additionalParams if needed)
    Json::Value zone(Json::objectValue);
    zone["Zones"] = Json::Value(Json::objectValue);
    json["Zone"] = zone;
    
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
    
    // Add CORS headers to error responses
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    
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

