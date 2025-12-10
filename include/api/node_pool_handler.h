#pragma once

#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <json/json.h>

using namespace drogon;

/**
 * @brief Node Pool Handler
 * 
 * Handles API endpoints for managing node pool:
 * - GET /v1/core/nodes/templates - List all node templates
 * - GET /v1/core/nodes/templates/{category} - List templates by category
 * - GET /v1/core/nodes/preconfigured - List all pre-configured nodes
 * - GET /v1/core/nodes/preconfigured/available - List available nodes
 * - POST /v1/core/nodes/preconfigured - Create a pre-configured node
 * - POST /v1/core/nodes/build-solution - Build solution from selected nodes
 * - GET /v1/core/nodes/stats - Get node pool statistics
 */
class NodePoolHandler : public drogon::HttpController<NodePoolHandler> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(NodePoolHandler::getTemplates, "/v1/core/nodes/templates", Get);
        ADD_METHOD_TO(NodePoolHandler::getTemplatesByCategory, "/v1/core/nodes/templates/{category}", Get);
        ADD_METHOD_TO(NodePoolHandler::getPreConfiguredNodes, "/v1/core/nodes/preconfigured", Get);
        ADD_METHOD_TO(NodePoolHandler::getAvailableNodes, "/v1/core/nodes/preconfigured/available", Get);
        ADD_METHOD_TO(NodePoolHandler::createPreConfiguredNode, "/v1/core/nodes/preconfigured", Post);
        ADD_METHOD_TO(NodePoolHandler::buildSolutionFromNodes, "/v1/core/nodes/build-solution", Post);
        ADD_METHOD_TO(NodePoolHandler::getStats, "/v1/core/nodes/stats", Get);
        ADD_METHOD_TO(NodePoolHandler::handleOptions, "/v1/core/nodes/*", Options);
    METHOD_LIST_END
    
    /**
     * @brief GET /v1/core/nodes/templates - List all node templates
     */
    void getTemplates(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief GET /v1/core/nodes/templates/{category} - List templates by category
     */
    void getTemplatesByCategory(const HttpRequestPtr &req,
                               std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief GET /v1/core/nodes/preconfigured - List all pre-configured nodes
     */
    void getPreConfiguredNodes(const HttpRequestPtr &req,
                               std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief GET /v1/core/nodes/preconfigured/available - List available nodes
     */
    void getAvailableNodes(const HttpRequestPtr &req,
                          std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief POST /v1/core/nodes/preconfigured - Create a pre-configured node
     */
    void createPreConfiguredNode(const HttpRequestPtr &req,
                                 std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief POST /v1/core/nodes/build-solution - Build solution from selected nodes
     */
    void buildSolutionFromNodes(const HttpRequestPtr &req,
                               std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief GET /v1/core/nodes/stats - Get node pool statistics
     */
    void getStats(const HttpRequestPtr &req,
                 std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle OPTIONS request for CORS preflight
     */
    void handleOptions(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback);
    
private:
    HttpResponsePtr createSuccessResponse(const Json::Value& data, int statusCode = 200) const;
    HttpResponsePtr createErrorResponse(int statusCode, const std::string& error, const std::string& message) const;
};

