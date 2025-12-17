#pragma once

#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <json/json.h>

using namespace drogon;

/**
 * @brief Create Instance Handler
 *
 * Handles POST /v1/core/instance endpoint for creating new AI instances.
 *
 * Endpoints:
 * - POST /v1/core/instance - Create a new instance
 */
class CreateInstanceHandler
    : public drogon::HttpController<CreateInstanceHandler> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(CreateInstanceHandler::createInstance, "/v1/core/instance",
                Post);
  ADD_METHOD_TO(CreateInstanceHandler::handleOptions, "/v1/core/instance",
                Options);
  METHOD_LIST_END

  /**
   * @brief Handle POST /v1/core/instance
   * Creates a new AI instance based on the request
   */
  void createInstance(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle OPTIONS request for CORS preflight
   */
  void handleOptions(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Set instance registry (dependency injection)
   */
  static void setInstanceRegistry(class InstanceRegistry *registry);

  /**
   * @brief Set solution registry (dependency injection)
   */
  static void setSolutionRegistry(class SolutionRegistry *registry);

private:
  static class InstanceRegistry *instance_registry_;
  static class SolutionRegistry *solution_registry_;

  /**
   * @brief Parse JSON request body to CreateInstanceRequest
   */
  bool parseRequest(const Json::Value &json, class CreateInstanceRequest &req,
                    std::string &error);

  /**
   * @brief Convert InstanceInfo to JSON response
   */
  Json::Value instanceInfoToJson(const class InstanceInfo &info) const;

  /**
   * @brief Create error response
   */
  HttpResponsePtr createErrorResponse(int statusCode, const std::string &error,
                                      const std::string &message = "") const;
};
