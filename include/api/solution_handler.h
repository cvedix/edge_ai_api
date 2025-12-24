#pragma once

#include "core/node_pool_manager.h"
#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <json/json.h>

using namespace drogon;

/**
 * @brief Solution Management Handler
 *
 * Handles solution management operations.
 *
 * Endpoints:
 * - GET /v1/core/solution - List all solutions
 * - GET /v1/core/solution/{solutionId} - Get solution details
 * - POST /v1/core/solution - Create a new solution
 * - PUT /v1/core/solution/{solutionId} - Update a solution
 * - DELETE /v1/core/solution/{solutionId} - Delete a solution
 */
class SolutionHandler : public drogon::HttpController<SolutionHandler> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(SolutionHandler::listSolutions, "/v1/core/solution", Get);
  ADD_METHOD_TO(SolutionHandler::getSolution, "/v1/core/solution/{solutionId}",
                Get);
  ADD_METHOD_TO(SolutionHandler::getSolutionParameters,
                "/v1/core/solution/{solutionId}/parameters", Get);
  ADD_METHOD_TO(SolutionHandler::getSolutionInstanceBody,
                "/v1/core/solution/{solutionId}/instance-body", Get);
  ADD_METHOD_TO(SolutionHandler::createSolution, "/v1/core/solution", Post);
  ADD_METHOD_TO(SolutionHandler::updateSolution,
                "/v1/core/solution/{solutionId}", Put);
  ADD_METHOD_TO(SolutionHandler::deleteSolution,
                "/v1/core/solution/{solutionId}", Delete);
  ADD_METHOD_TO(SolutionHandler::handleOptions, "/v1/core/solution", Options);
  ADD_METHOD_TO(SolutionHandler::handleOptions,
                "/v1/core/solution/{solutionId}", Options);
  ADD_METHOD_TO(SolutionHandler::handleOptions,
                "/v1/core/solution/{solutionId}/parameters", Options);
  ADD_METHOD_TO(SolutionHandler::handleOptions,
                "/v1/core/solution/{solutionId}/instance-body", Options);
  METHOD_LIST_END

  /**
   * @brief Handle GET /v1/core/solution
   * Lists all solutions with summary information
   */
  void listSolutions(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle GET /v1/core/solution/{solutionId}
   * Gets detailed information about a specific solution
   */
  void getSolution(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle POST /v1/core/solution
   * Creates a new solution
   */
  void createSolution(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle PUT /v1/core/solution/{solutionId}
   * Updates an existing solution
   */
  void updateSolution(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle DELETE /v1/core/solution/{solutionId}
   * Deletes a solution (default solutions cannot be deleted)
   */
  void deleteSolution(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle GET /v1/core/solution/{solutionId}/parameters
   * Returns parameter schema for creating an instance with this solution
   */
  void getSolutionParameters(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle GET /v1/core/solution/{solutionId}/instance-body
   * Returns example request body for creating an instance with this solution
   */
  void getSolutionInstanceBody(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle OPTIONS request for CORS preflight
   */
  void handleOptions(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Set solution registry and storage (dependency injection)
   */
  static void setSolutionRegistry(class SolutionRegistry *registry);
  static void setSolutionStorage(class SolutionStorage *storage);

private:
  static class SolutionRegistry *solution_registry_;
  static class SolutionStorage *solution_storage_;

  /**
   * @brief Extract solution ID from request path
   */
  std::string extractSolutionId(const HttpRequestPtr &req) const;

  /**
   * @brief Convert SolutionConfig to JSON
   */
  Json::Value solutionConfigToJson(const class SolutionConfig &config) const;

  /**
   * @brief Parse JSON request body to SolutionConfig
   */
  std::optional<class SolutionConfig>
  parseSolutionConfig(const Json::Value &json, std::string &error) const;

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
   * @brief Validate solution ID format
   */
  bool validateSolutionId(const std::string &solutionId,
                          std::string &error) const;

  // Helper functions for building parameter schema metadata
  void addStandardFieldSchema(
      Json::Value &schema, const std::string &fieldName,
      const std::string &type, bool required, const std::string &description,
      const std::string &pattern = "",
      const Json::Value &defaultValue = Json::Value(), int min = -1,
      int max = -1, const std::vector<std::string> &enumValues = {}) const;

  Json::Value buildParameterSchema(
      const std::string &paramName, const std::string &exampleValue,
      const std::set<std::string> &allParams,
      const std::map<std::string, std::string> &paramDefaults,
      const std::map<std::string, class NodePoolManager::NodeTemplate>
          &templatesByType,
      const class SolutionConfig &config) const;

  Json::Value buildFlexibleInputSchema() const;
  Json::Value buildFlexibleOutputSchema() const;

  // Helper functions for parameter metadata (similar to NodeHandler)
  std::string inferParameterType(const std::string &paramName) const;
  std::string getInputType(const std::string &paramName,
                           const std::string &paramType) const;
  std::string getWidgetType(const std::string &paramName,
                            const std::string &paramType) const;
  std::string getPlaceholder(const std::string &paramName) const;
  void addValidationRules(Json::Value &validation, const std::string &paramName,
                          const std::string &paramType) const;
  std::string getParameterDescription(const std::string &paramName) const;
  std::vector<std::string>
  getParameterExamples(const std::string &paramName) const;
  std::string getParameterCategory(const std::string &paramName) const;
};
