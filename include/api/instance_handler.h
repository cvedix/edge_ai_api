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
 * - GET /v1/core/instance/status/summary - Get instance status summary (total, running, stopped counts)
 * - GET /v1/core/instances - List all instances
 * - GET /v1/core/instances/{instanceId} - Get instance details
 * - PUT /v1/core/instances/{instanceId} - Update instance information
 * - POST /v1/core/instances/{instanceId}/start - Start an instance
 * - POST /v1/core/instances/{instanceId}/stop - Stop an instance
 * - POST /v1/core/instances/{instanceId}/restart - Restart an instance
 * - DELETE /v1/core/instances/{instanceId} - Delete an instance
 * - POST /v1/core/instances/batch/start - Start multiple instances concurrently
 * - POST /v1/core/instances/batch/stop - Stop multiple instances concurrently
 * - POST /v1/core/instances/batch/restart - Restart multiple instances concurrently
 * - GET /v1/core/instances/{instanceId}/output - Get instance output/processing results
 * - POST /v1/core/instance/{instanceId}/input - Set input source for an instance
 * - GET /v1/core/instance/{instanceId}/config - Get instance configuration
 * - POST /v1/core/instance/{instanceId}/config - Set config value at a specific path
 * - GET /v1/core/instance/{instanceId}/output/stream - Get stream output configuration
 * - POST /v1/core/instance/{instanceId}/output/stream - Configure stream output (RTMP/RTSP/HLS)
 */
class InstanceHandler : public drogon::HttpController<InstanceHandler> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(InstanceHandler::getStatusSummary, "/v1/core/instance/status/summary", Get);
        ADD_METHOD_TO(InstanceHandler::listInstances, "/v1/core/instances", Get);
        ADD_METHOD_TO(InstanceHandler::getInstance, "/v1/core/instances/{instanceId}", Get);
        ADD_METHOD_TO(InstanceHandler::updateInstance, "/v1/core/instances/{instanceId}", Put);
        ADD_METHOD_TO(InstanceHandler::startInstance, "/v1/core/instances/{instanceId}/start", Post);
        ADD_METHOD_TO(InstanceHandler::stopInstance, "/v1/core/instances/{instanceId}/stop", Post);
        ADD_METHOD_TO(InstanceHandler::restartInstance, "/v1/core/instances/{instanceId}/restart", Post);
        ADD_METHOD_TO(InstanceHandler::deleteInstance, "/v1/core/instances/{instanceId}", Delete);
        ADD_METHOD_TO(InstanceHandler::getInstanceOutput, "/v1/core/instances/{instanceId}/output", Get);
        ADD_METHOD_TO(InstanceHandler::batchStartInstances, "/v1/core/instances/batch/start", Post);
        ADD_METHOD_TO(InstanceHandler::batchStopInstances, "/v1/core/instances/batch/stop", Post);
        ADD_METHOD_TO(InstanceHandler::batchRestartInstances, "/v1/core/instances/batch/restart", Post);
        ADD_METHOD_TO(InstanceHandler::setInstanceInput, "/v1/core/instance/{instanceId}/input", Post);
        ADD_METHOD_TO(InstanceHandler::getConfig, "/v1/core/instance/{instanceId}/config", Get);
        ADD_METHOD_TO(InstanceHandler::setConfig, "/v1/core/instance/{instanceId}/config", Post);
        ADD_METHOD_TO(InstanceHandler::getStatistics, "/v1/core/instance/{instanceId}/statistics", Get);
        ADD_METHOD_TO(InstanceHandler::getStreamOutput, "/v1/core/instance/{instanceId}/output/stream", Get);
        ADD_METHOD_TO(InstanceHandler::configureStreamOutput, "/v1/core/instance/{instanceId}/output/stream", Post);
        ADD_METHOD_TO(InstanceHandler::handleOptions, "/v1/core/instances", Options);
        ADD_METHOD_TO(InstanceHandler::handleOptions, "/v1/core/instances/{instanceId}", Options);
        ADD_METHOD_TO(InstanceHandler::handleOptions, "/v1/core/instances/{instanceId}/start", Options);
        ADD_METHOD_TO(InstanceHandler::handleOptions, "/v1/core/instances/{instanceId}/stop", Options);
        ADD_METHOD_TO(InstanceHandler::handleOptions, "/v1/core/instances/{instanceId}/restart", Options);
        ADD_METHOD_TO(InstanceHandler::handleOptions, "/v1/core/instances/{instanceId}/output", Options);
        ADD_METHOD_TO(InstanceHandler::handleOptions, "/v1/core/instance/{instanceId}/input", Options);
        ADD_METHOD_TO(InstanceHandler::handleOptions, "/v1/core/instance/{instanceId}/config", Options);
        ADD_METHOD_TO(InstanceHandler::handleOptions, "/v1/core/instance/{instanceId}/statistics", Options);
        ADD_METHOD_TO(InstanceHandler::handleOptions, "/v1/core/instance/{instanceId}/output/stream", Options);
        ADD_METHOD_TO(InstanceHandler::handleOptions, "/v1/core/instances/batch/start", Options);
        ADD_METHOD_TO(InstanceHandler::handleOptions, "/v1/core/instances/batch/stop", Options);
        ADD_METHOD_TO(InstanceHandler::handleOptions, "/v1/core/instances/batch/restart", Options);
        ADD_METHOD_TO(InstanceHandler::handleOptions, "/v1/core/instance/status/summary", Options);
    METHOD_LIST_END
    
    /**
     * @brief Handle GET /v1/core/instance/status/summary
     * Returns status summary about instances (total, configured, running, stopped counts)
     */
    void getStatusSummary(const HttpRequestPtr &req,
                          std::function<void(const HttpResponsePtr &)> &&callback);
    
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
     * @brief Handle GET /v1/core/instances/{instanceId}/output
     * Gets real-time output/processing results for a specific instance
     */
    void getInstanceOutput(const HttpRequestPtr &req,
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
     * @brief Handle POST /v1/core/instances/{instanceId}/restart
     * Restarts an instance (stops then starts)
     */
    void restartInstance(const HttpRequestPtr &req,
                        std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle DELETE /v1/core/instances/{instanceId}
     * Deletes an instance
     */
    void deleteInstance(const HttpRequestPtr &req,
                       std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle PUT /v1/core/instances/{instanceId}
     * Updates instance information
     */
    void updateInstance(const HttpRequestPtr &req,
                       std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle POST /v1/core/instances/batch/start
     * Starts multiple instances concurrently
     */
    void batchStartInstances(const HttpRequestPtr &req,
                            std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle POST /v1/core/instances/batch/stop
     * Stops multiple instances concurrently
     */
    void batchStopInstances(const HttpRequestPtr &req,
                           std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle POST /v1/core/instances/batch/restart
     * Restarts multiple instances concurrently
     */
    void batchRestartInstances(const HttpRequestPtr &req,
                              std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle POST /v1/core/instance/{instanceId}/input
     * Sets input source for an instance
     */
    void setInstanceInput(const HttpRequestPtr &req,
                        std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle GET /v1/core/instance/{instanceId}/config
     * Gets instance configuration (config format, not runtime state)
     */
    void getConfig(const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle POST /v1/core/instance/{instanceId}/config
     * Sets config value at a specific path (nested path supported with "/" separator)
     */
    void setConfig(const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle GET /v1/core/instance/{instanceId}/statistics
     * Gets real-time statistics for a specific instance
     */
    void getStatistics(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle GET /v1/core/instance/{instanceId}/output/stream
     * Gets stream output configuration for an instance
     */
    void getStreamOutput(const HttpRequestPtr &req,
                        std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle POST /v1/core/instance/{instanceId}/output/stream
     * Configures stream output for an instance (RTMP/RTSP/HLS)
     */
    void configureStreamOutput(const HttpRequestPtr &req,
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
     * @brief Parse JSON request body to UpdateInstanceRequest
     */
    bool parseUpdateRequest(const Json::Value& json, class UpdateInstanceRequest& req, std::string& error);
    
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
    
    /**
     * @brief Extract instance ID from request path
     * @param req HTTP request
     * @return Instance ID if found, empty string otherwise
     */
    std::string extractInstanceId(const HttpRequestPtr &req) const;
    
    /**
     * @brief Create success JSON response with CORS headers
     */
    HttpResponsePtr createSuccessResponse(const Json::Value& data, int statusCode = 200) const;
    
    /**
     * @brief Get output file information for an instance
     * @param instanceId Instance ID
     * @return JSON object with file output information
     */
    Json::Value getOutputFileInfo(const std::string& instanceId) const;
    
    /**
     * @brief Set nested JSON value at a path (e.g., "Output/handlers/Mqtt")
     * @param root Root JSON object to modify
     * @param path Path string with "/" separator (e.g., "Output/handlers/Mqtt")
     * @param value JSON value to set
     * @return true if successful, false otherwise
     */
    bool setNestedJsonValue(Json::Value& root, const std::string& path, const Json::Value& value) const;
};

