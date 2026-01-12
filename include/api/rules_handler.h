#pragma once

#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <json/json.h>
#include <string>

using namespace drogon;

/**
 * @brief Rules Management Handler
 *
 * Handles vision rules configuration (zones and lines) for AI instances.
 * This endpoint accepts rules in USC format and applies them to edge_ai_api instances.
 *
 * Endpoints:
 * - GET /v1/core/instance/{instanceId}/rules - Get all rules (zones + lines)
 * - POST /v1/core/instance/{instanceId}/rules - Set/update rules configuration
 * - PUT /v1/core/instance/{instanceId}/rules - Replace rules configuration
 * - DELETE /v1/core/instance/{instanceId}/rules - Delete all rules
 */
class RulesHandler : public drogon::HttpController<RulesHandler> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(RulesHandler::getRules,
                "/v1/core/instance/{instanceId}/rules", Get);
  ADD_METHOD_TO(RulesHandler::setRules,
                "/v1/core/instance/{instanceId}/rules", Post);
  ADD_METHOD_TO(RulesHandler::updateRules,
                "/v1/core/instance/{instanceId}/rules", Put);
  ADD_METHOD_TO(RulesHandler::deleteRules,
                "/v1/core/instance/{instanceId}/rules", Delete);
  ADD_METHOD_TO(RulesHandler::handleOptions,
                "/v1/core/instance/{instanceId}/rules", Options);
  ADD_METHOD_TO(RulesHandler::getEntity,
                "/v1/core/instance/{instanceId}/rules/entities/{entityUuid}", Get);
  ADD_METHOD_TO(RulesHandler::createEntity,
                "/v1/core/instance/{instanceId}/rules/entities", Post);
  ADD_METHOD_TO(RulesHandler::updateEntity,
                "/v1/core/instance/{instanceId}/rules/entities/{entityUuid}", Put);
  ADD_METHOD_TO(RulesHandler::deleteEntity,
                "/v1/core/instance/{instanceId}/rules/entities/{entityUuid}", Delete);
  ADD_METHOD_TO(RulesHandler::toggleEntityEnabled,
                "/v1/core/instance/{instanceId}/rules/entities/{entityUuid}/enable", Patch);
  METHOD_LIST_END

  /**
   * @brief Handle GET /v1/core/instance/{instanceId}/rules
   * Gets all rules (zones and lines) for an instance
   */
  void getRules(const HttpRequestPtr &req,
                std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle POST /v1/core/instance/{instanceId}/rules
   * Sets/updates rules configuration (zones and lines)
   */
  void setRules(const HttpRequestPtr &req,
                std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle PUT /v1/core/instance/{instanceId}/rules
   * Replaces entire rules configuration
   */
  void updateRules(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle DELETE /v1/core/instance/{instanceId}/rules
   * Deletes all rules (zones and lines)
   */
  void deleteRules(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle OPTIONS request for CORS preflight
   */
  void handleOptions(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle GET /v1/core/instance/{instanceId}/rules/entities/{entityUuid}
   * Gets a specific entity (zone or line) by UUID
   */
  void getEntity(const HttpRequestPtr &req,
                 std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle POST /v1/core/instance/{instanceId}/rules/entities
   * Creates a new entity (zone or line)
   */
  void createEntity(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle PUT /v1/core/instance/{instanceId}/rules/entities/{entityUuid}
   * Updates a specific entity (zone or line) by UUID
   */
  void updateEntity(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle DELETE /v1/core/instance/{instanceId}/rules/entities/{entityUuid}
   * Deletes a specific entity (zone or line) by UUID
   */
  void deleteEntity(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle PATCH /v1/core/instance/{instanceId}/rules/entities/{entityUuid}/enable
   * Enables or disables a specific entity (zone or line) by UUID
   */
  void toggleEntityEnabled(const HttpRequestPtr &req,
                          std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Set instance manager (dependency injection)
   */
  static void setInstanceManager(class IInstanceManager *manager);

private:
  static class IInstanceManager *instance_manager_;

  /**
   * @brief Extract instance ID from request path
   */
  std::string extractInstanceId(const HttpRequestPtr &req) const;

  /**
   * @brief Create error response
   */
  HttpResponsePtr createErrorResponse(int statusCode, const std::string &error,
                                      const std::string &message = "") const;

  /**
   * @brief Create success JSON response with CORS headers
   */
  HttpResponsePtr createSuccessResponse(const Json::Value &data,
                                        int statusCode = 200) const;

  /**
   * @brief Load rules from instance config
   * @param instanceId Instance ID
   * @return JSON object with zones and lines, empty object if not found
   */
  Json::Value loadRulesFromConfig(const std::string &instanceId) const;

  /**
   * @brief Save rules to instance config
   * @param instanceId Instance ID
   * @param rules JSON object with zones and lines
   * @return true if successful, false otherwise
   */
  bool saveRulesToConfig(const std::string &instanceId,
                         const Json::Value &rules) const;

  /**
   * @brief Convert USC format lines to edge_ai_api format
   * @param uscLines JSON array of USC format lines
   * @return JSON array of edge_ai_api format lines
   */
  Json::Value convertUSCLinesToEdgeAI(const Json::Value &uscLines) const;

  /**
   * @brief Convert edge_ai_api format lines to USC format
   * @param edgeAILines JSON array of edge_ai_api format lines
   * @return JSON array of USC format lines
   */
  Json::Value convertEdgeAILinesToUSC(const Json::Value &edgeAILines) const;

  /**
   * @brief Apply lines to instance (using existing lines handler logic)
   * @param instanceId Instance ID
   * @param lines JSON array of lines in edge_ai_api format
   * @return true if successful, false otherwise
   */
  bool applyLinesToInstance(const std::string &instanceId,
                            const Json::Value &lines) const;

  /**
   * @brief Find entity (zone or line) by UUID in rules
   * @param rules Rules JSON object (with zones and lines arrays)
   * @param entityUuid Entity UUID to find
   * @return JSON object of entity if found, null JSON value otherwise
   */
  Json::Value findEntityByUuid(const Json::Value &rules,
                               const std::string &entityUuid) const;

  /**
   * @brief Determine if entity is a zone or line
   * @param rules Rules JSON object
   * @param entityUuid Entity UUID
   * @return "zone" or "line" if found, empty string otherwise
   */
  std::string getEntityType(const Json::Value &rules,
                            const std::string &entityUuid) const;

  /**
   * @brief Remove entity from rules by UUID
   * @param rules Rules JSON object (will be modified)
   * @param entityUuid Entity UUID to remove
   * @return true if entity was found and removed, false otherwise
   */
  bool removeEntityByUuid(Json::Value &rules,
                          const std::string &entityUuid) const;

  /**
   * @brief Update entity in rules by UUID
   * @param rules Rules JSON object (will be modified)
   * @param entityUuid Entity UUID to update
   * @param newEntity New entity JSON object
   * @return true if entity was found and updated, false otherwise
   */
  bool updateEntityByUuid(Json::Value &rules,
                          const std::string &entityUuid,
                          const Json::Value &newEntity) const;
};



