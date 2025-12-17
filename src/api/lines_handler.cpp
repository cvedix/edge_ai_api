#include "api/lines_handler.h"
#include "instances/instance_registry.h"
#include "core/uuid_generator.h"
#include "core/logging_flags.h"
#include "core/logger.h"
#include <drogon/HttpResponse.h>
#include <json/reader.h>
#include <json/writer.h>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <thread>

InstanceRegistry* LinesHandler::instance_registry_ = nullptr;

void LinesHandler::setInstanceRegistry(InstanceRegistry* registry) {
    instance_registry_ = registry;
}

std::string LinesHandler::extractInstanceId(const HttpRequestPtr &req) const {
    std::string instanceId = req->getParameter("instanceId");
    
    if (instanceId.empty()) {
        std::string path = req->getPath();
        size_t instancePos = path.find("/instance/");
        if (instancePos != std::string::npos) {
            size_t start = instancePos + 10; // length of "/instance/"
            size_t end = path.find("/", start);
            if (end == std::string::npos) {
                end = path.length();
            }
            instanceId = path.substr(start, end - start);
        }
    }
    
    return instanceId;
}

std::string LinesHandler::extractLineId(const HttpRequestPtr &req) const {
    std::string lineId = req->getParameter("lineId");
    
    if (lineId.empty()) {
        std::string path = req->getPath();
        size_t linesPos = path.find("/lines/");
        if (linesPos != std::string::npos) {
            size_t start = linesPos + 7; // length of "/lines/"
            size_t end = path.find("/", start);
            if (end == std::string::npos) {
                end = path.length();
            }
            lineId = path.substr(start, end - start);
        }
    }
    
    return lineId;
}

HttpResponsePtr LinesHandler::createErrorResponse(int statusCode,
                                                 const std::string& error,
                                                 const std::string& message) const {
    Json::Value errorJson;
    errorJson["error"] = error;
    if (!message.empty()) {
        errorJson["message"] = message;
    }
    
    auto resp = HttpResponse::newHttpJsonResponse(errorJson);
    resp->setStatusCode(static_cast<HttpStatusCode>(statusCode));
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    return resp;
}

HttpResponsePtr LinesHandler::createSuccessResponse(const Json::Value& data, int statusCode) const {
    auto resp = HttpResponse::newHttpJsonResponse(data);
    resp->setStatusCode(static_cast<HttpStatusCode>(statusCode));
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    return resp;
}

Json::Value LinesHandler::loadLinesFromConfig(const std::string& instanceId) const {
    Json::Value linesArray(Json::arrayValue);
    
    if (!instance_registry_) {
        return linesArray;
    }
    
    auto optInfo = instance_registry_->getInstance(instanceId);
    if (!optInfo.has_value()) {
        return linesArray;
    }
    
    const auto& info = optInfo.value();
    auto it = info.additionalParams.find("CrossingLines");
    if (it != info.additionalParams.end() && !it->second.empty()) {
        // Parse JSON string to JSON array
        Json::Reader reader;
        Json::Value parsedLines;
        if (reader.parse(it->second, parsedLines) && parsedLines.isArray()) {
            return parsedLines;
        }
    }
    
    return linesArray;
}

bool LinesHandler::saveLinesToConfig(const std::string& instanceId, const Json::Value& lines) const {
    if (!instance_registry_) {
        return false;
    }
    
    // Convert lines array to JSON string
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";  // Compact format
    std::string linesJsonStr = Json::writeString(builder, lines);
    
    // Create config update JSON
    Json::Value configUpdate(Json::objectValue);
    Json::Value additionalParams(Json::objectValue);
    additionalParams["CrossingLines"] = linesJsonStr;
    configUpdate["AdditionalParams"] = additionalParams;
    
    // Update instance config
    return instance_registry_->updateInstanceFromConfig(instanceId, configUpdate);
}

bool LinesHandler::validateCoordinates(const Json::Value& coordinates, std::string& error) const {
    if (!coordinates.isArray()) {
        error = "Coordinates must be an array";
        return false;
    }
    
    if (coordinates.size() < 2) {
        error = "Coordinates must contain at least 2 points";
        return false;
    }
    
    for (const auto& coord : coordinates) {
        if (!coord.isObject()) {
            error = "Each coordinate must be an object";
            return false;
        }
        
        if (!coord.isMember("x") || !coord.isMember("y")) {
            error = "Each coordinate must have 'x' and 'y' fields";
            return false;
        }
        
        if (!coord["x"].isNumeric() || !coord["y"].isNumeric()) {
            error = "Coordinate 'x' and 'y' must be numbers";
            return false;
        }
    }
    
    return true;
}

bool LinesHandler::validateDirection(const std::string& direction, std::string& error) const {
    std::string directionLower = direction;
    std::transform(directionLower.begin(), directionLower.end(), directionLower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    
    if (directionLower != "up" && directionLower != "down" && directionLower != "both") {
        error = "Direction must be one of: Up, Down, Both";
        return false;
    }
    
    return true;
}

bool LinesHandler::validateClasses(const Json::Value& classes, std::string& error) const {
    if (!classes.isArray()) {
        error = "Classes must be an array";
        return false;
    }
    
    static const std::vector<std::string> allowedClasses = {
        "Person", "Animal", "Vehicle", "Face", "Unknown"
    };
    
    for (const auto& cls : classes) {
        if (!cls.isString()) {
            error = "Each class must be a string";
            return false;
        }
        
        std::string classStr = cls.asString();
        bool found = false;
        for (const auto& allowed : allowedClasses) {
            if (classStr == allowed) {
                found = true;
                break;
            }
        }
        
        if (!found) {
            error = "Invalid class: " + classStr + ". Allowed values: Person, Animal, Vehicle, Face, Unknown";
            return false;
        }
    }
    
    return true;
}

bool LinesHandler::validateColor(const Json::Value& color, std::string& error) const {
    if (!color.isArray()) {
        error = "Color must be an array";
        return false;
    }
    
    if (color.size() != 4) {
        error = "Color must contain exactly 4 values (RGBA)";
        return false;
    }
    
    for (const auto& val : color) {
        if (!val.isNumeric()) {
            error = "Each color value must be a number";
            return false;
        }
    }
    
    return true;
}

void LinesHandler::getAllLines(const HttpRequestPtr &req,
                               std::function<void(const HttpResponsePtr &)> &&callback) {
    auto start_time = std::chrono::steady_clock::now();
    
    std::string instanceId = extractInstanceId(req);
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] GET /v1/core/instance/" << instanceId << "/lines - Get all lines";
        PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
    }
    
    try {
        if (!instance_registry_) {
            if (isApiLoggingEnabled()) {
                PLOG_ERROR << "[API] GET /v1/core/instance/" << instanceId << "/lines - Error: Instance registry not initialized";
            }
            callback(createErrorResponse(500, "Internal server error", "Instance registry not initialized"));
            return;
        }
        
        if (instanceId.empty()) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] GET /v1/core/instance/{instanceId}/lines - Error: Instance ID is empty";
            }
            callback(createErrorResponse(400, "Bad request", "Instance ID is required"));
            return;
        }
        
        // Check if instance exists
        auto optInfo = instance_registry_->getInstance(instanceId);
        if (!optInfo.has_value()) {
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] GET /v1/core/instance/" << instanceId << "/lines - Instance not found - " << duration.count() << "ms";
            }
            callback(createErrorResponse(404, "Not found", "Instance not found: " + instanceId));
            return;
        }
        
        // Load lines from config
        Json::Value linesArray = loadLinesFromConfig(instanceId);
        
        // Build response
        Json::Value response;
        response["crossingLines"] = linesArray;
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_INFO << "[API] GET /v1/core/instance/" << instanceId << "/lines - Success: " 
                      << linesArray.size() << " lines - " << duration.count() << "ms";
        }
        
        callback(createSuccessResponse(response));
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] GET /v1/core/instance/" << instanceId << "/lines - Exception: " 
                      << e.what() << " - " << duration.count() << "ms";
        }
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] GET /v1/core/instance/" << instanceId << "/lines - Unknown exception - " << duration.count() << "ms";
        }
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void LinesHandler::createLine(const HttpRequestPtr &req,
                              std::function<void(const HttpResponsePtr &)> &&callback) {
    auto start_time = std::chrono::steady_clock::now();
    
    std::string instanceId = extractInstanceId(req);
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] POST /v1/core/instance/" << instanceId << "/lines - Create line";
        PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
    }
    
    try {
        if (!instance_registry_) {
            if (isApiLoggingEnabled()) {
                PLOG_ERROR << "[API] POST /v1/core/instance/" << instanceId << "/lines - Error: Instance registry not initialized";
            }
            callback(createErrorResponse(500, "Internal server error", "Instance registry not initialized"));
            return;
        }
        
        if (instanceId.empty()) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] POST /v1/core/instance/{instanceId}/lines - Error: Instance ID is empty";
            }
            callback(createErrorResponse(400, "Bad request", "Instance ID is required"));
            return;
        }
        
        // Check if instance exists
        auto optInfo = instance_registry_->getInstance(instanceId);
        if (!optInfo.has_value()) {
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] POST /v1/core/instance/" << instanceId << "/lines - Instance not found - " << duration.count() << "ms";
            }
            callback(createErrorResponse(404, "Not found", "Instance not found: " + instanceId));
            return;
        }
        
        // Parse JSON body
        auto json = req->getJsonObject();
        if (!json) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] POST /v1/core/instance/" << instanceId << "/lines - Error: Invalid JSON body";
            }
            callback(createErrorResponse(400, "Bad request", "Request body must be valid JSON"));
            return;
        }
        
        // Validate required fields
        if (!json->isMember("coordinates") || !(*json)["coordinates"].isArray()) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] POST /v1/core/instance/" << instanceId << "/lines - Error: Missing or invalid 'coordinates' field";
            }
            callback(createErrorResponse(400, "Bad request", "Field 'coordinates' is required and must be an array"));
            return;
        }
        
        // Validate coordinates
        std::string coordError;
        if (!validateCoordinates((*json)["coordinates"], coordError)) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] POST /v1/core/instance/" << instanceId << "/lines - Validation error: " << coordError;
            }
            callback(createErrorResponse(400, "Bad request", coordError));
            return;
        }
        
        // Validate optional fields
        if (json->isMember("direction") && (*json)["direction"].isString()) {
            std::string dirError;
            if (!validateDirection((*json)["direction"].asString(), dirError)) {
                if (isApiLoggingEnabled()) {
                    PLOG_WARNING << "[API] POST /v1/core/instance/" << instanceId << "/lines - Validation error: " << dirError;
                }
                callback(createErrorResponse(400, "Bad request", dirError));
                return;
            }
        }
        
        if (json->isMember("classes") && (*json)["classes"].isArray()) {
            std::string classesError;
            if (!validateClasses((*json)["classes"], classesError)) {
                if (isApiLoggingEnabled()) {
                    PLOG_WARNING << "[API] POST /v1/core/instance/" << instanceId << "/lines - Validation error: " << classesError;
                }
                callback(createErrorResponse(400, "Bad request", classesError));
                return;
            }
        }
        
        if (json->isMember("color") && (*json)["color"].isArray()) {
            std::string colorError;
            if (!validateColor((*json)["color"], colorError)) {
                if (isApiLoggingEnabled()) {
                    PLOG_WARNING << "[API] POST /v1/core/instance/" << instanceId << "/lines - Validation error: " << colorError;
                }
                callback(createErrorResponse(400, "Bad request", colorError));
                return;
            }
        }
        
        // Load existing lines
        Json::Value linesArray = loadLinesFromConfig(instanceId);
        
        // Create new line object
        Json::Value newLine(Json::objectValue);
        newLine["id"] = UUIDGenerator::generateUUID();
        
        if (json->isMember("name") && (*json)["name"].isString()) {
            newLine["name"] = (*json)["name"];
        }
        
        newLine["coordinates"] = (*json)["coordinates"];
        
        if (json->isMember("classes") && (*json)["classes"].isArray()) {
            newLine["classes"] = (*json)["classes"];
        } else {
            newLine["classes"] = Json::Value(Json::arrayValue);
        }
        
        if (json->isMember("direction") && (*json)["direction"].isString()) {
            newLine["direction"] = (*json)["direction"];
        } else {
            newLine["direction"] = "Both";
        }
        
        if (json->isMember("color") && (*json)["color"].isArray()) {
            newLine["color"] = (*json)["color"];
        } else {
            Json::Value defaultColor(Json::arrayValue);
            defaultColor.append(255); // R
            defaultColor.append(0);   // G
            defaultColor.append(0);   // B
            defaultColor.append(255); // A
            newLine["color"] = defaultColor;
        }
        
        // Append new line to array
        linesArray.append(newLine);
        
        // Save to config
        if (!saveLinesToConfig(instanceId, linesArray)) {
            if (isApiLoggingEnabled()) {
                PLOG_ERROR << "[API] POST /v1/core/instance/" << instanceId << "/lines - Failed to save lines to config";
            }
            callback(createErrorResponse(500, "Internal server error", "Failed to save lines configuration"));
            return;
        }
        
        // Restart instance to apply line changes (real-time update)
        restartInstanceForLineUpdate(instanceId);
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_INFO << "[API] POST /v1/core/instance/" << instanceId << "/lines - Success: Created line " 
                      << newLine["id"].asString() << " - " << duration.count() << "ms";
        }
        
        callback(createSuccessResponse(newLine, 201));
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] POST /v1/core/instance/" << instanceId << "/lines - Exception: " 
                      << e.what() << " - " << duration.count() << "ms";
        }
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] POST /v1/core/instance/" << instanceId << "/lines - Unknown exception - " << duration.count() << "ms";
        }
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void LinesHandler::deleteAllLines(const HttpRequestPtr &req,
                                  std::function<void(const HttpResponsePtr &)> &&callback) {
    auto start_time = std::chrono::steady_clock::now();
    
    std::string instanceId = extractInstanceId(req);
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] DELETE /v1/core/instance/" << instanceId << "/lines - Delete all lines";
        PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
    }
    
    try {
        if (!instance_registry_) {
            if (isApiLoggingEnabled()) {
                PLOG_ERROR << "[API] DELETE /v1/core/instance/" << instanceId << "/lines - Error: Instance registry not initialized";
            }
            callback(createErrorResponse(500, "Internal server error", "Instance registry not initialized"));
            return;
        }
        
        if (instanceId.empty()) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] DELETE /v1/core/instance/{instanceId}/lines - Error: Instance ID is empty";
            }
            callback(createErrorResponse(400, "Bad request", "Instance ID is required"));
            return;
        }
        
        // Check if instance exists
        auto optInfo = instance_registry_->getInstance(instanceId);
        if (!optInfo.has_value()) {
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] DELETE /v1/core/instance/" << instanceId << "/lines - Instance not found - " << duration.count() << "ms";
            }
            callback(createErrorResponse(404, "Not found", "Instance not found: " + instanceId));
            return;
        }
        
        // Save empty array to config
        Json::Value emptyArray(Json::arrayValue);
        if (!saveLinesToConfig(instanceId, emptyArray)) {
            if (isApiLoggingEnabled()) {
                PLOG_ERROR << "[API] DELETE /v1/core/instance/" << instanceId << "/lines - Failed to save lines to config";
            }
            callback(createErrorResponse(500, "Internal server error", "Failed to save lines configuration"));
            return;
        }
        
        // Restart instance to apply line changes (real-time update)
        restartInstanceForLineUpdate(instanceId);
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_INFO << "[API] DELETE /v1/core/instance/" << instanceId << "/lines - Success - " << duration.count() << "ms";
        }
        
        Json::Value response;
        response["message"] = "All lines deleted successfully";
        callback(createSuccessResponse(response));
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] DELETE /v1/core/instance/" << instanceId << "/lines - Exception: " 
                      << e.what() << " - " << duration.count() << "ms";
        }
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] DELETE /v1/core/instance/" << instanceId << "/lines - Unknown exception - " << duration.count() << "ms";
        }
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void LinesHandler::deleteLine(const HttpRequestPtr &req,
                               std::function<void(const HttpResponsePtr &)> &&callback) {
    auto start_time = std::chrono::steady_clock::now();
    
    std::string instanceId = extractInstanceId(req);
    std::string lineId = extractLineId(req);
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] DELETE /v1/core/instance/" << instanceId << "/lines/" << lineId << " - Delete line";
        PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
    }
    
    try {
        if (!instance_registry_) {
            if (isApiLoggingEnabled()) {
                PLOG_ERROR << "[API] DELETE /v1/core/instance/" << instanceId << "/lines/" << lineId << " - Error: Instance registry not initialized";
            }
            callback(createErrorResponse(500, "Internal server error", "Instance registry not initialized"));
            return;
        }
        
        if (instanceId.empty()) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] DELETE /v1/core/instance/{instanceId}/lines/{lineId} - Error: Instance ID is empty";
            }
            callback(createErrorResponse(400, "Bad request", "Instance ID is required"));
            return;
        }
        
        if (lineId.empty()) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] DELETE /v1/core/instance/" << instanceId << "/lines/{lineId} - Error: Line ID is empty";
            }
            callback(createErrorResponse(400, "Bad request", "Line ID is required"));
            return;
        }
        
        // Check if instance exists
        auto optInfo = instance_registry_->getInstance(instanceId);
        if (!optInfo.has_value()) {
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] DELETE /v1/core/instance/" << instanceId << "/lines/" << lineId << " - Instance not found - " << duration.count() << "ms";
            }
            callback(createErrorResponse(404, "Not found", "Instance not found: " + instanceId));
            return;
        }
        
        // Load existing lines
        Json::Value linesArray = loadLinesFromConfig(instanceId);
        
        // Find and remove line with matching ID
        Json::Value newLinesArray(Json::arrayValue);
        bool found = false;
        
        for (const auto& line : linesArray) {
            if (line.isObject() && line.isMember("id") && line["id"].isString()) {
                if (line["id"].asString() == lineId) {
                    found = true;
                    // Skip this line (don't add to new array)
                    continue;
                }
            }
            newLinesArray.append(line);
        }
        
        if (!found) {
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] DELETE /v1/core/instance/" << instanceId << "/lines/" << lineId << " - Line not found - " << duration.count() << "ms";
            }
            callback(createErrorResponse(404, "Not found", "Line not found: " + lineId));
            return;
        }
        
        // Save updated lines to config
        if (!saveLinesToConfig(instanceId, newLinesArray)) {
            if (isApiLoggingEnabled()) {
                PLOG_ERROR << "[API] DELETE /v1/core/instance/" << instanceId << "/lines/" << lineId << " - Failed to save lines to config";
            }
            callback(createErrorResponse(500, "Internal server error", "Failed to save lines configuration"));
            return;
        }
        
        // Restart instance to apply line changes (real-time update)
        restartInstanceForLineUpdate(instanceId);
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_INFO << "[API] DELETE /v1/core/instance/" << instanceId << "/lines/" << lineId << " - Success - " << duration.count() << "ms";
        }
        
        Json::Value response;
        response["message"] = "Line deleted successfully";
        callback(createSuccessResponse(response));
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] DELETE /v1/core/instance/" << instanceId << "/lines/" << lineId << " - Exception: " 
                      << e.what() << " - " << duration.count() << "ms";
        }
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] DELETE /v1/core/instance/" << instanceId << "/lines/" << lineId << " - Unknown exception - " << duration.count() << "ms";
        }
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void LinesHandler::handleOptions(const HttpRequestPtr &req,
                                 std::function<void(const HttpResponsePtr &)> &&callback) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    resp->addHeader("Access-Control-Max-Age", "3600");
    callback(resp);
}

bool LinesHandler::restartInstanceForLineUpdate(const std::string& instanceId) const {
    if (!instance_registry_) {
        return false;
    }
    
    // Check if instance is running
    auto optInfo = instance_registry_->getInstance(instanceId);
    if (!optInfo.has_value() || !optInfo.value().running) {
        // Instance not running, no need to restart
        return true;
    }
    
    // Restart instance in background thread to apply line changes
    std::thread restartThread([this, instanceId]() {
        try {
            if (isApiLoggingEnabled()) {
                PLOG_INFO << "[API] Restarting instance " << instanceId << " to apply line changes";
            }
            
            // Stop instance
            instance_registry_->stopInstance(instanceId);
            
            // Wait for cleanup
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // Start instance again
            instance_registry_->startInstance(instanceId, true);
            
            if (isApiLoggingEnabled()) {
                PLOG_INFO << "[API] Instance " << instanceId << " restarted successfully for line update";
            }
        } catch (const std::exception& e) {
            PLOG_ERROR << "[API] Failed to restart instance " << instanceId << " for line update: " << e.what();
        } catch (...) {
            PLOG_ERROR << "[API] Unknown error restarting instance " << instanceId << " for line update";
        }
    });
    restartThread.detach();
    
    return true;
}

