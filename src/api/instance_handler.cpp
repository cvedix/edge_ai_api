#include "api/instance_handler.h"
#include "instances/instance_registry.h"
#include "instances/instance_info.h"
#include <drogon/HttpResponse.h>
#include <sstream>

InstanceRegistry* InstanceHandler::instance_registry_ = nullptr;

void InstanceHandler::setInstanceRegistry(InstanceRegistry* registry) {
    instance_registry_ = registry;
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
        
        // Get all instance IDs
        std::vector<std::string> instanceIds = instance_registry_->listInstances();
        
        // Build response with summary information
        Json::Value response;
        Json::Value instances(Json::arrayValue);
        
        int totalCount = 0;
        int runningCount = 0;
        int stoppedCount = 0;
        
        for (const auto& instanceId : instanceIds) {
            auto optInfo = instance_registry_->getInstance(instanceId);
            if (optInfo.has_value()) {
                const auto& info = optInfo.value();
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
                
                instances.append(instance);
                totalCount++;
                if (info.running) {
                    runningCount++;
                } else {
                    stoppedCount++;
                }
            }
        }
        
        response["instances"] = instances;
        response["total"] = totalCount;
        response["running"] = runningCount;
        response["stopped"] = stoppedCount;
        
        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k200OK);
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->addHeader("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
        
        callback(resp);
        
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
        // Check if registry is set
        if (!instance_registry_) {
            callback(createErrorResponse(500, "Internal server error", "Instance registry not initialized"));
            return;
        }
        
        // Get instance ID from path parameter
        std::string instanceId = req->getParameter("instanceId");
        if (instanceId.empty()) {
            callback(createErrorResponse(400, "Invalid request", "Instance ID is required"));
            return;
        }
        
        // Get instance info
        auto optInfo = instance_registry_->getInstance(instanceId);
        if (!optInfo.has_value()) {
            callback(createErrorResponse(404, "Not found", "Instance not found: " + instanceId));
            return;
        }
        
        // Build response
        Json::Value response = instanceInfoToJson(optInfo.value());
        
        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k200OK);
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->addHeader("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
        
        callback(resp);
        
    } catch (const std::exception& e) {
        std::cerr << "[InstanceHandler] Exception: " << e.what() << std::endl;
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        std::cerr << "[InstanceHandler] Unknown exception" << std::endl;
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
        std::string instanceId = req->getParameter("instanceId");
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
                
                auto resp = HttpResponse::newHttpJsonResponse(response);
                resp->setStatusCode(k200OK);
                resp->addHeader("Access-Control-Allow-Origin", "*");
                resp->addHeader("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
                resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
                
                callback(resp);
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
        std::string instanceId = req->getParameter("instanceId");
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
                
                auto resp = HttpResponse::newHttpJsonResponse(response);
                resp->setStatusCode(k200OK);
                resp->addHeader("Access-Control-Allow-Origin", "*");
                resp->addHeader("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
                resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
                
                callback(resp);
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
        std::string instanceId = req->getParameter("instanceId");
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
            
            auto resp = HttpResponse::newHttpJsonResponse(response);
            resp->setStatusCode(k200OK);
            resp->addHeader("Access-Control-Allow-Origin", "*");
            resp->addHeader("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
            resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
            
            callback(resp);
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

