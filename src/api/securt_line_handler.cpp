#include "api/securt_line_handler.h"
#include "core/logging_flags.h"
#include "core/logger.h"
#include "core/metrics_interceptor.h"
#include "core/securt_instance_manager.h"
#include "core/securt_line_manager.h"
#include <chrono>
#include <drogon/HttpResponse.h>

SecuRTInstanceManager *SecuRTLineHandler::instance_manager_ = nullptr;
SecuRTLineManager *SecuRTLineHandler::line_manager_ = nullptr;

void SecuRTLineHandler::setInstanceManager(SecuRTInstanceManager *manager) {
  instance_manager_ = manager;
}

void SecuRTLineHandler::setLineManager(SecuRTLineManager *manager) {
  line_manager_ = manager;
}

void SecuRTLineHandler::createCountingLine(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();

  std::string instanceId = extractInstanceId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] POST /v1/securt/instance/" << instanceId
              << "/line/counting - Create counting line";
  }

  try {
    if (!instance_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Instance manager not initialized"));
      return;
    }

    if (!line_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Line manager not initialized"));
      return;
    }

    if (instanceId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID is required"));
      return;
    }

    // Check if instance exists
    if (!instance_manager_->hasInstance(instanceId)) {
      callback(createErrorResponse(404, "Not Found",
                                   "Instance does not exist"));
      return;
    }

    // Parse JSON body
    auto json = req->getJsonObject();
    if (!json) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }

    // Create line
    std::string lineId = line_manager_->createCountingLine(instanceId, *json);

    if (lineId.empty()) {
      auto end_time = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
          end_time - start_time);
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] POST /v1/securt/instance/" << instanceId
                     << "/line/counting - Validation failed - "
                     << duration.count() << "ms";
      }
      callback(createErrorResponse(400, "Invalid request",
                                   "Line validation failed"));
      return;
    }

    // Build response
    Json::Value response;
    response["lineId"] = lineId;

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] POST /v1/securt/instance/" << instanceId
                << "/line/counting - Success: Created line " << lineId << " - "
                << duration.count() << "ms";
    }

    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k201Created);

    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");

    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] POST /v1/securt/instance/" << instanceId
                 << "/line/counting - Exception: " << e.what() << " - "
                 << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void SecuRTLineHandler::createCountingLineWithId(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();

  std::string instanceId = extractInstanceId(req);
  std::string lineId = extractLineId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] PUT /v1/securt/instance/" << instanceId
              << "/line/counting/" << lineId
              << " - Create counting line with ID";
  }

  try {
    if (!instance_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Instance manager not initialized"));
      return;
    }

    if (!line_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Line manager not initialized"));
      return;
    }

    if (instanceId.empty() || lineId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID and Line ID are required"));
      return;
    }

    // Check if instance exists
    if (!instance_manager_->hasInstance(instanceId)) {
      callback(createErrorResponse(404, "Not Found",
                                   "Instance does not exist"));
      return;
    }

    // Check if line already exists
    if (line_manager_->hasLine(instanceId, lineId)) {
      callback(createErrorResponse(409, "Conflict",
                                   "Line already exists"));
      return;
    }

    // Parse JSON body
    auto json = req->getJsonObject();
    if (!json) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }

    // Create line with ID
    std::string createdId = line_manager_->createCountingLine(instanceId, *json, lineId);

    if (createdId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Line validation failed"));
      return;
    }

    // Build response
    Json::Value response;
    response["lineId"] = createdId;

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] PUT /v1/securt/instance/" << instanceId
                << "/line/counting/" << lineId
                << " - Success: Created line - " << duration.count() << "ms";
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
                 << "/line/counting/" << lineId
                 << " - Exception: " << e.what() << " - " << duration.count()
                 << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void SecuRTLineHandler::createCrossingLine(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();

  std::string instanceId = extractInstanceId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] POST /v1/securt/instance/" << instanceId
              << "/line/crossing - Create crossing line";
  }

  try {
    if (!instance_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Instance manager not initialized"));
      return;
    }

    if (!line_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Line manager not initialized"));
      return;
    }

    if (instanceId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID is required"));
      return;
    }

    // Check if instance exists
    if (!instance_manager_->hasInstance(instanceId)) {
      callback(createErrorResponse(404, "Not Found",
                                   "Instance does not exist"));
      return;
    }

    // Parse JSON body
    auto json = req->getJsonObject();
    if (!json) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }

    // Create line
    std::string lineId = line_manager_->createCrossingLine(instanceId, *json);

    if (lineId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Line validation failed"));
      return;
    }

    // Build response
    Json::Value response;
    response["lineId"] = lineId;

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] POST /v1/securt/instance/" << instanceId
                << "/line/crossing - Success: Created line " << lineId << " - "
                << duration.count() << "ms";
    }

    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k201Created);

    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");

    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] POST /v1/securt/instance/" << instanceId
                 << "/line/crossing - Exception: " << e.what() << " - "
                 << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void SecuRTLineHandler::createCrossingLineWithId(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();

  std::string instanceId = extractInstanceId(req);
  std::string lineId = extractLineId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] PUT /v1/securt/instance/" << instanceId
              << "/line/crossing/" << lineId
              << " - Create crossing line with ID";
  }

  try {
    if (!instance_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Instance manager not initialized"));
      return;
    }

    if (!line_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Line manager not initialized"));
      return;
    }

    if (instanceId.empty() || lineId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID and Line ID are required"));
      return;
    }

    // Check if instance exists
    if (!instance_manager_->hasInstance(instanceId)) {
      callback(createErrorResponse(404, "Not Found",
                                   "Instance does not exist"));
      return;
    }

    // Check if line already exists
    if (line_manager_->hasLine(instanceId, lineId)) {
      callback(createErrorResponse(409, "Conflict",
                                   "Line already exists"));
      return;
    }

    // Parse JSON body
    auto json = req->getJsonObject();
    if (!json) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }

    // Create line with ID
    std::string createdId = line_manager_->createCrossingLine(instanceId, *json, lineId);

    if (createdId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Line validation failed"));
      return;
    }

    // Build response
    Json::Value response;
    response["lineId"] = createdId;

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] PUT /v1/securt/instance/" << instanceId
                << "/line/crossing/" << lineId
                << " - Success: Created line - " << duration.count() << "ms";
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
                 << "/line/crossing/" << lineId
                 << " - Exception: " << e.what() << " - " << duration.count()
                 << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void SecuRTLineHandler::createTailgatingLine(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();

  std::string instanceId = extractInstanceId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] POST /v1/securt/instance/" << instanceId
              << "/line/tailgating - Create tailgating line";
  }

  try {
    if (!instance_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Instance manager not initialized"));
      return;
    }

    if (!line_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Line manager not initialized"));
      return;
    }

    if (instanceId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID is required"));
      return;
    }

    // Check if instance exists
    if (!instance_manager_->hasInstance(instanceId)) {
      callback(createErrorResponse(404, "Not Found",
                                   "Instance does not exist"));
      return;
    }

    // Parse JSON body
    auto json = req->getJsonObject();
    if (!json) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }

    // Create line
    std::string lineId = line_manager_->createTailgatingLine(instanceId, *json);

    if (lineId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Line validation failed"));
      return;
    }

    // Build response
    Json::Value response;
    response["lineId"] = lineId;

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] POST /v1/securt/instance/" << instanceId
                << "/line/tailgating - Success: Created line " << lineId
                << " - " << duration.count() << "ms";
    }

    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k201Created);

    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");

    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] POST /v1/securt/instance/" << instanceId
                 << "/line/tailgating - Exception: " << e.what() << " - "
                 << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void SecuRTLineHandler::createTailgatingLineWithId(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();

  std::string instanceId = extractInstanceId(req);
  std::string lineId = extractLineId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] PUT /v1/securt/instance/" << instanceId
              << "/line/tailgating/" << lineId
              << " - Create tailgating line with ID";
  }

  try {
    if (!instance_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Instance manager not initialized"));
      return;
    }

    if (!line_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Line manager not initialized"));
      return;
    }

    if (instanceId.empty() || lineId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID and Line ID are required"));
      return;
    }

    // Check if instance exists
    if (!instance_manager_->hasInstance(instanceId)) {
      callback(createErrorResponse(404, "Not Found",
                                   "Instance does not exist"));
      return;
    }

    // Check if line already exists
    if (line_manager_->hasLine(instanceId, lineId)) {
      callback(createErrorResponse(409, "Conflict",
                                   "Line already exists"));
      return;
    }

    // Parse JSON body
    auto json = req->getJsonObject();
    if (!json) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }

    // Create line with ID
    std::string createdId = line_manager_->createTailgatingLine(instanceId, *json, lineId);

    if (createdId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Line validation failed"));
      return;
    }

    // Build response
    Json::Value response;
    response["lineId"] = createdId;

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] PUT /v1/securt/instance/" << instanceId
                << "/line/tailgating/" << lineId
                << " - Success: Created line - " << duration.count() << "ms";
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
                 << "/line/tailgating/" << lineId
                 << " - Exception: " << e.what() << " - " << duration.count()
                 << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void SecuRTLineHandler::getAllLines(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();

  std::string instanceId = extractInstanceId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] GET /v1/securt/instance/" << instanceId
              << "/lines - Get all lines";
  }

  try {
    if (!instance_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Instance manager not initialized"));
      return;
    }

    if (!line_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Line manager not initialized"));
      return;
    }

    if (instanceId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID is required"));
      return;
    }

    // Check if instance exists
    if (!instance_manager_->hasInstance(instanceId)) {
      callback(createErrorResponse(404, "Not Found",
                                   "Instance does not exist"));
      return;
    }

    // Get all lines
    Json::Value response = line_manager_->getAllLines(instanceId);

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] GET /v1/securt/instance/" << instanceId
                << "/lines - Success - " << duration.count() << "ms";
    }

    callback(createSuccessResponse(response));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] GET /v1/securt/instance/" << instanceId
                 << "/lines - Exception: " << e.what() << " - "
                 << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void SecuRTLineHandler::deleteAllLines(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();

  std::string instanceId = extractInstanceId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] DELETE /v1/securt/instance/" << instanceId
              << "/lines - Delete all lines";
  }

  try {
    if (!instance_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Instance manager not initialized"));
      return;
    }

    if (!line_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Line manager not initialized"));
      return;
    }

    if (instanceId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID is required"));
      return;
    }

    // Check if instance exists
    if (!instance_manager_->hasInstance(instanceId)) {
      callback(createErrorResponse(404, "Not Found",
                                   "Instance does not exist"));
      return;
    }

    // Delete all lines
    line_manager_->deleteAllLines(instanceId);

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] DELETE /v1/securt/instance/" << instanceId
                << "/lines - Success - " << duration.count() << "ms";
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
                 << "/lines - Exception: " << e.what() << " - "
                 << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void SecuRTLineHandler::deleteLine(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();

  std::string instanceId = extractInstanceId(req);
  std::string lineId = extractLineId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] DELETE /v1/securt/instance/" << instanceId
              << "/line/" << lineId << " - Delete line";
  }

  try {
    if (!instance_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Instance manager not initialized"));
      return;
    }

    if (!line_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Line manager not initialized"));
      return;
    }

    if (instanceId.empty() || lineId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID and Line ID are required"));
      return;
    }

    // Check if instance exists
    if (!instance_manager_->hasInstance(instanceId)) {
      callback(createErrorResponse(404, "Not Found",
                                   "Instance does not exist"));
      return;
    }

    // Check if line exists
    if (!line_manager_->hasLine(instanceId, lineId)) {
      callback(createErrorResponse(404, "Not Found",
                                   "Line does not exist"));
      return;
    }

    // Delete line
    bool success = line_manager_->deleteLine(instanceId, lineId);

    if (!success) {
      callback(createErrorResponse(404, "Not Found",
                                   "Line does not exist"));
      return;
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] DELETE /v1/securt/instance/" << instanceId
                << "/line/" << lineId << " - Success - " << duration.count()
                << "ms";
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
                 << "/line/" << lineId << " - Exception: " << e.what() << " - "
                 << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void SecuRTLineHandler::handleOptions(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  MetricsInterceptor::setHandlerStartTime(req);

  auto resp = HttpResponse::newHttpResponse();
  resp->setStatusCode(k200OK);
  resp->addHeader("Access-Control-Allow-Origin", "*");
  resp->addHeader("Access-Control-Allow-Methods",
                   "GET, POST, PUT, DELETE, OPTIONS");
  resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
  resp->addHeader("Access-Control-Max-Age", "3600");

  MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
}

std::string SecuRTLineHandler::extractInstanceId(const HttpRequestPtr &req) const {
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

std::string SecuRTLineHandler::extractLineId(const HttpRequestPtr &req) const {
  // Try getParameter first
  std::string lineId = req->getParameter("lineId");

  // Fallback: extract from path
  if (lineId.empty()) {
    std::string path = req->getPath();
    // Try patterns like /line/counting/{lineId}, /line/crossing/{lineId}, /line/tailgating/{lineId}, or /line/{lineId}
    size_t linePos = path.find("/line/");
    if (linePos != std::string::npos) {
      size_t start = linePos + 6; // length of "/line/"
      // Skip line type if present (counting/, crossing/, tailgating/)
      if (path.substr(start, 9) == "counting/") {
        start += 9;
      } else if (path.substr(start, 9) == "crossing/") {
        start += 9;
      } else if (path.substr(start, 11) == "tailgating/") {
        start += 11;
      }
      size_t end = path.find("/", start);
      if (end == std::string::npos) {
        end = path.length();
      }
      lineId = path.substr(start, end - start);
    }
  }

  return lineId;
}

HttpResponsePtr SecuRTLineHandler::createErrorResponse(int statusCode,
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
                   "GET, POST, PUT, DELETE, OPTIONS");
  resp->addHeader("Access-Control-Allow-Headers", "Content-Type");

  return resp;
}

HttpResponsePtr SecuRTLineHandler::createSuccessResponse(const Json::Value &data,
                                                           int statusCode) const {
  auto resp = HttpResponse::newHttpJsonResponse(data);
  resp->setStatusCode(static_cast<drogon::HttpStatusCode>(statusCode));

  resp->addHeader("Access-Control-Allow-Origin", "*");
  resp->addHeader("Access-Control-Allow-Methods",
                   "GET, POST, PUT, DELETE, OPTIONS");
  resp->addHeader("Access-Control-Allow-Headers", "Content-Type");

  return resp;
}

