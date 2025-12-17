#include "api/group_handler.h"
#include "core/logger.h"
#include "core/logging_flags.h"
#include "groups/group_registry.h"
#include "groups/group_storage.h"
#include "instances/instance_registry.h"
#include "models/group_info.h"
#include <chrono>
#include <drogon/HttpResponse.h>
#include <regex>

GroupRegistry *GroupHandler::group_registry_ = nullptr;
GroupStorage *GroupHandler::group_storage_ = nullptr;
InstanceRegistry *GroupHandler::instance_registry_ = nullptr;

void GroupHandler::setGroupRegistry(GroupRegistry *registry) {
  group_registry_ = registry;
}

void GroupHandler::setGroupStorage(GroupStorage *storage) {
  group_storage_ = storage;
}

void GroupHandler::setInstanceRegistry(InstanceRegistry *registry) {
  instance_registry_ = registry;
}

std::string GroupHandler::extractGroupId(const HttpRequestPtr &req) const {
  std::string groupId = req->getParameter("groupId");

  if (groupId.empty()) {
    std::string path = req->getPath();
    size_t groupsPos = path.find("/groups/");
    if (groupsPos != std::string::npos) {
      size_t start = groupsPos + 8; // length of "/groups/"
      size_t end = path.find("/", start);
      if (end == std::string::npos) {
        end = path.length();
      }
      groupId = path.substr(start, end - start);
    }
  }

  return groupId;
}

bool GroupHandler::validateGroupId(const std::string &groupId,
                                   std::string &error) const {
  if (groupId.empty()) {
    error = "Group ID cannot be empty";
    return false;
  }

  std::regex pattern("^[A-Za-z0-9_-]+$");
  if (!std::regex_match(groupId, pattern)) {
    error = "Group ID must contain only alphanumeric characters, underscores, "
            "and hyphens";
    return false;
  }

  return true;
}

Json::Value GroupHandler::groupInfoToJson(const GroupInfo &group) const {
  Json::Value json(Json::objectValue);

  json["groupId"] = group.groupId;
  json["groupName"] = group.groupName;
  json["description"] = group.description;
  json["isDefault"] = group.isDefault;
  json["readOnly"] = group.readOnly;
  json["instanceCount"] = group.instanceCount;
  json["createdAt"] = group.createdAt;
  json["updatedAt"] = group.updatedAt;

  return json;
}

HttpResponsePtr
GroupHandler::createErrorResponse(int statusCode, const std::string &error,
                                  const std::string &message) const {
  Json::Value response(Json::objectValue);
  response["error"] = error;
  if (!message.empty()) {
    response["message"] = message;
  }

  auto resp = HttpResponse::newHttpJsonResponse(response);
  resp->setStatusCode(static_cast<HttpStatusCode>(statusCode));
  resp->addHeader("Access-Control-Allow-Origin", "*");
  resp->addHeader("Access-Control-Allow-Methods",
                  "GET, POST, PUT, DELETE, OPTIONS");
  resp->addHeader("Access-Control-Allow-Headers",
                  "Content-Type, Authorization");

  return resp;
}

HttpResponsePtr GroupHandler::createSuccessResponse(const Json::Value &data,
                                                    int statusCode) const {
  auto resp = HttpResponse::newHttpJsonResponse(data);
  resp->setStatusCode(static_cast<HttpStatusCode>(statusCode));
  resp->addHeader("Access-Control-Allow-Origin", "*");
  resp->addHeader("Access-Control-Allow-Methods",
                  "GET, POST, PUT, DELETE, OPTIONS");
  resp->addHeader("Access-Control-Allow-Headers",
                  "Content-Type, Authorization");
  return resp;
}

void GroupHandler::listGroups(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {

  auto start_time = std::chrono::steady_clock::now();

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] GET /v1/core/groups - List groups";
    PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
  }

  try {
    if (!group_registry_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] GET /v1/core/groups - Error: Group registry not "
                      "initialized";
      }
      callback(createErrorResponse(500, "Internal server error",
                                   "Group registry not initialized"));
      return;
    }

    auto allGroups = group_registry_->getAllGroups();

    Json::Value response;
    Json::Value groups(Json::arrayValue);

    int totalCount = 0;
    int defaultCount = 0;

    for (const auto &[groupId, group] : allGroups) {
      Json::Value groupJson = groupInfoToJson(group);
      groups.append(groupJson);
      totalCount++;
      if (group.isDefault) {
        defaultCount++;
      }
    }

    response["groups"] = groups;
    response["total"] = totalCount;
    response["default"] = defaultCount;
    response["custom"] = totalCount - defaultCount;

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] GET /v1/core/groups - Success: " << totalCount
                << " groups - " << duration.count() << "ms";
    }

    callback(createSuccessResponse(response));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] GET /v1/core/groups - Exception: " << e.what()
                 << " - " << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] GET /v1/core/groups - Unknown exception - "
                 << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void GroupHandler::getGroup(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {

  auto start_time = std::chrono::steady_clock::now();

  std::string groupId = extractGroupId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] GET /v1/core/groups/" << groupId << " - Get group";
  }

  try {
    if (!group_registry_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] GET /v1/core/groups/" << groupId
                   << " - Error: Group registry not initialized";
      }
      callback(createErrorResponse(500, "Internal server error",
                                   "Group registry not initialized"));
      return;
    }

    if (groupId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING
            << "[API] GET /v1/core/groups/{id} - Error: Group ID is empty";
      }
      callback(
          createErrorResponse(400, "Invalid request", "Group ID is required"));
      return;
    }

    auto optGroup = group_registry_->getGroup(groupId);
    if (!optGroup.has_value()) {
      auto end_time = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
          end_time - start_time);
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] GET /v1/core/groups/" << groupId
                     << " - Not found - " << duration.count() << "ms";
      }
      callback(
          createErrorResponse(404, "Not found", "Group not found: " + groupId));
      return;
    }

    Json::Value response = groupInfoToJson(optGroup.value());

    // Add instance IDs if available
    auto instanceIds = group_registry_->getInstanceIds(groupId);
    Json::Value instances(Json::arrayValue);
    for (const auto &id : instanceIds) {
      instances.append(id);
    }
    response["instanceIds"] = instances;

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] GET /v1/core/groups/" << groupId << " - Success - "
                << duration.count() << "ms";
    }

    callback(createSuccessResponse(response));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] GET /v1/core/groups/" << groupId
                 << " - Exception: " << e.what() << " - " << duration.count()
                 << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] GET /v1/core/groups/" << groupId
                 << " - Unknown exception - " << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void GroupHandler::createGroup(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {

  auto start_time = std::chrono::steady_clock::now();

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] POST /v1/core/groups - Create group";
    PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
  }

  try {
    if (!group_registry_ || !group_storage_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] POST /v1/core/groups - Error: Group registry or "
                      "storage not initialized";
      }
      callback(
          createErrorResponse(500, "Internal server error",
                              "Group registry or storage not initialized"));
      return;
    }

    auto json = req->getJsonObject();
    if (!json) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] POST /v1/core/groups - Error: Invalid JSON body";
      }
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }

    // Required: groupId
    if (!json->isMember("groupId") || !(*json)["groupId"].isString()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] POST /v1/core/groups - Error: Missing required "
                        "field: groupId";
      }
      callback(createErrorResponse(400, "Invalid request",
                                   "Missing required field: groupId"));
      return;
    }

    std::string groupId = (*json)["groupId"].asString();
    std::string validationError;
    if (!validateGroupId(groupId, validationError)) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] POST /v1/core/groups - Error: "
                     << validationError;
      }
      callback(createErrorResponse(400, "Invalid request", validationError));
      return;
    }

    // Check if group already exists
    if (group_registry_->groupExists(groupId)) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING
            << "[API] POST /v1/core/groups - Error: Group already exists: "
            << groupId;
      }
      callback(createErrorResponse(400, "Invalid request",
                                   "Group already exists: " + groupId));
      return;
    }

    // Required: groupName
    std::string groupName;
    if (json->isMember("groupName") && (*json)["groupName"].isString()) {
      groupName = (*json)["groupName"].asString();
    } else {
      groupName = groupId; // Use groupId as default name
    }

    // Optional: description
    std::string description;
    if (json->isMember("description") && (*json)["description"].isString()) {
      description = (*json)["description"].asString();
    }

    // Register group
    if (!group_registry_->registerGroup(groupId, groupName, description)) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING
            << "[API] POST /v1/core/groups - Error: Failed to register group";
      }
      callback(createErrorResponse(500, "Internal server error",
                                   "Failed to register group"));
      return;
    }

    // Save to storage
    auto optGroup = group_registry_->getGroup(groupId);
    if (optGroup.has_value() && !group_storage_->saveGroup(optGroup.value())) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] POST /v1/core/groups - Warning: Failed to save "
                        "group to storage";
      }
    }

    Json::Value response = groupInfoToJson(optGroup.value());

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] POST /v1/core/groups - Success: Created group "
                << groupId << " - " << duration.count() << "ms";
    }

    callback(createSuccessResponse(response, 201));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] POST /v1/core/groups - Exception: " << e.what()
                 << " - " << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] POST /v1/core/groups - Unknown exception - "
                 << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void GroupHandler::updateGroup(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {

  auto start_time = std::chrono::steady_clock::now();

  std::string groupId = extractGroupId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] PUT /v1/core/groups/" << groupId << " - Update group";
  }

  try {
    if (!group_registry_ || !group_storage_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] PUT /v1/core/groups/" << groupId
                   << " - Error: Group registry or storage not initialized";
      }
      callback(
          createErrorResponse(500, "Internal server error",
                              "Group registry or storage not initialized"));
      return;
    }

    if (groupId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING
            << "[API] PUT /v1/core/groups/{id} - Error: Group ID is empty";
      }
      callback(
          createErrorResponse(400, "Invalid request", "Group ID is required"));
      return;
    }

    auto json = req->getJsonObject();
    if (!json) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] PUT /v1/core/groups/" << groupId
                     << " - Error: Invalid JSON body";
      }
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }

    // Optional: groupName
    std::string groupName;
    if (json->isMember("groupName") && (*json)["groupName"].isString()) {
      groupName = (*json)["groupName"].asString();
    }

    // Optional: description
    std::string description;
    if (json->isMember("description") && (*json)["description"].isString()) {
      description = (*json)["description"].asString();
    }

    // Update group
    if (!group_registry_->updateGroup(groupId, groupName, description)) {
      auto end_time = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
          end_time - start_time);
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] PUT /v1/core/groups/" << groupId
                     << " - Failed to update - " << duration.count() << "ms";
      }
      callback(createErrorResponse(400, "Failed to update",
                                   "Could not update group. Check if group "
                                   "exists and is not read-only."));
      return;
    }

    // Save to storage
    auto optGroup = group_registry_->getGroup(groupId);
    if (optGroup.has_value() && !group_storage_->saveGroup(optGroup.value())) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] PUT /v1/core/groups/" << groupId
                     << " - Warning: Failed to save group to storage";
      }
    }

    Json::Value response = groupInfoToJson(optGroup.value());

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] PUT /v1/core/groups/" << groupId << " - Success - "
                << duration.count() << "ms";
    }

    callback(createSuccessResponse(response));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] PUT /v1/core/groups/" << groupId
                 << " - Exception: " << e.what() << " - " << duration.count()
                 << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] PUT /v1/core/groups/" << groupId
                 << " - Unknown exception - " << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void GroupHandler::deleteGroup(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {

  auto start_time = std::chrono::steady_clock::now();

  std::string groupId = extractGroupId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] DELETE /v1/core/groups/" << groupId
              << " - Delete group";
  }

  try {
    if (!group_registry_ || !group_storage_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] DELETE /v1/core/groups/" << groupId
                   << " - Error: Group registry or storage not initialized";
      }
      callback(
          createErrorResponse(500, "Internal server error",
                              "Group registry or storage not initialized"));
      return;
    }

    if (groupId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING
            << "[API] DELETE /v1/core/groups/{id} - Error: Group ID is empty";
      }
      callback(
          createErrorResponse(400, "Invalid request", "Group ID is required"));
      return;
    }

    // Delete from registry
    if (!group_registry_->deleteGroup(groupId)) {
      auto end_time = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
          end_time - start_time);
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] DELETE /v1/core/groups/" << groupId
                     << " - Failed to delete - " << duration.count() << "ms";
      }
      callback(
          createErrorResponse(400, "Failed to delete",
                              "Could not delete group. Check if group exists, "
                              "is not default, and has no instances."));
      return;
    }

    // Delete from storage
    group_storage_->deleteGroup(groupId);

    Json::Value response;
    response["success"] = true;
    response["message"] = "Group deleted successfully";
    response["groupId"] = groupId;

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] DELETE /v1/core/groups/" << groupId << " - Success - "
                << duration.count() << "ms";
    }

    callback(createSuccessResponse(response));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] DELETE /v1/core/groups/" << groupId
                 << " - Exception: " << e.what() << " - " << duration.count()
                 << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] DELETE /v1/core/groups/" << groupId
                 << " - Unknown exception - " << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void GroupHandler::getGroupInstances(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {

  auto start_time = std::chrono::steady_clock::now();

  std::string groupId = extractGroupId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] GET /v1/core/groups/" << groupId
              << "/instances - Get group instances";
  }

  try {
    if (!group_registry_ || !instance_registry_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] GET /v1/core/groups/" << groupId
                   << "/instances - Error: Registry not initialized";
      }
      callback(createErrorResponse(500, "Internal server error",
                                   "Registry not initialized"));
      return;
    }

    if (groupId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] GET /v1/core/groups/{id}/instances - Error: "
                        "Group ID is empty";
      }
      callback(
          createErrorResponse(400, "Invalid request", "Group ID is required"));
      return;
    }

    // Check if group exists
    if (!group_registry_->groupExists(groupId)) {
      auto end_time = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
          end_time - start_time);
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] GET /v1/core/groups/" << groupId
                     << "/instances - Group not found - " << duration.count()
                     << "ms";
      }
      callback(
          createErrorResponse(404, "Not found", "Group not found: " + groupId));
      return;
    }

    // Get instance IDs
    auto instanceIds = group_registry_->getInstanceIds(groupId);

    // Get instance details
    Json::Value response;
    Json::Value instances(Json::arrayValue);

    for (const auto &instanceId : instanceIds) {
      auto optInfo = instance_registry_->getInstance(instanceId);
      if (optInfo.has_value()) {
        Json::Value instance;
        instance["instanceId"] = optInfo.value().instanceId;
        instance["displayName"] = optInfo.value().displayName;
        instance["solutionId"] = optInfo.value().solutionId;
        instance["running"] = optInfo.value().running;
        instance["loaded"] = optInfo.value().loaded;
        instances.append(instance);
      }
    }

    response["groupId"] = groupId;
    response["instances"] = instances;
    response["count"] = static_cast<int>(instances.size());

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] GET /v1/core/groups/" << groupId
                << "/instances - Success: " << instances.size()
                << " instances - " << duration.count() << "ms";
    }

    callback(createSuccessResponse(response));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] GET /v1/core/groups/" << groupId
                 << "/instances - Exception: " << e.what() << " - "
                 << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] GET /v1/core/groups/" << groupId
                 << "/instances - Unknown exception - " << duration.count()
                 << "ms";
    }
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void GroupHandler::handleOptions(
    const HttpRequestPtr & /*req*/,
    std::function<void(const HttpResponsePtr &)> &&callback) {

  auto resp = HttpResponse::newHttpResponse();
  resp->setStatusCode(k200OK);
  resp->addHeader("Access-Control-Allow-Origin", "*");
  resp->addHeader("Access-Control-Allow-Methods",
                  "GET, POST, PUT, DELETE, OPTIONS");
  resp->addHeader("Access-Control-Allow-Headers",
                  "Content-Type, Authorization");
  resp->addHeader("Access-Control-Max-Age", "3600");
  callback(resp);
}
