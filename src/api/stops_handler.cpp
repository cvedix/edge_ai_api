#include "api/stops_handler.h"
#include "core/logger.h"
#include "core/logging_flags.h"
#include "core/metrics_interceptor.h"
#include "core/uuid_generator.h"
#include "instances/instance_manager.h"
#include <algorithm>
#include <chrono>
#include <drogon/HttpResponse.h>
#include <json/reader.h>
#include <json/writer.h>
#include <sstream>
#include <thread>
#include <cvedix/nodes/ba/cvedix_ba_stop_node.h>
#include <cvedix/objects/shapes/cvedix_point.h>

IInstanceManager *StopsHandler::instance_manager_ = nullptr;

void StopsHandler::setInstanceManager(IInstanceManager *manager) {
  instance_manager_ = manager;
}

std::string StopsHandler::extractInstanceId(const HttpRequestPtr &req) const {
  std::string instanceId = req->getParameter("instanceId");

  if (instanceId.empty()) {
    std::string path = req->getPath();
    size_t instancesPos = path.find("/instances/");
    if (instancesPos != std::string::npos) {
      size_t start = instancesPos + 11; // length of "/instances/"
      size_t end = path.find("/", start);
      if (end == std::string::npos) {
        end = path.length();
      }
      instanceId = path.substr(start, end - start);
    } else {
      size_t instancePos = path.find("/instance/");
      if (instancePos != std::string::npos) {
        size_t start = instancePos + 10; // length of "/instance/"
        size_t end = path.find("/", start);
        if (end == std::string::npos) {
          end = path.length();
        }
        instanceId = path.substr(start, end - start);
      }
    }
  }

  return instanceId;
}

std::string StopsHandler::extractStopId(const HttpRequestPtr &req) const {
  std::string stopId = req->getParameter("stopId");

  if (stopId.empty()) {
    std::string path = req->getPath();
    size_t stopsPos = path.find("/stops/");
    if (stopsPos != std::string::npos) {
      size_t start = stopsPos + 7; // length of "/stops/"
      size_t end = path.find("/", start);
      if (end == std::string::npos) {
        end = path.length();
      }
      stopId = path.substr(start, end - start);
    }
  }

  return stopId;
}

HttpResponsePtr StopsHandler::createErrorResponse(int statusCode,
                                                   const std::string &error,
                                                   const std::string &message) const {
  Json::Value errorJson;
  errorJson["error"] = error;
  if (!message.empty()) {
    errorJson["message"] = message;
  }

  auto resp = HttpResponse::newHttpJsonResponse(errorJson);
  resp->setStatusCode(static_cast<HttpStatusCode>(statusCode));
  resp->addHeader("Access-Control-Allow-Origin", "*");
  resp->addHeader("Access-Control-Allow-Methods",
                  "GET, POST, PUT, DELETE, OPTIONS");
  resp->addHeader("Access-Control-Allow-Headers",
                  "Content-Type, Authorization");
  return resp;
}

HttpResponsePtr StopsHandler::createSuccessResponse(const Json::Value &data,
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

Json::Value StopsHandler::loadStopsFromConfig(const std::string &instanceId) const {
  Json::Value stopsArray(Json::arrayValue);

  if (!instance_manager_) {
    return stopsArray;
  }

  auto optInfo = instance_manager_->getInstance(instanceId);
  if (!optInfo.has_value()) {
    return stopsArray;
  }

  const auto &info = optInfo.value();
  auto it = info.additionalParams.find("StopZones");
  if (it != info.additionalParams.end() && !it->second.empty()) {
    Json::Reader reader;
    Json::Value parsedStops;
    if (reader.parse(it->second, parsedStops) && parsedStops.isArray()) {
      for (Json::ArrayIndex i = 0; i < parsedStops.size(); ++i) {
        Json::Value &stop = parsedStops[i];
        if (!stop.isObject()) {
          continue;
        }

        if (!stop.isMember("id") || !stop["id"].isString() || stop["id"].asString().empty()) {
          stop["id"] = UUIDGenerator::generateUUID();
          if (isApiLoggingEnabled()) {
            PLOG_DEBUG << "[API] loadStopsFromConfig: Generated UUID for stop at index " << i;
          }
        }
      }
      return parsedStops;
    }
  }

  return stopsArray;
}

bool StopsHandler::saveStopsToConfig(const std::string &instanceId,
                                     const Json::Value &stops) const {
  if (!instance_manager_) {
    return false;
  }

  Json::Value normalizedStops = stops;
  if (normalizedStops.isArray()) {
    for (Json::ArrayIndex i = 0; i < normalizedStops.size(); ++i) {
      Json::Value &stop = normalizedStops[i];
      if (!stop.isObject()) {
        continue;
      }

      if (!stop.isMember("id") || !stop["id"].isString() || stop["id"].asString().empty()) {
        stop["id"] = UUIDGenerator::generateUUID();
        if (isApiLoggingEnabled()) {
          PLOG_DEBUG << "[API] saveStopsToConfig: Generated UUID for stop at index " << i << " before saving";
        }
      }
    }
  }

  Json::StreamWriterBuilder builder;
  builder["indentation"] = "";
  std::string stopsJsonStr = Json::writeString(builder, normalizedStops);

  Json::Value configUpdate(Json::objectValue);
  Json::Value additionalParams(Json::objectValue);
  additionalParams["StopZones"] = stopsJsonStr;
  configUpdate["AdditionalParams"] = additionalParams;

  bool result = instance_manager_->updateInstanceFromConfig(instanceId, configUpdate);
  if (!result && isApiLoggingEnabled()) {
    PLOG_WARNING << "[API] saveStopsToConfig: updateInstanceFromConfig failed for instance " << instanceId;
  }

  return result;
}

bool StopsHandler::validateROI(const Json::Value &roi, std::string &error) const {
  if (!roi.isArray()) {
    error = "ROI must be an array of coordinates";
    return false;
  }

  if (roi.size() < 3) {
    error = "ROI must contain at least 3 points";
    return false;
  }

  for (const auto &coord : roi) {
    if (!coord.isObject()) {
      error = "Each coordinate must be an object";
      return false;
    }

    if (!coord.isMember("x") || !coord.isMember("y")) {
      error = "Each coordinate must have 'x' and 'y' fields";
      return false;
    }

    if (!coord["x"].isNumeric() || !coord["y"].isNumeric()) {
      error = "Coordinate 'x' and 'y' must be numbers";
      return false;
    }
  }

  return true;
}

bool StopsHandler::validateClasses(const Json::Value &classes, std::string &error) const {
  if (!classes.isArray()) {
    error = "Classes must be an array";
    return false;
  }

  static const std::vector<std::string> allowedClasses = {
      "Person", "Animal", "Vehicle", "Face", "Unknown"};

  for (const auto &cls : classes) {
    if (!cls.isString()) {
      error = "Each class must be a string";
      return false;
    }

    std::string classStr = cls.asString();
    bool found = false;
    for (const auto &allowed : allowedClasses) {
      if (classStr == allowed) {
        found = true;
        break;
      }
    }

    if (!found) {
      error = "Invalid class: " + classStr + ". Allowed values: Person, Animal, Vehicle, Face, Unknown";
      return false;
    }
  }

  return true;
}

bool StopsHandler::validateStopParameters(const Json::Value &stop, std::string &error) const {
  // minStopSeconds (camelCase) or min_stop_seconds (snake_case)
  if (stop.isMember("minStopSeconds") && !stop["minStopSeconds"].isNumeric()) {
    error = "minStopSeconds must be a number";
    return false;
  }
  if (stop.isMember("min_stop_seconds") && !stop["min_stop_seconds"].isNumeric()) {
    error = "min_stop_seconds must be a number";
    return false;
  }

  // checkIntervalFrames / check_interval_frames
  if (stop.isMember("checkIntervalFrames")) {
    if (!stop["checkIntervalFrames"].isInt() || stop["checkIntervalFrames"].asInt() < 1) {
      error = "checkIntervalFrames must be an integer >= 1";
      return false;
    }
  }
  if (stop.isMember("check_interval_frames")) {
    if (!stop["check_interval_frames"].isInt() || stop["check_interval_frames"].asInt() < 1) {
      error = "check_interval_frames must be an integer >= 1";
      return false;
    }
  }

  // checkMinHitFrames / check_min_hit_frames
  if (stop.isMember("checkMinHitFrames")) {
    if (!stop["checkMinHitFrames"].isInt() || stop["checkMinHitFrames"].asInt() < 1) {
      error = "checkMinHitFrames must be an integer >= 1";
      return false;
    }
  }
  if (stop.isMember("check_min_hit_frames")) {
    if (!stop["check_min_hit_frames"].isInt() || stop["check_min_hit_frames"].asInt() < 1) {
      error = "check_min_hit_frames must be an integer >= 1";
      return false;
    }
  }

  // checkMaxDistance / check_max_distance
  if (stop.isMember("checkMaxDistance")) {
    if (!stop["checkMaxDistance"].isNumeric() || stop["checkMaxDistance"].asDouble() < 0) {
      error = "checkMaxDistance must be a non-negative number";
      return false;
    }
  }
  if (stop.isMember("check_max_distance")) {
    if (!stop["check_max_distance"].isNumeric() || stop["check_max_distance"].asDouble() < 0) {
      error = "check_max_distance must be a non-negative number";
      return false;
    }
  }

  return true;
}

bool StopsHandler::restartInstanceForStopUpdate(const std::string &instanceId) const {
  if (!instance_manager_) {
    return false;
  }

  auto optInfo = instance_manager_->getInstance(instanceId);
  if (!optInfo.has_value() || !optInfo.value().running) {
    return true;
  }

  std::thread restartThread([this, instanceId]() {
    try {
      if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] ========================================";
        PLOG_INFO << "[API] Restarting instance " << instanceId << " to apply stop zone changes";
        PLOG_INFO << "[API] This will rebuild pipeline with new StopZones from additionalParams[\"StopZones\"]";
        PLOG_INFO << "[API] ========================================";
      }

      if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] Step 1/3: Stopping instance " << instanceId;
      }
      instance_manager_->stopInstance(instanceId);

      if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] Step 2/3: Waiting for pipeline cleanup (500ms)";
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(500));

      if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] Step 3/3: Starting instance " << instanceId << " (will rebuild pipeline with new stops)";
      }
      bool startSuccess = instance_manager_->startInstance(instanceId, true);

      if (startSuccess) {
        if (isApiLoggingEnabled()) {
          PLOG_INFO << "[API] ========================================";
          PLOG_INFO << "[API] ✓ Instance " << instanceId << " restarted successfully for stop update";
          PLOG_INFO << "[API] Pipeline rebuilt with new stops - stop zones should now be active";
          PLOG_INFO << "[API] ========================================";
        }
      } else {
        if (isApiLoggingEnabled()) {
          PLOG_ERROR << "[API] ✗ Failed to start instance " << instanceId << " after restart";
        }
      }
    } catch (const std::exception &e) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] ✗ Exception restarting instance " << instanceId << " for stop update: " << e.what();
      }
    } catch (...) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] ✗ Unknown error restarting instance " << instanceId << " for stop update";
      }
    }
  });
  restartThread.detach();

  return true;
}

std::shared_ptr<cvedix_nodes::cvedix_ba_stop_node>
StopsHandler::findBAStopNode(const std::string &instanceId) const {
  // Note: In subprocess mode, nodes are not directly accessible. This will
  // return nullptr and let updateStopsRuntime() fallback to restart.
  if (!instance_manager_) {
    return nullptr;
  }

  if (instance_manager_->isSubprocessMode()) {
    if (isApiLoggingEnabled()) {
      PLOG_DEBUG << "[API] findBAStopNode: Subprocess mode - nodes not directly accessible";
    }
    return nullptr;
  }

  // In-process direct access not implemented - use restart fallback
  if (isApiLoggingEnabled()) {
    PLOG_DEBUG << "[API] findBAStopNode: Direct node access not available, will use restart fallback";
  }
  return nullptr;
}

std::map<int, std::vector<cvedix_objects::cvedix_point>>
StopsHandler::parseStopsFromJson(const Json::Value &stopsArray) const {
  std::map<int, std::vector<cvedix_objects::cvedix_point>> stops;

  if (!stopsArray.isArray()) {
    if (isApiLoggingEnabled()) {
      PLOG_WARNING << "[API] parseStopsFromJson: Input is not a JSON array";
    }
    return stops;
  }

  for (Json::ArrayIndex i = 0; i < stopsArray.size(); ++i) {
    const Json::Value &stopObj = stopsArray[i];
    if (!stopObj.isObject()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] parseStopsFromJson: Stop at index " << i << " is not an object, skipping";
      }
      continue;
    }

    if (!stopObj.isMember("roi") || !stopObj["roi"].isArray()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] parseStopsFromJson: Stop at index " << i << " missing or invalid 'roi', skipping";
      }
      continue;
    }

    std::vector<cvedix_objects::cvedix_point> roiPoints;
    bool ok = true;
    for (const auto &coord : stopObj["roi"]) {
      if (!coord.isObject() || !coord.isMember("x") || !coord.isMember("y") || !coord["x"].isNumeric() || !coord["y"].isNumeric()) {
        ok = false;
        break;
      }
      cvedix_objects::cvedix_point p;
      p.x = static_cast<int>(coord["x"].asDouble());
      p.y = static_cast<int>(coord["y"].asDouble());
      roiPoints.push_back(p);
    }

    if (!ok || roiPoints.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] parseStopsFromJson: Invalid ROI at index " << i << " - skipping";
      }
      continue;
    }

    // Use index as key - SDK typically maps by channel; we don't have channel info here
    stops[static_cast<int>(i)] = std::move(roiPoints);
  }

  return stops;
}

bool StopsHandler::updateStopsRuntime(const std::string &instanceId,
                                      const Json::Value &stopsArray) const {
  if (!instance_manager_) {
    if (isApiLoggingEnabled()) {
      PLOG_WARNING << "[API] updateStopsRuntime: Instance registry not initialized";
    }
    return false;
  }

  // Check if instance exists
  auto optInfo = instance_manager_->getInstance(instanceId);
  if (!optInfo.has_value()) {
    if (isApiLoggingEnabled()) {
      PLOG_DEBUG << "[API] updateStopsRuntime: Instance " << instanceId << " not found";
    }
    return false;
  }

  if (!optInfo.value().running) {
    if (isApiLoggingEnabled()) {
      PLOG_DEBUG << "[API] updateStopsRuntime: Instance " << instanceId << " is not running, no need to update runtime";
    }
    return true; // Apply on next start
  }

  // Find ba_stop_node in pipeline
  auto baStopNode = findBAStopNode(instanceId);
  if (!baStopNode) {
    if (isApiLoggingEnabled()) {
      PLOG_WARNING << "[API] updateStopsRuntime: ba_stop_node not found in pipeline for instance " << instanceId << ", fallback to restart";
    }
    return false; // Fallback to restart
  }

  // Parse stops from JSON
  auto stops = parseStopsFromJson(stopsArray);
  if (stops.empty() && stopsArray.isArray() && stopsArray.size() > 0) {
    if (isApiLoggingEnabled()) {
      PLOG_WARNING << "[API] updateStopsRuntime: Failed to parse stops from JSON, fallback to restart";
    }
    return false; // Fallback to restart
  }

  // Try to update stops via SDK API
  try {
    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] updateStopsRuntime: Found ba_stop_node, attempting to update " << stops.size() << " stop zone(s)";
      PLOG_INFO << "[API] updateStopsRuntime: Note - Direct runtime update requires SDK API access";
    }

    // SDK does not expose setters in header; fallback to restart
    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] updateStopsRuntime: Stops configuration saved, restarting instance to apply changes";
    }

    return false; // Trigger restart fallback
  } catch (const std::exception &e) {
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] updateStopsRuntime: Exception updating stops: " << e.what() << ", fallback to restart";
    }
    return false;
  } catch (...) {
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] updateStopsRuntime: Unknown exception updating stops, fallback to restart";
    }
    return false;
  }
}

void StopsHandler::getAllStops(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();
  std::string instanceId = extractInstanceId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] GET /v1/core/instance/" << instanceId << "/stops - Get all stops";
    PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
  }

  try {
    if (!instance_manager_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] GET /v1/core/instance/" << instanceId << "/stops - Error: Instance manager not initialized";
      }
      callback(createErrorResponse(500, "Internal server error", "Instance manager not initialized"));
      return;
    }

    if (instanceId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] GET /v1/core/instance/{instanceId}/stops - Error: Instance ID is empty";
      }
      callback(createErrorResponse(400, "Bad request", "Instance ID is required"));
      return;
    }

    auto optInfo = instance_manager_->getInstance(instanceId);
    if (!optInfo.has_value()) {
      auto end_time = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] GET /v1/core/instance/" << instanceId << "/stops - Instance not found - " << duration.count() << "ms";
      }
      callback(createErrorResponse(404, "Not found", "Instance not found: " + instanceId));
      return;
    }

    Json::Value stopsArray = loadStopsFromConfig(instanceId);

    Json::Value response;
    response["stopZones"] = stopsArray;

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] GET /v1/core/instance/" << instanceId << "/stops - Success: " << stopsArray.size() << " stop(s) - " << duration.count() << "ms";
    }

    callback(createSuccessResponse(response));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] GET /v1/core/instance/" << instanceId << "/stops - Exception: " << e.what() << " - " << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] GET /v1/core/instance/" << instanceId << "/stops - Unknown exception - " << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
  }
}

void StopsHandler::createStop(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();
  std::string instanceId = extractInstanceId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] POST /v1/core/instance/" << instanceId << "/stops - Create stop";
    PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
  }

  try {
    if (!instance_manager_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] POST /v1/core/instance/" << instanceId << "/stops - Error: Instance registry not initialized";
      }
      callback(createErrorResponse(500, "Internal server error", "Instance manager not initialized"));
      return;
    }

    if (instanceId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] POST /v1/core/instance/{instanceId}/stops - Error: Instance ID is empty";
      }
      callback(createErrorResponse(400, "Bad request", "Instance ID is required"));
      return;
    }

    auto optInfo = instance_manager_->getInstance(instanceId);
    if (!optInfo.has_value()) {
      auto end_time = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] POST /v1/core/instance/" << instanceId << "/stops - Instance not found - " << duration.count() << "ms";
      }
      callback(createErrorResponse(404, "Not found", "Instance not found: " + instanceId));
      return;
    }

    auto json = req->getJsonObject();
    if (!json) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] POST /v1/core/instance/" << instanceId << "/stops - Error: Invalid JSON body";
      }
      callback(createErrorResponse(400, "Bad request", "Request body must be valid JSON"));
      return;
    }

    Json::Value stopsToAdd(Json::arrayValue);
    bool isArrayRequest = json->isArray();

    if (isArrayRequest) {
      for (Json::ArrayIndex i = 0; i < json->size(); ++i) {
        const Json::Value &stop = (*json)[i];
        if (!stop.isObject()) {
          callback(createErrorResponse(400, "Bad request", "Each stop must be an object"));
          return;
        }

        if (!stop.isMember("roi") || !validateROI(stop["roi"], *new std::string())) {
          callback(createErrorResponse(400, "Bad request", "Each stop must contain a valid 'roi' array with at least 3 points"));
          return;
        }

        if (stop.isMember("classes")) {
          std::string err;
          if (!validateClasses(stop["classes"], err)) {
            callback(createErrorResponse(400, "Bad request", "Invalid 'classes' array"));
            return;
          }
        }

        // Validate optional numeric parameters
        {
          std::string err;
          if (!validateStopParameters(stop, err)) {
            callback(createErrorResponse(400, "Bad request", err));
            return;
          }
        }

        stopsToAdd.append(stop);
      }
    } else {
      const Json::Value &stop = *json;
      if (!stop.isObject()) {
        callback(createErrorResponse(400, "Bad request", "Stop must be an object"));
        return;
      }

      if (!stop.isMember("roi") || !validateROI(stop["roi"], *new std::string())) {
        callback(createErrorResponse(400, "Bad request", "Stop must contain a valid 'roi' array with at least 3 points"));
        return;
      }

      if (stop.isMember("classes")) {
        std::string err;
        if (!validateClasses(stop["classes"], err)) {
          callback(createErrorResponse(400, "Bad request", "Invalid 'classes' array"));
          return;
        }
      }

      {
        std::string err;
        if (!validateStopParameters(stop, err)) {
          callback(createErrorResponse(400, "Bad request", err));
          return;
        }
      }

      stopsToAdd.append(stop);
    }

    // Load existing stops and append
    Json::Value existingStops = loadStopsFromConfig(instanceId);
    for (Json::ArrayIndex i = 0; i < stopsToAdd.size(); ++i) {
      existingStops.append(stopsToAdd[i]);
    }

    bool saveResult = saveStopsToConfig(instanceId, existingStops);
    if (!saveResult) {
      callback(createErrorResponse(500, "Internal server error", "Failed to save stop zones to instance config"));
      return;
    }

    // Try updating runtime; fallback to restart
    if (!updateStopsRuntime(instanceId, existingStops)) {
      restartInstanceForStopUpdate(instanceId);
    }

    // If single object request, return the created object with generated id
    if (!isArrayRequest) {
      const Json::Value &created = existingStops[existingStops.size() - 1];
      callback(createSuccessResponse(created, 201));
      return;
    }

    // Array request - return metadata
    Json::Value result;
    result["message"] = "Stops created successfully";
    result["count"] = (int)stopsToAdd.size();
    result["stops"] = stopsToAdd;
    callback(createSuccessResponse(result, 201));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] POST /v1/core/instance/" << instanceId << "/stops - Exception: " << e.what() << " - " << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] POST /v1/core/instance/" << instanceId << "/stops - Unknown exception - " << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
  }
}

void StopsHandler::deleteAllStops(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();
  std::string instanceId = extractInstanceId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] DELETE /v1/core/instance/" << instanceId << "/stops - Delete all stops";
    PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
  }

  try {
    if (!instance_manager_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] DELETE /v1/core/instance/" << instanceId << "/stops - Error: Instance registry not initialized";
      }
      callback(createErrorResponse(500, "Internal server error", "Instance manager not initialized"));
      return;
    }

    if (instanceId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] DELETE /v1/core/instance/{instanceId}/stops - Error: Instance ID is empty";
      }
      callback(createErrorResponse(400, "Bad request", "Instance ID is required"));
      return;
    }

    auto optInfo = instance_manager_->getInstance(instanceId);
    if (!optInfo.has_value()) {
      auto end_time = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] DELETE /v1/core/instance/" << instanceId << "/stops - Instance not found - " << duration.count() << "ms";
      }
      callback(createErrorResponse(404, "Not found", "Instance not found: " + instanceId));
      return;
    }

    Json::Value emptyArray(Json::arrayValue);
    bool saveResult = saveStopsToConfig(instanceId, emptyArray);
    if (!saveResult) {
      callback(createErrorResponse(500, "Internal server error", "Failed to clear stop zones in instance config"));
      return;
    }

    if (!updateStopsRuntime(instanceId, emptyArray)) {
      restartInstanceForStopUpdate(instanceId);
    }

    Json::Value result;
    result["message"] = "All stops deleted successfully";
    callback(createSuccessResponse(result));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] DELETE /v1/core/instance/" << instanceId << "/stops - Exception: " << e.what() << " - " << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] DELETE /v1/core/instance/" << instanceId << "/stops - Unknown exception - " << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
  }
}

void StopsHandler::getStop(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();
  std::string instanceId = extractInstanceId(req);
  std::string stopId = extractStopId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] GET /v1/core/instance/" << instanceId << "/stops/" << stopId << " - Get stop";
    PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
  }

  try {
    if (!instance_manager_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] GET /v1/core/instance/" << instanceId << "/stops/" << stopId << " - Error: Instance manager not initialized";
      }
      callback(createErrorResponse(500, "Internal server error", "Instance manager not initialized"));
      return;
    }

    if (instanceId.empty() || stopId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] GET /v1/core/instance/{instanceId}/stops/{stopId} - Error: Missing parameters";
      }
      callback(createErrorResponse(400, "Bad request", "Instance ID and stop ID are required"));
      return;
    }

    auto optInfo = instance_manager_->getInstance(instanceId);
    if (!optInfo.has_value()) {
      auto end_time = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] GET /v1/core/instance/" << instanceId << "/stops/" << stopId << " - Instance not found - " << duration.count() << "ms";
      }
      callback(createErrorResponse(404, "Not found", "Instance not found: " + instanceId));
      return;
    }

    Json::Value stopsArray = loadStopsFromConfig(instanceId);
    for (const auto &stop : stopsArray) {
      if (stop.isMember("id") && stop["id"].isString() && stop["id"].asString() == stopId) {
        callback(createSuccessResponse(stop));
        return;
      }
    }

    callback(createErrorResponse(404, "Not found", "Stop not found: " + stopId));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] GET /v1/core/instance/" << instanceId << "/stops/" << stopId << " - Exception: " << e.what() << " - " << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] GET /v1/core/instance/" << instanceId << "/stops/" << stopId << " - Unknown exception - " << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
  }
}

void StopsHandler::updateStop(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();
  std::string instanceId = extractInstanceId(req);
  std::string stopId = extractStopId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] PUT /v1/core/instance/" << instanceId << "/stops/" << stopId << " - Update stop";
    PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
  }

  try {
    if (!instance_manager_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] PUT /v1/core/instance/" << instanceId << "/stops/" << stopId << " - Error: Instance registry not initialized";
      }
      callback(createErrorResponse(500, "Internal server error", "Instance manager not initialized"));
      return;
    }

    if (instanceId.empty() || stopId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] PUT /v1/core/instance/{instanceId}/stops/{stopId} - Error: Missing parameters";
      }
      callback(createErrorResponse(400, "Bad request", "Instance ID and stop ID are required"));
      return;
    }

    auto optInfo = instance_manager_->getInstance(instanceId);
    if (!optInfo.has_value()) {
      auto end_time = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] PUT /v1/core/instance/" << instanceId << "/stops/" << stopId << " - Instance not found - " << duration.count() << "ms";
      }
      callback(createErrorResponse(404, "Not found", "Instance not found: " + instanceId));
      return;
    }

    auto json = req->getJsonObject();
    if (!json) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] PUT /v1/core/instance/" << instanceId << "/stops/" << stopId << " - Error: Invalid JSON body";
      }
      callback(createErrorResponse(400, "Bad request", "Request body must be valid JSON"));
      return;
    }

    if (!json->isObject()) {
      callback(createErrorResponse(400, "Bad request", "Request body must be an object with fields to update"));
      return;
    }

    Json::Value stopsArray = loadStopsFromConfig(instanceId);
    bool found = false;
    for (Json::ArrayIndex i = 0; i < stopsArray.size(); ++i) {
      Json::Value &stop = stopsArray[i];
      if (stop.isMember("id") && stop["id"].isString() && stop["id"].asString() == stopId) {
        // Merge fields
        for (const auto &memberName : json->getMemberNames()) {
          stop[memberName] = (*json)[memberName];
        }

        // Validate roi and classes if present
        if (stop.isMember("roi")) {
          std::string error;
          if (!validateROI(stop["roi"], error)) {
            callback(createErrorResponse(400, "Bad request", error));
            return;
          }
        }

        if (stop.isMember("classes")) {
          std::string error;
          if (!validateClasses(stop["classes"], error)) {
            callback(createErrorResponse(400, "Bad request", error));
            return;
          }
        }

        // Validate numeric params if present on merged stop
        {
          std::string err;
          if (!validateStopParameters(stop, err)) {
            callback(createErrorResponse(400, "Bad request", err));
            return;
          }
        }

        found = true;
        break;
      }
    }

    if (!found) {
      callback(createErrorResponse(404, "Not found", "Stop not found: " + stopId));
      return;
    }

    bool saveResult = saveStopsToConfig(instanceId, stopsArray);
    if (!saveResult) {
      callback(createErrorResponse(500, "Internal server error", "Failed to save updated stops to instance config"));
      return;
    }

    if (!updateStopsRuntime(instanceId, stopsArray)) {
      restartInstanceForStopUpdate(instanceId);
    }

    callback(createSuccessResponse(Json::Value(Json::objectValue)));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] PUT /v1/core/instance/" << instanceId << "/stops/" << stopId << " - Exception: " << e.what() << " - " << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] PUT /v1/core/instance/" << instanceId << "/stops/" << stopId << " - Unknown exception - " << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
  }
}

void StopsHandler::deleteStop(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();
  std::string instanceId = extractInstanceId(req);
  std::string stopId = extractStopId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] DELETE /v1/core/instance/" << instanceId << "/stops/" << stopId << " - Delete stop";
    PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
  }

  try {
    if (!instance_manager_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] DELETE /v1/core/instance/" << instanceId << "/stops/" << stopId << " - Error: Instance registry not initialized";
      }
      callback(createErrorResponse(500, "Internal server error", "Instance manager not initialized"));
      return;
    }

    if (instanceId.empty() || stopId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] DELETE /v1/core/instance/{instanceId}/stops/{stopId} - Error: Missing parameters";
      }
      callback(createErrorResponse(400, "Bad request", "Instance ID and stop ID are required"));
      return;
    }

    auto optInfo = instance_manager_->getInstance(instanceId);
    if (!optInfo.has_value()) {
      auto end_time = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] DELETE /v1/core/instance/" << instanceId << "/stops/" << stopId << " - Instance not found - " << duration.count() << "ms";
      }
      callback(createErrorResponse(404, "Not found", "Instance not found: " + instanceId));
      return;
    }

    Json::Value stopsArray = loadStopsFromConfig(instanceId);
    Json::Value newArray(Json::arrayValue);
    bool found = false;
    for (const auto &stop : stopsArray) {
      if (stop.isMember("id") && stop["id"].isString() && stop["id"].asString() == stopId) {
        found = true;
        continue;
      }
      newArray.append(stop);
    }

    if (!found) {
      callback(createErrorResponse(404, "Not found", "Stop not found: " + stopId));
      return;
    }

    bool saveResult = saveStopsToConfig(instanceId, newArray);
    if (!saveResult) {
      callback(createErrorResponse(500, "Internal server error", "Failed to delete stop in instance config"));
      return;
    }

    if (!updateStopsRuntime(instanceId, newArray)) {
      restartInstanceForStopUpdate(instanceId);
    }

    callback(createSuccessResponse(Json::Value(Json::objectValue)));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] DELETE /v1/core/instance/" << instanceId << "/stops/" << stopId << " - Exception: " << e.what() << " - " << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] DELETE /v1/core/instance/" << instanceId << "/stops/" << stopId << " - Unknown exception - " << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
  }
}

void StopsHandler::batchUpdateStops(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();
  std::string instanceId = extractInstanceId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] POST /v1/core/instance/" << instanceId << "/stops/batch - Batch update stops";
    PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
  }

  try {
    if (!instance_manager_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] POST /v1/core/instance/" << instanceId << "/stops/batch - Error: Instance registry not initialized";
      }
      callback(createErrorResponse(500, "Internal server error", "Instance manager not initialized"));
      return;
    }

    if (instanceId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] POST /v1/core/instance/{instanceId}/stops/batch - Error: Instance ID is empty";
      }
      callback(createErrorResponse(400, "Bad request", "Instance ID is required"));
      return;
    }

    auto optInfo = instance_manager_->getInstance(instanceId);
    if (!optInfo.has_value()) {
      auto end_time = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] POST /v1/core/instance/" << instanceId << "/stops/batch - Instance not found - " << duration.count() << "ms";
      }
      callback(createErrorResponse(404, "Not found", "Instance not found: " + instanceId));
      return;
    }

    auto json = req->getJsonObject();
    if (!json || !json->isArray()) {
      callback(createErrorResponse(400, "Bad request", "Request body must be an array of stop objects"));
      return;
    }

    Json::Value newStops(Json::arrayValue);
    for (Json::ArrayIndex i = 0; i < json->size(); ++i) {
      const Json::Value &stop = (*json)[i];
      if (!stop.isObject()) {
        callback(createErrorResponse(400, "Bad request", "Each stop must be an object"));
        return;
      }

      if (!stop.isMember("roi") || !validateROI(stop["roi"], *new std::string())) {
        callback(createErrorResponse(400, "Bad request", "Each stop must contain a valid 'roi' array with at least 3 points"));
        return;
      }

      {
        std::string err;
        if (!validateStopParameters(stop, err)) {
          callback(createErrorResponse(400, "Bad request", err));
          return;
        }
      }

      if (stop.isMember("classes") && !validateClasses(stop["classes"], *new std::string())) {
        callback(createErrorResponse(400, "Bad request", "Invalid 'classes' array"));
        return;
      }

      newStops.append(stop);
    }

    bool saveResult = saveStopsToConfig(instanceId, newStops);
    if (!saveResult) {
      callback(createErrorResponse(500, "Internal server error", "Failed to save stops to instance config"));
      return;
    }

    if (!updateStopsRuntime(instanceId, newStops)) {
      restartInstanceForStopUpdate(instanceId);
    }

    Json::Value result;
    result["message"] = "Stops batch updated successfully";
    result["count"] = (int)newStops.size();
    callback(createSuccessResponse(result));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] POST /v1/core/instance/" << instanceId << "/stops/batch - Exception: " << e.what() << " - " << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] POST /v1/core/instance/" << instanceId << "/stops/batch - Unknown exception - " << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
  }
}

void StopsHandler::handleOptions(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto resp = HttpResponse::newHttpResponse();
  resp->setStatusCode(k200OK);
  resp->addHeader("Access-Control-Allow-Origin", "*");
  resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
  resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
  resp->addHeader("Access-Control-Max-Age", "3600");

  MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
}
