#pragma once

#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <json/json.h>
#include "core/node_pool_manager.h"

using namespace drogon;

/**
 * @brief Node Management Handler
 * 
 * Handles CRUD operations for pre-configured nodes.
 * 
 * Endpoints:
 * - GET /v1/core/nodes - List all nodes
 * - GET /v1/core/nodes/{nodeId} - Get node details
 * - POST /v1/core/nodes - Create a new node
 * - PUT /v1/core/nodes/{nodeId} - Update a node
 * - DELETE /v1/core/nodes/{nodeId} - Delete a node
 * - GET /v1/core/nodes/templates - List all node templates
 * - GET /v1/core/nodes/templates/{templateId} - Get template details
 * - GET /v1/core/nodes/stats - Get node pool statistics
 */
class NodeHandler : public drogon::HttpController<NodeHandler> {
public:
    METHOD_LIST_BEGIN
        // Node CRUD operations
        ADD_METHOD_TO(NodeHandler::listNodes, "/v1/core/nodes", Get);
        ADD_METHOD_TO(NodeHandler::getNode, "/v1/core/nodes/{nodeId}", Get);
        ADD_METHOD_TO(NodeHandler::createNode, "/v1/core/nodes", Post);
        ADD_METHOD_TO(NodeHandler::updateNode, "/v1/core/nodes/{nodeId}", Put);
        ADD_METHOD_TO(NodeHandler::deleteNode, "/v1/core/nodes/{nodeId}", Delete);
        
        // Template operations
        ADD_METHOD_TO(NodeHandler::listTemplates, "/v1/core/nodes/templates", Get);
        ADD_METHOD_TO(NodeHandler::getTemplate, "/v1/core/nodes/templates/{templateId}", Get);
        
        // Statistics
        ADD_METHOD_TO(NodeHandler::getStats, "/v1/core/nodes/stats", Get);
        
        // CORS preflight
        ADD_METHOD_TO(NodeHandler::handleOptions, "/v1/core/nodes", Options);
        ADD_METHOD_TO(NodeHandler::handleOptions, "/v1/core/nodes/{nodeId}", Options);
        ADD_METHOD_TO(NodeHandler::handleOptions, "/v1/core/nodes/templates", Options);
        ADD_METHOD_TO(NodeHandler::handleOptions, "/v1/core/nodes/templates/{templateId}", Options);
        ADD_METHOD_TO(NodeHandler::handleOptions, "/v1/core/nodes/stats", Options);
    METHOD_LIST_END
    
    /**
     * @brief Handle GET /v1/core/nodes
     * Lists all pre-configured nodes
     */
    void listNodes(const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle GET /v1/core/nodes/{nodeId}
     * Gets detailed information about a specific node
     */
    void getNode(const HttpRequestPtr &req,
                std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle POST /v1/core/nodes
     * Creates a new pre-configured node
     */
    void createNode(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle PUT /v1/core/nodes/{nodeId}
     * Updates an existing node
     */
    void updateNode(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle DELETE /v1/core/nodes/{nodeId}
     * Deletes a node
     */
    void deleteNode(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle GET /v1/core/nodes/templates
     * Lists all available node templates
     */
    void listTemplates(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle GET /v1/core/nodes/templates/{templateId}
     * Gets detailed information about a specific template
     */
    void getTemplate(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle GET /v1/core/nodes/stats
     * Gets node pool statistics
     */
    void getStats(const HttpRequestPtr &req,
                 std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle OPTIONS request for CORS preflight
     */
    void handleOptions(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback);
    
private:
    /**
     * @brief Create success response
     */
    HttpResponsePtr createSuccessResponse(const Json::Value& data, int statusCode = 200) const;
    
    /**
     * @brief Create error response
     */
    HttpResponsePtr createErrorResponse(int statusCode, const std::string& error, const std::string& message) const;
    
    /**
     * @brief Convert node to JSON
     */
    Json::Value nodeToJson(const class NodePoolManager::PreConfiguredNode& node) const;
    
    /**
     * @brief Convert template to JSON
     */
    Json::Value templateToJson(const class NodePoolManager::NodeTemplate& nodeTemplate) const;
};

