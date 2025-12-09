#include "api/config_handler.h"
#include "config/system_config.h"
#include "core/logging_flags.h"
#include "core/logger.h"
#include <drogon/HttpResponse.h>
#include <chrono>
#include <sstream>
#include <algorithm>

void ConfigHandler::getConfig(const HttpRequestPtr &req,
                              std::function<void(const HttpResponsePtr &)> &&callback) {
    // Check if path query parameter exists - if yes, route to getConfigSection
    std::string pathParam = req->getParameter("path");
    if (!pathParam.empty()) {
        getConfigSection(req, std::move(callback));
        return;
    }
    
    auto start_time = std::chrono::steady_clock::now();
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] GET /v1/core/config - Get full configuration";
    }
    
    try {
        auto& config = SystemConfig::getInstance();
        Json::Value configJson = config.getConfigJson();
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_INFO << "[API] GET /v1/core/config - Success - " << duration.count() << "ms";
        }
        
        callback(createSuccessResponse(configJson));
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] GET /v1/core/config - Exception: " << e.what() << " - " << duration.count() << "ms";
        }
        
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] GET /v1/core/config - Unknown exception - " << duration.count() << "ms";
        }
        
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void ConfigHandler::getConfigSection(const HttpRequestPtr &req,
                                    std::function<void(const HttpResponsePtr &)> &&callback) {
    auto start_time = std::chrono::steady_clock::now();
    
    // Extract path from query parameter (preferred) or URL path
    std::string path = req->getParameter("path");
    if (path.empty()) {
        path = extractPath(req);
    }
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] GET /v1/core/config/" << path << " - Get configuration section";
    }
    
    try {
        if (path.empty()) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] GET /v1/core/config/{path} - Empty path";
            }
            callback(createErrorResponse(400, "Invalid request", "Path parameter is required"));
            return;
        }
        
        auto& config = SystemConfig::getInstance();
        Json::Value section = config.getConfigSection(path);
        
        if (section.isNull()) {
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] GET /v1/core/config/" << path << " - Not found - " << duration.count() << "ms";
            }
            
            callback(createErrorResponse(404, "Not found", "Configuration section not found: " + path));
            return;
        }
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_INFO << "[API] GET /v1/core/config/" << path << " - Success - " << duration.count() << "ms";
        }
        
        callback(createSuccessResponse(section));
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] GET /v1/core/config/" << path << " - Exception: " << e.what() << " - " << duration.count() << "ms";
        }
        
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] GET /v1/core/config/" << path << " - Unknown exception - " << duration.count() << "ms";
        }
        
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void ConfigHandler::createOrUpdateConfig(const HttpRequestPtr &req,
                                        std::function<void(const HttpResponsePtr &)> &&callback) {
    auto start_time = std::chrono::steady_clock::now();
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] POST /v1/core/config - Create or update configuration";
    }
    
    try {
        auto json = req->getJsonObject();
        if (!json) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] POST /v1/core/config - Error: Invalid JSON body";
            }
            callback(createErrorResponse(400, "Invalid request", "Request body must be valid JSON"));
            return;
        }
        
        std::string error;
        if (!validateConfigJson(*json, error)) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] POST /v1/core/config - Validation failed: " << error;
            }
            callback(createErrorResponse(400, "Validation failed", error));
            return;
        }
        
        auto& config = SystemConfig::getInstance();
        if (!config.updateConfig(*json)) {
            if (isApiLoggingEnabled()) {
                PLOG_ERROR << "[API] POST /v1/core/config - Failed to update configuration";
            }
            callback(createErrorResponse(500, "Internal server error", "Failed to update configuration"));
            return;
        }
        
        // Save to file
        if (!config.saveConfig()) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] POST /v1/core/config - Updated but failed to save to file";
            }
        }
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_INFO << "[API] POST /v1/core/config - Success - " << duration.count() << "ms";
        }
        
        Json::Value response;
        response["message"] = "Configuration updated successfully";
        response["config"] = config.getConfigJson();
        
        callback(createSuccessResponse(response, 200));
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] POST /v1/core/config - Exception: " << e.what() << " - " << duration.count() << "ms";
        }
        
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] POST /v1/core/config - Unknown exception - " << duration.count() << "ms";
        }
        
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void ConfigHandler::replaceConfig(const HttpRequestPtr &req,
                                 std::function<void(const HttpResponsePtr &)> &&callback) {
    auto start_time = std::chrono::steady_clock::now();
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] PUT /v1/core/config - Replace entire configuration";
    }
    
    try {
        auto json = req->getJsonObject();
        if (!json) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] PUT /v1/core/config - Error: Invalid JSON body";
            }
            callback(createErrorResponse(400, "Invalid request", "Request body must be valid JSON"));
            return;
        }
        
        std::string error;
        if (!validateConfigJson(*json, error)) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] PUT /v1/core/config - Validation failed: " << error;
            }
            callback(createErrorResponse(400, "Validation failed", error));
            return;
        }
        
        auto& config = SystemConfig::getInstance();
        if (!config.replaceConfig(*json)) {
            if (isApiLoggingEnabled()) {
                PLOG_ERROR << "[API] PUT /v1/core/config - Failed to replace configuration";
            }
            callback(createErrorResponse(500, "Internal server error", "Failed to replace configuration"));
            return;
        }
        
        // Save to file
        if (!config.saveConfig()) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] PUT /v1/core/config - Replaced but failed to save to file";
            }
        }
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_INFO << "[API] PUT /v1/core/config - Success - " << duration.count() << "ms";
        }
        
        Json::Value response;
        response["message"] = "Configuration replaced successfully";
        response["config"] = config.getConfigJson();
        
        callback(createSuccessResponse(response, 200));
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] PUT /v1/core/config - Exception: " << e.what() << " - " << duration.count() << "ms";
        }
        
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] PUT /v1/core/config - Unknown exception - " << duration.count() << "ms";
        }
        
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void ConfigHandler::updateConfigSection(const HttpRequestPtr &req,
                                       std::function<void(const HttpResponsePtr &)> &&callback) {
    auto start_time = std::chrono::steady_clock::now();
    
    // Extract path from query parameter (preferred) or URL path
    std::string path = req->getParameter("path");
    if (path.empty()) {
        path = extractPath(req);
    }
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] PATCH /v1/core/config/" << path << " - Update configuration section";
    }
    
    try {
        if (path.empty()) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] PATCH /v1/core/config/{path} - Empty path";
            }
            callback(createErrorResponse(400, "Invalid request", "Path parameter is required"));
            return;
        }
        
        auto json = req->getJsonObject();
        if (!json) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] PATCH /v1/core/config/" << path << " - Error: Invalid JSON body";
            }
            callback(createErrorResponse(400, "Invalid request", "Request body must be valid JSON"));
            return;
        }
        
        auto& config = SystemConfig::getInstance();
        if (!config.updateConfigSection(path, *json)) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] PATCH /v1/core/config/" << path << " - Failed to update section";
            }
            callback(createErrorResponse(500, "Internal server error", "Failed to update configuration section"));
            return;
        }
        
        // Save to file
        if (!config.saveConfig()) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] PATCH /v1/core/config/" << path << " - Updated but failed to save to file";
            }
        }
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_INFO << "[API] PATCH /v1/core/config/" << path << " - Success - " << duration.count() << "ms";
        }
        
        Json::Value response;
        response["message"] = "Configuration section updated successfully";
        response["path"] = path;
        response["value"] = config.getConfigSection(path);
        
        callback(createSuccessResponse(response, 200));
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] PATCH /v1/core/config/" << path << " - Exception: " << e.what() << " - " << duration.count() << "ms";
        }
        
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] PATCH /v1/core/config/" << path << " - Unknown exception - " << duration.count() << "ms";
        }
        
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void ConfigHandler::deleteConfigSection(const HttpRequestPtr &req,
                                       std::function<void(const HttpResponsePtr &)> &&callback) {
    auto start_time = std::chrono::steady_clock::now();
    
    // Extract path from query parameter (preferred) or URL path
    std::string path = req->getParameter("path");
    if (path.empty()) {
        path = extractPath(req);
    }
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] DELETE /v1/core/config/" << path << " - Delete configuration section";
    }
    
    try {
        if (path.empty()) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] DELETE /v1/core/config/{path} - Empty path";
            }
            callback(createErrorResponse(400, "Invalid request", "Path parameter is required"));
            return;
        }
        
        auto& config = SystemConfig::getInstance();
        if (!config.deleteConfigSection(path)) {
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] DELETE /v1/core/config/" << path << " - Not found - " << duration.count() << "ms";
            }
            
            callback(createErrorResponse(404, "Not found", "Configuration section not found: " + path));
            return;
        }
        
        // Save to file
        if (!config.saveConfig()) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] DELETE /v1/core/config/" << path << " - Deleted but failed to save to file";
            }
        }
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_INFO << "[API] DELETE /v1/core/config/" << path << " - Success - " << duration.count() << "ms";
        }
        
        Json::Value response;
        response["message"] = "Configuration section deleted successfully";
        response["path"] = path;
        
        callback(createSuccessResponse(response, 200));
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] DELETE /v1/core/config/" << path << " - Exception: " << e.what() << " - " << duration.count() << "ms";
        }
        
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] DELETE /v1/core/config/" << path << " - Unknown exception - " << duration.count() << "ms";
        }
        
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void ConfigHandler::resetConfig(const HttpRequestPtr &req,
                                std::function<void(const HttpResponsePtr &)> &&callback) {
    auto start_time = std::chrono::steady_clock::now();
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] POST /v1/core/config/reset - Reset configuration to defaults";
    }
    
    try {
        auto& config = SystemConfig::getInstance();
        if (!config.resetToDefaults()) {
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] POST /v1/core/config/reset - Failed to reset configuration - " << duration.count() << "ms";
            }
            
            callback(createErrorResponse(500, "Internal server error", "Failed to reset configuration to defaults"));
            return;
        }
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_INFO << "[API] POST /v1/core/config/reset - Success - " << duration.count() << "ms";
        }
        
        Json::Value response;
        response["message"] = "Configuration reset to defaults successfully";
        response["config"] = config.getConfigJson();
        
        callback(createSuccessResponse(response, 200));
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] POST /v1/core/config/reset - Exception: " << e.what() << " - " << duration.count() << "ms";
        }
        
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] POST /v1/core/config/reset - Unknown exception - " << duration.count() << "ms";
        }
        
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void ConfigHandler::handleOptions(const HttpRequestPtr &req,
                                 std::function<void(const HttpResponsePtr &)> &&callback) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, PATCH, DELETE, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    resp->addHeader("Access-Control-Max-Age", "3600");
    callback(resp);
}

std::string ConfigHandler::extractPath(const HttpRequestPtr &req) const {
    // First, try query parameter (most reliable for paths with slashes)
    std::string path = req->getParameter("path");
    
    // If not in query parameter, try to extract from URL path
    if (path.empty()) {
        std::string fullPath = req->getPath();
        size_t configPos = fullPath.find("/config/");
        
        if (configPos != std::string::npos) {
            size_t start = configPos + 8; // length of "/config/"
            path = fullPath.substr(start);
        }
    }
    
    // URL decode the path if it contains encoded characters
    if (!path.empty()) {
        std::string decoded;
        decoded.reserve(path.length());
        for (size_t i = 0; i < path.length(); ++i) {
            if (path[i] == '%' && i + 2 < path.length()) {
                // Try to decode hex value
                char hex[3] = {path[i+1], path[i+2], '\0'};
                char* end;
                unsigned long value = std::strtoul(hex, &end, 16);
                if (*end == '\0' && value <= 255) {
                    decoded += static_cast<char>(value);
                    i += 2; // Skip the hex digits
                } else {
                    decoded += path[i]; // Invalid encoding, keep as-is
                }
            } else {
                decoded += path[i];
            }
        }
        path = decoded;
    }
    
    return path;
}

HttpResponsePtr ConfigHandler::createErrorResponse(int statusCode,
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
    resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, PATCH, DELETE, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    
    return resp;
}

HttpResponsePtr ConfigHandler::createSuccessResponse(const Json::Value& data, int statusCode) const {
    auto resp = HttpResponse::newHttpJsonResponse(data);
    resp->setStatusCode(static_cast<HttpStatusCode>(statusCode));
    
    // Add CORS headers
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, PATCH, DELETE, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    
    return resp;
}

bool ConfigHandler::validateConfigJson(const Json::Value& json, std::string& error) const {
    // Basic validation - must be an object
    if (!json.isObject()) {
        error = "Configuration must be a JSON object";
        return false;
    }
    
    // Optional: Add more specific validations here
    // For now, we allow any valid JSON object
    
    return true;
}

