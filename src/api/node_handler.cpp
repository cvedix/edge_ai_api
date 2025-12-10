#include "api/node_handler.h"
#include "core/node_pool_manager.h"
#include "core/logging_flags.h"
#include "core/logger.h"
#include <drogon/HttpResponse.h>
#include <json/json.h>
#include <sstream>
#include <iomanip>
#include <ctime>

void NodeHandler::listNodes(const HttpRequestPtr &req,
                           std::function<void(const HttpResponsePtr &)> &&callback) {
    auto start_time = std::chrono::steady_clock::now();
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] GET /v1/core/nodes - List all nodes";
    }
    
    try {
        auto& nodePool = NodePoolManager::getInstance();
        
        // Get query parameters
        bool availableOnly = false;
        std::string category;
        std::string type = req->getParameter("type"); // "preconfigured" (default) or "templates"
        
        auto availableParam = req->getParameter("available");
        if (availableParam == "true" || availableParam == "1") {
            availableOnly = true;
        }
        
        category = req->getParameter("category");
        
        // If type=templates, return templates instead of pre-configured nodes
        if (type == "templates") {
            std::vector<NodePoolManager::NodeTemplate> templates;
            if (!category.empty()) {
                templates = nodePool.getTemplatesByCategory(category);
            } else {
                templates = nodePool.getAllTemplates();
            }
            
            Json::Value response;
            Json::Value templatesArray(Json::arrayValue);
            
            for (const auto& nodeTemplate : templates) {
                templatesArray.append(templateToJson(nodeTemplate));
            }
            
            response["nodes"] = templatesArray;
            response["total"] = static_cast<Json::Int64>(templates.size());
            response["type"] = "templates";
            
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            if (isApiLoggingEnabled()) {
                PLOG_INFO << "[API] GET /v1/core/nodes - Success: " << templates.size() 
                          << " templates - " << duration.count() << "ms";
            }
            
            callback(createSuccessResponse(response));
            return;
        }
        
        // Get pre-configured nodes
        std::vector<NodePoolManager::PreConfiguredNode> nodes;
        if (availableOnly) {
            nodes = nodePool.getAvailableNodes();
        } else {
            nodes = nodePool.getAllPreConfiguredNodes();
        }
        
        // If no pre-configured nodes exist, return templates as available node types
        if (nodes.empty()) {
            std::vector<NodePoolManager::NodeTemplate> templates;
            if (!category.empty()) {
                templates = nodePool.getTemplatesByCategory(category);
            } else {
                templates = nodePool.getAllTemplates();
            }
            
            Json::Value response;
            Json::Value nodesArray(Json::arrayValue);
            
            for (const auto& nodeTemplate : templates) {
                // Convert template to node-like JSON format
                Json::Value nodeJson;
                nodeJson["nodeId"] = nodeTemplate.templateId; // Use templateId as nodeId
                nodeJson["templateId"] = nodeTemplate.templateId;
                nodeJson["displayName"] = nodeTemplate.displayName;
                nodeJson["nodeType"] = nodeTemplate.nodeType;
                nodeJson["category"] = nodeTemplate.category;
                nodeJson["description"] = nodeTemplate.description;
                nodeJson["inUse"] = false;
                nodeJson["isTemplate"] = true; // Indicate this is a template, not a pre-configured node
                
                // Parameters from default parameters
                Json::Value params(Json::objectValue);
                for (const auto& [key, value] : nodeTemplate.defaultParameters) {
                    params[key] = value;
                }
                nodeJson["parameters"] = params;
                
                // Required and optional parameters info
                Json::Value requiredParams(Json::arrayValue);
                for (const auto& param : nodeTemplate.requiredParameters) {
                    requiredParams.append(param);
                }
                nodeJson["requiredParameters"] = requiredParams;
                
                Json::Value optionalParams(Json::arrayValue);
                for (const auto& param : nodeTemplate.optionalParameters) {
                    optionalParams.append(param);
                }
                nodeJson["optionalParameters"] = optionalParams;
                
                nodesArray.append(nodeJson);
            }
            
            response["nodes"] = nodesArray;
            response["total"] = static_cast<Json::Int64>(templates.size());
            response["available"] = static_cast<Json::Int64>(templates.size());
            response["inUse"] = 0;
            response["type"] = "templates"; // Indicate these are templates, not pre-configured nodes
            response["message"] = "No pre-configured nodes found. Showing available node templates.";
            
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            if (isApiLoggingEnabled()) {
                PLOG_INFO << "[API] GET /v1/core/nodes - Success: " << templates.size() 
                          << " templates (no pre-configured nodes) - " << duration.count() << "ms";
            }
            
            callback(createSuccessResponse(response));
            return;
        }
        
        // Filter by category if specified
        if (!category.empty()) {
            std::vector<NodePoolManager::PreConfiguredNode> filtered;
            for (const auto& node : nodes) {
                auto template_opt = nodePool.getTemplate(node.templateId);
                if (template_opt.has_value() && template_opt.value().category == category) {
                    filtered.push_back(node);
                }
            }
            nodes = filtered;
        }
        
        // Build response
        Json::Value response;
        Json::Value nodesArray(Json::arrayValue);
        
        for (const auto& node : nodes) {
            nodesArray.append(nodeToJson(node));
        }
        
        response["nodes"] = nodesArray;
        response["total"] = static_cast<Json::Int64>(nodes.size());
        response["available"] = static_cast<Json::Int64>(nodePool.getAvailableNodes().size());
        response["inUse"] = static_cast<Json::Int64>(
            nodePool.getAllPreConfiguredNodes().size() - nodePool.getAvailableNodes().size());
        response["type"] = "preconfigured"; // Indicate these are pre-configured nodes
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_INFO << "[API] GET /v1/core/nodes - Success: " << nodes.size() 
                      << " nodes - " << duration.count() << "ms";
        }
        
        callback(createSuccessResponse(response));
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] GET /v1/core/nodes - Exception: " << e.what() 
                      << " - " << duration.count() << "ms";
        }
        
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void NodeHandler::getNode(const HttpRequestPtr &req,
                         std::function<void(const HttpResponsePtr &)> &&callback) {
    auto start_time = std::chrono::steady_clock::now();
    
    // Extract nodeId from path
    std::string nodeId = req->getParameter("nodeId");
    if (nodeId.empty()) {
        // Try to extract from path
        std::string path = req->getPath();
        size_t nodesPos = path.find("/nodes/");
        if (nodesPos != std::string::npos) {
            size_t start = nodesPos + 7; // length of "/nodes/"
            size_t end = path.find("/", start);
            if (end == std::string::npos) {
                end = path.length();
            }
            nodeId = path.substr(start, end - start);
        }
    }
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] GET /v1/core/nodes/" << nodeId << " - Get node details";
    }
    
    try {
        if (nodeId.empty()) {
            callback(createErrorResponse(400, "Bad request", "Missing nodeId parameter"));
            return;
        }
        
        auto& nodePool = NodePoolManager::getInstance();
        auto nodeOpt = nodePool.getPreConfiguredNode(nodeId);
        
        if (!nodeOpt.has_value()) {
            callback(createErrorResponse(404, "Not found", "Node not found: " + nodeId));
            return;
        }
        
        Json::Value response = nodeToJson(nodeOpt.value());
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_INFO << "[API] GET /v1/core/nodes/" << nodeId << " - Success - " 
                      << duration.count() << "ms";
        }
        
        callback(createSuccessResponse(response));
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] GET /v1/core/nodes/" << nodeId << " - Exception: " << e.what() 
                      << " - " << duration.count() << "ms";
        }
        
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void NodeHandler::createNode(const HttpRequestPtr &req,
                            std::function<void(const HttpResponsePtr &)> &&callback) {
    auto start_time = std::chrono::steady_clock::now();
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] POST /v1/core/nodes - Create new node";
    }
    
    try {
        // Parse JSON body
        auto json = req->getJsonObject();
        if (!json) {
            callback(createErrorResponse(400, "Bad request", "Invalid JSON body"));
            return;
        }
        
        // Validate required fields
        if (!json->isMember("templateId") || (*json)["templateId"].asString().empty()) {
            callback(createErrorResponse(400, "Bad request", "Missing required field: templateId"));
            return;
        }
        
        std::string templateId = (*json)["templateId"].asString();
        
        // Parse parameters
        std::map<std::string, std::string> parameters;
        if (json->isMember("parameters") && (*json)["parameters"].isObject()) {
            const auto& paramsObj = (*json)["parameters"];
            for (const auto& key : paramsObj.getMemberNames()) {
                parameters[key] = paramsObj[key].asString();
            }
        }
        
        // Create node
        auto& nodePool = NodePoolManager::getInstance();
        std::string nodeId = nodePool.createPreConfiguredNode(templateId, parameters);
        
        if (nodeId.empty()) {
            callback(createErrorResponse(400, "Bad request", 
                "Failed to create node. Check templateId and required parameters."));
            return;
        }
        
        // Get created node
        auto nodeOpt = nodePool.getPreConfiguredNode(nodeId);
        if (!nodeOpt.has_value()) {
            callback(createErrorResponse(500, "Internal server error", 
                "Node created but could not be retrieved"));
            return;
        }
        
        Json::Value response = nodeToJson(nodeOpt.value());
        response["message"] = "Node created successfully";
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_INFO << "[API] POST /v1/core/nodes - Success: Created node " << nodeId 
                      << " - " << duration.count() << "ms";
        }
        
        callback(createSuccessResponse(response, 201));
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] POST /v1/core/nodes - Exception: " << e.what() 
                      << " - " << duration.count() << "ms";
        }
        
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void NodeHandler::updateNode(const HttpRequestPtr &req,
                            std::function<void(const HttpResponsePtr &)> &&callback) {
    auto start_time = std::chrono::steady_clock::now();
    
    // Extract nodeId from path
    std::string nodeId = req->getParameter("nodeId");
    if (nodeId.empty()) {
        std::string path = req->getPath();
        size_t nodesPos = path.find("/nodes/");
        if (nodesPos != std::string::npos) {
            size_t start = nodesPos + 7;
            size_t end = path.find("/", start);
            if (end == std::string::npos) {
                end = path.length();
            }
            nodeId = path.substr(start, end - start);
        }
    }
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] PUT /v1/core/nodes/" << nodeId << " - Update node";
    }
    
    try {
        if (nodeId.empty()) {
            callback(createErrorResponse(400, "Bad request", "Missing nodeId parameter"));
            return;
        }
        
        // Parse JSON body
        auto json = req->getJsonObject();
        if (!json) {
            callback(createErrorResponse(400, "Bad request", "Invalid JSON body"));
            return;
        }
        
        auto& nodePool = NodePoolManager::getInstance();
        auto nodeOpt = nodePool.getPreConfiguredNode(nodeId);
        
        if (!nodeOpt.has_value()) {
            callback(createErrorResponse(404, "Not found", "Node not found: " + nodeId));
            return;
        }
        
        // Check if node is in use
        if (nodeOpt.value().inUse) {
            callback(createErrorResponse(409, "Conflict", 
                "Cannot update node that is currently in use"));
            return;
        }
        
        // For now, update means delete and recreate
        // In future, can implement actual update logic
        if (json->isMember("parameters") && (*json)["parameters"].isObject()) {
            // Parse new parameters
            std::map<std::string, std::string> parameters;
            const auto& paramsObj = (*json)["parameters"];
            for (const auto& key : paramsObj.getMemberNames()) {
                parameters[key] = paramsObj[key].asString();
            }
            
            // Delete old node
            nodePool.removePreConfiguredNode(nodeId);
            
            // Create new node with same templateId
            std::string newTemplateId = nodeOpt.value().templateId;
            std::string newNodeId = nodePool.createPreConfiguredNode(newTemplateId, parameters);
            
            if (newNodeId.empty()) {
                callback(createErrorResponse(500, "Internal server error", 
                    "Failed to update node"));
                return;
            }
            
            // Get updated node
            auto updatedNodeOpt = nodePool.getPreConfiguredNode(newNodeId);
            if (!updatedNodeOpt.has_value()) {
                callback(createErrorResponse(500, "Internal server error", 
                    "Node updated but could not be retrieved"));
                return;
            }
            
            Json::Value response = nodeToJson(updatedNodeOpt.value());
            response["message"] = "Node updated successfully";
            response["oldNodeId"] = nodeId;
            response["newNodeId"] = newNodeId;
            
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            if (isApiLoggingEnabled()) {
                PLOG_INFO << "[API] PUT /v1/core/nodes/" << nodeId << " - Success: Updated to " 
                          << newNodeId << " - " << duration.count() << "ms";
            }
            
            callback(createSuccessResponse(response));
        } else {
            callback(createErrorResponse(400, "Bad request", "Missing parameters field"));
        }
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] PUT /v1/core/nodes/" << nodeId << " - Exception: " << e.what() 
                      << " - " << duration.count() << "ms";
        }
        
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void NodeHandler::deleteNode(const HttpRequestPtr &req,
                           std::function<void(const HttpResponsePtr &)> &&callback) {
    auto start_time = std::chrono::steady_clock::now();
    
    // Extract nodeId from path
    std::string nodeId = req->getParameter("nodeId");
    if (nodeId.empty()) {
        std::string path = req->getPath();
        size_t nodesPos = path.find("/nodes/");
        if (nodesPos != std::string::npos) {
            size_t start = nodesPos + 7;
            size_t end = path.find("/", start);
            if (end == std::string::npos) {
                end = path.length();
            }
            nodeId = path.substr(start, end - start);
        }
    }
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] DELETE /v1/core/nodes/" << nodeId << " - Delete node";
    }
    
    try {
        if (nodeId.empty()) {
            callback(createErrorResponse(400, "Bad request", "Missing nodeId parameter"));
            return;
        }
        
        auto& nodePool = NodePoolManager::getInstance();
        auto nodeOpt = nodePool.getPreConfiguredNode(nodeId);
        
        if (!nodeOpt.has_value()) {
            callback(createErrorResponse(404, "Not found", "Node not found: " + nodeId));
            return;
        }
        
        // Check if node is in use
        if (nodeOpt.value().inUse) {
            callback(createErrorResponse(409, "Conflict", 
                "Cannot delete node that is currently in use"));
            return;
        }
        
        bool deleted = nodePool.removePreConfiguredNode(nodeId);
        
        if (!deleted) {
            callback(createErrorResponse(500, "Internal server error", "Failed to delete node"));
            return;
        }
        
        Json::Value response;
        response["message"] = "Node deleted successfully";
        response["nodeId"] = nodeId;
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_INFO << "[API] DELETE /v1/core/nodes/" << nodeId << " - Success - " 
                      << duration.count() << "ms";
        }
        
        callback(createSuccessResponse(response));
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] DELETE /v1/core/nodes/" << nodeId << " - Exception: " << e.what() 
                      << " - " << duration.count() << "ms";
        }
        
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void NodeHandler::listTemplates(const HttpRequestPtr &req,
                               std::function<void(const HttpResponsePtr &)> &&callback) {
    auto start_time = std::chrono::steady_clock::now();
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] GET /v1/core/nodes/templates - List all templates";
    }
    
    try {
        auto& nodePool = NodePoolManager::getInstance();
        
        // Get query parameter for category filter
        std::string category = req->getParameter("category");
        
        std::vector<NodePoolManager::NodeTemplate> templates;
        if (!category.empty()) {
            templates = nodePool.getTemplatesByCategory(category);
        } else {
            templates = nodePool.getAllTemplates();
        }
        
        // Build response
        Json::Value response;
        Json::Value templatesArray(Json::arrayValue);
        
        for (const auto& nodeTemplate : templates) {
            templatesArray.append(templateToJson(nodeTemplate));
        }
        
        response["templates"] = templatesArray;
        response["total"] = static_cast<Json::Int64>(templates.size());
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_INFO << "[API] GET /v1/core/nodes/templates - Success: " << templates.size() 
                      << " templates - " << duration.count() << "ms";
        }
        
        callback(createSuccessResponse(response));
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] GET /v1/core/nodes/templates - Exception: " << e.what() 
                      << " - " << duration.count() << "ms";
        }
        
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void NodeHandler::getTemplate(const HttpRequestPtr &req,
                             std::function<void(const HttpResponsePtr &)> &&callback) {
    auto start_time = std::chrono::steady_clock::now();
    
    // Extract templateId from path
    std::string templateId = req->getParameter("templateId");
    if (templateId.empty()) {
        std::string path = req->getPath();
        size_t templatesPos = path.find("/templates/");
        if (templatesPos != std::string::npos) {
            size_t start = templatesPos + 11; // length of "/templates/"
            size_t end = path.find("/", start);
            if (end == std::string::npos) {
                end = path.length();
            }
            templateId = path.substr(start, end - start);
        }
    }
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] GET /v1/core/nodes/templates/" << templateId << " - Get template details";
    }
    
    try {
        if (templateId.empty()) {
            callback(createErrorResponse(400, "Bad request", "Missing templateId parameter"));
            return;
        }
        
        auto& nodePool = NodePoolManager::getInstance();
        auto templateOpt = nodePool.getTemplate(templateId);
        
        if (!templateOpt.has_value()) {
            callback(createErrorResponse(404, "Not found", "Template not found: " + templateId));
            return;
        }
        
        Json::Value response = templateToJson(templateOpt.value());
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_INFO << "[API] GET /v1/core/nodes/templates/" << templateId << " - Success - " 
                      << duration.count() << "ms";
        }
        
        callback(createSuccessResponse(response));
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] GET /v1/core/nodes/templates/" << templateId << " - Exception: " 
                      << e.what() << " - " << duration.count() << "ms";
        }
        
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void NodeHandler::getStats(const HttpRequestPtr &req,
                          std::function<void(const HttpResponsePtr &)> &&callback) {
    auto start_time = std::chrono::steady_clock::now();
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] GET /v1/core/nodes/stats - Get node pool statistics";
    }
    
    try {
        auto& nodePool = NodePoolManager::getInstance();
        auto stats = nodePool.getStats();
        
        Json::Value response;
        response["totalTemplates"] = static_cast<Json::Int64>(stats.totalTemplates);
        response["totalPreConfiguredNodes"] = static_cast<Json::Int64>(stats.totalPreConfiguredNodes);
        response["availableNodes"] = static_cast<Json::Int64>(stats.availableNodes);
        response["inUseNodes"] = static_cast<Json::Int64>(stats.inUseNodes);
        
        Json::Value nodesByCategory(Json::objectValue);
        for (const auto& [category, count] : stats.nodesByCategory) {
            nodesByCategory[category] = static_cast<Json::Int64>(count);
        }
        response["nodesByCategory"] = nodesByCategory;
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_INFO << "[API] GET /v1/core/nodes/stats - Success - " << duration.count() << "ms";
        }
        
        callback(createSuccessResponse(response));
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] GET /v1/core/nodes/stats - Exception: " << e.what() 
                      << " - " << duration.count() << "ms";
        }
        
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void NodeHandler::handleOptions(const HttpRequestPtr &req,
                               std::function<void(const HttpResponsePtr &)> &&callback) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    resp->addHeader("Access-Control-Max-Age", "3600");
    callback(resp);
}

HttpResponsePtr NodeHandler::createSuccessResponse(const Json::Value& data, int statusCode) const {
    auto resp = HttpResponse::newHttpJsonResponse(data);
    resp->setStatusCode(static_cast<HttpStatusCode>(statusCode));
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    return resp;
}

HttpResponsePtr NodeHandler::createErrorResponse(int statusCode, const std::string& error, const std::string& message) const {
    Json::Value errorResponse;
    errorResponse["error"] = error;
    errorResponse["message"] = message;
    
    auto resp = HttpResponse::newHttpJsonResponse(errorResponse);
    resp->setStatusCode(static_cast<HttpStatusCode>(statusCode));
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
    return resp;
}

Json::Value NodeHandler::nodeToJson(const NodePoolManager::PreConfiguredNode& node) const {
    Json::Value json;
    json["nodeId"] = node.nodeId;
    json["templateId"] = node.templateId;
    json["inUse"] = node.inUse;
    
    // Get template info for display
    auto& nodePool = NodePoolManager::getInstance();
    auto templateOpt = nodePool.getTemplate(node.templateId);
    if (templateOpt.has_value()) {
        json["displayName"] = templateOpt.value().displayName;
        json["nodeType"] = templateOpt.value().nodeType;
        json["category"] = templateOpt.value().category;
        json["description"] = templateOpt.value().description;
    }
    
    // Parameters
    Json::Value params(Json::objectValue);
    for (const auto& [key, value] : node.parameters) {
        params[key] = value;
    }
    json["parameters"] = params;
    
    // Timestamp
    // Convert steady_clock to system_clock for display
    // Calculate elapsed time since creation and subtract from current system time
    auto steady_now = std::chrono::steady_clock::now();
    auto elapsed = steady_now - node.createdAt;
    auto system_now = std::chrono::system_clock::now();
    auto system_time = system_now - elapsed;
    auto time_t = std::chrono::system_clock::to_time_t(system_time);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    json["createdAt"] = ss.str();
    
    return json;
}

Json::Value NodeHandler::templateToJson(const NodePoolManager::NodeTemplate& nodeTemplate) const {
    Json::Value json;
    json["templateId"] = nodeTemplate.templateId;
    json["nodeType"] = nodeTemplate.nodeType;
    json["displayName"] = nodeTemplate.displayName;
    json["description"] = nodeTemplate.description;
    json["category"] = nodeTemplate.category;
    json["isPreConfigured"] = nodeTemplate.isPreConfigured;
    
    // Default parameters
    Json::Value defaultParams(Json::objectValue);
    for (const auto& [key, value] : nodeTemplate.defaultParameters) {
        defaultParams[key] = value;
    }
    json["defaultParameters"] = defaultParams;
    
    // Required parameters
    Json::Value requiredParams(Json::arrayValue);
    for (const auto& param : nodeTemplate.requiredParameters) {
        requiredParams.append(param);
    }
    json["requiredParameters"] = requiredParams;
    
    // Optional parameters
    Json::Value optionalParams(Json::arrayValue);
    for (const auto& param : nodeTemplate.optionalParameters) {
        optionalParams.append(param);
    }
    json["optionalParameters"] = optionalParams;
    
    return json;
}

