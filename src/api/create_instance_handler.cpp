#include "api/create_instance_handler.h"
#include "models/create_instance_request.h"
#include "instances/instance_registry.h"
#include "instances/instance_info.h"
#include <drogon/HttpResponse.h>
#include <json/json.h>
#include <sstream>

InstanceRegistry* CreateInstanceHandler::instance_registry_ = nullptr;

void CreateInstanceHandler::setInstanceRegistry(InstanceRegistry* registry) {
    instance_registry_ = registry;
}

void CreateInstanceHandler::createInstance(
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
        
        // Parse request
        CreateInstanceRequest createReq;
        std::string parseError;
        if (!parseRequest(*json, createReq, parseError)) {
            callback(createErrorResponse(400, "Invalid request", parseError));
            return;
        }
        
        // Validate request
        if (!createReq.validate()) {
            callback(createErrorResponse(400, "Validation failed", createReq.getValidationError()));
            return;
        }
        
        // Create instance
        std::string instanceId = instance_registry_->createInstance(createReq);
        if (instanceId.empty()) {
            callback(createErrorResponse(500, "Failed to create instance", "Could not create instance. Check solution ID and parameters."));
            return;
        }
        
        // Get instance info
        auto optInfo = instance_registry_->getInstance(instanceId);
        if (!optInfo.has_value()) {
            callback(createErrorResponse(500, "Internal server error", "Instance created but could not retrieve info"));
            return;
        }
        
        // Build response
        Json::Value response = instanceInfoToJson(optInfo.value());
        
        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k201Created);
        
        // Add CORS headers
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
        
        callback(resp);
        
    } catch (const std::exception& e) {
        std::cerr << "[CreateInstanceHandler] Exception: " << e.what() << std::endl;
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
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
    
    return resp;
}

