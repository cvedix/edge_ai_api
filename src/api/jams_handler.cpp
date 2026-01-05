#include "api/jams_handler.h"
#include "core/logger.h"
#include "core/logging_flags.h"
#include "core/metrics_interceptor.h"
#include "core/uuid_generator.h"
#include "instances/instance_manager.h"
#include <cvedix/nodes/ba/cvedix_ba_jam_node.h>
#include "solutions/solution_registry.h"
#include <algorithm>
#include <drogon/HttpResponse.h>
#include <json/reader.h>
#include <json/writer.h>

IInstanceManager *JamsHandler::instance_manager_ = nullptr;

void JamsHandler::setInstanceManager(IInstanceManager *manager) { instance_manager_ = manager; }

std::string JamsHandler::extractInstanceId(const HttpRequestPtr &req) const {
  std::string instanceId = req->getParameter("instanceId");
  if (instanceId.empty()) {
    std::string path = req->getPath();
    size_t instancesPos = path.find("/instances/");
    if (instancesPos != std::string::npos) {
      size_t start = instancesPos + 11;
      size_t end = path.find("/", start);
      if (end == std::string::npos) end = path.length();
      instanceId = path.substr(start, end - start);
    } else {
      size_t instancePos = path.find("/instance/");
      if (instancePos != std::string::npos) {
        size_t start = instancePos + 10;
        size_t end = path.find("/", start);
        if (end == std::string::npos) end = path.length();
        instanceId = path.substr(start, end - start);
      }
    }
  }
  return instanceId;
}

std::string JamsHandler::extractJamId(const HttpRequestPtr &req) const {
  std::string jamId = req->getParameter("jamId");
  if (jamId.empty()) {
    std::string path = req->getPath();
    size_t jamsPos = path.find("/jams/");
    if (jamsPos != std::string::npos) {
      size_t start = jamsPos + 6;
      size_t end = path.find("/", start);
      if (end == std::string::npos) end = path.length();
      jamId = path.substr(start, end - start);
    }
  }
  return jamId;
}

HttpResponsePtr JamsHandler::createErrorResponse(int statusCode, const std::string &error, const std::string &message) const {
  Json::Value errorJson;
  errorJson["error"] = error;
  if (!message.empty()) errorJson["message"] = message;
  auto resp = HttpResponse::newHttpJsonResponse(errorJson);
  resp->setStatusCode(static_cast<HttpStatusCode>(statusCode));
  resp->addHeader("Access-Control-Allow-Origin", "*");
  resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
  resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
  return resp;
}

HttpResponsePtr JamsHandler::createSuccessResponse(const Json::Value &data, int statusCode) const {
  auto resp = HttpResponse::newHttpJsonResponse(data);
  resp->setStatusCode(static_cast<HttpStatusCode>(statusCode));
  resp->addHeader("Access-Control-Allow-Origin", "*");
  resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
  resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
  return resp;
}

Json::Value JamsHandler::loadJamsFromConfig(const std::string &instanceId) const {
  Json::Value jamsArray(Json::arrayValue);
  if (!instance_manager_) return jamsArray;
  auto optInfo = instance_manager_->getInstance(instanceId);
  if (!optInfo.has_value()) return jamsArray;
  const auto &info = optInfo.value();
  auto it = info.additionalParams.find("JamZones");
  if (it != info.additionalParams.end() && !it->second.empty()) {
    Json::Reader reader;
    Json::Value parsed;
    if (reader.parse(it->second, parsed) && parsed.isArray()) {
      for (Json::ArrayIndex i = 0; i < parsed.size(); ++i) {
        Json::Value &jam = parsed[i];
        if (!jam.isObject()) continue;
        if (!jam.isMember("id") || !jam["id"].isString() || jam["id"].asString().empty()) {
          jam["id"] = UUIDGenerator::generateUUID();
          if (isApiLoggingEnabled()) PLOG_DEBUG << "[API] loadJamsFromConfig: Generated UUID for jam at index " << i;
        }
      }
      return parsed;
    }
  }
  return jamsArray;
}

bool JamsHandler::saveJamsToConfig(const std::string &instanceId, const Json::Value &jams) const {
  if (!instance_manager_) return false;
  Json::Value normalized = jams;
  if (normalized.isArray()) {
    for (Json::ArrayIndex i = 0; i < normalized.size(); ++i) {
      Json::Value &jam = normalized[i];
      if (!jam.isObject()) continue;
      if (!jam.isMember("id") || !jam["id"].isString() || jam["id"].asString().empty()) {
        jam["id"] = UUIDGenerator::generateUUID();
        if (isApiLoggingEnabled()) PLOG_DEBUG << "[API] saveJamsToConfig: Generated UUID for jam at index " << i << " before saving";
      }
    }
  }
  Json::StreamWriterBuilder builder; builder["indentation"] = "";
  std::string jamsStr = Json::writeString(builder, normalized);
  Json::Value configUpdate(Json::objectValue);
  Json::Value additionalParams(Json::objectValue);
  additionalParams["JamZones"] = jamsStr;
  configUpdate["AdditionalParams"] = additionalParams;
  bool result = instance_manager_->updateInstanceFromConfig(instanceId, configUpdate);
  if (!result && isApiLoggingEnabled()) PLOG_WARNING << "[API] saveJamsToConfig: updateInstanceFromConfig failed for instance " << instanceId;
  return result;
}

bool JamsHandler::validateROI(const Json::Value &roi, std::string &error) const {
  if (!roi.isArray()) { error = "ROI must be an array"; return false; }
  if (roi.size() < 3) { error = "ROI must contain at least 3 points"; return false; }
  for (const auto &pt : roi) {
    if (!pt.isObject()) { error = "Each ROI point must be an object"; return false; }
    if (!pt.isMember("x") || !pt.isMember("y")) { error = "Each ROI point must have 'x' and 'y'"; return false; }
    if (!pt["x"].isNumeric() || !pt["y"].isNumeric()) { error = "ROI 'x' and 'y' must be numbers"; return false; }
  }
  return true;
}

bool JamsHandler::validateClasses(const Json::Value &classes, std::string &error) const {
  if (!classes.isArray()) { error = "Classes must be an array"; return false; }
  for (const auto &c : classes) {
    if (!c.isString()) { error = "Each class must be a string"; return false; }
  }
  return true;
}

bool JamsHandler::restartInstanceForJamUpdate(const std::string &instanceId) const {
  if (!instance_manager_) {
    return false;
  }

  // Check if instance is running
  auto optInfo = instance_manager_->getInstance(instanceId);
  if (!optInfo.has_value() || !optInfo.value().running) {
    // Instance not running, no need to restart
    return true;
  }

  // Restart instance in background thread to apply jam changes
  std::thread restartThread([this, instanceId]() {
    try {
      if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] ========================================";
        PLOG_INFO << "[API] Restarting instance " << instanceId
                  << " to apply jam changes";
        PLOG_INFO << "[API] This will rebuild pipeline with new zones from additionalParams[\"JamZones\"]";
        PLOG_INFO << "[API] ========================================";
      }

      // Stop instance (this will stop the pipeline)
      if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] Step 1/3: Stopping instance " << instanceId;
      }
      instance_manager_->stopInstance(instanceId);

      // Wait for cleanup to ensure pipeline is fully stopped
      if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] Step 2/3: Waiting for pipeline cleanup (500ms)";
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(500));

      // Start instance again (this will rebuild pipeline with new zones)
      if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] Step 3/3: Starting instance " << instanceId
                  << " (will rebuild pipeline with new JamZones)";
      }
      bool startSuccess = instance_manager_->startInstance(instanceId, true);

      if (startSuccess) {
        if (isApiLoggingEnabled()) {
          PLOG_INFO << "[API] ========================================";
          PLOG_INFO << "[API] ✓ Instance " << instanceId
                    << " restarted successfully for jam update";
          PLOG_INFO << "[API] Pipeline rebuilt with new JamZones - zones should now be active";
          PLOG_INFO << "[API] ========================================";
        }
      } else {
        if (isApiLoggingEnabled()) {
          PLOG_ERROR << "[API] ✗ Failed to start instance " << instanceId
                     << " after restart";
        }
      }
    } catch (const std::exception &e) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] ✗ Exception restarting instance " << instanceId
                   << " for jam update: " << e.what();
      }
    } catch (...) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] ✗ Unknown error restarting instance " << instanceId
                   << " for jam update";
      }
    }
  });

  restartThread.detach();
  return true;
}


std::shared_ptr<cvedix_nodes::cvedix_ba_jam_node>
JamsHandler::findBAJamNode(const std::string &instanceId) const {
  // Note: In subprocess mode, nodes are not directly accessible. This will
  // return nullptr and let updateJamsRuntime() fallback to restart.
  if (!instance_manager_) {
    return nullptr;
  }

  if (instance_manager_->isSubprocessMode()) {
    if (isApiLoggingEnabled()) {
      PLOG_DEBUG << "[API] findBAJamNode: Subprocess mode - nodes not directly accessible";
    }
    return nullptr;
  }
  // In in-process mode, try to access nodes via InstanceRegistry
  // This requires casting to InProcessInstanceManager to access registry
  // For now, return nullptr and let updateLinesRuntime() handle via restart
  // TODO: Add getInstanceNodes() to IInstanceManager interface if needed
  if (isApiLoggingEnabled()) {
    PLOG_DEBUG << "[API] findBAJamNode: Direct node access not "
                  "available, will use restart fallback";
  }
  return nullptr;
}

bool JamsHandler::validateJamParameters(const Json::Value &jam, std::string &error) const {
  // ROI must be present and valid
  if (!jam.isMember("roi")) { error = "Missing 'roi' in jam zone"; return false; }
  if (!validateROI(jam["roi"], error)) return false;

  // Optional classes
  if (jam.isMember("vehicle_classes")) {
    if (!validateClasses(jam["vehicle_classes"], error)) return false;
  }

  // Detection tuning parameters (optional)
  if (jam.isMember("check_interval_frames")) {
    if (!jam["check_interval_frames"].isInt() || jam["check_interval_frames"].asInt() < 1) {
      error = "check_interval_frames must be an integer >= 1"; return false;
    }
  }
  if (jam.isMember("check_min_hit_frames")) {
    if (!jam["check_min_hit_frames"].isInt() || jam["check_min_hit_frames"].asInt() < 1) {
      error = "check_min_hit_frames must be an integer >= 1"; return false;
    }
  }
  if (jam.isMember("check_max_distance")) {
    if (!jam["check_max_distance"].isInt() || jam["check_max_distance"].asInt() < 0) {
      error = "check_max_distance must be an integer >= 0"; return false;
    }
  }
  if (jam.isMember("check_min_stops")) {
    if (!jam["check_min_stops"].isInt() || jam["check_min_stops"].asInt() < 1) {
      error = "check_min_stops must be an integer >= 1"; return false;
    }
  }
  if (jam.isMember("check_notify_interval")) {
    if (!jam["check_notify_interval"].isInt() || jam["check_notify_interval"].asInt() < 0) {
      error = "check_notify_interval must be an integer >= 0"; return false;
    }
  }

  return true;
}

Json::Value JamsHandler::parseJamsFromJson(const Json::Value &jamsArray) const {
  Json::Value out(Json::arrayValue);
  if (!jamsArray.isArray()) return out;

  for (Json::ArrayIndex i = 0; i < jamsArray.size(); ++i) {
    if (!jamsArray[i].isObject()) continue;
    Json::Value jam = jamsArray[i];

    // Ensure id exists
    if (!jam.isMember("id") || !jam["id"].isString() || jam["id"].asString().empty()) {
      jam["id"] = UUIDGenerator::generateUUID();
      if (isApiLoggingEnabled()) PLOG_DEBUG << "[API] parseJamsFromJson: Generated UUID for jam at index " << i;
    }

    // Validate parameters
    std::string err;
    if (!validateJamParameters(jam, err)) {
      if (isApiLoggingEnabled()) PLOG_WARNING << "[API] parseJamsFromJson: Skipping invalid jam at index " << i << ": " << err;
      continue; // skip invalid entries
    }

    out.append(jam);
  }

  return out;
}

bool JamsHandler::updateJamsRuntime(const std::string &instanceId, const Json::Value &jamsArray) const {
  if (!instance_manager_) return false;
  auto optInfo = instance_manager_->getInstance(instanceId);
  if (!optInfo.has_value()) return false;
  if (!optInfo.value().running) {
    if (isApiLoggingEnabled()) PLOG_DEBUG << "[API] updateJamsRuntime: Instance " << instanceId << " is not running, no runtime update needed";
    return true;
  }

  // Try to find ba_jam node
  auto node = findBAJamNode(instanceId);
  if (!node) {
    if (isApiLoggingEnabled()) PLOG_WARNING << "[API] updateJamsRuntime: ba_jam node not found in pipeline for instance " << instanceId << ", falling back to restart";
    return false;
  }

  // If SDK provided a method to update jam zones at runtime we would call it here
  // For now, return false to trigger restart fallback
  if (isApiLoggingEnabled()) PLOG_INFO << "[API] updateJamsRuntime: ba_jam runtime update not implemented, falling back to restart";
  return false;
}



void JamsHandler::getAllJams(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();
  std::string instanceId = extractInstanceId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] GET /v1/core/instance/" << instanceId << "/jams - Get all jams";
    PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
  }

  if (!instance_manager_) {
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] GET /v1/core/instance/" << instanceId << "/jams - Error: Instance manager not initialized";
    }
    callback(createErrorResponse(500, "internal_error", "Instance manager not initialized"));
    return;
  }

  if (instanceId.empty()) {
    if (isApiLoggingEnabled()) {
      PLOG_WARNING << "[API] GET /v1/core/instance/{instanceId}/jams - Error: Instance ID is empty";
    }
    callback(createErrorResponse(400, "bad_request", "Instance ID is required"));
    return;
  }

  auto optInfo = instance_manager_->getInstance(instanceId);
  if (!optInfo.has_value()) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_WARNING << "[API] GET /v1/core/instance/" << instanceId << "/jams - Instance not found - " << duration.count() << "ms";
    }
    callback(createErrorResponse(404, "not_found", "Instance not found: " + instanceId));
    return;
  }

  Json::Value res = loadJamsFromConfig(instanceId);
  Json::Value wrapper(Json::objectValue);
  wrapper["jamZones"] = res;
  callback(createSuccessResponse(wrapper));
}

void JamsHandler::createJam(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
  std::string instanceId = extractInstanceId(req);
  Json::Reader reader;
  Json::Value body;
  std::string bodyStr = std::string(req->getBody());
  if (!reader.parse(bodyStr, body)) {
    callback(createErrorResponse(400, "invalid_request", "Expected JSON object or array"));
    return;
  }

  // Handle single object (existing behavior)
  if (body.isObject()) {
    // Validate jam parameters (roi is required)
    {
      std::string err;
      if (!validateJamParameters(body, err)) { callback(createErrorResponse(400, "invalid_request", err)); return; }
    }

    // Ensure ID exists for the created jam
    if (!body.isMember("id") || !body["id"].isString() || body["id"].asString().empty()) {
      body["id"] = UUIDGenerator::generateUUID();
    }

    Json::Value jamsArray = loadJamsFromConfig(instanceId);
    jamsArray.append(body);
    if (!saveJamsToConfig(instanceId, jamsArray)) { callback(createErrorResponse(500, "save_failed", "Failed to save jam configuration")); return; }

    // Try to update runtime; if not supported, restart will be triggered by controller
    if (!updateJamsRuntime(instanceId, jamsArray)) { restartInstanceForJamUpdate(instanceId); }

    callback(createSuccessResponse(body, 201));
    return;
  }

  // Handle array of jam objects -> create multiple
  if (body.isArray()) {
    Json::Value parsed = parseJamsFromJson(body);
    if (!parsed.isArray() || parsed.empty()) {
      callback(createErrorResponse(400, "invalid_request", "Expected array of valid jam objects"));
      return;
    }

    Json::Value jamsArray = loadJamsFromConfig(instanceId);
    for (const auto &j : parsed) jamsArray.append(j);

    if (!saveJamsToConfig(instanceId, jamsArray)) { callback(createErrorResponse(500, "save_failed", "Failed to save jam configuration")); return; }

    if (!updateJamsRuntime(instanceId, jamsArray)) { restartInstanceForJamUpdate(instanceId); }

    Json::Value resp(Json::objectValue);
    resp["message"] = "Zones created successfully";
    resp["count"] = static_cast<int>(parsed.size());
    resp["zones"] = parsed;
    callback(createSuccessResponse(resp, 201));
    return;
  }

  // Otherwise invalid request format
  callback(createErrorResponse(400, "invalid_request", "Expected JSON object or array"));
}

void JamsHandler::deleteAllJams(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
  std::string instanceId = extractInstanceId(req);
  Json::Value empty(Json::arrayValue);
  if (!saveJamsToConfig(instanceId, empty)) { callback(createErrorResponse(500, "delete_failed", "Failed to delete jams")); return; }
  if (!updateJamsRuntime(instanceId, empty)) { restartInstanceForJamUpdate(instanceId); }
  Json::Value wrapper(Json::objectValue);
  wrapper["jamZones"] = empty;
  callback(createSuccessResponse(wrapper, 200));
}

void JamsHandler::getJam(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();

  std::string instanceId = extractInstanceId(req);
  std::string jamId = extractJamId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] GET /v1/core/instance/" << instanceId << "/jams/"
              << jamId << " - Get jam";
    PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
  }

  try {
    if (instanceId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING
            << "[API] GET /v1/core/instance/{instanceId}/jams/{jamId} - "
               "Error: Instance ID is empty";
      }
      callback(
          createErrorResponse(400, "Bad request", "Instance ID is required"));
      return;
    }

    if (jamId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] GET /v1/core/instance/" << instanceId
                     << "/jams/{jamId} - Error: Jam ID is empty";
      }
      callback(createErrorResponse(400, "Bad request", "Jam ID is required"));
      return;
    }

    Json::Value jamsArray = loadJamsFromConfig(instanceId);

    for (const auto &jam : jamsArray) {
      if (jam.isObject() && jam.isMember("id") && jam["id"].isString()) {
        if (jam["id"].asString() == jamId) {
          auto end_time = std::chrono::steady_clock::now();
          auto duration =
              std::chrono::duration_cast<std::chrono::milliseconds>(
                  end_time - start_time);

          if (isApiLoggingEnabled()) {
            PLOG_INFO << "[API] GET /v1/core/instance/" << instanceId
                      << "/jams/" << jamId << " - Success - "
                      << duration.count() << "ms";
          }

          callback(createSuccessResponse(jam));
          return;
        }
      }
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_WARNING << "[API] GET /v1/core/instance/" << instanceId << "/jams/"
                   << jamId << " - Jam not found - " << duration.count()
                   << "ms";
    }
    callback(
        createErrorResponse(404, "Not found", "Jam not found: " + jamId));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] GET /v1/core/instance/" << instanceId << "/jams/"
                 << jamId << " - Exception: " << e.what() << " - "
                 << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] GET /v1/core/instance/" << instanceId << "/jams/"
                 << jamId << " - Unknown exception - " << duration.count()
                 << "ms";
    }
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void JamsHandler::updateJam(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
  std::string instanceId = extractInstanceId(req);
  std::string jamId = extractJamId(req);
  Json::Reader reader; Json::Value body;
  std::string bodyStr = std::string(req->getBody());
  if (!reader.parse(bodyStr, body) || !body.isObject()) { callback(createErrorResponse(400, "invalid_request", "Expected JSON object")); return; }

  Json::Value jams = loadJamsFromConfig(instanceId);
  bool found = false;
  for (Json::ArrayIndex i = 0; i < jams.size(); ++i) {
    auto &j = jams[i];
    if (!j.isObject()) continue;
    if (!j.isMember("id") || j["id"].asString() != jamId) continue;
    // Merge update fields
    for (const auto &name : body.getMemberNames()) j[name] = body[name];

    // Validate the merged jam object
    {
      std::string err;
      if (!validateJamParameters(j, err)) { callback(createErrorResponse(400, "invalid_request", err)); return; }
    }

    found = true; break;
  }
  if (!found) { callback(createErrorResponse(404, "not_found", "Jam zone not found")); return; }

  if (!saveJamsToConfig(instanceId, jams)) { callback(createErrorResponse(500, "save_failed", "Failed to save jam configuration")); return; }
  if (!updateJamsRuntime(instanceId, jams)) { restartInstanceForJamUpdate(instanceId); }
  callback(createSuccessResponse(Json::Value(Json::objectValue), 200));
}

void JamsHandler::deleteJam(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
  std::string instanceId = extractInstanceId(req);
  std::string jamId = extractJamId(req);
  Json::Value jams = loadJamsFromConfig(instanceId);
  Json::Value newJams(Json::arrayValue);
  bool found = false;
  for (const auto &j : jams) { if (j.isObject() && j.isMember("id") && j["id"].asString() == jamId) { found = true; continue; } newJams.append(j); }
  if (!found) { callback(createErrorResponse(404, "not_found", "Jam zone not found")); return; }
  if (!saveJamsToConfig(instanceId, newJams)) { callback(createErrorResponse(500, "save_failed", "Failed to save jam configuration")); return; }
  if (!updateJamsRuntime(instanceId, newJams)) { restartInstanceForJamUpdate(instanceId); }
  callback(createSuccessResponse(Json::Value(Json::objectValue), 200));
}

void JamsHandler::batchUpdateJams(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
  std::string instanceId = extractInstanceId(req);
  Json::Reader reader; Json::Value body;
  std::string bodyStr = std::string(req->getBody());
  if (!reader.parse(bodyStr, body) || !body.isArray()) { callback(createErrorResponse(400, "invalid_request", "Expected JSON array")); return; }
  Json::Value parsed = parseJamsFromJson(body);
  if (!saveJamsToConfig(instanceId, parsed)) { callback(createErrorResponse(500, "save_failed", "Failed to save jams")); return; }
  if (!updateJamsRuntime(instanceId, parsed)) { restartInstanceForJamUpdate(instanceId); }
  Json::Value wrapper(Json::objectValue);
  wrapper["jamZones"] = parsed;
  callback(createSuccessResponse(wrapper, 200));
}

void JamsHandler::handleOptions(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) {
  auto resp = HttpResponse::newHttpResponse();
  resp->setStatusCode(k200OK);
  resp->addHeader("Access-Control-Allow-Origin", "*");
  resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
  resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
  callback(resp);
}
