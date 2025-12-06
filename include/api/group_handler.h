#pragma once

#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <json/json.h>

using namespace drogon;

/**
 * @brief Group Management Handler
 * 
 * Handles group management operations.
 * 
 * Endpoints:
 * - GET /v1/core/groups - List all groups
 * - GET /v1/core/groups/{groupId} - Get group details
 * - POST /v1/core/groups - Create a new group
 * - PUT /v1/core/groups/{groupId} - Update a group
 * - DELETE /v1/core/groups/{groupId} - Delete a group
 * - GET /v1/core/groups/{groupId}/instances - Get instances in a group
 */
class GroupHandler : public drogon::HttpController<GroupHandler> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(GroupHandler::listGroups, "/v1/core/groups", Get);
        ADD_METHOD_TO(GroupHandler::getGroup, "/v1/core/groups/{groupId}", Get);
        ADD_METHOD_TO(GroupHandler::createGroup, "/v1/core/groups", Post);
        ADD_METHOD_TO(GroupHandler::updateGroup, "/v1/core/groups/{groupId}", Put);
        ADD_METHOD_TO(GroupHandler::deleteGroup, "/v1/core/groups/{groupId}", Delete);
        ADD_METHOD_TO(GroupHandler::getGroupInstances, "/v1/core/groups/{groupId}/instances", Get);
        ADD_METHOD_TO(GroupHandler::handleOptions, "/v1/core/groups", Options);
        ADD_METHOD_TO(GroupHandler::handleOptions, "/v1/core/groups/{groupId}", Options);
        ADD_METHOD_TO(GroupHandler::handleOptions, "/v1/core/groups/{groupId}/instances", Options);
    METHOD_LIST_END
    
    /**
     * @brief Handle GET /v1/core/groups
     * Lists all groups with summary information
     */
    void listGroups(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle GET /v1/core/groups/{groupId}
     * Gets detailed information about a specific group
     */
    void getGroup(const HttpRequestPtr &req,
                 std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle POST /v1/core/groups
     * Creates a new group
     */
    void createGroup(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle PUT /v1/core/groups/{groupId}
     * Updates an existing group
     */
    void updateGroup(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle DELETE /v1/core/groups/{groupId}
     * Deletes a group (default groups cannot be deleted)
     */
    void deleteGroup(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle GET /v1/core/groups/{groupId}/instances
     * Gets list of instances in a group
     */
    void getGroupInstances(const HttpRequestPtr &req,
                          std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle OPTIONS request for CORS preflight
     */
    void handleOptions(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Set group registry, storage, and instance registry (dependency injection)
     */
    static void setGroupRegistry(class GroupRegistry* registry);
    static void setGroupStorage(class GroupStorage* storage);
    static void setInstanceRegistry(class InstanceRegistry* registry);
    
private:
    static class GroupRegistry* group_registry_;
    static class GroupStorage* group_storage_;
    static class InstanceRegistry* instance_registry_;
    
    /**
     * @brief Extract group ID from request path
     */
    std::string extractGroupId(const HttpRequestPtr &req) const;
    
    /**
     * @brief Convert GroupInfo to JSON
     */
    Json::Value groupInfoToJson(const class GroupInfo& group) const;
    
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
     * @brief Validate group ID format
     */
    bool validateGroupId(const std::string& groupId, std::string& error) const;
};

