#include "api/area_handler.h"
#include "core/area_manager.h"
#include "core/area_types_specific.h"
#include "core/logging_flags.h"
#include "core/logger.h"
#include "core/metrics_interceptor.h"
#include <chrono>
#include <drogon/HttpResponse.h>

AreaManager *AreaHandler::area_manager_ = nullptr;

void AreaHandler::setAreaManager(AreaManager *manager) {
  area_manager_ = manager;
}

// ========================================================================
// Standard Areas - POST handlers
// ========================================================================

void AreaHandler::createCrossingArea(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] POST /v1/securt/instance/{instanceId}/area/crossing";
  }

  try {
    if (!area_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Area manager not initialized"));
      return;
    }

    std::string instanceId = extractInstanceId(req);
    if (instanceId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID is required"));
      return;
    }

    auto json = req->getJsonObject();
    if (!json) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }

    CrossingAreaWrite write = CrossingAreaWrite::fromJson(*json);
    std::string areaId = area_manager_->createCrossingArea(instanceId, write);

    if (areaId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Failed to create area. Check validation errors."));
      return;
    }

    Json::Value response;
    response["areaId"] = areaId;

    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k201Created);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");

    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));

  } catch (const std::exception &e) {
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void AreaHandler::createIntrusionArea(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();

  try {
    if (!area_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Area manager not initialized"));
      return;
    }

    std::string instanceId = extractInstanceId(req);
    if (instanceId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID is required"));
      return;
    }

    auto json = req->getJsonObject();
    if (!json) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }

    IntrusionAreaWrite write = IntrusionAreaWrite::fromJson(*json);
    std::string areaId = area_manager_->createIntrusionArea(instanceId, write);

    if (areaId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Failed to create area"));
      return;
    }

    Json::Value response;
    response["areaId"] = areaId;

    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k201Created);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");

    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));

  } catch (const std::exception &e) {
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void AreaHandler::createLoiteringArea(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  try {
    if (!area_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Area manager not initialized"));
      return;
    }

    std::string instanceId = extractInstanceId(req);
    if (instanceId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID is required"));
      return;
    }

    auto json = req->getJsonObject();
    if (!json) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }

    LoiteringAreaWrite write = LoiteringAreaWrite::fromJson(*json);
    std::string areaId = area_manager_->createLoiteringArea(instanceId, write);

    if (areaId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Failed to create area"));
      return;
    }

    Json::Value response;
    response["areaId"] = areaId;

    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k201Created);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");

    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));

  } catch (const std::exception &e) {
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void AreaHandler::createCrowdingArea(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  try {
    if (!area_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Area manager not initialized"));
      return;
    }

    std::string instanceId = extractInstanceId(req);
    if (instanceId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID is required"));
      return;
    }

    auto json = req->getJsonObject();
    if (!json) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }

    CrowdingAreaWrite write = CrowdingAreaWrite::fromJson(*json);
    std::string areaId = area_manager_->createCrowdingArea(instanceId, write);

    if (areaId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Failed to create area"));
      return;
    }

    Json::Value response;
    response["areaId"] = areaId;

    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k201Created);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");

    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));

  } catch (const std::exception &e) {
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void AreaHandler::createOccupancyArea(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  try {
    if (!area_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Area manager not initialized"));
      return;
    }

    std::string instanceId = extractInstanceId(req);
    if (instanceId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID is required"));
      return;
    }

    auto json = req->getJsonObject();
    if (!json) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }

    OccupancyAreaWrite write = OccupancyAreaWrite::fromJson(*json);
    std::string areaId = area_manager_->createOccupancyArea(instanceId, write);

    if (areaId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Failed to create area"));
      return;
    }

    Json::Value response;
    response["areaId"] = areaId;

    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k201Created);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");

    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));

  } catch (const std::exception &e) {
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void AreaHandler::createCrowdEstimationArea(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  try {
    if (!area_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Area manager not initialized"));
      return;
    }

    std::string instanceId = extractInstanceId(req);
    if (instanceId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID is required"));
      return;
    }

    auto json = req->getJsonObject();
    if (!json) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }

    CrowdEstimationAreaWrite write = CrowdEstimationAreaWrite::fromJson(*json);
    std::string areaId =
        area_manager_->createCrowdEstimationArea(instanceId, write);

    if (areaId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Failed to create area"));
      return;
    }

    Json::Value response;
    response["areaId"] = areaId;

    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k201Created);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");

    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));

  } catch (const std::exception &e) {
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void AreaHandler::createDwellingArea(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  try {
    if (!area_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Area manager not initialized"));
      return;
    }

    std::string instanceId = extractInstanceId(req);
    if (instanceId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID is required"));
      return;
    }

    auto json = req->getJsonObject();
    if (!json) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }

    DwellingAreaWrite write = DwellingAreaWrite::fromJson(*json);
    std::string areaId = area_manager_->createDwellingArea(instanceId, write);

    if (areaId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Failed to create area"));
      return;
    }

    Json::Value response;
    response["areaId"] = areaId;

    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k201Created);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");

    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));

  } catch (const std::exception &e) {
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void AreaHandler::createArmedPersonArea(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  try {
    if (!area_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Area manager not initialized"));
      return;
    }

    std::string instanceId = extractInstanceId(req);
    if (instanceId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID is required"));
      return;
    }

    auto json = req->getJsonObject();
    if (!json) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }

    ArmedPersonAreaWrite write = ArmedPersonAreaWrite::fromJson(*json);
    std::string areaId = area_manager_->createArmedPersonArea(instanceId, write);

    if (areaId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Failed to create area"));
      return;
    }

    Json::Value response;
    response["areaId"] = areaId;

    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k201Created);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");

    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));

  } catch (const std::exception &e) {
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void AreaHandler::createObjectLeftArea(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  try {
    if (!area_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Area manager not initialized"));
      return;
    }

    std::string instanceId = extractInstanceId(req);
    if (instanceId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID is required"));
      return;
    }

    auto json = req->getJsonObject();
    if (!json) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }

    ObjectLeftAreaWrite write = ObjectLeftAreaWrite::fromJson(*json);
    std::string areaId = area_manager_->createObjectLeftArea(instanceId, write);

    if (areaId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Failed to create area"));
      return;
    }

    Json::Value response;
    response["areaId"] = areaId;

    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k201Created);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");

    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));

  } catch (const std::exception &e) {
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void AreaHandler::createObjectRemovedArea(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  try {
    if (!area_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Area manager not initialized"));
      return;
    }

    std::string instanceId = extractInstanceId(req);
    if (instanceId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID is required"));
      return;
    }

    auto json = req->getJsonObject();
    if (!json) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }

    ObjectRemovedAreaWrite write = ObjectRemovedAreaWrite::fromJson(*json);
    std::string areaId =
        area_manager_->createObjectRemovedArea(instanceId, write);

    if (areaId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Failed to create area"));
      return;
    }

    Json::Value response;
    response["areaId"] = areaId;

    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k201Created);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");

    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));

  } catch (const std::exception &e) {
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void AreaHandler::createFallenPersonArea(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  try {
    if (!area_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Area manager not initialized"));
      return;
    }

    std::string instanceId = extractInstanceId(req);
    if (instanceId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID is required"));
      return;
    }

    auto json = req->getJsonObject();
    if (!json) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }

    FallenPersonAreaWrite write = FallenPersonAreaWrite::fromJson(*json);
    std::string areaId = area_manager_->createFallenPersonArea(instanceId, write);

    if (areaId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Failed to create area"));
      return;
    }

    Json::Value response;
    response["areaId"] = areaId;

    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k201Created);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");

    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));

  } catch (const std::exception &e) {
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

// ========================================================================
// Standard Areas - PUT handlers (create with ID)
// ========================================================================

void AreaHandler::createCrossingAreaWithId(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  try {
    if (!area_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Area manager not initialized"));
      return;
    }

    std::string instanceId = extractInstanceId(req);
    std::string areaId = extractAreaId(req);

    if (instanceId.empty() || areaId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID and Area ID are required"));
      return;
    }

    auto json = req->getJsonObject();
    if (!json) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }

    CrossingAreaWrite write = CrossingAreaWrite::fromJson(*json);
    std::string createdId =
        area_manager_->createCrossingAreaWithId(instanceId, areaId, write);

    if (createdId.empty()) {
      callback(createErrorResponse(409, "Conflict",
                                   "Area already exists or creation failed"));
      return;
    }

    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k201Created);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "PUT, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");

    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));

  } catch (const std::exception &e) {
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

// Similar pattern for other PUT handlers - implementing key ones
void AreaHandler::createIntrusionAreaWithId(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  try {
    if (!area_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Area manager not initialized"));
      return;
    }

    std::string instanceId = extractInstanceId(req);
    std::string areaId = extractAreaId(req);

    if (instanceId.empty() || areaId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID and Area ID are required"));
      return;
    }

    auto json = req->getJsonObject();
    if (!json) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }

    IntrusionAreaWrite write = IntrusionAreaWrite::fromJson(*json);
    std::string createdId =
        area_manager_->createIntrusionAreaWithId(instanceId, areaId, write);

    if (createdId.empty()) {
      callback(createErrorResponse(409, "Conflict",
                                   "Area already exists or creation failed"));
      return;
    }

    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k201Created);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "PUT, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");

    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));

  } catch (const std::exception &e) {
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

// Implement remaining PUT handlers with similar pattern
// (Loitering, Crowding, Occupancy, CrowdEstimation, Dwelling, ArmedPerson,
// ObjectLeft, ObjectRemoved, FallenPerson)

void AreaHandler::createLoiteringAreaWithId(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  try {
    if (!area_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Area manager not initialized"));
      return;
    }
    std::string instanceId = extractInstanceId(req);
    std::string areaId = extractAreaId(req);
    if (instanceId.empty() || areaId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID and Area ID are required"));
      return;
    }
    auto json = req->getJsonObject();
    if (!json) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }
    LoiteringAreaWrite write = LoiteringAreaWrite::fromJson(*json);
    std::string createdId =
        area_manager_->createLoiteringAreaWithId(instanceId, areaId, write);
    if (createdId.empty()) {
      callback(createErrorResponse(409, "Conflict",
                                   "Area already exists or creation failed"));
      return;
    }
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k201Created);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "PUT, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
  } catch (const std::exception &e) {
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void AreaHandler::createCrowdingAreaWithId(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  try {
    if (!area_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Area manager not initialized"));
      return;
    }
    std::string instanceId = extractInstanceId(req);
    std::string areaId = extractAreaId(req);
    if (instanceId.empty() || areaId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID and Area ID are required"));
      return;
    }
    auto json = req->getJsonObject();
    if (!json) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }
    CrowdingAreaWrite write = CrowdingAreaWrite::fromJson(*json);
    std::string createdId =
        area_manager_->createCrowdingAreaWithId(instanceId, areaId, write);
    if (createdId.empty()) {
      callback(createErrorResponse(409, "Conflict",
                                   "Area already exists or creation failed"));
      return;
    }
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k201Created);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "PUT, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
  } catch (const std::exception &e) {
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void AreaHandler::createOccupancyAreaWithId(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  try {
    if (!area_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Area manager not initialized"));
      return;
    }
    std::string instanceId = extractInstanceId(req);
    std::string areaId = extractAreaId(req);
    if (instanceId.empty() || areaId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID and Area ID are required"));
      return;
    }
    auto json = req->getJsonObject();
    if (!json) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }
    OccupancyAreaWrite write = OccupancyAreaWrite::fromJson(*json);
    std::string createdId =
        area_manager_->createOccupancyAreaWithId(instanceId, areaId, write);
    if (createdId.empty()) {
      callback(createErrorResponse(409, "Conflict",
                                   "Area already exists or creation failed"));
      return;
    }
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k201Created);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "PUT, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
  } catch (const std::exception &e) {
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void AreaHandler::createCrowdEstimationAreaWithId(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  try {
    if (!area_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Area manager not initialized"));
      return;
    }
    std::string instanceId = extractInstanceId(req);
    std::string areaId = extractAreaId(req);
    if (instanceId.empty() || areaId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID and Area ID are required"));
      return;
    }
    auto json = req->getJsonObject();
    if (!json) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }
    CrowdEstimationAreaWrite write = CrowdEstimationAreaWrite::fromJson(*json);
    std::string createdId = area_manager_->createCrowdEstimationAreaWithId(
        instanceId, areaId, write);
    if (createdId.empty()) {
      callback(createErrorResponse(409, "Conflict",
                                   "Area already exists or creation failed"));
      return;
    }
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k201Created);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "PUT, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
  } catch (const std::exception &e) {
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void AreaHandler::createDwellingAreaWithId(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  try {
    if (!area_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Area manager not initialized"));
      return;
    }
    std::string instanceId = extractInstanceId(req);
    std::string areaId = extractAreaId(req);
    if (instanceId.empty() || areaId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID and Area ID are required"));
      return;
    }
    auto json = req->getJsonObject();
    if (!json) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }
    DwellingAreaWrite write = DwellingAreaWrite::fromJson(*json);
    std::string createdId =
        area_manager_->createDwellingAreaWithId(instanceId, areaId, write);
    if (createdId.empty()) {
      callback(createErrorResponse(409, "Conflict",
                                   "Area already exists or creation failed"));
      return;
    }
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k201Created);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "PUT, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
  } catch (const std::exception &e) {
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void AreaHandler::createArmedPersonAreaWithId(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  try {
    if (!area_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Area manager not initialized"));
      return;
    }
    std::string instanceId = extractInstanceId(req);
    std::string areaId = extractAreaId(req);
    if (instanceId.empty() || areaId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID and Area ID are required"));
      return;
    }
    auto json = req->getJsonObject();
    if (!json) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }
    ArmedPersonAreaWrite write = ArmedPersonAreaWrite::fromJson(*json);
    std::string createdId =
        area_manager_->createArmedPersonAreaWithId(instanceId, areaId, write);
    if (createdId.empty()) {
      callback(createErrorResponse(409, "Conflict",
                                   "Area already exists or creation failed"));
      return;
    }
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k201Created);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "PUT, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
  } catch (const std::exception &e) {
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void AreaHandler::createObjectLeftAreaWithId(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  try {
    if (!area_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Area manager not initialized"));
      return;
    }
    std::string instanceId = extractInstanceId(req);
    std::string areaId = extractAreaId(req);
    if (instanceId.empty() || areaId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID and Area ID are required"));
      return;
    }
    auto json = req->getJsonObject();
    if (!json) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }
    ObjectLeftAreaWrite write = ObjectLeftAreaWrite::fromJson(*json);
    std::string createdId =
        area_manager_->createObjectLeftAreaWithId(instanceId, areaId, write);
    if (createdId.empty()) {
      callback(createErrorResponse(409, "Conflict",
                                   "Area already exists or creation failed"));
      return;
    }
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k201Created);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "PUT, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
  } catch (const std::exception &e) {
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void AreaHandler::createObjectRemovedAreaWithId(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  try {
    if (!area_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Area manager not initialized"));
      return;
    }
    std::string instanceId = extractInstanceId(req);
    std::string areaId = extractAreaId(req);
    if (instanceId.empty() || areaId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID and Area ID are required"));
      return;
    }
    auto json = req->getJsonObject();
    if (!json) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }
    ObjectRemovedAreaWrite write = ObjectRemovedAreaWrite::fromJson(*json);
    std::string createdId =
        area_manager_->createObjectRemovedAreaWithId(instanceId, areaId, write);
    if (createdId.empty()) {
      callback(createErrorResponse(409, "Conflict",
                                   "Area already exists or creation failed"));
      return;
    }
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k201Created);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "PUT, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
  } catch (const std::exception &e) {
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void AreaHandler::createFallenPersonAreaWithId(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  try {
    if (!area_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Area manager not initialized"));
      return;
    }
    std::string instanceId = extractInstanceId(req);
    std::string areaId = extractAreaId(req);
    if (instanceId.empty() || areaId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID and Area ID are required"));
      return;
    }
    auto json = req->getJsonObject();
    if (!json) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }
    FallenPersonAreaWrite write = FallenPersonAreaWrite::fromJson(*json);
    std::string createdId =
        area_manager_->createFallenPersonAreaWithId(instanceId, areaId, write);
    if (createdId.empty()) {
      callback(createErrorResponse(409, "Conflict",
                                   "Area already exists or creation failed"));
      return;
    }
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k201Created);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "PUT, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
  } catch (const std::exception &e) {
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

// ========================================================================
// Experimental Areas
// ========================================================================

void AreaHandler::createVehicleGuardArea(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  try {
    if (!area_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Area manager not initialized"));
      return;
    }
    std::string instanceId = extractInstanceId(req);
    if (instanceId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID is required"));
      return;
    }
    auto json = req->getJsonObject();
    if (!json) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }
    VehicleGuardAreaWrite write = VehicleGuardAreaWrite::fromJson(*json);
    std::string areaId = area_manager_->createVehicleGuardArea(instanceId, write);
    if (areaId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Failed to create area"));
      return;
    }
    Json::Value response;
    response["areaId"] = areaId;
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k201Created);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
  } catch (const std::exception &e) {
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void AreaHandler::createFaceCoveredArea(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  try {
    if (!area_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Area manager not initialized"));
      return;
    }
    std::string instanceId = extractInstanceId(req);
    if (instanceId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID is required"));
      return;
    }
    auto json = req->getJsonObject();
    if (!json) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }
    FaceCoveredAreaWrite write = FaceCoveredAreaWrite::fromJson(*json);
    std::string areaId = area_manager_->createFaceCoveredArea(instanceId, write);
    if (areaId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Failed to create area"));
      return;
    }
    Json::Value response;
    response["areaId"] = areaId;
    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k201Created);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
  } catch (const std::exception &e) {
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void AreaHandler::createVehicleGuardAreaWithId(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  try {
    if (!area_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Area manager not initialized"));
      return;
    }
    std::string instanceId = extractInstanceId(req);
    std::string areaId = extractAreaId(req);
    if (instanceId.empty() || areaId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID and Area ID are required"));
      return;
    }
    auto json = req->getJsonObject();
    if (!json) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }
    VehicleGuardAreaWrite write = VehicleGuardAreaWrite::fromJson(*json);
    std::string createdId =
        area_manager_->createVehicleGuardAreaWithId(instanceId, areaId, write);
    if (createdId.empty()) {
      callback(createErrorResponse(409, "Conflict",
                                   "Area already exists or creation failed"));
      return;
    }
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k201Created);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "PUT, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
  } catch (const std::exception &e) {
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void AreaHandler::createFaceCoveredAreaWithId(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  try {
    if (!area_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Area manager not initialized"));
      return;
    }
    std::string instanceId = extractInstanceId(req);
    std::string areaId = extractAreaId(req);
    if (instanceId.empty() || areaId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID and Area ID are required"));
      return;
    }
    auto json = req->getJsonObject();
    if (!json) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }
    FaceCoveredAreaWrite write = FaceCoveredAreaWrite::fromJson(*json);
    std::string createdId =
        area_manager_->createFaceCoveredAreaWithId(instanceId, areaId, write);
    if (createdId.empty()) {
      callback(createErrorResponse(409, "Conflict",
                                   "Area already exists or creation failed"));
      return;
    }
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k201Created);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "PUT, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
  } catch (const std::exception &e) {
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

// ========================================================================
// Common handlers
// ========================================================================

void AreaHandler::getAllAreas(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  try {
    if (!area_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Area manager not initialized"));
      return;
    }

    std::string instanceId = extractInstanceId(req);
    if (instanceId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID is required"));
      return;
    }

    auto areasMap = area_manager_->getAllAreas(instanceId);

    // Convert to JSON response
    Json::Value response;
    for (const auto &[type, areas] : areasMap) {
      Json::Value areasArray(Json::arrayValue);
      for (const auto &area : areas) {
        areasArray.append(area);
      }
      response[type] = areasArray;
    }

    callback(createSuccessResponse(response));

  } catch (const std::exception &e) {
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void AreaHandler::deleteAllAreas(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  try {
    if (!area_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Area manager not initialized"));
      return;
    }

    std::string instanceId = extractInstanceId(req);
    if (instanceId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID is required"));
      return;
    }

    bool success = area_manager_->deleteAllAreas(instanceId);
    if (!success) {
      callback(createErrorResponse(404, "Not Found",
                                   "Instance not found"));
      return;
    }

    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k204NoContent);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "DELETE, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");

    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));

  } catch (const std::exception &e) {
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void AreaHandler::deleteArea(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  try {
    if (!area_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Area manager not initialized"));
      return;
    }

    std::string instanceId = extractInstanceId(req);
    std::string areaId = extractAreaId(req);

    if (instanceId.empty() || areaId.empty()) {
      callback(createErrorResponse(400, "Invalid request",
                                   "Instance ID and Area ID are required"));
      return;
    }

    bool success = area_manager_->deleteArea(instanceId, areaId);
    if (!success) {
      callback(createErrorResponse(404, "Not Found",
                                   "Area not found"));
      return;
    }

    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k204NoContent);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "DELETE, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");

    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));

  } catch (const std::exception &e) {
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void AreaHandler::handleOptions(
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

// ========================================================================
// Helper methods
// ========================================================================

std::string AreaHandler::extractInstanceId(const HttpRequestPtr &req) const {
  std::string instanceId = req->getParameter("instanceId");
  if (instanceId.empty()) {
    std::string path = req->getPath();
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

std::string AreaHandler::extractAreaId(const HttpRequestPtr &req) const {
  std::string areaId = req->getParameter("areaId");
  if (areaId.empty()) {
    std::string path = req->getPath();
    // Try to find area ID after area type
    // Pattern: /v1/securt/instance/{instanceId}/area/{areaType}/{areaId}
    size_t areaPos = path.find("/area/");
    if (areaPos != std::string::npos) {
      size_t start = areaPos + 6; // length of "/area/"
      size_t nextSlash = path.find("/", start);
      if (nextSlash != std::string::npos) {
        size_t areaIdStart = nextSlash + 1;
        size_t areaIdEnd = path.find("/", areaIdStart);
        if (areaIdEnd == std::string::npos) {
          areaIdEnd = path.length();
        }
        areaId = path.substr(areaIdStart, areaIdEnd - areaIdStart);
      }
    }
  }
  return areaId;
}

HttpResponsePtr AreaHandler::createErrorResponse(int statusCode,
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

HttpResponsePtr AreaHandler::createSuccessResponse(const Json::Value &data,
                                                    int statusCode) const {
  auto resp = HttpResponse::newHttpJsonResponse(data);
  resp->setStatusCode(static_cast<drogon::HttpStatusCode>(statusCode));

  resp->addHeader("Access-Control-Allow-Origin", "*");
  resp->addHeader("Access-Control-Allow-Methods",
                   "GET, POST, PUT, DELETE, OPTIONS");
  resp->addHeader("Access-Control-Allow-Headers", "Content-Type");

  return resp;
}

