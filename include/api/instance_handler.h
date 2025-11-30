#pragma once

#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <json/json.h>

using namespace drogon;

/**
 * @brief Instance Management Handler
 * 
 * Handles instance management operations.
 * 
 * Endpoints:
 * - GET /v1/core/instances - List all instances
 * - GET /v1/core/instances/{instanceId} - Get instance details
 * - POST /v1/core/instances/{instanceId}/start - Start an instance
 * - POST /v1/core/instances/{instanceId}/stop - Stop an instance
 * - DELETE /v1/core/instances/{instanceId} - Delete an instance
 */
class InstanceHandler : public drogon::HttpController<InstanceHandler> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(InstanceHandler::listInstances, "/v1/core/instances", Get);
        ADD_METHOD_TO(InstanceHandler::getInstance, "/v1/core/instances/{instanceId}", Get);
        ADD_METHOD_TO(InstanceHandler::startInstance, "/v1/core/instances/{instanceId}/start", Post);
        ADD_METHOD_TO(InstanceHandler::stopInstance, "/v1/core/instances/{instanceId}/stop", Post);
        ADD_METHOD_TO(InstanceHandler::deleteInstance, "/v1/core/instances/{instanceId}", Delete);
        ADD_METHOD_TO(InstanceHandler::handleOptions, "/v1/core/instances", Options);
        ADD_METHOD_TO(InstanceHandler::handleOptions, "/v1/core/instances/{instanceId}", Options);
        ADD_METHOD_TO(InstanceHandler::handleOptions, "/v1/core/instances/{instanceId}/start", Options);
        ADD_METHOD_TO(InstanceHandler::handleOptions, "/v1/core/instances/{instanceId}/stop", Options);
    METHOD_LIST_END
    
    /**
     * @brief Handle GET /v1/core/instances
     * Lists all instances with summary information
     */
    void listInstances(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle GET /v1/core/instances/{instanceId}
     * Gets detailed information about a specific instance
     */
    void getInstance(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle POST /v1/core/instances/{instanceId}/start
     * Starts an instance pipeline
     */
    void startInstance(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle POST /v1/core/instances/{instanceId}/stop
     * Stops an instance pipeline
     */
    void stopInstance(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle DELETE /v1/core/instances/{instanceId}
     * Deletes an instance
     */
    void deleteInstance(const HttpRequestPtr &req,
                       std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle OPTIONS request for CORS preflight
     */
    void handleOptions(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Set instance registry (dependency injection)
     */
    static void setInstanceRegistry(class InstanceRegistry* registry);
    
private:
    static class InstanceRegistry* instance_registry_;
    
    /**
     * @brief Convert InstanceInfo to JSON
     */
    Json::Value instanceInfoToJson(const class InstanceInfo& info) const;
    
    /**
     * @brief Create error response
     */
    HttpResponsePtr createErrorResponse(int statusCode,
                                       const std::string& error,
                                       const std::string& message = "") const;
};

