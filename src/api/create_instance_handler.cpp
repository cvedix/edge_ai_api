#include "api/create_instance_handler.h"
#include "models/create_instance_request.h"
#include "instances/instance_registry.h"
#include "instances/instance_info.h"
#include "solutions/solution_registry.h"
#include "core/logging_flags.h"
#include "core/logger.h"
#include <drogon/HttpResponse.h>
#include <json/json.h>
#include <sstream>
#include <chrono>

InstanceRegistry* CreateInstanceHandler::instance_registry_ = nullptr;
SolutionRegistry* CreateInstanceHandler::solution_registry_ = nullptr;

void CreateInstanceHandler::setInstanceRegistry(InstanceRegistry* registry) {
    instance_registry_ = registry;
}

void CreateInstanceHandler::setSolutionRegistry(SolutionRegistry* registry) {
    solution_registry_ = registry;
}

void CreateInstanceHandler::createInstance(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
    
    auto start_time = std::chrono::steady_clock::now();
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] POST /v1/core/instance - Create instance";
        PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
    }
    
    try {
        // Check if registry is set
        if (!instance_registry_) {
            if (isApiLoggingEnabled()) {
                PLOG_ERROR << "[API] POST /v1/core/instance - Error: Instance registry not initialized";
            }
            callback(createErrorResponse(500, "Internal server error", "Instance registry not initialized"));
            return;
        }
        
        // Parse JSON body
        auto json = req->getJsonObject();
        if (!json) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] POST /v1/core/instance - Error: Invalid JSON body";
            }
            callback(createErrorResponse(400, "Invalid request", "Request body must be valid JSON"));
            return;
        }
        
        // Parse request
        CreateInstanceRequest createReq;
        std::string parseError;
        if (!parseRequest(*json, createReq, parseError)) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] POST /v1/core/instance - Parse error: " << parseError;
            }
            callback(createErrorResponse(400, "Invalid request", parseError));
            return;
        }
        
        // Validate request
        if (!createReq.validate()) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] POST /v1/core/instance - Validation failed: " << createReq.getValidationError();
            }
            callback(createErrorResponse(400, "Validation failed", createReq.getValidationError()));
            return;
        }
        
        // Validate solution if provided
        if (!createReq.solution.empty()) {
            if (!solution_registry_) {
                if (isApiLoggingEnabled()) {
                    PLOG_ERROR << "[API] POST /v1/core/instance - Error: Solution registry not initialized";
                }
                callback(createErrorResponse(500, "Internal server error", "Solution registry not initialized"));
                return;
            }
            
            if (!solution_registry_->hasSolution(createReq.solution)) {
                if (isApiLoggingEnabled()) {
                    PLOG_WARNING << "[API] POST /v1/core/instance - Solution not found: " << createReq.solution;
                }
                callback(createErrorResponse(400, "Invalid solution", 
                    "Solution not found: " + createReq.solution + ". Please check available solutions using GET /v1/core/solutions"));
                return;
            }
        }
        
        // Create instance
        std::string instanceId = instance_registry_->createInstance(createReq);
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (instanceId.empty()) {
            if (isApiLoggingEnabled()) {
                PLOG_ERROR << "[API] POST /v1/core/instance - Failed to create instance - " << duration.count() << "ms";
            }
            callback(createErrorResponse(500, "Failed to create instance", "Could not create instance. Check solution ID and parameters."));
            return;
        }
        
        // Get instance info
        auto optInfo = instance_registry_->getInstance(instanceId);
        if (!optInfo.has_value()) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] POST /v1/core/instance - Created but could not retrieve info - " << duration.count() << "ms";
            }
            callback(createErrorResponse(500, "Internal server error", "Instance created but could not retrieve info"));
            return;
        }
        
        // Build response
        Json::Value response = instanceInfoToJson(optInfo.value());
        
        if (isApiLoggingEnabled()) {
            const auto& info = optInfo.value();
            PLOG_INFO << "[API] POST /v1/core/instance - Success: Created instance " << instanceId 
                      << " (" << info.displayName << ", solution: " << info.solutionId 
                      << ") - " << duration.count() << "ms";
        }
        
        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k201Created);
        
        // Add CORS headers
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
        
        callback(resp);
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] POST /v1/core/instance - Exception: " << e.what() << " - " << duration.count() << "ms";
        }
        std::cerr << "[CreateInstanceHandler] Exception: " << e.what() << std::endl;
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] POST /v1/core/instance - Unknown exception - " << duration.count() << "ms";
        }
        std::cerr << "[CreateInstanceHandler] Unknown exception" << std::endl;
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void CreateInstanceHandler::handleOptions(const HttpRequestPtr &req,
                                         std::function<void(const HttpResponsePtr &)> &&callback)
{
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
    resp->addHeader("Access-Control-Max-Age", "3600");
    callback(resp);
}

bool CreateInstanceHandler::parseRequest(
    const Json::Value& json,
    CreateInstanceRequest& req,
    std::string& error) {
    
    // Required field: name
    if (!json.isMember("name") || !json["name"].isString()) {
        error = "Missing required field: name";
        return false;
    }
    req.name = json["name"].asString();
    
    // Optional fields
    if (json.isMember("group") && json["group"].isString()) {
        req.group = json["group"].asString();
    }
    
    if (json.isMember("solution") && json["solution"].isString()) {
        req.solution = json["solution"].asString();
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
    
    if (json.isMember("blockingReadaheadQueue") && json["blockingReadaheadQueue"].isBool()) {
        req.blockingReadaheadQueue = json["blockingReadaheadQueue"].asBool();
    }
    
    if (json.isMember("inputOrientation") && json["inputOrientation"].isNumeric()) {
        req.inputOrientation = json["inputOrientation"].asInt();
    }
    
    if (json.isMember("inputPixelLimit") && json["inputPixelLimit"].isNumeric()) {
        req.inputPixelLimit = json["inputPixelLimit"].asInt();
    }
    
    // Detector configuration (detailed)
    if (json.isMember("detectorModelFile") && json["detectorModelFile"].isString()) {
        req.detectorModelFile = json["detectorModelFile"].asString();
    }
    if (json.isMember("animalConfidenceThreshold") && json["animalConfidenceThreshold"].isNumeric()) {
        req.animalConfidenceThreshold = json["animalConfidenceThreshold"].asDouble();
    }
    if (json.isMember("personConfidenceThreshold") && json["personConfidenceThreshold"].isNumeric()) {
        req.personConfidenceThreshold = json["personConfidenceThreshold"].asDouble();
    }
    if (json.isMember("vehicleConfidenceThreshold") && json["vehicleConfidenceThreshold"].isNumeric()) {
        req.vehicleConfidenceThreshold = json["vehicleConfidenceThreshold"].asDouble();
    }
    if (json.isMember("faceConfidenceThreshold") && json["faceConfidenceThreshold"].isNumeric()) {
        req.faceConfidenceThreshold = json["faceConfidenceThreshold"].asDouble();
    }
    if (json.isMember("licensePlateConfidenceThreshold") && json["licensePlateConfidenceThreshold"].isNumeric()) {
        req.licensePlateConfidenceThreshold = json["licensePlateConfidenceThreshold"].asDouble();
    }
    if (json.isMember("confThreshold") && json["confThreshold"].isNumeric()) {
        req.confThreshold = json["confThreshold"].asDouble();
    }
    
    // DetectorThermal configuration
    if (json.isMember("detectorThermalModelFile") && json["detectorThermalModelFile"].isString()) {
        req.detectorThermalModelFile = json["detectorThermalModelFile"].asString();
    }
    
    // Performance mode
    if (json.isMember("performanceMode") && json["performanceMode"].isString()) {
        req.performanceMode = json["performanceMode"].asString();
    }
    
    // SolutionManager settings
    if (json.isMember("recommendedFrameRate") && json["recommendedFrameRate"].isNumeric()) {
        req.recommendedFrameRate = json["recommendedFrameRate"].asInt();
    }
    
    // Additional parameters (e.g., RTSP_URL)
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

Json::Value CreateInstanceHandler::instanceInfoToJson(const InstanceInfo& info) const {
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

HttpResponsePtr CreateInstanceHandler::createErrorResponse(
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
    resp->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    
    return resp;
}

