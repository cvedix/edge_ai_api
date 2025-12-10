#include "api/solution_handler.h"
#include "solutions/solution_registry.h"
#include "solutions/solution_storage.h"
#include "models/solution_config.h"
#include "core/logging_flags.h"
#include "core/logger.h"
#include <drogon/HttpResponse.h>
#include <regex>
#include <chrono>
#include <set>
#include <map>

SolutionRegistry* SolutionHandler::solution_registry_ = nullptr;
SolutionStorage* SolutionHandler::solution_storage_ = nullptr;

void SolutionHandler::setSolutionRegistry(SolutionRegistry* registry) {
    solution_registry_ = registry;
}

void SolutionHandler::setSolutionStorage(SolutionStorage* storage) {
    solution_storage_ = storage;
}

std::string SolutionHandler::extractSolutionId(const HttpRequestPtr &req) const {
    // Try getParameter first (standard way)
    std::string solutionId = req->getParameter("solutionId");
    
    // Fallback: extract from path if getParameter doesn't work
    if (solutionId.empty()) {
        std::string path = req->getPath();
        size_t solutionsPos = path.find("/solutions/");
        if (solutionsPos != std::string::npos) {
            size_t start = solutionsPos + 11; // length of "/solutions/"
            size_t end = path.find("/", start);
            if (end == std::string::npos) {
                end = path.length();
            }
            solutionId = path.substr(start, end - start);
        }
    }
    
    return solutionId;
}

bool SolutionHandler::validateSolutionId(const std::string& solutionId, std::string& error) const {
    if (solutionId.empty()) {
        error = "Solution ID cannot be empty";
        return false;
    }
    
    // Validate format: alphanumeric, underscore, hyphen only
    std::regex pattern("^[A-Za-z0-9_-]+$");
    if (!std::regex_match(solutionId, pattern)) {
        error = "Solution ID must contain only alphanumeric characters, underscores, and hyphens";
        return false;
    }
    
    return true;
}

Json::Value SolutionHandler::solutionConfigToJson(const SolutionConfig& config) const {
    Json::Value json(Json::objectValue);
    
    json["solutionId"] = config.solutionId;
    json["solutionName"] = config.solutionName;
    json["solutionType"] = config.solutionType;
    json["isDefault"] = config.isDefault;
    
    // Convert pipeline
    Json::Value pipeline(Json::arrayValue);
    for (const auto& node : config.pipeline) {
        Json::Value nodeJson(Json::objectValue);
        nodeJson["nodeType"] = node.nodeType;
        nodeJson["nodeName"] = node.nodeName;
        
        Json::Value params(Json::objectValue);
        for (const auto& param : node.parameters) {
            params[param.first] = param.second;
        }
        nodeJson["parameters"] = params;
        
        pipeline.append(nodeJson);
    }
    json["pipeline"] = pipeline;
    
    // Convert defaults
    Json::Value defaults(Json::objectValue);
    for (const auto& def : config.defaults) {
        defaults[def.first] = def.second;
    }
    json["defaults"] = defaults;
    
    return json;
}

std::optional<SolutionConfig> SolutionHandler::parseSolutionConfig(const Json::Value& json, std::string& error) const {
    try {
        SolutionConfig config;
        
        // Required: solutionId
        if (!json.isMember("solutionId") || !json["solutionId"].isString()) {
            error = "Missing required field: solutionId";
            return std::nullopt;
        }
        config.solutionId = json["solutionId"].asString();
        
        // Validate solutionId format
        std::string validationError;
        if (!validateSolutionId(config.solutionId, validationError)) {
            error = validationError;
            return std::nullopt;
        }
        
        // Required: solutionName
        if (!json.isMember("solutionName") || !json["solutionName"].isString()) {
            error = "Missing required field: solutionName";
            return std::nullopt;
        }
        config.solutionName = json["solutionName"].asString();
        
        if (config.solutionName.empty()) {
            error = "solutionName cannot be empty";
            return std::nullopt;
        }
        
        // Required: solutionType
        if (!json.isMember("solutionType") || !json["solutionType"].isString()) {
            error = "Missing required field: solutionType";
            return std::nullopt;
        }
        config.solutionType = json["solutionType"].asString();
        
        // SECURITY: Ignore isDefault from user input - users cannot create default solutions
        // Default solutions are hardcoded in the application and cannot be created via API
        // We explicitly ignore this field if provided by the user
            config.isDefault = false;
        
        // Required: pipeline
        if (!json.isMember("pipeline") || !json["pipeline"].isArray()) {
            error = "Missing required field: pipeline (must be an array)";
            return std::nullopt;
        }
        
        if (json["pipeline"].size() == 0) {
            error = "pipeline cannot be empty";
            return std::nullopt;
        }
        
        // Parse pipeline nodes
        for (const auto& nodeJson : json["pipeline"]) {
            if (!nodeJson.isObject()) {
                error = "Pipeline nodes must be objects";
                return std::nullopt;
            }
            
            SolutionConfig::NodeConfig node;
            
            // Required: nodeType
            if (!nodeJson.isMember("nodeType") || !nodeJson["nodeType"].isString()) {
                error = "Pipeline node missing required field: nodeType";
                return std::nullopt;
            }
            node.nodeType = nodeJson["nodeType"].asString();
            
            // Required: nodeName
            if (!nodeJson.isMember("nodeName") || !nodeJson["nodeName"].isString()) {
                error = "Pipeline node missing required field: nodeName";
                return std::nullopt;
            }
            node.nodeName = nodeJson["nodeName"].asString();
            
            // Optional: parameters
            if (nodeJson.isMember("parameters") && nodeJson["parameters"].isObject()) {
                for (const auto& key : nodeJson["parameters"].getMemberNames()) {
                    if (nodeJson["parameters"][key].isString()) {
                        node.parameters[key] = nodeJson["parameters"][key].asString();
                    } else {
                        error = "Pipeline node parameters must be strings";
                        return std::nullopt;
                    }
                }
            }
            
            config.pipeline.push_back(node);
        }
        
        // Optional: defaults
        if (json.isMember("defaults") && json["defaults"].isObject()) {
            for (const auto& key : json["defaults"].getMemberNames()) {
                if (json["defaults"][key].isString()) {
                    config.defaults[key] = json["defaults"][key].asString();
                }
            }
        }
        
        return config;
    } catch (const std::exception& e) {
        error = std::string("Error parsing solution config: ") + e.what();
        return std::nullopt;
    }
}

HttpResponsePtr SolutionHandler::createErrorResponse(int statusCode,
                                                     const std::string& error,
                                                     const std::string& message) const {
    Json::Value response(Json::objectValue);
    response["error"] = error;
    if (!message.empty()) {
        response["message"] = message;
    }
    
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(static_cast<HttpStatusCode>(statusCode));
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    
    return resp;
}

HttpResponsePtr SolutionHandler::createSuccessResponse(const Json::Value& data, int statusCode) const {
    auto resp = HttpResponse::newHttpJsonResponse(data);
    resp->setStatusCode(static_cast<HttpStatusCode>(statusCode));
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    return resp;
}

void SolutionHandler::listSolutions(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
    
    auto start_time = std::chrono::steady_clock::now();
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] GET /v1/core/solutions - List solutions";
        PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
    }
    
    try {
        if (!solution_registry_) {
            if (isApiLoggingEnabled()) {
                PLOG_ERROR << "[API] GET /v1/core/solutions - Error: Solution registry not initialized";
            }
            callback(createErrorResponse(500, "Internal server error", "Solution registry not initialized"));
            return;
        }
        
        // Get all solutions
        auto allSolutions = solution_registry_->getAllSolutions();
        
        // Build response
        Json::Value response;
        Json::Value solutions(Json::arrayValue);
        
        int totalCount = 0;
        int defaultCount = 0;
        int customCount = 0;
        
        for (const auto& [solutionId, config] : allSolutions) {
            Json::Value solution;
            solution["solutionId"] = config.solutionId;
            solution["solutionName"] = config.solutionName;
            solution["solutionType"] = config.solutionType;
            solution["isDefault"] = config.isDefault;
            solution["pipelineNodeCount"] = static_cast<int>(config.pipeline.size());
            
            solutions.append(solution);
            totalCount++;
            if (config.isDefault) {
                defaultCount++;
            } else {
                customCount++;
            }
        }
        
        response["solutions"] = solutions;
        response["total"] = totalCount;
        response["default"] = defaultCount;
        response["custom"] = customCount;
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_INFO << "[API] GET /v1/core/solutions - Success: " << totalCount 
                      << " solutions (default: " << defaultCount << ", custom: " << customCount 
                      << ") - " << duration.count() << "ms";
        }
        
        callback(createSuccessResponse(response));
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] GET /v1/core/solutions - Exception: " << e.what() << " - " << duration.count() << "ms";
        }
        std::cerr << "[SolutionHandler] Exception: " << e.what() << std::endl;
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] GET /v1/core/solutions - Unknown exception - " << duration.count() << "ms";
        }
        std::cerr << "[SolutionHandler] Unknown exception" << std::endl;
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void SolutionHandler::getSolution(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
    
    auto start_time = std::chrono::steady_clock::now();
    
    std::string solutionId = extractSolutionId(req);
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] GET /v1/core/solutions/" << solutionId << " - Get solution";
    }
    
    try {
        if (!solution_registry_) {
            if (isApiLoggingEnabled()) {
                PLOG_ERROR << "[API] GET /v1/core/solutions/" << solutionId << " - Error: Solution registry not initialized";
            }
            callback(createErrorResponse(500, "Internal server error", "Solution registry not initialized"));
            return;
        }
        
        if (solutionId.empty()) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] GET /v1/core/solutions/{id} - Error: Solution ID is empty";
            }
            callback(createErrorResponse(400, "Invalid request", "Solution ID is required"));
            return;
        }
        
        auto optConfig = solution_registry_->getSolution(solutionId);
        if (!optConfig.has_value()) {
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] GET /v1/core/solutions/" << solutionId << " - Not found - " << duration.count() << "ms";
            }
            callback(createErrorResponse(404, "Not found", "Solution not found: " + solutionId));
            return;
        }
        
        Json::Value response = solutionConfigToJson(optConfig.value());
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_INFO << "[API] GET /v1/core/solutions/" << solutionId << " - Success - " << duration.count() << "ms";
        }
        
        callback(createSuccessResponse(response));
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] GET /v1/core/solutions/" << solutionId << " - Exception: " << e.what() << " - " << duration.count() << "ms";
        }
        std::cerr << "[SolutionHandler] Exception: " << e.what() << std::endl;
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] GET /v1/core/solutions/" << solutionId << " - Unknown exception - " << duration.count() << "ms";
        }
        std::cerr << "[SolutionHandler] Unknown exception" << std::endl;
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void SolutionHandler::createSolution(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
    
    auto start_time = std::chrono::steady_clock::now();
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] POST /v1/core/solutions - Create solution";
        PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
    }
    
    try {
        if (!solution_registry_ || !solution_storage_) {
            if (isApiLoggingEnabled()) {
                PLOG_ERROR << "[API] POST /v1/core/solutions - Error: Solution registry or storage not initialized";
            }
            callback(createErrorResponse(500, "Internal server error", "Solution registry or storage not initialized"));
            return;
        }
        
        // Parse JSON body
        auto json = req->getJsonObject();
        if (!json) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] POST /v1/core/solutions - Error: Invalid JSON body";
            }
            callback(createErrorResponse(400, "Invalid request", "Request body must be valid JSON"));
            return;
        }
        
        // Parse solution config
        std::string parseError;
        auto optConfig = parseSolutionConfig(*json, parseError);
        if (!optConfig.has_value()) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] POST /v1/core/solutions - Parse error: " << parseError;
            }
            callback(createErrorResponse(400, "Invalid request", parseError));
            return;
        }
        
        SolutionConfig config = optConfig.value();
        
        // Ensure isDefault is false for custom solutions
        config.isDefault = false;
        
        // Check if solution already exists
        if (solution_registry_->hasSolution(config.solutionId)) {
            // Check if it's a default solution - cannot override default solutions
            if (solution_registry_->isDefaultSolution(config.solutionId)) {
                if (isApiLoggingEnabled()) {
                    PLOG_WARNING << "[API] POST /v1/core/solutions - Error: Cannot override default solution: " << config.solutionId;
                }
                callback(createErrorResponse(403, "Forbidden", 
                    "Cannot create solution with ID '" + config.solutionId + 
                    "': This ID is reserved for a default system solution. Please use a different solution ID."));
                return;
            }
            
            // Solution exists and is not default - return conflict
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] POST /v1/core/solutions - Error: Solution already exists: " << config.solutionId;
            }
            callback(createErrorResponse(409, "Conflict", 
                "Solution with ID '" + config.solutionId + 
                "' already exists. Use PUT /v1/core/solutions/" + config.solutionId + " to update it."));
            return;
        }
        
        // Register solution
        solution_registry_->registerSolution(config);
        
        // Save to storage
        std::cerr << "[SolutionHandler] Attempting to save solution to storage: " << config.solutionId << std::endl;
        if (!solution_storage_->saveSolution(config)) {
            std::cerr << "[SolutionHandler] Warning: Failed to save solution to storage: " << config.solutionId << std::endl;
        } else {
            std::cerr << "[SolutionHandler] ✓ Solution saved successfully to storage: " << config.solutionId << std::endl;
        }
        
        Json::Value response = solutionConfigToJson(config);
        response["message"] = "Solution created successfully";
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_INFO << "[API] POST /v1/core/solutions - Success: Created solution " << config.solutionId 
                      << " (" << config.solutionName << ") - " << duration.count() << "ms";
        }
        
        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k201Created);
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
        
        callback(resp);
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] POST /v1/core/solutions - Exception: " << e.what() << " - " << duration.count() << "ms";
        }
        std::cerr << "[SolutionHandler] Exception: " << e.what() << std::endl;
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] POST /v1/core/solutions - Unknown exception - " << duration.count() << "ms";
        }
        std::cerr << "[SolutionHandler] Unknown exception" << std::endl;
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void SolutionHandler::updateSolution(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
    
    auto start_time = std::chrono::steady_clock::now();
    
    std::string solutionId = extractSolutionId(req);
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] PUT /v1/core/solutions/" << solutionId << " - Update solution";
    }
    
    try {
        if (!solution_registry_ || !solution_storage_) {
            if (isApiLoggingEnabled()) {
                PLOG_ERROR << "[API] PUT /v1/core/solutions/" << solutionId << " - Error: Solution registry or storage not initialized";
            }
            callback(createErrorResponse(500, "Internal server error", "Solution registry or storage not initialized"));
            return;
        }
        
        if (solutionId.empty()) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] PUT /v1/core/solutions/{id} - Error: Solution ID is empty";
            }
            callback(createErrorResponse(400, "Invalid request", "Solution ID is required"));
            return;
        }
        
        // Check if solution exists
        if (!solution_registry_->hasSolution(solutionId)) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] PUT /v1/core/solutions/" << solutionId << " - Not found";
            }
            callback(createErrorResponse(404, "Not found", "Solution not found: " + solutionId));
            return;
        }
        
        // Check if it's a default solution
        if (solution_registry_->isDefaultSolution(solutionId)) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] PUT /v1/core/solutions/" << solutionId << " - Cannot update default solution";
            }
            callback(createErrorResponse(403, "Forbidden", "Cannot update default solution: " + solutionId));
            return;
        }
        
        // Parse JSON body
        auto json = req->getJsonObject();
        if (!json) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] PUT /v1/core/solutions/" << solutionId << " - Error: Invalid JSON body";
            }
            callback(createErrorResponse(400, "Invalid request", "Request body must be valid JSON"));
            return;
        }
        
        // Parse solution config
        std::string parseError;
        auto optConfig = parseSolutionConfig(*json, parseError);
        if (!optConfig.has_value()) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] PUT /v1/core/solutions/" << solutionId << " - Parse error: " << parseError;
            }
            callback(createErrorResponse(400, "Invalid request", parseError));
            return;
        }
        
        SolutionConfig config = optConfig.value();
        
        // Ensure solutionId matches
        if (config.solutionId != solutionId) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] PUT /v1/core/solutions/" << solutionId << " - Solution ID mismatch";
            }
            callback(createErrorResponse(400, "Invalid request", "Solution ID in body must match URL parameter"));
            return;
        }
        
        // Ensure isDefault is false for custom solutions
        config.isDefault = false;
        
        // Update solution
        if (!solution_registry_->updateSolution(config)) {
            if (isApiLoggingEnabled()) {
                PLOG_ERROR << "[API] PUT /v1/core/solutions/" << solutionId << " - Failed to update";
            }
            callback(createErrorResponse(500, "Internal server error", "Failed to update solution"));
            return;
        }
        
        // Save to storage
        std::cerr << "[SolutionHandler] Attempting to save updated solution to storage: " << config.solutionId << std::endl;
        if (!solution_storage_->saveSolution(config)) {
            std::cerr << "[SolutionHandler] Warning: Failed to save solution to storage: " << config.solutionId << std::endl;
        } else {
            std::cerr << "[SolutionHandler] ✓ Solution updated and saved successfully to storage: " << config.solutionId << std::endl;
        }
        
        Json::Value response = solutionConfigToJson(config);
        response["message"] = "Solution updated successfully";
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_INFO << "[API] PUT /v1/core/solutions/" << solutionId << " - Success - " << duration.count() << "ms";
        }
        
        callback(createSuccessResponse(response));
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] PUT /v1/core/solutions/" << solutionId << " - Exception: " << e.what() << " - " << duration.count() << "ms";
        }
        std::cerr << "[SolutionHandler] Exception: " << e.what() << std::endl;
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] PUT /v1/core/solutions/" << solutionId << " - Unknown exception - " << duration.count() << "ms";
        }
        std::cerr << "[SolutionHandler] Unknown exception" << std::endl;
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void SolutionHandler::deleteSolution(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
    
    auto start_time = std::chrono::steady_clock::now();
    
    std::string solutionId = extractSolutionId(req);
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] DELETE /v1/core/solutions/" << solutionId << " - Delete solution";
    }
    
    try {
        if (!solution_registry_ || !solution_storage_) {
            if (isApiLoggingEnabled()) {
                PLOG_ERROR << "[API] DELETE /v1/core/solutions/" << solutionId << " - Error: Solution registry or storage not initialized";
            }
            callback(createErrorResponse(500, "Internal server error", "Solution registry or storage not initialized"));
            return;
        }
        
        if (solutionId.empty()) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] DELETE /v1/core/solutions/{id} - Error: Solution ID is empty";
            }
            callback(createErrorResponse(400, "Invalid request", "Solution ID is required"));
            return;
        }
        
        // Check if solution exists
        if (!solution_registry_->hasSolution(solutionId)) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] DELETE /v1/core/solutions/" << solutionId << " - Not found";
            }
            callback(createErrorResponse(404, "Not found", "Solution not found: " + solutionId));
            return;
        }
        
        // Check if it's a default solution
        if (solution_registry_->isDefaultSolution(solutionId)) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] DELETE /v1/core/solutions/" << solutionId << " - Cannot delete default solution";
            }
            callback(createErrorResponse(403, "Forbidden", "Cannot delete default solution: " + solutionId));
            return;
        }
        
        // Delete from registry
        if (!solution_registry_->deleteSolution(solutionId)) {
            if (isApiLoggingEnabled()) {
                PLOG_ERROR << "[API] DELETE /v1/core/solutions/" << solutionId << " - Failed to delete from registry";
            }
            callback(createErrorResponse(500, "Internal server error", "Failed to delete solution"));
            return;
        }
        
        // Delete from storage
        if (!solution_storage_->deleteSolution(solutionId)) {
            std::cerr << "[SolutionHandler] Warning: Failed to delete solution from storage" << std::endl;
        }
        
        Json::Value response(Json::objectValue);
        response["message"] = "Solution deleted successfully";
        response["solutionId"] = solutionId;
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_INFO << "[API] DELETE /v1/core/solutions/" << solutionId << " - Success - " << duration.count() << "ms";
        }
        
        callback(createSuccessResponse(response));
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] DELETE /v1/core/solutions/" << solutionId << " - Exception: " << e.what() << " - " << duration.count() << "ms";
        }
        std::cerr << "[SolutionHandler] Exception: " << e.what() << std::endl;
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] DELETE /v1/core/solutions/" << solutionId << " - Unknown exception - " << duration.count() << "ms";
        }
        std::cerr << "[SolutionHandler] Unknown exception" << std::endl;
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void SolutionHandler::getSolutionParameters(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
    
    auto start_time = std::chrono::steady_clock::now();
    
    std::string solutionId = extractSolutionId(req);
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] GET /v1/core/solutions/" << solutionId << "/parameters - Get solution parameters";
    }
    
    try {
        if (!solution_registry_) {
            if (isApiLoggingEnabled()) {
                PLOG_ERROR << "[API] GET /v1/core/solutions/" << solutionId << "/parameters - Error: Solution registry not initialized";
            }
            callback(createErrorResponse(500, "Internal server error", "Solution registry not initialized"));
            return;
        }
        
        if (solutionId.empty()) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] GET /v1/core/solutions/{id}/parameters - Error: Solution ID is empty";
            }
            callback(createErrorResponse(400, "Invalid request", "Solution ID is required"));
            return;
        }
        
        auto optConfig = solution_registry_->getSolution(solutionId);
        if (!optConfig.has_value()) {
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] GET /v1/core/solutions/" << solutionId << "/parameters - Not found - " << duration.count() << "ms";
            }
            callback(createErrorResponse(404, "Not found", "Solution not found: " + solutionId));
            return;
        }
        
        const SolutionConfig& config = optConfig.value();
        
        // Extract parameters from solution
        Json::Value response(Json::objectValue);
        response["solutionId"] = config.solutionId;
        response["solutionName"] = config.solutionName;
        response["solutionType"] = config.solutionType;
        
        // Extract variables from pipeline nodes and defaults
        std::set<std::string> allParams;  // All parameters found
        std::set<std::string> requiredParams;  // Parameters that are required
        std::map<std::string, std::string> paramDefaults;
        std::map<std::string, std::string> paramDescriptions;
        
        // Extract from pipeline node parameters
        std::regex varPattern("\\$\\{([A-Za-z0-9_]+)\\}");
        for (const auto& node : config.pipeline) {
            for (const auto& param : node.parameters) {
                std::string value = param.second;
                std::sregex_iterator iter(value.begin(), value.end(), varPattern);
                std::sregex_iterator end;
                
                for (; iter != end; ++iter) {
                    std::string varName = (*iter)[1].str();
                    allParams.insert(varName);
                    // Initially mark as required (will be overridden if has default)
                    requiredParams.insert(varName);
                    
                    // Try to infer description from parameter name
                    if (paramDescriptions.find(varName) == paramDescriptions.end()) {
                        std::string desc = "Parameter for " + node.nodeType + " node";
                        if (param.first == "url" || varName.find("URL") != std::string::npos) {
                            desc = "URL for " + node.nodeType;
                        } else if (varName.find("MODEL") != std::string::npos || varName.find("PATH") != std::string::npos) {
                            desc = "File path for " + node.nodeType;
                        }
                        paramDescriptions[varName] = desc;
                    }
                }
            }
        }
        
        // Extract from defaults
        // If a parameter has a default value (literal, not containing variables), it's optional
        // If default value contains variables, those variables are still required
        for (const auto& def : config.defaults) {
            std::string paramName = def.first;
            std::string defaultValue = def.second;
            allParams.insert(paramName);
            paramDefaults[paramName] = defaultValue;
            
            // Check if default value contains variables
            std::sregex_iterator iter(defaultValue.begin(), defaultValue.end(), varPattern);
            std::sregex_iterator end;
            bool hasVariables = false;
            for (; iter != end; ++iter) {
                std::string varName = (*iter)[1].str();
                allParams.insert(varName);
                requiredParams.insert(varName);  // Variables in defaults are required
                hasVariables = true;
            }
            
            // If default value is literal (no variables), the parameter is optional
            if (!hasVariables) {
                requiredParams.erase(paramName);
            }
        }
        
        // Build additionalParams schema
        Json::Value additionalParams(Json::objectValue);
        Json::Value required(Json::arrayValue);
        
        for (const auto& param : allParams) {
            Json::Value paramInfo(Json::objectValue);
            paramInfo["name"] = param;
            paramInfo["type"] = "string";
            
            // Check if has default
            auto defIt = paramDefaults.find(param);
            bool isRequired = (requiredParams.find(param) != requiredParams.end());
            
            if (defIt != paramDefaults.end()) {
                std::string defaultValue = defIt->second;
                // Only set default if it's a literal value (doesn't contain variables)
                std::sregex_iterator iter(defaultValue.begin(), defaultValue.end(), varPattern);
                std::sregex_iterator end;
                if (iter == end) {
                    // No variables in default, it's a literal value
                    paramInfo["default"] = defaultValue;
                    paramInfo["required"] = false;
                } else {
                    // Default contains variables, parameter is still required
                    paramInfo["required"] = true;
                    required.append(param);
                }
            } else {
                paramInfo["required"] = isRequired;
                if (isRequired) {
                    required.append(param);
                }
            }
            
            // Add description
            auto descIt = paramDescriptions.find(param);
            if (descIt != paramDescriptions.end()) {
                paramInfo["description"] = descIt->second;
            } else {
                // Generate default description
                if (param.find("URL") != std::string::npos) {
                    paramInfo["description"] = "URL parameter";
                } else if (param.find("PATH") != std::string::npos || param.find("MODEL") != std::string::npos) {
                    paramInfo["description"] = "File path parameter";
                } else {
                    paramInfo["description"] = "Solution parameter";
                }
            }
            
            additionalParams[param] = paramInfo;
        }
        
        response["additionalParams"] = additionalParams;
        response["requiredAdditionalParams"] = required;
        
        // Add standard instance creation fields
        Json::Value standardFields(Json::objectValue);
        
        // Required fields
        Json::Value requiredFields(Json::arrayValue);
        requiredFields.append("name");
        standardFields["name"] = Json::Value(Json::objectValue);
        standardFields["name"]["type"] = "string";
        standardFields["name"]["required"] = true;
        standardFields["name"]["description"] = "Instance name (pattern: ^[A-Za-z0-9 -_]+$)";
        standardFields["name"]["pattern"] = "^[A-Za-z0-9 -_]+$";
        
        // Optional fields
        standardFields["group"] = Json::Value(Json::objectValue);
        standardFields["group"]["type"] = "string";
        standardFields["group"]["required"] = false;
        standardFields["group"]["description"] = "Group name (pattern: ^[A-Za-z0-9 -_]+$)";
        standardFields["group"]["pattern"] = "^[A-Za-z0-9 -_]+$";
        
        standardFields["solution"] = Json::Value(Json::objectValue);
        standardFields["solution"]["type"] = "string";
        standardFields["solution"]["required"] = false;
        standardFields["solution"]["description"] = "Solution ID (must match existing solution)";
        standardFields["solution"]["default"] = solutionId;
        
        standardFields["persistent"] = Json::Value(Json::objectValue);
        standardFields["persistent"]["type"] = "boolean";
        standardFields["persistent"]["required"] = false;
        standardFields["persistent"]["description"] = "Save instance to JSON file";
        standardFields["persistent"]["default"] = false;
        
        standardFields["autoStart"] = Json::Value(Json::objectValue);
        standardFields["autoStart"]["type"] = "boolean";
        standardFields["autoStart"]["required"] = false;
        standardFields["autoStart"]["description"] = "Automatically start instance when created";
        standardFields["autoStart"]["default"] = false;
        
        standardFields["frameRateLimit"] = Json::Value(Json::objectValue);
        standardFields["frameRateLimit"]["type"] = "integer";
        standardFields["frameRateLimit"]["required"] = false;
        standardFields["frameRateLimit"]["description"] = "Frame rate limit (FPS)";
        standardFields["frameRateLimit"]["default"] = 0;
        standardFields["frameRateLimit"]["minimum"] = 0;
        
        standardFields["detectionSensitivity"] = Json::Value(Json::objectValue);
        standardFields["detectionSensitivity"]["type"] = "string";
        standardFields["detectionSensitivity"]["required"] = false;
        standardFields["detectionSensitivity"]["description"] = "Detection sensitivity level";
        standardFields["detectionSensitivity"]["enum"] = Json::Value(Json::arrayValue);
        standardFields["detectionSensitivity"]["enum"].append("Low");
        standardFields["detectionSensitivity"]["enum"].append("Medium");
        standardFields["detectionSensitivity"]["enum"].append("High");
        standardFields["detectionSensitivity"]["enum"].append("Normal");
        standardFields["detectionSensitivity"]["enum"].append("Slow");
        standardFields["detectionSensitivity"]["default"] = "Low";
        
        standardFields["detectorMode"] = Json::Value(Json::objectValue);
        standardFields["detectorMode"]["type"] = "string";
        standardFields["detectorMode"]["required"] = false;
        standardFields["detectorMode"]["description"] = "Detector mode";
        standardFields["detectorMode"]["enum"] = Json::Value(Json::arrayValue);
        standardFields["detectorMode"]["enum"].append("SmartDetection");
        standardFields["detectorMode"]["enum"].append("FullRegionInference");
        standardFields["detectorMode"]["enum"].append("MosaicInference");
        standardFields["detectorMode"]["default"] = "SmartDetection";
        
        standardFields["metadataMode"] = Json::Value(Json::objectValue);
        standardFields["metadataMode"]["type"] = "boolean";
        standardFields["metadataMode"]["required"] = false;
        standardFields["metadataMode"]["description"] = "Enable metadata mode";
        standardFields["metadataMode"]["default"] = false;
        
        standardFields["statisticsMode"] = Json::Value(Json::objectValue);
        standardFields["statisticsMode"]["type"] = "boolean";
        standardFields["statisticsMode"]["required"] = false;
        standardFields["statisticsMode"]["description"] = "Enable statistics mode";
        standardFields["statisticsMode"]["default"] = false;
        
        standardFields["diagnosticsMode"] = Json::Value(Json::objectValue);
        standardFields["diagnosticsMode"]["type"] = "boolean";
        standardFields["diagnosticsMode"]["required"] = false;
        standardFields["diagnosticsMode"]["description"] = "Enable diagnostics mode";
        standardFields["diagnosticsMode"]["default"] = false;
        
        standardFields["debugMode"] = Json::Value(Json::objectValue);
        standardFields["debugMode"]["type"] = "boolean";
        standardFields["debugMode"]["required"] = false;
        standardFields["debugMode"]["description"] = "Enable debug mode";
        standardFields["debugMode"]["default"] = false;
        
        response["standardFields"] = standardFields;
        response["requiredStandardFields"] = requiredFields;
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_INFO << "[API] GET /v1/core/solutions/" << solutionId << "/parameters - Success - " << duration.count() << "ms";
        }
        
        callback(createSuccessResponse(response));
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] GET /v1/core/solutions/" << solutionId << "/parameters - Exception: " << e.what() << " - " << duration.count() << "ms";
        }
        std::cerr << "[SolutionHandler] Exception: " << e.what() << std::endl;
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] GET /v1/core/solutions/" << solutionId << "/parameters - Unknown exception - " << duration.count() << "ms";
        }
        std::cerr << "[SolutionHandler] Unknown exception" << std::endl;
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void SolutionHandler::handleOptions(const HttpRequestPtr &req,
                                    std::function<void(const HttpResponsePtr &)> &&callback) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    resp->addHeader("Access-Control-Max-Age", "3600");
    callback(resp);
}

