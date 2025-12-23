#include "api/lines_handler.h"
#include "core/logger.h"
#include "core/logging_flags.h"
#include "core/uuid_generator.h"
#include "instances/instance_manager.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cvedix/nodes/ba/cvedix_ba_crossline_node.h>
#include <cvedix/objects/shapes/cvedix_line.h>
#include <cvedix/objects/shapes/cvedix_point.h>
#include <drogon/HttpResponse.h>
#include <json/reader.h>
#include <json/writer.h>
#include <sstream>
#include <thread>

IInstanceManager *LinesHandler::instance_manager_ = nullptr;

void LinesHandler::setInstanceManager(IInstanceManager *manager) {
  instance_manager_ = manager;
}

std::string LinesHandler::extractInstanceId(const HttpRequestPtr &req) const {
  std::string instanceId = req->getParameter("instanceId");

  if (instanceId.empty()) {
    std::string path = req->getPath();
    // Try /instances/ pattern first (plural, standard)
    size_t instancesPos = path.find("/instances/");
    if (instancesPos != std::string::npos) {
      size_t start = instancesPos + 11; // length of "/instances/"
      size_t end = path.find("/", start);
      if (end == std::string::npos) {
        end = path.length();
      }
      instanceId = path.substr(start, end - start);
    } else {
      // Try /instance/ pattern (singular, backward compatibility)
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

std::string LinesHandler::extractLineId(const HttpRequestPtr &req) const {
  std::string lineId = req->getParameter("lineId");

  if (lineId.empty()) {
    std::string path = req->getPath();
    size_t linesPos = path.find("/lines/");
    if (linesPos != std::string::npos) {
      size_t start = linesPos + 7; // length of "/lines/"
      size_t end = path.find("/", start);
      if (end == std::string::npos) {
        end = path.length();
      }
      lineId = path.substr(start, end - start);
    }
  }

  return lineId;
}

HttpResponsePtr
LinesHandler::createErrorResponse(int statusCode, const std::string &error,
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

HttpResponsePtr LinesHandler::createSuccessResponse(const Json::Value &data,
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

Json::Value
LinesHandler::loadLinesFromConfig(const std::string &instanceId) const {
  Json::Value linesArray(Json::arrayValue);

  if (!instance_manager_) {
    return linesArray;
  }

  auto optInfo = instance_manager_->getInstance(instanceId);
  if (!optInfo.has_value()) {
    return linesArray;
  }

  const auto &info = optInfo.value();
  auto it = info.additionalParams.find("CrossingLines");
  if (it != info.additionalParams.end() && !it->second.empty()) {
    // Parse JSON string to JSON array
    Json::Reader reader;
    Json::Value parsedLines;
    if (reader.parse(it->second, parsedLines) && parsedLines.isArray()) {
      // Ensure all lines have an id - generate UUID if missing
      for (Json::ArrayIndex i = 0; i < parsedLines.size(); ++i) {
        Json::Value &line = parsedLines[i];
        if (!line.isObject()) {
          continue;
        }

        // Check if id is missing or empty
        if (!line.isMember("id") || !line["id"].isString() ||
            line["id"].asString().empty()) {
          // Generate UUID for line without id
          line["id"] = UUIDGenerator::generateUUID();
          if (isApiLoggingEnabled()) {
            PLOG_DEBUG
                << "[API] loadLinesFromConfig: Generated UUID for line at "
                   "index "
                << i;
          }
        }
      }
      return parsedLines;
    }
  }

  return linesArray;
}

bool LinesHandler::saveLinesToConfig(const std::string &instanceId,
                                     const Json::Value &lines) const {
  if (!instance_manager_) {
    return false;
  }

  // Ensure all lines have an id - generate UUID if missing
  Json::Value normalizedLines = lines;
  if (normalizedLines.isArray()) {
    for (Json::ArrayIndex i = 0; i < normalizedLines.size(); ++i) {
      Json::Value &line = normalizedLines[i];
      if (!line.isObject()) {
        continue;
      }

      // Check if id is missing or empty
      if (!line.isMember("id") || !line["id"].isString() ||
          line["id"].asString().empty()) {
        // Generate UUID for line without id
        line["id"] = UUIDGenerator::generateUUID();
        if (isApiLoggingEnabled()) {
          PLOG_DEBUG << "[API] saveLinesToConfig: Generated UUID for line at "
                        "index "
                     << i << " before saving";
        }
      }
    }
  }

  // Convert lines array to JSON string
  Json::StreamWriterBuilder builder;
  builder["indentation"] = ""; // Compact format
  std::string linesJsonStr = Json::writeString(builder, normalizedLines);

  // Create config update JSON
  Json::Value configUpdate(Json::objectValue);
  Json::Value additionalParams(Json::objectValue);
  additionalParams["CrossingLines"] = linesJsonStr;
  configUpdate["AdditionalParams"] = additionalParams;

  // Update instance config
  // Note: updateInstanceFromConfig will merge AdditionalParams correctly,
  // preserving existing keys like "input" and adding/updating "CrossingLines"
  bool result =
      instance_manager_->updateInstanceFromConfig(instanceId, configUpdate);

  if (!result && isApiLoggingEnabled()) {
    PLOG_WARNING << "[API] saveLinesToConfig: updateInstanceFromConfig failed "
                    "for instance "
                 << instanceId;
  }

  return result;
}

bool LinesHandler::validateCoordinates(const Json::Value &coordinates,
                                       std::string &error) const {
  if (!coordinates.isArray()) {
    error = "Coordinates must be an array";
    return false;
  }

  if (coordinates.size() < 2) {
    error = "Coordinates must contain at least 2 points";
    return false;
  }

  for (const auto &coord : coordinates) {
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

bool LinesHandler::validateDirection(const std::string &direction,
                                     std::string &error) const {
  std::string directionLower = direction;
  std::transform(directionLower.begin(), directionLower.end(),
                 directionLower.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  if (directionLower != "up" && directionLower != "down" &&
      directionLower != "both") {
    error = "Direction must be one of: Up, Down, Both";
    return false;
  }

  return true;
}

bool LinesHandler::validateClasses(const Json::Value &classes,
                                   std::string &error) const {
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
      error = "Invalid class: " + classStr +
              ". Allowed values: Person, Animal, Vehicle, Face, Unknown";
      return false;
    }
  }

  return true;
}

bool LinesHandler::validateColor(const Json::Value &color,
                                 std::string &error) const {
  if (!color.isArray()) {
    error = "Color must be an array";
    return false;
  }

  if (color.size() != 4) {
    error = "Color must contain exactly 4 values (RGBA)";
    return false;
  }

  for (const auto &val : color) {
    if (!val.isNumeric()) {
      error = "Each color value must be a number";
      return false;
    }
  }

  return true;
}

void LinesHandler::getAllLines(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();

  std::string instanceId = extractInstanceId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] GET /v1/core/instance/" << instanceId
              << "/lines - Get all lines";
    PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
  }

  try {
    if (!instance_manager_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] GET /v1/core/instance/" << instanceId
                   << "/lines - Error: Instance manager not initialized";
      }
      callback(createErrorResponse(500, "Internal server error",
                                   "Instance manager not initialized"));
      return;
    }

    if (instanceId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] GET /v1/core/instance/{instanceId}/lines - "
                        "Error: Instance ID is empty";
      }
      callback(
          createErrorResponse(400, "Bad request", "Instance ID is required"));
      return;
    }

    // Check if instance exists
    auto optInfo = instance_manager_->getInstance(instanceId);
    if (!optInfo.has_value()) {
      auto end_time = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
          end_time - start_time);
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] GET /v1/core/instance/" << instanceId
                     << "/lines - Instance not found - " << duration.count()
                     << "ms";
      }
      callback(createErrorResponse(404, "Not found",
                                   "Instance not found: " + instanceId));
      return;
    }

    // Load lines from config
    Json::Value linesArray = loadLinesFromConfig(instanceId);

    // Build response
    Json::Value response;
    response["crossingLines"] = linesArray;

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] GET /v1/core/instance/" << instanceId
                << "/lines - Success: " << linesArray.size() << " lines - "
                << duration.count() << "ms";
    }

    callback(createSuccessResponse(response));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] GET /v1/core/instance/" << instanceId
                 << "/lines - Exception: " << e.what() << " - "
                 << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] GET /v1/core/instance/" << instanceId
                 << "/lines - Unknown exception - " << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void LinesHandler::createLine(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();

  std::string instanceId = extractInstanceId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] POST /v1/core/instance/" << instanceId
              << "/lines - Create line";
    PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
  }

  try {
    if (!instance_manager_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] POST /v1/core/instance/" << instanceId
                   << "/lines - Error: Instance registry not initialized";
      }
      callback(createErrorResponse(500, "Internal server error",
                                   "Instance manager not initialized"));
      return;
    }

    if (instanceId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] POST /v1/core/instance/{instanceId}/lines - "
                        "Error: Instance ID is empty";
      }
      callback(
          createErrorResponse(400, "Bad request", "Instance ID is required"));
      return;
    }

    // Check if instance exists
    auto optInfo = instance_manager_->getInstance(instanceId);
    if (!optInfo.has_value()) {
      auto end_time = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
          end_time - start_time);
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] POST /v1/core/instance/" << instanceId
                     << "/lines - Instance not found - " << duration.count()
                     << "ms";
      }
      callback(createErrorResponse(404, "Not found",
                                   "Instance not found: " + instanceId));
      return;
    }

    // Parse JSON body
    auto json = req->getJsonObject();
    if (!json) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] POST /v1/core/instance/" << instanceId
                     << "/lines - Error: Invalid JSON body";
      }
      callback(createErrorResponse(400, "Bad request",
                                   "Request body must be valid JSON"));
      return;
    }

    // Validate required fields
    if (!json->isMember("coordinates") || !(*json)["coordinates"].isArray()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING
            << "[API] POST /v1/core/instance/" << instanceId
            << "/lines - Error: Missing or invalid 'coordinates' field";
      }
      callback(createErrorResponse(
          400, "Bad request",
          "Field 'coordinates' is required and must be an array"));
      return;
    }

    // Validate coordinates
    std::string coordError;
    if (!validateCoordinates((*json)["coordinates"], coordError)) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] POST /v1/core/instance/" << instanceId
                     << "/lines - Validation error: " << coordError;
      }
      callback(createErrorResponse(400, "Bad request", coordError));
      return;
    }

    // Validate optional fields
    if (json->isMember("direction") && (*json)["direction"].isString()) {
      std::string dirError;
      if (!validateDirection((*json)["direction"].asString(), dirError)) {
        if (isApiLoggingEnabled()) {
          PLOG_WARNING << "[API] POST /v1/core/instance/" << instanceId
                       << "/lines - Validation error: " << dirError;
        }
        callback(createErrorResponse(400, "Bad request", dirError));
        return;
      }
    }

    if (json->isMember("classes") && (*json)["classes"].isArray()) {
      std::string classesError;
      if (!validateClasses((*json)["classes"], classesError)) {
        if (isApiLoggingEnabled()) {
          PLOG_WARNING << "[API] POST /v1/core/instance/" << instanceId
                       << "/lines - Validation error: " << classesError;
        }
        callback(createErrorResponse(400, "Bad request", classesError));
        return;
      }
    }

    if (json->isMember("color") && (*json)["color"].isArray()) {
      std::string colorError;
      if (!validateColor((*json)["color"], colorError)) {
        if (isApiLoggingEnabled()) {
          PLOG_WARNING << "[API] POST /v1/core/instance/" << instanceId
                       << "/lines - Validation error: " << colorError;
        }
        callback(createErrorResponse(400, "Bad request", colorError));
        return;
      }
    }

    // Load existing lines
    Json::Value linesArray = loadLinesFromConfig(instanceId);

    // Create new line object
    Json::Value newLine(Json::objectValue);
    newLine["id"] = UUIDGenerator::generateUUID();

    if (json->isMember("name") && (*json)["name"].isString()) {
      newLine["name"] = (*json)["name"];
    }

    newLine["coordinates"] = (*json)["coordinates"];

    if (json->isMember("classes") && (*json)["classes"].isArray()) {
      newLine["classes"] = (*json)["classes"];
    } else {
      newLine["classes"] = Json::Value(Json::arrayValue);
    }

    if (json->isMember("direction") && (*json)["direction"].isString()) {
      newLine["direction"] = (*json)["direction"];
    } else {
      newLine["direction"] = "Both";
    }

    if (json->isMember("color") && (*json)["color"].isArray()) {
      newLine["color"] = (*json)["color"];
    } else {
      Json::Value defaultColor(Json::arrayValue);
      defaultColor.append(255); // R
      defaultColor.append(0);   // G
      defaultColor.append(0);   // B
      defaultColor.append(255); // A
      newLine["color"] = defaultColor;
    }

    // Append new line to array
    linesArray.append(newLine);

    // Save to config
    if (!saveLinesToConfig(instanceId, linesArray)) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] POST /v1/core/instance/" << instanceId
                   << "/lines - Failed to save lines to config";
      }
      callback(createErrorResponse(500, "Internal server error",
                                   "Failed to save lines configuration"));
      return;
    }

    // Try runtime update first (without restart)
    if (updateLinesRuntime(instanceId, linesArray)) {
      if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] POST /v1/core/instance/" << instanceId
                  << "/lines - Lines updated runtime without restart";
      }
    } else {
      // Fallback to restart if runtime update failed
      if (isApiLoggingEnabled()) {
        PLOG_WARNING
            << "[API] POST /v1/core/instance/" << instanceId
            << "/lines - Runtime update failed, falling back to restart";
      }
      restartInstanceForLineUpdate(instanceId);
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] POST /v1/core/instance/" << instanceId
                << "/lines - Success: Created line " << newLine["id"].asString()
                << " - " << duration.count() << "ms";
    }

    callback(createSuccessResponse(newLine, 201));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] POST /v1/core/instance/" << instanceId
                 << "/lines - Exception: " << e.what() << " - "
                 << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] POST /v1/core/instance/" << instanceId
                 << "/lines - Unknown exception - " << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void LinesHandler::deleteAllLines(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();

  std::string instanceId = extractInstanceId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] DELETE /v1/core/instance/" << instanceId
              << "/lines - Delete all lines";
    PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
  }

  try {
    if (!instance_manager_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] DELETE /v1/core/instance/" << instanceId
                   << "/lines - Error: Instance registry not initialized";
      }
      callback(createErrorResponse(500, "Internal server error",
                                   "Instance manager not initialized"));
      return;
    }

    if (instanceId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] DELETE /v1/core/instance/{instanceId}/lines - "
                        "Error: Instance ID is empty";
      }
      callback(
          createErrorResponse(400, "Bad request", "Instance ID is required"));
      return;
    }

    // Check if instance exists
    auto optInfo = instance_manager_->getInstance(instanceId);
    if (!optInfo.has_value()) {
      auto end_time = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
          end_time - start_time);
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] DELETE /v1/core/instance/" << instanceId
                     << "/lines - Instance not found - " << duration.count()
                     << "ms";
      }
      callback(createErrorResponse(404, "Not found",
                                   "Instance not found: " + instanceId));
      return;
    }

    // Save empty array to config
    Json::Value emptyArray(Json::arrayValue);
    if (!saveLinesToConfig(instanceId, emptyArray)) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] DELETE /v1/core/instance/" << instanceId
                   << "/lines - Failed to save lines to config";
      }
      callback(createErrorResponse(500, "Internal server error",
                                   "Failed to save lines configuration"));
      return;
    }

    // Try runtime update first (without restart)
    if (updateLinesRuntime(instanceId, emptyArray)) {
      if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] DELETE /v1/core/instance/" << instanceId
                  << "/lines - Lines updated runtime without restart";
      }
    } else {
      // Fallback to restart if runtime update failed
      if (isApiLoggingEnabled()) {
        PLOG_WARNING
            << "[API] DELETE /v1/core/instance/" << instanceId
            << "/lines - Runtime update failed, falling back to restart";
      }
      restartInstanceForLineUpdate(instanceId);
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] DELETE /v1/core/instance/" << instanceId
                << "/lines - Success - " << duration.count() << "ms";
    }

    Json::Value response;
    response["message"] = "All lines deleted successfully";
    callback(createSuccessResponse(response));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] DELETE /v1/core/instance/" << instanceId
                 << "/lines - Exception: " << e.what() << " - "
                 << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] DELETE /v1/core/instance/" << instanceId
                 << "/lines - Unknown exception - " << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void LinesHandler::getLine(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();

  std::string instanceId = extractInstanceId(req);
  std::string lineId = extractLineId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] GET /v1/core/instance/" << instanceId << "/lines/"
              << lineId << " - Get line";
    PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
  }

  try {
    if (!instance_manager_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] GET /v1/core/instance/" << instanceId << "/lines/"
                   << lineId << " - Error: Instance registry not initialized";
      }
      callback(createErrorResponse(500, "Internal server error",
                                   "Instance manager not initialized"));
      return;
    }

    if (instanceId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING
            << "[API] GET /v1/core/instance/{instanceId}/lines/{lineId} - "
               "Error: Instance ID is empty";
      }
      callback(
          createErrorResponse(400, "Bad request", "Instance ID is required"));
      return;
    }

    if (lineId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] GET /v1/core/instance/" << instanceId
                     << "/lines/{lineId} - Error: Line ID is empty";
      }
      callback(createErrorResponse(400, "Bad request", "Line ID is required"));
      return;
    }

    // Check if instance exists
    auto optInfo = instance_manager_->getInstance(instanceId);
    if (!optInfo.has_value()) {
      auto end_time = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
          end_time - start_time);
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] GET /v1/core/instance/" << instanceId
                     << "/lines/" << lineId << " - Instance not found - "
                     << duration.count() << "ms";
      }
      callback(createErrorResponse(404, "Not found",
                                   "Instance not found: " + instanceId));
      return;
    }

    // Load lines from config
    Json::Value linesArray = loadLinesFromConfig(instanceId);

    // Find line with matching ID
    for (const auto &line : linesArray) {
      if (line.isObject() && line.isMember("id") && line["id"].isString()) {
        if (line["id"].asString() == lineId) {
          auto end_time = std::chrono::steady_clock::now();
          auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
              end_time - start_time);

          if (isApiLoggingEnabled()) {
            PLOG_INFO << "[API] GET /v1/core/instance/" << instanceId
                      << "/lines/" << lineId << " - Success - "
                      << duration.count() << "ms";
          }

          callback(createSuccessResponse(line));
          return;
        }
      }
    }

    // Line not found
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_WARNING << "[API] GET /v1/core/instance/" << instanceId << "/lines/"
                   << lineId << " - Line not found - " << duration.count()
                   << "ms";
    }
    callback(
        createErrorResponse(404, "Not found", "Line not found: " + lineId));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] GET /v1/core/instance/" << instanceId << "/lines/"
                 << lineId << " - Exception: " << e.what() << " - "
                 << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] GET /v1/core/instance/" << instanceId << "/lines/"
                 << lineId << " - Unknown exception - " << duration.count()
                 << "ms";
    }
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void LinesHandler::updateLine(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();

  std::string instanceId = extractInstanceId(req);
  std::string lineId = extractLineId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] PUT /v1/core/instance/" << instanceId << "/lines/"
              << lineId << " - Update line";
    PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
  }

  try {
    if (!instance_manager_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] PUT /v1/core/instance/" << instanceId << "/lines/"
                   << lineId << " - Error: Instance registry not initialized";
      }
      callback(createErrorResponse(500, "Internal server error",
                                   "Instance manager not initialized"));
      return;
    }

    if (instanceId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING
            << "[API] PUT /v1/core/instance/{instanceId}/lines/{lineId} - "
               "Error: Instance ID is empty";
      }
      callback(
          createErrorResponse(400, "Bad request", "Instance ID is required"));
      return;
    }

    if (lineId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] PUT /v1/core/instance/" << instanceId
                     << "/lines/{lineId} - Error: Line ID is empty";
      }
      callback(createErrorResponse(400, "Bad request", "Line ID is required"));
      return;
    }

    // Check if instance exists
    auto optInfo = instance_manager_->getInstance(instanceId);
    if (!optInfo.has_value()) {
      auto end_time = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
          end_time - start_time);
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] PUT /v1/core/instance/" << instanceId
                     << "/lines/" << lineId << " - Instance not found - "
                     << duration.count() << "ms";
      }
      callback(createErrorResponse(404, "Not found",
                                   "Instance not found: " + instanceId));
      return;
    }

    // Parse JSON body
    auto json = req->getJsonObject();
    if (!json) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] PUT /v1/core/instance/" << instanceId
                     << "/lines/" << lineId << " - Error: Invalid JSON body";
      }
      callback(createErrorResponse(400, "Bad request",
                                   "Request body must be valid JSON"));
      return;
    }

    // Validate required fields
    if (!json->isMember("coordinates") || !(*json)["coordinates"].isArray()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] PUT /v1/core/instance/" << instanceId
                     << "/lines/" << lineId
                     << " - Error: Missing or invalid 'coordinates' field";
      }
      callback(createErrorResponse(
          400, "Bad request",
          "Field 'coordinates' is required and must be an array"));
      return;
    }

    // Validate coordinates
    std::string coordError;
    if (!validateCoordinates((*json)["coordinates"], coordError)) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] PUT /v1/core/instance/" << instanceId
                     << "/lines/" << lineId
                     << " - Validation error: " << coordError;
      }
      callback(createErrorResponse(400, "Bad request", coordError));
      return;
    }

    // Validate optional fields
    if (json->isMember("direction") && (*json)["direction"].isString()) {
      std::string dirError;
      if (!validateDirection((*json)["direction"].asString(), dirError)) {
        if (isApiLoggingEnabled()) {
          PLOG_WARNING << "[API] PUT /v1/core/instance/" << instanceId
                       << "/lines/" << lineId
                       << " - Validation error: " << dirError;
        }
        callback(createErrorResponse(400, "Bad request", dirError));
        return;
      }
    }

    if (json->isMember("classes") && (*json)["classes"].isArray()) {
      std::string classesError;
      if (!validateClasses((*json)["classes"], classesError)) {
        if (isApiLoggingEnabled()) {
          PLOG_WARNING << "[API] PUT /v1/core/instance/" << instanceId
                       << "/lines/" << lineId
                       << " - Validation error: " << classesError;
        }
        callback(createErrorResponse(400, "Bad request", classesError));
        return;
      }
    }

    if (json->isMember("color") && (*json)["color"].isArray()) {
      std::string colorError;
      if (!validateColor((*json)["color"], colorError)) {
        if (isApiLoggingEnabled()) {
          PLOG_WARNING << "[API] PUT /v1/core/instance/" << instanceId
                       << "/lines/" << lineId
                       << " - Validation error: " << colorError;
        }
        callback(createErrorResponse(400, "Bad request", colorError));
        return;
      }
    }

    // Load existing lines
    Json::Value linesArray = loadLinesFromConfig(instanceId);

    // Find and update line with matching ID
    bool found = false;
    for (Json::ArrayIndex i = 0; i < linesArray.size(); ++i) {
      Json::Value &line = linesArray[i];
      if (line.isObject() && line.isMember("id") && line["id"].isString()) {
        if (line["id"].asString() == lineId) {
          found = true;

          // Update line fields (preserve ID)
          if (json->isMember("name") && (*json)["name"].isString()) {
            line["name"] = (*json)["name"];
          }

          line["coordinates"] = (*json)["coordinates"];

          if (json->isMember("classes") && (*json)["classes"].isArray()) {
            line["classes"] = (*json)["classes"];
          } else {
            line["classes"] = Json::Value(Json::arrayValue);
          }

          if (json->isMember("direction") && (*json)["direction"].isString()) {
            line["direction"] = (*json)["direction"];
          } else {
            line["direction"] = "Both";
          }

          if (json->isMember("color") && (*json)["color"].isArray()) {
            line["color"] = (*json)["color"];
          } else {
            Json::Value defaultColor(Json::arrayValue);
            defaultColor.append(255); // R
            defaultColor.append(0);   // G
            defaultColor.append(0);   // B
            defaultColor.append(255); // A
            line["color"] = defaultColor;
          }

          break;
        }
      }
    }

    if (!found) {
      auto end_time = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
          end_time - start_time);
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] PUT /v1/core/instance/" << instanceId
                     << "/lines/" << lineId << " - Line not found - "
                     << duration.count() << "ms";
      }
      callback(
          createErrorResponse(404, "Not found", "Line not found: " + lineId));
      return;
    }

    // Save updated lines to config
    if (!saveLinesToConfig(instanceId, linesArray)) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] PUT /v1/core/instance/" << instanceId << "/lines/"
                   << lineId << " - Failed to save lines to config";
      }
      callback(createErrorResponse(500, "Internal server error",
                                   "Failed to save lines configuration"));
      return;
    }

    // Try runtime update first (without restart)
    if (updateLinesRuntime(instanceId, linesArray)) {
      if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] PUT /v1/core/instance/" << instanceId << "/lines/"
                  << lineId << " - Lines updated runtime without restart";
      }
    } else {
      // Fallback to restart if runtime update failed
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] PUT /v1/core/instance/" << instanceId
                     << "/lines/" << lineId
                     << " - Runtime update failed, falling back to restart";
      }
      restartInstanceForLineUpdate(instanceId);
    }

    // Find updated line to return
    Json::Value updatedLine;
    for (const auto &line : linesArray) {
      if (line.isObject() && line.isMember("id") && line["id"].isString()) {
        if (line["id"].asString() == lineId) {
          updatedLine = line;
          break;
        }
      }
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] PUT /v1/core/instance/" << instanceId << "/lines/"
                << lineId << " - Success - " << duration.count() << "ms";
    }

    callback(createSuccessResponse(updatedLine));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] PUT /v1/core/instance/" << instanceId << "/lines/"
                 << lineId << " - Exception: " << e.what() << " - "
                 << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] PUT /v1/core/instance/" << instanceId << "/lines/"
                 << lineId << " - Unknown exception - " << duration.count()
                 << "ms";
    }
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void LinesHandler::deleteLine(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();

  std::string instanceId = extractInstanceId(req);
  std::string lineId = extractLineId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] DELETE /v1/core/instance/" << instanceId << "/lines/"
              << lineId << " - Delete line";
    PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
  }

  try {
    if (!instance_manager_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] DELETE /v1/core/instance/" << instanceId
                   << "/lines/" << lineId
                   << " - Error: Instance registry not initialized";
      }
      callback(createErrorResponse(500, "Internal server error",
                                   "Instance manager not initialized"));
      return;
    }

    if (instanceId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING
            << "[API] DELETE /v1/core/instance/{instanceId}/lines/{lineId} - "
               "Error: Instance ID is empty";
      }
      callback(
          createErrorResponse(400, "Bad request", "Instance ID is required"));
      return;
    }

    if (lineId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] DELETE /v1/core/instance/" << instanceId
                     << "/lines/{lineId} - Error: Line ID is empty";
      }
      callback(createErrorResponse(400, "Bad request", "Line ID is required"));
      return;
    }

    // Check if instance exists
    auto optInfo = instance_manager_->getInstance(instanceId);
    if (!optInfo.has_value()) {
      auto end_time = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
          end_time - start_time);
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] DELETE /v1/core/instance/" << instanceId
                     << "/lines/" << lineId << " - Instance not found - "
                     << duration.count() << "ms";
      }
      callback(createErrorResponse(404, "Not found",
                                   "Instance not found: " + instanceId));
      return;
    }

    // Load existing lines
    Json::Value linesArray = loadLinesFromConfig(instanceId);

    // Find and remove line with matching ID
    Json::Value newLinesArray(Json::arrayValue);
    bool found = false;

    for (const auto &line : linesArray) {
      if (line.isObject() && line.isMember("id") && line["id"].isString()) {
        if (line["id"].asString() == lineId) {
          found = true;
          // Skip this line (don't add to new array)
          continue;
        }
      }
      newLinesArray.append(line);
    }

    if (!found) {
      auto end_time = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
          end_time - start_time);
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] DELETE /v1/core/instance/" << instanceId
                     << "/lines/" << lineId << " - Line not found - "
                     << duration.count() << "ms";
      }
      callback(
          createErrorResponse(404, "Not found", "Line not found: " + lineId));
      return;
    }

    // Save updated lines to config
    if (!saveLinesToConfig(instanceId, newLinesArray)) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] DELETE /v1/core/instance/" << instanceId
                   << "/lines/" << lineId
                   << " - Failed to save lines to config";
      }
      callback(createErrorResponse(500, "Internal server error",
                                   "Failed to save lines configuration"));
      return;
    }

    // Try runtime update first (without restart)
    if (updateLinesRuntime(instanceId, newLinesArray)) {
      if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] DELETE /v1/core/instance/" << instanceId
                  << "/lines/" << lineId
                  << " - Lines updated runtime without restart";
      }
    } else {
      // Fallback to restart if runtime update failed
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] DELETE /v1/core/instance/" << instanceId
                     << "/lines/" << lineId
                     << " - Runtime update failed, falling back to restart";
      }
      restartInstanceForLineUpdate(instanceId);
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] DELETE /v1/core/instance/" << instanceId << "/lines/"
                << lineId << " - Success - " << duration.count() << "ms";
    }

    Json::Value response;
    response["message"] = "Line deleted successfully";
    callback(createSuccessResponse(response));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] DELETE /v1/core/instance/" << instanceId << "/lines/"
                 << lineId << " - Exception: " << e.what() << " - "
                 << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] DELETE /v1/core/instance/" << instanceId << "/lines/"
                 << lineId << " - Unknown exception - " << duration.count()
                 << "ms";
    }
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void LinesHandler::handleOptions(
    const HttpRequestPtr &req,
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

bool LinesHandler::restartInstanceForLineUpdate(
    const std::string &instanceId) const {
  if (!instance_manager_) {
    return false;
  }

  // Check if instance is running
  auto optInfo = instance_manager_->getInstance(instanceId);
  if (!optInfo.has_value() || !optInfo.value().running) {
    // Instance not running, no need to restart
    return true;
  }

  // Restart instance in background thread to apply line changes
  std::thread restartThread([this, instanceId]() {
    try {
      if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] ========================================";
        PLOG_INFO << "[API] Restarting instance " << instanceId
                  << " to apply line changes";
        PLOG_INFO << "[API] This will rebuild pipeline with new lines from "
                     "additionalParams[\"CrossingLines\"]";
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

      // Start instance again (this will rebuild pipeline with new lines)
      // startInstance() calls rebuildPipelineFromInstanceInfo() which rebuilds
      // pipeline with lines from additionalParams["CrossingLines"]
      if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] Step 3/3: Starting instance " << instanceId
                  << " (will rebuild pipeline with new lines)";
      }
      bool startSuccess = instance_manager_->startInstance(instanceId, true);

      if (startSuccess) {
        if (isApiLoggingEnabled()) {
          PLOG_INFO << "[API] ========================================";
          PLOG_INFO << "[API] ✓ Instance " << instanceId
                    << " restarted successfully for line update";
          PLOG_INFO << "[API] Pipeline rebuilt with new lines - lines should "
                       "now be visible on stream";
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
                   << " for line update: " << e.what();
      }
    } catch (...) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] ✗ Unknown error restarting instance " << instanceId
                   << " for line update";
      }
    }
  });
  restartThread.detach();

  return true;
}

std::shared_ptr<cvedix_nodes::cvedix_ba_crossline_node>
LinesHandler::findBACrosslineNode(const std::string &instanceId) const {
  // Note: In subprocess mode, we cannot access nodes directly
  // as they run in separate worker processes.
  // This method will return nullptr in subprocess mode,
  // and updateLinesRuntime() will fallback to restart.

  if (!instance_manager_) {
    return nullptr;
  }

  // Check if we're in subprocess mode
  if (instance_manager_->isSubprocessMode()) {
    // In subprocess mode, nodes are in worker process, not accessible directly
    if (isApiLoggingEnabled()) {
      PLOG_DEBUG << "[API] findBACrosslineNode: Subprocess mode - nodes not "
                    "directly accessible";
    }
    return nullptr;
  }

  // In in-process mode, try to access nodes via InstanceRegistry
  // This requires casting to InProcessInstanceManager to access registry
  // For now, return nullptr and let updateLinesRuntime() handle via restart
  // TODO: Add getInstanceNodes() to IInstanceManager interface if needed
  if (isApiLoggingEnabled()) {
    PLOG_DEBUG << "[API] findBACrosslineNode: Direct node access not "
                  "available, will use restart fallback";
  }
  return nullptr;
}

std::map<int, cvedix_objects::cvedix_line>
LinesHandler::parseLinesFromJson(const Json::Value &linesArray) const {
  std::map<int, cvedix_objects::cvedix_line> lines;

  if (!linesArray.isArray()) {
    if (isApiLoggingEnabled()) {
      PLOG_WARNING << "[API] parseLinesFromJson: Input is not a JSON array";
    }
    return lines;
  }

  // Iterate through lines array
  for (Json::ArrayIndex i = 0; i < linesArray.size(); ++i) {
    const Json::Value &lineObj = linesArray[i];

    // Check if line has coordinates
    if (!lineObj.isMember("coordinates") || !lineObj["coordinates"].isArray()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] parseLinesFromJson: Line at index " << i
                     << " missing or invalid 'coordinates' field, skipping";
      }
      continue;
    }

    const Json::Value &coordinates = lineObj["coordinates"];
    if (coordinates.size() < 2) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] parseLinesFromJson: Line at index " << i
                     << " has less than 2 coordinates, skipping";
      }
      continue;
    }

    // Get first and last coordinates
    const Json::Value &startCoord = coordinates[0];
    const Json::Value &endCoord = coordinates[coordinates.size() - 1];

    if (!startCoord.isMember("x") || !startCoord.isMember("y") ||
        !endCoord.isMember("x") || !endCoord.isMember("y")) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] parseLinesFromJson: Line at index " << i
                     << " has invalid coordinate format, skipping";
      }
      continue;
    }

    if (!startCoord["x"].isNumeric() || !startCoord["y"].isNumeric() ||
        !endCoord["x"].isNumeric() || !endCoord["y"].isNumeric()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] parseLinesFromJson: Line at index " << i
                     << " has non-numeric coordinates, skipping";
      }
      continue;
    }

    // Convert to cvedix_line
    int start_x = startCoord["x"].asInt();
    int start_y = startCoord["y"].asInt();
    int end_x = endCoord["x"].asInt();
    int end_y = endCoord["y"].asInt();

    cvedix_objects::cvedix_point start(start_x, start_y);
    cvedix_objects::cvedix_point end(end_x, end_y);

    // Use array index as channel (0, 1, 2, ...)
    int channel = static_cast<int>(i);
    lines[channel] = cvedix_objects::cvedix_line(start, end);
  }

  if (isApiLoggingEnabled() && !lines.empty()) {
    PLOG_DEBUG << "[API] parseLinesFromJson: Parsed " << lines.size()
               << " line(s) from JSON";
  }

  return lines;
}

bool LinesHandler::updateLinesRuntime(const std::string &instanceId,
                                      const Json::Value &linesArray) const {
  if (!instance_manager_) {
    if (isApiLoggingEnabled()) {
      PLOG_WARNING
          << "[API] updateLinesRuntime: Instance registry not initialized";
    }
    return false;
  }

  // Check if instance is running
  auto optInfo = instance_manager_->getInstance(instanceId);
  if (!optInfo.has_value()) {
    if (isApiLoggingEnabled()) {
      PLOG_DEBUG << "[API] updateLinesRuntime: Instance " << instanceId
                 << " not found";
    }
    return false;
  }

  if (!optInfo.value().running) {
    if (isApiLoggingEnabled()) {
      PLOG_DEBUG << "[API] updateLinesRuntime: Instance " << instanceId
                 << " is not running, no need to update runtime";
    }
    return true; // Not an error - instance not running, config will apply on
                 // next start
  }

  // Find ba_crossline_node in pipeline
  auto baCrosslineNode = findBACrosslineNode(instanceId);
  if (!baCrosslineNode) {
    if (isApiLoggingEnabled()) {
      PLOG_WARNING << "[API] updateLinesRuntime: ba_crossline_node not found "
                      "in pipeline for instance "
                   << instanceId << ", fallback to restart";
    }
    return false; // Fallback to restart
  }

  // Parse lines from JSON
  auto lines = parseLinesFromJson(linesArray);
  if (lines.empty() && linesArray.isArray() && linesArray.size() > 0) {
    // Parse failed but array is not empty - error
    if (isApiLoggingEnabled()) {
      PLOG_WARNING << "[API] updateLinesRuntime: Failed to parse lines from "
                      "JSON, fallback to restart";
    }
    return false; // Fallback to restart
  }

  // Try to update lines via SDK API
  // NOTE: CVEDIX SDK's ba_crossline_node stores lines in member variable
  // 'all_lines' We'll try to access and update it directly if possible
  try {
    // Attempt: Try to access all_lines member and update it directly
    // Based on SDK header, ba_crossline_node has: std::map<int, cvedix_line>
    // all_lines; We'll try to access it through public interface or friend
    // class

    // Since we don't have direct access to private members, we need to use
    // restart However, we can verify that lines are correctly saved to config
    // first

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] updateLinesRuntime: Found ba_crossline_node, "
                   "attempting to update "
                << lines.size() << " line(s)";
      PLOG_INFO << "[API] updateLinesRuntime: Lines parsed successfully, will "
                   "apply via restart";
      PLOG_INFO << "[API] updateLinesRuntime: Note - Direct runtime update "
                   "requires SDK API access";
    }

    // Verify lines were parsed correctly
    if (lines.empty() && linesArray.isArray() && linesArray.size() == 0) {
      // Empty array is valid (delete all lines)
      if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] updateLinesRuntime: Empty lines array - will clear "
                     "all lines via restart";
      }
    } else if (lines.empty()) {
      // Parse failed
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] updateLinesRuntime: Failed to parse lines, "
                        "fallback to restart";
      }
      return false;
    }

    // Since SDK doesn't expose runtime update API, we need to restart
    // But we've verified that lines are correctly parsed and saved to config
    // The restart will rebuild pipeline with new lines from
    // additionalParams["CrossingLines"]
    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] updateLinesRuntime: Lines configuration saved, "
                   "restarting instance to apply changes";
    }

    // Return false to trigger fallback to restart
    // This ensures lines are applied correctly through pipeline rebuild
    return false;

  } catch (const std::exception &e) {
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] updateLinesRuntime: Exception updating lines: "
                 << e.what() << ", fallback to restart";
    }
    return false; // Fallback to restart
  } catch (...) {
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] updateLinesRuntime: Unknown exception updating "
                    "lines, fallback to restart";
    }
    return false; // Fallback to restart
  }
}
