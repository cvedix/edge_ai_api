#pragma once

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
 * - GET /v1/core/solutions - List all solutions
 * - GET /v1/core/solutions/{solutionId} - Get solution details
 * - POST /v1/core/solutions - Create a new solution
 * - PUT /v1/core/solutions/{solutionId} - Update a solution
 * - DELETE /v1/core/solutions/{solutionId} - Delete a solution
 */
class SolutionHandler : public drogon::HttpController<SolutionHandler> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SolutionHandler::listSolutions, "/v1/core/solutions", Get);
        ADD_METHOD_TO(SolutionHandler::getSolution, "/v1/core/solutions/{solutionId}", Get);
        ADD_METHOD_TO(SolutionHandler::createSolution, "/v1/core/solutions", Post);
        ADD_METHOD_TO(SolutionHandler::updateSolution, "/v1/core/solutions/{solutionId}", Put);
        ADD_METHOD_TO(SolutionHandler::deleteSolution, "/v1/core/solutions/{solutionId}", Delete);
        ADD_METHOD_TO(SolutionHandler::handleOptions, "/v1/core/solutions", Options);
        ADD_METHOD_TO(SolutionHandler::handleOptions, "/v1/core/solutions/{solutionId}", Options);
    METHOD_LIST_END
    
    /**
     * @brief Handle GET /v1/core/solutions
     * Lists all solutions with summary information
     */
    void listSolutions(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle GET /v1/core/solutions/{solutionId}
     * Gets detailed information about a specific solution
     */
    void getSolution(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle POST /v1/core/solutions
     * Creates a new solution
     */
    void createSolution(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle PUT /v1/core/solutions/{solutionId}
     * Updates an existing solution
     */
    void updateSolution(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle DELETE /v1/core/solutions/{solutionId}
     * Deletes a solution (default solutions cannot be deleted)
     */
    void deleteSolution(const HttpRequestPtr &req,
                       std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle OPTIONS request for CORS preflight
     */
    void handleOptions(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Set solution registry and storage (dependency injection)
     */
    static void setSolutionRegistry(class SolutionRegistry* registry);
    static void setSolutionStorage(class SolutionStorage* storage);
    
private:
    static class SolutionRegistry* solution_registry_;
    static class SolutionStorage* solution_storage_;
    
    /**
     * @brief Extract solution ID from request path
     */
    std::string extractSolutionId(const HttpRequestPtr &req) const;
    
    /**
     * @brief Convert SolutionConfig to JSON
     */
    Json::Value solutionConfigToJson(const class SolutionConfig& config) const;
    
    /**
     * @brief Parse JSON request body to SolutionConfig
     */
    std::optional<class SolutionConfig> parseSolutionConfig(const Json::Value& json, std::string& error) const;
    
    /**
     * @brief Create error response
     */
    HttpResponsePtr createErrorResponse(int statusCode,
                                       const std::string& error,
                                       const std::string& message = "") const;
    
    /**
     * @brief Create success JSON response with CORS headers
     */
    HttpResponsePtr createSuccessResponse(const Json::Value& data, int statusCode = 200) const;
    
    /**
     * @brief Validate solution ID format
     */
    bool validateSolutionId(const std::string& solutionId, std::string& error) const;
};

