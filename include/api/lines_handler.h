#pragma once

#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <json/json.h>

using namespace drogon;

/**
 * @brief Lines Management Handler
 * 
 * Handles crossing lines management for ba_crossline instances.
 * 
 * Endpoints:
 * - GET /v1/core/instance/{instanceId}/lines - Get all lines
 * - POST /v1/core/instance/{instanceId}/lines - Create a new line
 * - DELETE /v1/core/instance/{instanceId}/lines - Delete all lines
 * - DELETE /v1/core/instance/{instanceId}/lines/{lineId} - Delete a specific line
 */
class LinesHandler : public drogon::HttpController<LinesHandler> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(LinesHandler::getAllLines, "/v1/core/instance/{instanceId}/lines", Get);
        ADD_METHOD_TO(LinesHandler::createLine, "/v1/core/instance/{instanceId}/lines", Post);
        ADD_METHOD_TO(LinesHandler::deleteAllLines, "/v1/core/instance/{instanceId}/lines", Delete);
        ADD_METHOD_TO(LinesHandler::deleteLine, "/v1/core/instance/{instanceId}/lines/{lineId}", Delete);
        ADD_METHOD_TO(LinesHandler::handleOptions, "/v1/core/instance/{instanceId}/lines", Options);
        ADD_METHOD_TO(LinesHandler::handleOptions, "/v1/core/instance/{instanceId}/lines/{lineId}", Options);
    METHOD_LIST_END
    
    /**
     * @brief Handle GET /v1/core/instance/{instanceId}/lines
     * Gets all crossing lines for an instance
     */
    void getAllLines(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle POST /v1/core/instance/{instanceId}/lines
     * Creates a new crossing line for an instance
     */
    void createLine(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle DELETE /v1/core/instance/{instanceId}/lines
     * Deletes all crossing lines for an instance
     */
    void deleteAllLines(const HttpRequestPtr &req,
                       std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle DELETE /v1/core/instance/{instanceId}/lines/{lineId}
     * Deletes a specific crossing line by ID
     */
    void deleteLine(const HttpRequestPtr &req,
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
     * @brief Extract instance ID from request path
     */
    std::string extractInstanceId(const HttpRequestPtr &req) const;
    
    /**
     * @brief Extract line ID from request path
     */
    std::string extractLineId(const HttpRequestPtr &req) const;
    
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
     * @brief Load lines from instance config
     * @param instanceId Instance ID
     * @return JSON array of lines, empty array if not found or error
     */
    Json::Value loadLinesFromConfig(const std::string& instanceId) const;
    
    /**
     * @brief Save lines to instance config
     * @param instanceId Instance ID
     * @param lines JSON array of lines
     * @return true if successful, false otherwise
     */
    bool saveLinesToConfig(const std::string& instanceId, const Json::Value& lines) const;
    
    /**
     * @brief Validate line coordinates
     * @param coordinates JSON array of coordinate objects
     * @param error Error message output
     * @return true if valid, false otherwise
     */
    bool validateCoordinates(const Json::Value& coordinates, std::string& error) const;
    
    /**
     * @brief Validate direction value
     * @param direction Direction string
     * @param error Error message output
     * @return true if valid, false otherwise
     */
    bool validateDirection(const std::string& direction, std::string& error) const;
    
    /**
     * @brief Validate classes array
     * @param classes JSON array of class strings
     * @param error Error message output
     * @return true if valid, false otherwise
     */
    bool validateClasses(const Json::Value& classes, std::string& error) const;
    
    /**
     * @brief Validate color array
     * @param color JSON array of color values
     * @param error Error message output
     * @return true if valid, false otherwise
     */
    bool validateColor(const Json::Value& color, std::string& error) const;
};

