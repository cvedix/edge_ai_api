#pragma once

#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <json/json.h>

using namespace drogon;

// Forward declarations
class SecuRTInstanceManager;
class AnalyticsEntitiesManager;

/**
 * @brief SecuRT Instance Handler
 *
 * Handles SecuRT instance management endpoints.
 *
 * Endpoints:
 * - POST /v1/securt/instance - Create a new SecuRT instance
 * - PUT /v1/securt/instance/{instanceId} - Create SecuRT instance with ID
 * - PATCH /v1/securt/instance/{instanceId} - Update SecuRT instance
 * - DELETE /v1/securt/instance/{instanceId} - Delete SecuRT instance
 * - GET /v1/securt/instance/{instanceId}/stats - Get instance statistics
 * - GET /v1/securt/instance/{instanceId}/analytics_entities - Get analytics entities
 */
class SecuRTHandler : public drogon::HttpController<SecuRTHandler> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(SecuRTHandler::createInstance, "/v1/securt/instance", Post);
  ADD_METHOD_TO(SecuRTHandler::createInstanceWithId,
                "/v1/securt/instance/{instanceId}", Put);
  ADD_METHOD_TO(SecuRTHandler::updateInstance,
                "/v1/securt/instance/{instanceId}", Patch);
  ADD_METHOD_TO(SecuRTHandler::deleteInstance,
                "/v1/securt/instance/{instanceId}", Delete);
  ADD_METHOD_TO(SecuRTHandler::getInstanceStats,
                "/v1/securt/instance/{instanceId}/stats", Get);
  ADD_METHOD_TO(SecuRTHandler::getAnalyticsEntities,
                "/v1/securt/instance/{instanceId}/analytics_entities", Get);
  ADD_METHOD_TO(SecuRTHandler::handleOptions, "/v1/securt/instance", Options);
  ADD_METHOD_TO(SecuRTHandler::handleOptions,
                "/v1/securt/instance/{instanceId}", Options);
  ADD_METHOD_TO(SecuRTHandler::handleOptions,
                "/v1/securt/instance/{instanceId}/stats", Options);
  ADD_METHOD_TO(SecuRTHandler::handleOptions,
                "/v1/securt/instance/{instanceId}/analytics_entities", Options);
  METHOD_LIST_END

  /**
   * @brief Handle POST /v1/securt/instance
   * Creates a new SecuRT instance
   */
  void createInstance(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle PUT /v1/securt/instance/{instanceId}
   * Creates a SecuRT instance with specific ID
   */
  void createInstanceWithId(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle PATCH /v1/securt/instance/{instanceId}
   * Updates a SecuRT instance
   */
  void updateInstance(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle DELETE /v1/securt/instance/{instanceId}
   * Deletes a SecuRT instance
   */
  void deleteInstance(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle GET /v1/securt/instance/{instanceId}/stats
   * Gets statistics for a SecuRT instance
   */
  void getInstanceStats(const HttpRequestPtr &req,
                        std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle GET /v1/securt/instance/{instanceId}/analytics_entities
   * Gets all analytics entities (areas and lines) for a SecuRT instance
   */
  void getAnalyticsEntities(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle OPTIONS request for CORS preflight
   */
  void handleOptions(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Set instance manager (dependency injection)
   */
  static void setInstanceManager(SecuRTInstanceManager *manager);

  /**
   * @brief Set analytics entities manager (dependency injection)
   */
  static void setAnalyticsEntitiesManager(AnalyticsEntitiesManager *manager);

private:
  static SecuRTInstanceManager *instance_manager_;
  static AnalyticsEntitiesManager *analytics_entities_manager_;

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
};

