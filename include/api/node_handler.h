#pragma once

#include "core/node_pool_manager.h"
#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <json/json.h>

using namespace drogon;

/**
 * @brief Node Management Handler
 *
 * Handles CRUD operations for pre-configured nodes.
 *
 * Endpoints:
 * - GET /v1/core/node - List all nodes
 * - GET /v1/core/node/{nodeId} - Get node details
 * - POST /v1/core/node - Create a new node
 * - PUT /v1/core/node/{nodeId} - Update a node
 * - DELETE /v1/core/node/{nodeId} - Delete a node
 * - GET /v1/core/node/template - List all node templates
 * - GET /v1/core/node/template/{templateId} - Get template details
 * - GET /v1/core/node/stats - Get node pool statistics
 */
class NodeHandler : public drogon::HttpController<NodeHandler> {
public:
  METHOD_LIST_BEGIN
  // Node CRUD operations
  ADD_METHOD_TO(NodeHandler::listNodes, "/v1/core/node", Get);
  ADD_METHOD_TO(NodeHandler::getNode, "/v1/core/node/{nodeId}", Get);
  ADD_METHOD_TO(NodeHandler::createNode, "/v1/core/node", Post);
  ADD_METHOD_TO(NodeHandler::updateNode, "/v1/core/node/{nodeId}", Put);
  ADD_METHOD_TO(NodeHandler::deleteNode, "/v1/core/node/{nodeId}", Delete);

  // Template operations
  ADD_METHOD_TO(NodeHandler::listTemplates, "/v1/core/node/template", Get);
  ADD_METHOD_TO(NodeHandler::getTemplate, "/v1/core/node/template/{templateId}",
                Get);

  // Statistics
  ADD_METHOD_TO(NodeHandler::getStats, "/v1/core/node/stats", Get);

  // CORS preflight
  ADD_METHOD_TO(NodeHandler::handleOptions, "/v1/core/node", Options);
  ADD_METHOD_TO(NodeHandler::handleOptions, "/v1/core/node/{nodeId}", Options);
  ADD_METHOD_TO(NodeHandler::handleOptions, "/v1/core/node/template", Options);
  ADD_METHOD_TO(NodeHandler::handleOptions,
                "/v1/core/node/template/{templateId}", Options);
  ADD_METHOD_TO(NodeHandler::handleOptions, "/v1/core/node/stats", Options);
  METHOD_LIST_END

  /**
   * @brief Handle GET /v1/core/node
   * Lists all pre-configured nodes
   */
  void listNodes(const HttpRequestPtr &req,
                 std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle GET /v1/core/node/{nodeId}
   * Gets detailed information about a specific node
   */
  void getNode(const HttpRequestPtr &req,
               std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle POST /v1/core/node
   * Creates a new pre-configured node
   */
  void createNode(const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle PUT /v1/core/node/{nodeId}
   * Updates an existing node
   */
  void updateNode(const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle DELETE /v1/core/node/{nodeId}
   * Deletes a node
   */
  void deleteNode(const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle GET /v1/core/node/template
   * Lists all available node templates
   */
  void listTemplates(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle GET /v1/core/node/template/{templateId}
   * Gets detailed information about a specific template
   */
  void getTemplate(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle GET /v1/core/node/stats
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
  HttpResponsePtr createSuccessResponse(const Json::Value &data,
                                        int statusCode = 200) const;

  /**
   * @brief Create error response
   */
  HttpResponsePtr createErrorResponse(int statusCode, const std::string &error,
                                      const std::string &message) const;

  /**
   * @brief Convert node to JSON
   */
  Json::Value
  nodeToJson(const class NodePoolManager::PreConfiguredNode &node) const;

  /**
   * @brief Convert template to JSON
   */
  Json::Value
  templateToJson(const class NodePoolManager::NodeTemplate &nodeTemplate) const;

  // Helper functions for parameter metadata generation
  std::string inferParameterType(const std::string &paramName,
                                 const std::string &nodeType) const;
  std::string getInputType(const std::string &paramName,
                           const std::string &paramType) const;
  std::string getWidgetType(const std::string &paramName,
                            const std::string &paramType) const;
  std::string getPlaceholder(const std::string &paramName,
                             const std::string &nodeType) const;
  void addValidationRules(Json::Value &validation, const std::string &paramName,
                          const std::string &paramType,
                          const std::string &nodeType) const;
  std::string getParameterDescription(const std::string &paramName,
                                      const std::string &nodeType) const;
  std::vector<std::string>
  getParameterExamples(const std::string &paramName,
                       const std::string &nodeType) const;
  std::string getParameterCategory(const std::string &paramName,
                                   const std::string &nodeType) const;
};
