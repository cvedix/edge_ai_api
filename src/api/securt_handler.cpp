#include "api/securt_handler.h"
#include "core/analytics_entities_manager.h"
#include "core/logging_flags.h"
#include "core/logger.h"
#include "core/metrics_interceptor.h"
#include "core/securt_instance.h"
#include "core/securt_instance_manager.h"
#include <chrono>
#include <drogon/HttpResponse.h>
#include <iostream>

SecuRTInstanceManager *SecuRTHandler::instance_manager_ = nullptr;
AnalyticsEntitiesManager *SecuRTHandler::analytics_entities_manager_ = nullptr;

void SecuRTHandler::setInstanceManager(SecuRTInstanceManager *manager) {
  instance_manager_ = manager;
}

void SecuRTHandler::setAnalyticsEntitiesManager(
    AnalyticsEntitiesManager *manager) {
  analytics_entities_manager_ = manager;
}

void SecuRTHandler::createInstance(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {

  auto start_time = std::chrono::steady_clock::now();

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] POST /v1/securt/instance - Create SecuRT instance";
    PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
    PLOG_DEBUG << "[API] Request path: " << req->getPath();
  }

  try {
    if (!instance_manager_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] POST /v1/securt/instance - Error: Instance manager "
                      "not initialized";
      }
      callback(createErrorResponse(500, "Internal server error",
                                   "Instance manager not initialized"));
      return;
    }

    // Parse JSON body
    auto json = req->getJsonObject();
    if (!json) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING
            << "[API] POST /v1/securt/instance - Error: Invalid JSON body";
        PLOG_DEBUG << "[API] Request body: " << req->getBody();
      }
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }

    if (isApiLoggingEnabled()) {
      Json::StreamWriterBuilder builder;
      builder["indentation"] = "";
      std::string jsonStr = Json::writeString(builder, *json);
      PLOG_DEBUG << "[API] POST /v1/securt/instance - Request body: " << jsonStr;
    }

    // Parse request - instanceId is optional
    std::string instanceId;
    if (json->isMember("instanceId") && (*json)["instanceId"].isString()) {
      instanceId = (*json)["instanceId"].asString();
      if (isApiLoggingEnabled()) {
        PLOG_DEBUG << "[API] POST /v1/securt/instance - InstanceId from request: " << instanceId;
      }
    } else {
      if (isApiLoggingEnabled()) {
        PLOG_DEBUG << "[API] POST /v1/securt/instance - No instanceId provided, will generate";
      }
    }

    // Parse SecuRTInstanceWrite
    SecuRTInstanceWrite write = SecuRTInstanceWrite::fromJson(*json);
    if (isApiLoggingEnabled()) {
      PLOG_DEBUG << "[API] POST /v1/securt/instance - Parsed instance data - name: " << write.name
                 << ", detectorMode: " << write.detectorMode;
    }

    // Validate name is provided
    if (write.name.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] POST /v1/securt/instance - Error: name is required";
      }
      callback(createErrorResponse(400, "Invalid request",
                                   "Field 'name' is required"));
      return;
    }

    // Create instance
    if (isApiLoggingEnabled()) {
      PLOG_DEBUG << "[API] POST /v1/securt/instance - Calling instance_manager_->createInstance()";
    }
    std::string createdId = instance_manager_->createInstance(instanceId, write);

    if (createdId.empty()) {
      auto end_time = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
          end_time - start_time);
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] POST /v1/securt/instance - Instance already exists "
                        "or creation failed - "
                     << duration.count() << "ms";
        if (!instanceId.empty()) {
          PLOG_DEBUG << "[API] POST /v1/securt/instance - Requested instanceId: " << instanceId;
        }
      }
      callback(createErrorResponse(409, "Conflict",
                                   "Instance already exists or creation failed"));
      return;
    }

    // Build response
    Json::Value response;
    response["instanceId"] = createdId;

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] POST /v1/securt/instance - Success: Created instance "
                << createdId << " - " << duration.count() << "ms";
      Json::StreamWriterBuilder builder;
      builder["indentation"] = "";
      std::string responseStr = Json::writeString(builder, response);
      PLOG_DEBUG << "[API] POST /v1/securt/instance - Response: " << responseStr;
    }

    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k201Created);

    // Add CORS headers
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");

    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] POST /v1/securt/instance - Exception: " << e.what()
                 << " - " << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] POST /v1/securt/instance - Unknown exception - "
                 << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void SecuRTHandler::createInstanceWithId(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {

  auto start_time = std::chrono::steady_clock::now();

  std::string instanceId = extractInstanceId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] PUT /v1/securt/instance/" << instanceId
              << " - Create SecuRT instance with ID";
    PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
    PLOG_DEBUG << "[API] Request path: " << req->getPath();
  }

  try {
    if (!instance_manager_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] PUT /v1/securt/instance/" << instanceId
                   << " - Error: Instance manager not initialized";
      }
      callback(createErrorResponse(500, "Internal server error",
                                   "Instance manager not initialized"));
      return;
    }

    if (instanceId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] PUT /v1/securt/instance/{instanceId} - Error: Instance ID is required";
      }
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID is required"));
      return;
    }

    // Parse JSON body
    auto json = req->getJsonObject();
    if (!json) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] PUT /v1/securt/instance/" << instanceId
                     << " - Error: Invalid JSON body";
        PLOG_DEBUG << "[API] Request body: " << req->getBody();
      }
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }

    if (isApiLoggingEnabled()) {
      Json::StreamWriterBuilder builder;
      builder["indentation"] = "";
      std::string jsonStr = Json::writeString(builder, *json);
      PLOG_DEBUG << "[API] PUT /v1/securt/instance/" << instanceId
                 << " - Request body: " << jsonStr;
    }

    // Parse SecuRTInstanceWrite
    SecuRTInstanceWrite write = SecuRTInstanceWrite::fromJson(*json);
    if (isApiLoggingEnabled()) {
      PLOG_DEBUG << "[API] PUT /v1/securt/instance/" << instanceId
                 << " - Parsed instance data - name: " << write.name
                 << ", detectorMode: " << write.detectorMode;
    }

    // Validate name is provided
    if (write.name.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] PUT /v1/securt/instance/" << instanceId
                     << " - Error: name is required";
      }
      callback(createErrorResponse(400, "Invalid request",
                                   "Field 'name' is required"));
      return;
    }

    // Create instance with ID
    if (isApiLoggingEnabled()) {
      PLOG_DEBUG << "[API] PUT /v1/securt/instance/" << instanceId
                 << " - Calling instance_manager_->createInstance()";
    }
    std::string createdId = instance_manager_->createInstance(instanceId, write);

    if (createdId.empty()) {
      auto end_time = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
          end_time - start_time);
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] PUT /v1/securt/instance/" << instanceId
                     << " - Instance already exists - " << duration.count()
                     << "ms";
      }
      callback(createErrorResponse(409, "Conflict",
                                   "Instance already exists"));
      return;
    }

    // Build response
    Json::Value response;
    response["instanceId"] = createdId;

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] PUT /v1/securt/instance/" << instanceId
                << " - Success: Created instance " << createdId
                << " - " << duration.count() << "ms";
      Json::StreamWriterBuilder builder;
      builder["indentation"] = "";
      std::string responseStr = Json::writeString(builder, response);
      PLOG_DEBUG << "[API] PUT /v1/securt/instance/" << instanceId
                 << " - Response: " << responseStr;
    }

    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k201Created);

    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "PUT, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");

    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] PUT /v1/securt/instance/" << instanceId
                 << " - Exception: " << e.what() << " - " << duration.count()
                 << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void SecuRTHandler::updateInstance(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {

  auto start_time = std::chrono::steady_clock::now();

  std::string instanceId = extractInstanceId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] PATCH /v1/securt/instance/" << instanceId
              << " - Update SecuRT instance";
    PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
    PLOG_DEBUG << "[API] Request path: " << req->getPath();
  }

  try {
    if (!instance_manager_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] PATCH /v1/securt/instance/" << instanceId
                   << " - Error: Instance manager not initialized";
      }
      callback(createErrorResponse(500, "Internal server error",
                                   "Instance manager not initialized"));
      return;
    }

    if (instanceId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] PATCH /v1/securt/instance/{instanceId} - Error: Instance ID is required";
      }
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID is required"));
      return;
    }

    // Check if instance exists
    if (isApiLoggingEnabled()) {
      PLOG_DEBUG << "[API] PATCH /v1/securt/instance/" << instanceId
                 << " - Checking if instance exists";
    }
    if (!instance_manager_->hasInstance(instanceId)) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] PATCH /v1/securt/instance/" << instanceId
                     << " - Error: Instance does not exist";
      }
      callback(createErrorResponse(404, "Not Found",
                                   "Instance does not exist"));
      return;
    }

    // Parse JSON body
    auto json = req->getJsonObject();
    if (!json) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] PATCH /v1/securt/instance/" << instanceId
                     << " - Error: Invalid JSON body";
        PLOG_DEBUG << "[API] Request body: " << req->getBody();
      }
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }

    if (isApiLoggingEnabled()) {
      Json::StreamWriterBuilder builder;
      builder["indentation"] = "";
      std::string jsonStr = Json::writeString(builder, *json);
      PLOG_DEBUG << "[API] PATCH /v1/securt/instance/" << instanceId
                 << " - Request body: " << jsonStr;
    }

    // Parse SecuRTInstanceWrite
    SecuRTInstanceWrite write = SecuRTInstanceWrite::fromJson(*json);
    if (isApiLoggingEnabled()) {
      PLOG_DEBUG << "[API] PATCH /v1/securt/instance/" << instanceId
                 << " - Parsed update data - name: " << write.name;
    }

    // Update instance
    if (isApiLoggingEnabled()) {
      PLOG_DEBUG << "[API] PATCH /v1/securt/instance/" << instanceId
                 << " - Calling instance_manager_->updateInstance()";
    }
    bool success = instance_manager_->updateInstance(instanceId, write);

    if (!success) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] PATCH /v1/securt/instance/" << instanceId
                     << " - Error: Update failed - instance does not exist";
      }
      callback(createErrorResponse(404, "Not Found",
                                   "Instance does not exist"));
      return;
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] PATCH /v1/securt/instance/" << instanceId
                << " - Success: Updated instance - " << duration.count() << "ms";
    }

    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k204NoContent);

    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "PATCH, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");

    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] PATCH /v1/securt/instance/" << instanceId
                 << " - Exception: " << e.what() << " - " << duration.count()
                 << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] PATCH /v1/securt/instance/" << instanceId
                 << " - Unknown exception - " << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void SecuRTHandler::deleteInstance(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {

  auto start_time = std::chrono::steady_clock::now();

  std::string instanceId = extractInstanceId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] DELETE /v1/securt/instance/" << instanceId
              << " - Delete SecuRT instance";
    PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
    PLOG_DEBUG << "[API] Request path: " << req->getPath();
  }

  try {
    if (!instance_manager_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] DELETE /v1/securt/instance/" << instanceId
                   << " - Error: Instance manager not initialized";
      }
      callback(createErrorResponse(500, "Internal server error",
                                   "Instance manager not initialized"));
      return;
    }

    if (instanceId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] DELETE /v1/securt/instance/{instanceId} - Error: Instance ID is required";
      }
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID is required"));
      return;
    }

    // Check if instance exists
    if (isApiLoggingEnabled()) {
      PLOG_DEBUG << "[API] DELETE /v1/securt/instance/" << instanceId
                 << " - Checking if instance exists";
    }
    if (!instance_manager_->hasInstance(instanceId)) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] DELETE /v1/securt/instance/" << instanceId
                     << " - Error: Instance does not exist";
      }
      callback(createErrorResponse(404, "Not Found",
                                   "Instance does not exist"));
      return;
    }

    // Delete instance
    if (isApiLoggingEnabled()) {
      PLOG_DEBUG << "[API] DELETE /v1/securt/instance/" << instanceId
                 << " - Calling instance_manager_->deleteInstance()";
    }
    bool success = instance_manager_->deleteInstance(instanceId);

    if (!success) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] DELETE /v1/securt/instance/" << instanceId
                     << " - Error: Delete failed - instance does not exist";
      }
      callback(createErrorResponse(404, "Not Found",
                                   "Instance does not exist"));
      return;
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] DELETE /v1/securt/instance/" << instanceId
                << " - Success: Deleted instance - " << duration.count() << "ms";
    }

    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k204NoContent);

    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "DELETE, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");

    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] DELETE /v1/securt/instance/" << instanceId
                 << " - Exception: " << e.what() << " - " << duration.count()
                 << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] DELETE /v1/securt/instance/" << instanceId
                 << " - Unknown exception - " << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void SecuRTHandler::getInstanceStats(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {

  auto start_time = std::chrono::steady_clock::now();

  std::string instanceId = extractInstanceId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] GET /v1/securt/instance/" << instanceId
              << "/stats - Get instance statistics";
    PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
    PLOG_DEBUG << "[API] Request path: " << req->getPath();
  }

  try {
    if (!instance_manager_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] GET /v1/securt/instance/" << instanceId
                   << "/stats - Error: Instance manager not initialized";
      }
      callback(createErrorResponse(500, "Internal server error",
                                   "Instance manager not initialized"));
      return;
    }

    if (instanceId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] GET /v1/securt/instance/{instanceId}/stats - Error: Instance ID is required";
      }
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID is required"));
      return;
    }

    // Check if instance exists
    if (isApiLoggingEnabled()) {
      PLOG_DEBUG << "[API] GET /v1/securt/instance/" << instanceId
                 << "/stats - Checking if instance exists";
    }
    if (!instance_manager_->hasInstance(instanceId)) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] GET /v1/securt/instance/" << instanceId
                     << "/stats - Error: Instance does not exist";
      }
      callback(createErrorResponse(404, "Not Found",
                                   "Instance does not exist"));
      return;
    }

    // Get statistics
    if (isApiLoggingEnabled()) {
      PLOG_DEBUG << "[API] GET /v1/securt/instance/" << instanceId
                 << "/stats - Calling instance_manager_->getStatistics()";
    }
    SecuRTInstanceStats stats = instance_manager_->getStatistics(instanceId);

    // Build response
    Json::Value response = stats.toJson();

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] GET /v1/securt/instance/" << instanceId
                << "/stats - Success - " << duration.count() << "ms";
      Json::StreamWriterBuilder builder;
      builder["indentation"] = "";
      std::string responseStr = Json::writeString(builder, response);
      PLOG_DEBUG << "[API] GET /v1/securt/instance/" << instanceId
                 << "/stats - Response: " << responseStr;
    }

    callback(createSuccessResponse(response));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] GET /v1/securt/instance/" << instanceId
                 << "/stats - Exception: " << e.what() << " - "
                 << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] GET /v1/securt/instance/" << instanceId
                 << "/stats - Unknown exception - " << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void SecuRTHandler::getAnalyticsEntities(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {

  auto start_time = std::chrono::steady_clock::now();

  std::string instanceId = extractInstanceId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] GET /v1/securt/instance/" << instanceId
              << "/analytics_entities - Get analytics entities";
    PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
    PLOG_DEBUG << "[API] Request path: " << req->getPath();
  }

  try {
    if (!instance_manager_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] GET /v1/securt/instance/" << instanceId
                   << "/analytics_entities - Error: Instance manager not initialized";
      }
      callback(createErrorResponse(500, "Internal server error",
                                   "Instance manager not initialized"));
      return;
    }

    if (!analytics_entities_manager_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] GET /v1/securt/instance/" << instanceId
                   << "/analytics_entities - Error: Analytics entities manager not initialized";
      }
      callback(createErrorResponse(500, "Internal server error",
                                   "Analytics entities manager not initialized"));
      return;
    }

    if (instanceId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] GET /v1/securt/instance/{instanceId}/analytics_entities - Error: Instance ID is required";
      }
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID is required"));
      return;
    }

    // Check if instance exists
    if (isApiLoggingEnabled()) {
      PLOG_DEBUG << "[API] GET /v1/securt/instance/" << instanceId
                 << "/analytics_entities - Checking if instance exists";
    }
    if (!instance_manager_->hasInstance(instanceId)) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] GET /v1/securt/instance/" << instanceId
                     << "/analytics_entities - Error: Instance does not exist";
      }
      callback(createErrorResponse(404, "Not Found",
                                   "Instance does not exist"));
      return;
    }

    // Get analytics entities
    if (isApiLoggingEnabled()) {
      PLOG_DEBUG << "[API] GET /v1/securt/instance/" << instanceId
                 << "/analytics_entities - Calling analytics_entities_manager_->getAnalyticsEntities()";
    }
    Json::Value response = analytics_entities_manager_->getAnalyticsEntities(instanceId);

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] GET /v1/securt/instance/" << instanceId
                << "/analytics_entities - Success - " << duration.count() << "ms";
      Json::StreamWriterBuilder builder;
      builder["indentation"] = "";
      std::string responseStr = Json::writeString(builder, response);
      PLOG_DEBUG << "[API] GET /v1/securt/instance/" << instanceId
                 << "/analytics_entities - Response size: " << responseStr.length() << " chars";
    }

    callback(createSuccessResponse(response));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] GET /v1/securt/instance/" << instanceId
                 << "/analytics_entities - Exception: " << e.what() << " - "
                 << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void SecuRTHandler::handleOptions(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  MetricsInterceptor::setHandlerStartTime(req);

  auto resp = HttpResponse::newHttpResponse();
  resp->setStatusCode(k200OK);
  resp->addHeader("Access-Control-Allow-Origin", "*");
  resp->addHeader("Access-Control-Allow-Methods",
                   "GET, POST, PUT, PATCH, DELETE, OPTIONS");
  resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
  resp->addHeader("Access-Control-Max-Age", "3600");

  MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
}

std::string SecuRTHandler::extractInstanceId(const HttpRequestPtr &req) const {
  // Try getParameter first (standard way for path parameters)
  std::string instanceId = req->getParameter("instanceId");

  // Fallback: extract from path if getParameter doesn't work
  if (instanceId.empty()) {
    std::string path = req->getPath();
    // Try /securt/instance/ pattern
    size_t instancePos = path.find("/securt/instance/");
    if (instancePos != std::string::npos) {
      size_t start = instancePos + 17; // length of "/securt/instance/"
      size_t end = path.find("/", start);
      if (end == std::string::npos) {
        end = path.length();
      }
      instanceId = path.substr(start, end - start);
    }
  }

  return instanceId;
}

HttpResponsePtr SecuRTHandler::createErrorResponse(int statusCode,
                                                    const std::string &error,
                                                    const std::string &message) const {
  Json::Value json;
  json["error"] = error;
  if (!message.empty()) {
    json["message"] = message;
  }

  auto resp = HttpResponse::newHttpJsonResponse(json);
  resp->setStatusCode(static_cast<drogon::HttpStatusCode>(statusCode));

  resp->addHeader("Access-Control-Allow-Origin", "*");
  resp->addHeader("Access-Control-Allow-Methods",
                   "GET, POST, PUT, PATCH, DELETE, OPTIONS");
  resp->addHeader("Access-Control-Allow-Headers", "Content-Type");

  return resp;
}

HttpResponsePtr SecuRTHandler::createSuccessResponse(const Json::Value &data,
                                                      int statusCode) const {
  auto resp = HttpResponse::newHttpJsonResponse(data);
  resp->setStatusCode(static_cast<drogon::HttpStatusCode>(statusCode));

  resp->addHeader("Access-Control-Allow-Origin", "*");
  resp->addHeader("Access-Control-Allow-Methods",
                   "GET, POST, PUT, PATCH, DELETE, OPTIONS");
  resp->addHeader("Access-Control-Allow-Headers", "Content-Type");

  return resp;
}

