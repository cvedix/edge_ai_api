#include "api/rules_handler.h"
#include "core/logger.h"
#include "core/logging_flags.h"
#include "core/metrics_interceptor.h"
#include "core/rule_type_validator.h"
#include "core/rule_processor.h"
#include "instances/instance_manager.h"
#include <chrono>
#include <drogon/HttpResponse.h>
#include <json/reader.h>
#include <json/writer.h>
#include <sstream>

IInstanceManager *RulesHandler::instance_manager_ = nullptr;

void RulesHandler::setInstanceManager(IInstanceManager *manager) {
  instance_manager_ = manager;
  // Also set for RuleProcessor
  RuleProcessor::setInstanceManager(manager);
}

std::string RulesHandler::extractInstanceId(const HttpRequestPtr &req) const {
  std::string instanceId = req->getParameter("instanceId");

  if (instanceId.empty()) {
    std::string path = req->getPath();
    // Try /instance/ pattern
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

  return instanceId;
}

HttpResponsePtr RulesHandler::createErrorResponse(int statusCode,
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

HttpResponsePtr RulesHandler::createSuccessResponse(const Json::Value &data,
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
RulesHandler::loadRulesFromConfig(const std::string &instanceId) const {
  Json::Value rules(Json::objectValue);
  rules["zones"] = Json::Value(Json::arrayValue);
  rules["lines"] = Json::Value(Json::arrayValue);
  rules["metadata"] = Json::Value(Json::objectValue);

  if (!instance_manager_) {
    return rules;
  }

  auto optInfo = instance_manager_->getInstance(instanceId);
  if (!optInfo.has_value()) {
    return rules;
  }

  const auto &info = optInfo.value();

  // Load zones from additionalParams
  auto zonesIt = info.additionalParams.find("RulesZones");
  if (zonesIt != info.additionalParams.end() && !zonesIt->second.empty()) {
    Json::Reader reader;
    Json::Value parsedZones;
    if (reader.parse(zonesIt->second, parsedZones) && parsedZones.isArray()) {
      rules["zones"] = parsedZones;
    }
  }

  // Load lines from additionalParams (or from CrossingLines for backward compatibility)
  auto linesIt = info.additionalParams.find("RulesLines");
  if (linesIt != info.additionalParams.end() && !linesIt->second.empty()) {
    Json::Reader reader;
    Json::Value parsedLines;
    if (reader.parse(linesIt->second, parsedLines) && parsedLines.isArray()) {
      rules["lines"] = parsedLines;
    }
  } else {
    // Fallback to CrossingLines for backward compatibility
    auto crossingLinesIt = info.additionalParams.find("CrossingLines");
    if (crossingLinesIt != info.additionalParams.end() &&
        !crossingLinesIt->second.empty()) {
      Json::Reader reader;
      Json::Value parsedLines;
      if (reader.parse(crossingLinesIt->second, parsedLines) &&
          parsedLines.isArray()) {
        rules["lines"] = parsedLines;
      }
    }
  }

  // Load metadata
  auto metadataIt = info.additionalParams.find("RulesMetadata");
  if (metadataIt != info.additionalParams.end() &&
      !metadataIt->second.empty()) {
    Json::Reader reader;
    Json::Value parsedMetadata;
    if (reader.parse(metadataIt->second, parsedMetadata) &&
        parsedMetadata.isObject()) {
      rules["metadata"] = parsedMetadata;
    }
  } else {
    // Set default metadata from instance info
    rules["metadata"]["instanceName"] = info.displayName;
    rules["metadata"]["groupName"] = info.group;
    rules["metadata"]["solutionName"] = info.solutionName;
  }

  return rules;
}

bool RulesHandler::saveRulesToConfig(const std::string &instanceId,
                                      const Json::Value &rules) const {
  if (!instance_manager_) {
    return false;
  }

  auto optInfo = instance_manager_->getInstance(instanceId);
  if (!optInfo.has_value()) {
    return false;
  }

  // Get instance info (non-const reference)
  auto &info = const_cast<InstanceInfo &>(optInfo.value());

  // Save zones
  if (rules.isMember("zones") && rules["zones"].isArray()) {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::string zonesJson =
        Json::writeString(builder, rules["zones"]);
    info.additionalParams["RulesZones"] = zonesJson;
  }

  // Save lines
  if (rules.isMember("lines") && rules["lines"].isArray()) {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::string linesJson =
        Json::writeString(builder, rules["lines"]);
    info.additionalParams["RulesLines"] = linesJson;
  }

  // Save metadata
  if (rules.isMember("metadata") && rules["metadata"].isObject()) {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::string metadataJson =
        Json::writeString(builder, rules["metadata"]);
    info.additionalParams["RulesMetadata"] = metadataJson;
  }

  // Update instance in manager using updateInstanceFromConfig
  // Convert additionalParams to JSON config format
  Json::Value configUpdate(Json::objectValue);
  configUpdate["InstanceId"] = instanceId;
  Json::Value additionalParams(Json::objectValue);
  for (const auto &pair : info.additionalParams) {
    additionalParams[pair.first] = pair.second;
  }
  configUpdate["AdditionalParams"] = additionalParams;

  // Note: updateInstanceFromConfig will merge AdditionalParams correctly,
  // preserving other fields and saving to storage if instance is persistent
  if (!instance_manager_->updateInstanceFromConfig(instanceId, configUpdate)) {
    if (isApiLoggingEnabled()) {
      PLOG_WARNING << "[API] saveRulesToConfig: updateInstanceFromConfig failed "
                      "for instance "
                   << instanceId;
    }
    return false;
  }

  return true;
}

Json::Value
RulesHandler::convertUSCLinesToEdgeAI(const Json::Value &uscLines) const {
  Json::Value edgeAILines(Json::arrayValue);

  if (!uscLines.isArray()) {
    return edgeAILines;
  }

  for (Json::ArrayIndex i = 0; i < uscLines.size(); ++i) {
    const Json::Value &uscLine = uscLines[i];
    if (!uscLine.isObject()) {
      continue;
    }

    Json::Value edgeAILine(Json::objectValue);

    // Copy id
    if (uscLine.isMember("id") && uscLine["id"].isString()) {
      edgeAILine["id"] = uscLine["id"];
    } else if (uscLine.isMember("entityUuid") &&
               uscLine["entityUuid"].isString()) {
      edgeAILine["id"] = uscLine["entityUuid"];
    }

    // Copy name
    if (uscLine.isMember("name") && uscLine["name"].isString()) {
      edgeAILine["name"] = uscLine["name"];
    } else if (uscLine.isMember("entityName") &&
               uscLine["entityName"].isString()) {
      edgeAILine["name"] = uscLine["entityName"];
    }

    // Convert coordinates
    if (uscLine.isMember("coordinates") && uscLine["coordinates"].isArray()) {
      edgeAILine["coordinates"] = uscLine["coordinates"];
    } else if (uscLine.isMember("coordinatesJson") &&
               uscLine["coordinatesJson"].isString()) {
      Json::Reader reader;
      Json::Value coords;
      if (reader.parse(uscLine["coordinatesJson"].asString(), coords) &&
          coords.isArray()) {
        edgeAILine["coordinates"] = coords;
      }
    }

    // Copy direction
    if (uscLine.isMember("direction") && uscLine["direction"].isString()) {
      edgeAILine["direction"] = uscLine["direction"];
    } else if (uscLine.isMember("attributesJson") &&
               uscLine["attributesJson"].isString()) {
      Json::Reader reader;
      Json::Value attrs;
      if (reader.parse(uscLine["attributesJson"].asString(), attrs) &&
          attrs.isObject() && attrs.isMember("direction") &&
          attrs["direction"].isString()) {
        edgeAILine["direction"] = attrs["direction"];
      }
    }

    // Copy classes
    if (uscLine.isMember("classes") && uscLine["classes"].isArray()) {
      edgeAILine["classes"] = uscLine["classes"];
    } else if (uscLine.isMember("classesJson") &&
               uscLine["classesJson"].isString()) {
      Json::Reader reader;
      Json::Value classes;
      if (reader.parse(uscLine["classesJson"].asString(), classes) &&
          classes.isArray()) {
        edgeAILine["classes"] = classes;
      }
    }

    // Copy color
    if (uscLine.isMember("color") && uscLine["color"].isArray()) {
      edgeAILine["color"] = uscLine["color"];
    } else if (uscLine.isMember("colorJson") &&
               uscLine["colorJson"].isString()) {
      Json::Reader reader;
      Json::Value color;
      if (reader.parse(uscLine["colorJson"].asString(), color) &&
          color.isArray()) {
        edgeAILine["color"] = color;
      }
    }

    // Copy enabled
    if (uscLine.isMember("enabled") && uscLine["enabled"].isBool()) {
      edgeAILine["enabled"] = uscLine["enabled"];
    } else {
      edgeAILine["enabled"] = true; // Default to enabled
    }

    edgeAILines.append(edgeAILine);
  }

  return edgeAILines;
}

Json::Value
RulesHandler::convertEdgeAILinesToUSC(const Json::Value &edgeAILines) const {
  Json::Value uscLines(Json::arrayValue);

  if (!edgeAILines.isArray()) {
    return uscLines;
  }

  for (Json::ArrayIndex i = 0; i < edgeAILines.size(); ++i) {
    const Json::Value &edgeAILine = edgeAILines[i];
    if (!edgeAILine.isObject()) {
      continue;
    }

    Json::Value uscLine(Json::objectValue);

    // Copy id as entityUuid
    if (edgeAILine.isMember("id") && edgeAILine["id"].isString()) {
      uscLine["entityUuid"] = edgeAILine["id"];
    }

    // Copy name as entityName
    if (edgeAILine.isMember("name") && edgeAILine["name"].isString()) {
      uscLine["entityName"] = edgeAILine["name"];
    }

    // Convert coordinates to coordinatesJson string
    if (edgeAILine.isMember("coordinates") &&
        edgeAILine["coordinates"].isArray()) {
      Json::StreamWriterBuilder builder;
      builder["indentation"] = "";
      uscLine["coordinatesJson"] =
          Json::writeString(builder, edgeAILine["coordinates"]);
    }

    // Convert direction to attributesJson
    Json::Value attrs(Json::objectValue);
    if (edgeAILine.isMember("direction") &&
        edgeAILine["direction"].isString()) {
      attrs["direction"] = edgeAILine["direction"];
    }
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    uscLine["attributesJson"] = Json::writeString(builder, attrs);

    // Convert classes to classesJson string
    if (edgeAILine.isMember("classes") && edgeAILine["classes"].isArray()) {
      Json::StreamWriterBuilder builder;
      builder["indentation"] = "";
      uscLine["classesJson"] =
          Json::writeString(builder, edgeAILine["classes"]);
    }

    // Convert color to colorJson string
    if (edgeAILine.isMember("color") && edgeAILine["color"].isArray()) {
      Json::StreamWriterBuilder builder;
      builder["indentation"] = "";
      uscLine["colorJson"] = Json::writeString(builder, edgeAILine["color"]);
    }

    // Copy enabled
    if (edgeAILine.isMember("enabled") && edgeAILine["enabled"].isBool()) {
      uscLine["enabled"] = edgeAILine["enabled"];
    } else {
      uscLine["enabled"] = true;
    }

    uscLines.append(uscLine);
  }

  return uscLines;
}

bool RulesHandler::applyLinesToInstance(const std::string &instanceId,
                                        const Json::Value &lines) const {
  // Apply lines to instance pipeline
  // Lines are already saved to config (RulesLines and CrossingLines)
  // The pipeline will load them on next start/restart
  // For runtime updates, we would need to access ba_crossline_node directly
  // which requires SDK API access (similar to LinesHandler::updateLinesRuntime)
  
  if (!instance_manager_) {
    return false;
  }

  auto optInfo = instance_manager_->getInstance(instanceId);
  if (!optInfo.has_value() || !optInfo.value().running) {
    // Instance not running - rules will be applied on next start
    return true;
  }

  // Rules are saved to config, pipeline will reload on restart
  // For now, return true - rules are saved correctly
  // Future enhancement: Add runtime update via SDK API if available
  if (isApiLoggingEnabled()) {
    PLOG_DEBUG << "[API] applyLinesToInstance: Rules saved to config for instance " << instanceId
               << ". Pipeline will reload on restart.";
  }
  
  return true;
}

void RulesHandler::getRules(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();

  std::string instanceId = extractInstanceId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] GET /v1/core/instance/" << instanceId
              << "/rules - Get all rules";
    PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
  }

  try {
    if (!instance_manager_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] GET /v1/core/instance/" << instanceId
                   << "/rules - Error: Instance manager not initialized";
      }
      callback(createErrorResponse(500, "Internal server error",
                                   "Instance manager not initialized"));
      return;
    }

    if (instanceId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] GET /v1/core/instance/{instanceId}/rules - "
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
                     << "/rules - Instance not found - " << duration.count()
                     << "ms";
      }
      callback(createErrorResponse(404, "Not found",
                                   "Instance not found: " + instanceId));
      return;
    }

    // Load rules from config
    Json::Value rules = loadRulesFromConfig(instanceId);

    // Check if includeDisabled query param is set
    bool includeDisabled = false;
    auto includeDisabledParam = req->getParameter("includeDisabled");
    if (!includeDisabledParam.empty() &&
        (includeDisabledParam == "true" || includeDisabledParam == "1")) {
      includeDisabled = true;
    }

    // Filter out disabled entities if includeDisabled is false
    if (!includeDisabled) {
      // Filter zones
      if (rules.isMember("zones") && rules["zones"].isArray()) {
        Json::Value filteredZones(Json::arrayValue);
        for (Json::ArrayIndex i = 0; i < rules["zones"].size(); ++i) {
          const Json::Value &zone = rules["zones"][i];
          if (zone.isObject()) {
            bool enabled = true;
            if (zone.isMember("enabled") && zone["enabled"].isBool()) {
              enabled = zone["enabled"].asBool();
            }
            if (enabled) {
              filteredZones.append(zone);
            }
          }
        }
        rules["zones"] = filteredZones;
      }

      // Filter lines
      if (rules.isMember("lines") && rules["lines"].isArray()) {
        Json::Value filteredLines(Json::arrayValue);
        for (Json::ArrayIndex i = 0; i < rules["lines"].size(); ++i) {
          const Json::Value &line = rules["lines"][i];
          if (line.isObject()) {
            bool enabled = true;
            if (line.isMember("enabled") && line["enabled"].isBool()) {
              enabled = line["enabled"].asBool();
            }
            if (enabled) {
              filteredLines.append(line);
            }
          }
        }
        rules["lines"] = filteredLines;
      }
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] GET /v1/core/instance/" << instanceId
                << "/rules - Success - " << duration.count() << "ms";
    }

    callback(createSuccessResponse(rules, 200));
  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] GET /v1/core/instance/" << instanceId
                 << "/rules - Exception: " << e.what() << " - "
                 << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void RulesHandler::setRules(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();

  std::string instanceId = extractInstanceId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] POST /v1/core/instance/" << instanceId
              << "/rules - Set/update rules";
    PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
  }

  try {
    if (!instance_manager_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] POST /v1/core/instance/" << instanceId
                   << "/rules - Error: Instance manager not initialized";
      }
      callback(createErrorResponse(500, "Internal server error",
                                   "Instance manager not initialized"));
      return;
    }

    if (instanceId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] POST /v1/core/instance/{instanceId}/rules - "
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
                     << "/rules - Instance not found - " << duration.count()
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
                     << "/rules - Error: Invalid JSON body";
      }
      callback(createErrorResponse(400, "Bad request",
                                   "Request body must be valid JSON"));
      return;
    }

    // Validate input
    Json::Value rulesToSet = *json;

    // Convert USC format to edge_ai_api format if needed
    if (rulesToSet.isMember("lines") && rulesToSet["lines"].isArray()) {
      rulesToSet["lines"] = convertUSCLinesToEdgeAI(rulesToSet["lines"]);
    }

    // Validate rules using RuleTypeValidator
    std::vector<std::string> validationErrors;
    
    // Validate zones
    if (rulesToSet.isMember("zones") && rulesToSet["zones"].isArray()) {
      for (Json::ArrayIndex i = 0; i < rulesToSet["zones"].size(); ++i) {
        const Json::Value &zone = rulesToSet["zones"][i];
        std::vector<std::string> zoneErrors;
        if (!RuleTypeValidator::validateZone(zone, zoneErrors)) {
          for (const auto &error : zoneErrors) {
            validationErrors.push_back("Zone[" + std::to_string(i) + "]: " + error);
          }
        }
      }
    }

    // Validate lines
    if (rulesToSet.isMember("lines") && rulesToSet["lines"].isArray()) {
      for (Json::ArrayIndex i = 0; i < rulesToSet["lines"].size(); ++i) {
        const Json::Value &line = rulesToSet["lines"][i];
        std::vector<std::string> lineErrors;
        if (!RuleTypeValidator::validateLine(line, lineErrors)) {
          for (const auto &error : lineErrors) {
            validationErrors.push_back("Line[" + std::to_string(i) + "]: " + error);
          }
        }
      }
    }

    // Return validation errors if any
    if (!validationErrors.empty()) {
      std::string errorMessage = "Validation failed:\n";
      for (const auto &error : validationErrors) {
        errorMessage += "  - " + error + "\n";
      }
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] POST /v1/core/instance/" << instanceId
                     << "/rules - Validation errors: " << errorMessage;
      }
      callback(createErrorResponse(400, "Bad request", errorMessage));
      return;
    }

    // Load existing rules
    Json::Value existingRules = loadRulesFromConfig(instanceId);

    // Merge rules (update if entityUuid/id matches, add new if not)
    if (rulesToSet.isMember("zones") && rulesToSet["zones"].isArray()) {
      Json::Value mergedZones = existingRules["zones"];
      if (!mergedZones.isArray()) {
        mergedZones = Json::Value(Json::arrayValue);
      }

      for (Json::ArrayIndex i = 0; i < rulesToSet["zones"].size(); ++i) {
        const Json::Value &newZone = rulesToSet["zones"][i];
        if (!newZone.isObject()) {
          continue;
        }

        std::string newId;
        if (newZone.isMember("id") && newZone["id"].isString()) {
          newId = newZone["id"].asString();
        } else if (newZone.isMember("entityUuid") &&
                   newZone["entityUuid"].isString()) {
          newId = newZone["entityUuid"].asString();
        }

        // Find existing zone with same id
        bool found = false;
        for (Json::ArrayIndex j = 0; j < mergedZones.size(); ++j) {
          Json::Value &existingZone = mergedZones[j];
          if (!existingZone.isObject()) {
            continue;
          }

          std::string existingId;
          if (existingZone.isMember("id") && existingZone["id"].isString()) {
            existingId = existingZone["id"].asString();
          } else if (existingZone.isMember("entityUuid") &&
                     existingZone["entityUuid"].isString()) {
            existingId = existingZone["entityUuid"].asString();
          }

          if (existingId == newId && !newId.empty()) {
            // Update existing zone
            existingZone = newZone;
            // Ensure id is set
            if (!existingZone.isMember("id")) {
              existingZone["id"] = newId;
            }
            found = true;
            break;
          }
        }

        if (!found && !newId.empty()) {
          // Add new zone
          Json::Value zoneToAdd = newZone;
          if (!zoneToAdd.isMember("id")) {
            zoneToAdd["id"] = newId;
          }
          mergedZones.append(zoneToAdd);
        }
      }

      rulesToSet["zones"] = mergedZones;
    }

    // Merge lines similarly
    if (rulesToSet.isMember("lines") && rulesToSet["lines"].isArray()) {
      Json::Value mergedLines = existingRules["lines"];
      if (!mergedLines.isArray()) {
        mergedLines = Json::Value(Json::arrayValue);
      }

      for (Json::ArrayIndex i = 0; i < rulesToSet["lines"].size(); ++i) {
        const Json::Value &newLine = rulesToSet["lines"][i];
        if (!newLine.isObject()) {
          continue;
        }

        std::string newId;
        if (newLine.isMember("id") && newLine["id"].isString()) {
          newId = newLine["id"].asString();
        } else if (newLine.isMember("entityUuid") &&
                   newLine["entityUuid"].isString()) {
          newId = newLine["entityUuid"].asString();
        }

        // Find existing line with same id
        bool found = false;
        for (Json::ArrayIndex j = 0; j < mergedLines.size(); ++j) {
          Json::Value &existingLine = mergedLines[j];
          if (!existingLine.isObject()) {
            continue;
          }

          std::string existingId;
          if (existingLine.isMember("id") && existingLine["id"].isString()) {
            existingId = existingLine["id"].asString();
          } else if (existingLine.isMember("entityUuid") &&
                     existingLine["entityUuid"].isString()) {
            existingId = existingLine["entityUuid"].asString();
          }

          if (existingId == newId && !newId.empty()) {
            // Update existing line
            existingLine = newLine;
            // Ensure id is set
            if (!existingLine.isMember("id")) {
              existingLine["id"] = newId;
            }
            found = true;
            break;
          }
        }

        if (!found && !newId.empty()) {
          // Add new line
          Json::Value lineToAdd = newLine;
          if (!lineToAdd.isMember("id")) {
            lineToAdd["id"] = newId;
          }
          mergedLines.append(lineToAdd);
        }
      }

      rulesToSet["lines"] = mergedLines;
    }

    // Merge metadata
    if (rulesToSet.isMember("metadata") && rulesToSet["metadata"].isObject()) {
      Json::Value mergedMetadata = existingRules["metadata"];
      if (!mergedMetadata.isObject()) {
        mergedMetadata = Json::Value(Json::objectValue);
      }

      for (const auto &key : rulesToSet["metadata"].getMemberNames()) {
        mergedMetadata[key] = rulesToSet["metadata"][key];
      }

      rulesToSet["metadata"] = mergedMetadata;
    } else {
      // Preserve existing metadata
      rulesToSet["metadata"] = existingRules["metadata"];
    }

    // Save rules to config
    if (!saveRulesToConfig(instanceId, rulesToSet)) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] POST /v1/core/instance/" << instanceId
                   << "/rules - Failed to save rules to config";
      }
      callback(createErrorResponse(500, "Internal server error",
                                   "Failed to save rules to config"));
      return;
    }

    // Process rules using RuleProcessor
    RuleProcessor::setInstanceManager(instance_manager_);
    
    // Process zones
    if (rulesToSet.isMember("zones") && rulesToSet["zones"].isArray() &&
        !rulesToSet["zones"].empty()) {
      if (!RuleProcessor::processZones(instanceId, rulesToSet["zones"])) {
        if (isApiLoggingEnabled()) {
          PLOG_WARNING << "[API] POST /v1/core/instance/" << instanceId
                       << "/rules - Some zones failed to process";
        }
        // Continue anyway - rules are saved, processing is best-effort
      }
    }

    // Process lines
    if (rulesToSet.isMember("lines") && rulesToSet["lines"].isArray() &&
        !rulesToSet["lines"].empty()) {
      if (!RuleProcessor::processLines(instanceId, rulesToSet["lines"])) {
        if (isApiLoggingEnabled()) {
          PLOG_WARNING << "[API] POST /v1/core/instance/" << instanceId
                       << "/rules - Some lines failed to process";
        }
        // Continue anyway - rules are saved, processing is best-effort
      }
    }

    // Apply lines to instance if instance is running (legacy support)
    if (optInfo.value().running && rulesToSet.isMember("lines") &&
        rulesToSet["lines"].isArray() && !rulesToSet["lines"].empty()) {
      applyLinesToInstance(instanceId, rulesToSet["lines"]);
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] POST /v1/core/instance/" << instanceId
                << "/rules - Success - " << duration.count() << "ms";
    }

    // Return saved rules
    Json::Value savedRules = loadRulesFromConfig(instanceId);
    callback(createSuccessResponse(savedRules, 200));
  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] POST /v1/core/instance/" << instanceId
                 << "/rules - Exception: " << e.what() << " - "
                 << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error",
                                  "Unknown error occurred"));
  }
}

void RulesHandler::updateRules(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();

  std::string instanceId = extractInstanceId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] PUT /v1/core/instance/" << instanceId
              << "/rules - Replace rules";
    PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
  }

  try {
    if (!instance_manager_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] PUT /v1/core/instance/" << instanceId
                   << "/rules - Error: Instance manager not initialized";
      }
      callback(createErrorResponse(500, "Internal server error",
                                   "Instance manager not initialized"));
      return;
    }

    if (instanceId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] PUT /v1/core/instance/{instanceId}/rules - "
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
        PLOG_WARNING << "[API] PUT /v1/core/instance/" << instanceId
                     << "/rules - Instance not found - " << duration.count()
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
        PLOG_WARNING << "[API] PUT /v1/core/instance/" << instanceId
                     << "/rules - Error: Invalid JSON body";
      }
      callback(createErrorResponse(400, "Bad request",
                                   "Request body must be valid JSON"));
      return;
    }

    // Replace entire rules (no merge)
    Json::Value rulesToSet = *json;

    // Convert USC format to edge_ai_api format if needed
    if (rulesToSet.isMember("lines") && rulesToSet["lines"].isArray()) {
      rulesToSet["lines"] = convertUSCLinesToEdgeAI(rulesToSet["lines"]);
    }

    // Ensure zones and lines are arrays
    if (!rulesToSet.isMember("zones") || !rulesToSet["zones"].isArray()) {
      rulesToSet["zones"] = Json::Value(Json::arrayValue);
    }
    if (!rulesToSet.isMember("lines") || !rulesToSet["lines"].isArray()) {
      rulesToSet["lines"] = Json::Value(Json::arrayValue);
    }
    if (!rulesToSet.isMember("metadata") || !rulesToSet["metadata"].isObject()) {
      rulesToSet["metadata"] = Json::Value(Json::objectValue);
    }

    // Save rules to config
    if (!saveRulesToConfig(instanceId, rulesToSet)) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] PUT /v1/core/instance/" << instanceId
                   << "/rules - Failed to save rules to config";
      }
      callback(createErrorResponse(500, "Internal server error",
                                   "Failed to save rules to config"));
      return;
    }

    // Apply lines to instance if instance is running
    if (optInfo.value().running && rulesToSet.isMember("lines") &&
        rulesToSet["lines"].isArray() && !rulesToSet["lines"].empty()) {
      applyLinesToInstance(instanceId, rulesToSet["lines"]);
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] PUT /v1/core/instance/" << instanceId
                << "/rules - Success - " << duration.count() << "ms";
    }

    // Return saved rules
    Json::Value savedRules = loadRulesFromConfig(instanceId);
    callback(createSuccessResponse(savedRules, 200));
  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] PUT /v1/core/instance/" << instanceId
                 << "/rules - Exception: " << e.what() << " - "
                 << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void RulesHandler::deleteRules(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();

  std::string instanceId = extractInstanceId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] DELETE /v1/core/instance/" << instanceId
              << "/rules - Delete all rules";
    PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
  }

  try {
    if (!instance_manager_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] DELETE /v1/core/instance/" << instanceId
                   << "/rules - Error: Instance manager not initialized";
      }
      callback(createErrorResponse(500, "Internal server error",
                                   "Instance manager not initialized"));
      return;
    }

    if (instanceId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] DELETE /v1/core/instance/{instanceId}/rules - "
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
                     << "/rules - Instance not found - " << duration.count()
                     << "ms";
      }
      callback(createErrorResponse(404, "Not found",
                                   "Instance not found: " + instanceId));
      return;
    }

    // Clear rules (set empty arrays)
    Json::Value emptyRules(Json::objectValue);
    emptyRules["zones"] = Json::Value(Json::arrayValue);
    emptyRules["lines"] = Json::Value(Json::arrayValue);
    emptyRules["metadata"] = Json::Value(Json::objectValue);

    // Save empty rules to config
    if (!saveRulesToConfig(instanceId, emptyRules)) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] DELETE /v1/core/instance/" << instanceId
                   << "/rules - Failed to delete rules from config";
      }
      callback(createErrorResponse(500, "Internal server error",
                                   "Failed to delete rules from config"));
      return;
    }

    // Remove lines from instance if instance is running
    if (optInfo.value().running) {
      // Clear lines in instance (similar to LinesHandler::deleteAllLines)
      // This would need to be implemented based on how lines are applied
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] DELETE /v1/core/instance/" << instanceId
                << "/rules - Success - " << duration.count() << "ms";
    }

    callback(createSuccessResponse(Json::Value(Json::objectValue), 204));
  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] DELETE /v1/core/instance/" << instanceId
                 << "/rules - Exception: " << e.what() << " - "
                 << duration.count() << "ms";
    }
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void RulesHandler::handleOptions(
    const HttpRequestPtr & /* req */,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto resp = HttpResponse::newHttpResponse();
  resp->setStatusCode(k204NoContent);
  resp->addHeader("Access-Control-Allow-Origin", "*");
  resp->addHeader("Access-Control-Allow-Methods",
                  "GET, POST, PUT, DELETE, OPTIONS, PATCH");
  resp->addHeader("Access-Control-Allow-Headers",
                  "Content-Type, Authorization");
  resp->addHeader("Access-Control-Max-Age", "3600");
  callback(resp);
}

Json::Value RulesHandler::findEntityByUuid(const Json::Value &rules,
                                            const std::string &entityUuid) const {
  // Search in zones
  if (rules.isMember("zones") && rules["zones"].isArray()) {
    for (Json::ArrayIndex i = 0; i < rules["zones"].size(); ++i) {
      const Json::Value &zone = rules["zones"][i];
      if (!zone.isObject()) {
        continue;
      }
      std::string id;
      if (zone.isMember("id") && zone["id"].isString()) {
        id = zone["id"].asString();
      } else if (zone.isMember("entityUuid") && zone["entityUuid"].isString()) {
        id = zone["entityUuid"].asString();
      }
      if (id == entityUuid) {
        return zone;
      }
    }
  }

  // Search in lines
  if (rules.isMember("lines") && rules["lines"].isArray()) {
    for (Json::ArrayIndex i = 0; i < rules["lines"].size(); ++i) {
      const Json::Value &line = rules["lines"][i];
      if (!line.isObject()) {
        continue;
      }
      std::string id;
      if (line.isMember("id") && line["id"].isString()) {
        id = line["id"].asString();
      } else if (line.isMember("entityUuid") && line["entityUuid"].isString()) {
        id = line["entityUuid"].asString();
      }
      if (id == entityUuid) {
        return line;
      }
    }
  }

  return Json::Value(); // Return null JSON value if not found
}

std::string RulesHandler::getEntityType(const Json::Value &rules,
                                         const std::string &entityUuid) const {
  // Check in zones
  if (rules.isMember("zones") && rules["zones"].isArray()) {
    for (Json::ArrayIndex i = 0; i < rules["zones"].size(); ++i) {
      const Json::Value &zone = rules["zones"][i];
      if (!zone.isObject()) {
        continue;
      }
      std::string id;
      if (zone.isMember("id") && zone["id"].isString()) {
        id = zone["id"].asString();
      } else if (zone.isMember("entityUuid") && zone["entityUuid"].isString()) {
        id = zone["entityUuid"].asString();
      }
      if (id == entityUuid) {
        return "zone";
      }
    }
  }

  // Check in lines
  if (rules.isMember("lines") && rules["lines"].isArray()) {
    for (Json::ArrayIndex i = 0; i < rules["lines"].size(); ++i) {
      const Json::Value &line = rules["lines"][i];
      if (!line.isObject()) {
        continue;
      }
      std::string id;
      if (line.isMember("id") && line["id"].isString()) {
        id = line["id"].asString();
      } else if (line.isMember("entityUuid") && line["entityUuid"].isString()) {
        id = line["entityUuid"].asString();
      }
      if (id == entityUuid) {
        return "line";
      }
    }
  }

  return ""; // Return empty string if not found
}

bool RulesHandler::removeEntityByUuid(Json::Value &rules,
                                      const std::string &entityUuid) const {
  // Remove from zones
  if (rules.isMember("zones") && rules["zones"].isArray()) {
    Json::Value newZones(Json::arrayValue);
    bool found = false;
    for (Json::ArrayIndex i = 0; i < rules["zones"].size(); ++i) {
      const Json::Value &zone = rules["zones"][i];
      if (!zone.isObject()) {
        newZones.append(zone);
        continue;
      }
      std::string id;
      if (zone.isMember("id") && zone["id"].isString()) {
        id = zone["id"].asString();
      } else if (zone.isMember("entityUuid") && zone["entityUuid"].isString()) {
        id = zone["entityUuid"].asString();
      }
      if (id != entityUuid) {
        newZones.append(zone);
      } else {
        found = true;
      }
    }
    if (found) {
      rules["zones"] = newZones;
      return true;
    }
  }

  // Remove from lines
  if (rules.isMember("lines") && rules["lines"].isArray()) {
    Json::Value newLines(Json::arrayValue);
    bool found = false;
    for (Json::ArrayIndex i = 0; i < rules["lines"].size(); ++i) {
      const Json::Value &line = rules["lines"][i];
      if (!line.isObject()) {
        newLines.append(line);
        continue;
      }
      std::string id;
      if (line.isMember("id") && line["id"].isString()) {
        id = line["id"].asString();
      } else if (line.isMember("entityUuid") && line["entityUuid"].isString()) {
        id = line["entityUuid"].asString();
      }
      if (id != entityUuid) {
        newLines.append(line);
      } else {
        found = true;
      }
    }
    if (found) {
      rules["lines"] = newLines;
      return true;
    }
  }

  return false;
}

bool RulesHandler::updateEntityByUuid(Json::Value &rules,
                                      const std::string &entityUuid,
                                      const Json::Value &newEntity) const {
  // Update in zones
  if (rules.isMember("zones") && rules["zones"].isArray()) {
    for (Json::ArrayIndex i = 0; i < rules["zones"].size(); ++i) {
      Json::Value &zone = rules["zones"][i];
      if (!zone.isObject()) {
        continue;
      }
      std::string id;
      if (zone.isMember("id") && zone["id"].isString()) {
        id = zone["id"].asString();
      } else if (zone.isMember("entityUuid") && zone["entityUuid"].isString()) {
        id = zone["entityUuid"].asString();
      }
      if (id == entityUuid) {
        // Update entity, ensure id is preserved
        zone = newEntity;
        if (!zone.isMember("id")) {
          zone["id"] = entityUuid;
        }
        return true;
      }
    }
  }

  // Update in lines
  if (rules.isMember("lines") && rules["lines"].isArray()) {
    for (Json::ArrayIndex i = 0; i < rules["lines"].size(); ++i) {
      Json::Value &line = rules["lines"][i];
      if (!line.isObject()) {
        continue;
      }
      std::string id;
      if (line.isMember("id") && line["id"].isString()) {
        id = line["id"].asString();
      } else if (line.isMember("entityUuid") && line["entityUuid"].isString()) {
        id = line["entityUuid"].asString();
      }
      if (id == entityUuid) {
        // Update entity, ensure id is preserved
        line = newEntity;
        if (!line.isMember("id")) {
          line["id"] = entityUuid;
        }
        return true;
      }
    }
  }

  return false;
}

void RulesHandler::getEntity(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();

  std::string instanceId = extractInstanceId(req);
  std::string entityUuid = req->getParameter("entityUuid");

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] GET /v1/core/instance/" << instanceId
              << "/rules/entities/" << entityUuid;
  }

  try {
    if (!instance_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Instance manager not initialized"));
      return;
    }

    if (instanceId.empty() || entityUuid.empty()) {
      callback(createErrorResponse(400, "Bad request",
                                   "Instance ID and entity UUID are required"));
      return;
    }

    // Check if instance exists
    auto optInfo = instance_manager_->getInstance(instanceId);
    if (!optInfo.has_value()) {
      callback(createErrorResponse(404, "Not found",
                                   "Instance not found: " + instanceId));
      return;
    }

    // Load rules
    Json::Value rules = loadRulesFromConfig(instanceId);

    // Find entity
    Json::Value entity = findEntityByUuid(rules, entityUuid);
    if (entity.isNull()) {
      callback(createErrorResponse(404, "Not found",
                                   "Entity not found: " + entityUuid));
      return;
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] GET /v1/core/instance/" << instanceId
                << "/rules/entities/" << entityUuid << " - Success - "
                << duration.count() << "ms";
    }

    callback(createSuccessResponse(entity, 200));
  } catch (const std::exception &e) {
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] GET /v1/core/instance/" << instanceId
                 << "/rules/entities/" << entityUuid
                 << " - Exception: " << e.what();
    }
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void RulesHandler::createEntity(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();

  std::string instanceId = extractInstanceId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] POST /v1/core/instance/" << instanceId
              << "/rules/entities - Create entity";
  }

  try {
    if (!instance_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Instance manager not initialized"));
      return;
    }

    if (instanceId.empty()) {
      callback(createErrorResponse(400, "Bad request",
                                   "Instance ID is required"));
      return;
    }

    // Check if instance exists
    auto optInfo = instance_manager_->getInstance(instanceId);
    if (!optInfo.has_value()) {
      callback(createErrorResponse(404, "Not found",
                                   "Instance not found: " + instanceId));
      return;
    }

    // Parse JSON body
    auto json = req->getJsonObject();
    if (!json) {
      callback(createErrorResponse(400, "Bad request",
                                   "Request body must be valid JSON"));
      return;
    }

    Json::Value newEntity = *json;

    // Determine if entity is zone or line (check for type field or coordinates length)
    bool isZone = false;
    if (newEntity.isMember("type") && newEntity["type"].isString()) {
      std::string type = newEntity["type"].asString();
      // Zone types typically end with "Areas" or are "stopZone", "jamZone"
      if (type.find("Areas") != std::string::npos || type == "stopZone" ||
          type == "jamZone") {
        isZone = true;
      }
    } else if (newEntity.isMember("coordinates") &&
               newEntity["coordinates"].isArray()) {
      // If coordinates has more than 2 points, it's likely a zone (polygon)
      // If coordinates has exactly 2 points, it's likely a line
      if (newEntity["coordinates"].size() > 2) {
        isZone = true;
      }
    } else {
      // Default: assume it's a line if no clear indication
      isZone = false;
    }

    // Ensure entity has an id
    std::string entityUuid;
    if (newEntity.isMember("id") && newEntity["id"].isString()) {
      entityUuid = newEntity["id"].asString();
    } else if (newEntity.isMember("entityUuid") &&
               newEntity["entityUuid"].isString()) {
      entityUuid = newEntity["entityUuid"].asString();
      newEntity["id"] = entityUuid;
    } else {
      // Generate UUID if not provided
      // For simplicity, use a simple UUID generator or require client to provide
      callback(createErrorResponse(400, "Bad request",
                                   "Entity must have 'id' or 'entityUuid' field"));
      return;
    }

    // Validate entity
    std::vector<std::string> validationErrors;
    if (isZone) {
      if (!RuleTypeValidator::validateZone(newEntity, validationErrors)) {
        std::string errorMessage = "Validation failed:\n";
        for (const auto &error : validationErrors) {
          errorMessage += "  - " + error + "\n";
        }
        callback(createErrorResponse(400, "Bad request", errorMessage));
        return;
      }
    } else {
      if (!RuleTypeValidator::validateLine(newEntity, validationErrors)) {
        std::string errorMessage = "Validation failed:\n";
        for (const auto &error : validationErrors) {
          errorMessage += "  - " + error + "\n";
        }
        callback(createErrorResponse(400, "Bad request", errorMessage));
        return;
      }
    }

    // Load existing rules
    Json::Value rules = loadRulesFromConfig(instanceId);

    // Check if entity with same UUID already exists
    Json::Value existingEntity = findEntityByUuid(rules, entityUuid);
    if (!existingEntity.isNull()) {
      callback(createErrorResponse(409, "Conflict",
                                   "Entity with UUID " + entityUuid +
                                       " already exists"));
      return;
    }

    // Add entity to appropriate array
    if (isZone) {
      if (!rules.isMember("zones") || !rules["zones"].isArray()) {
        rules["zones"] = Json::Value(Json::arrayValue);
      }
      rules["zones"].append(newEntity);
    } else {
      if (!rules.isMember("lines") || !rules["lines"].isArray()) {
        rules["lines"] = Json::Value(Json::arrayValue);
      }
      rules["lines"].append(newEntity);
    }

    // Save rules
    if (!saveRulesToConfig(instanceId, rules)) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Failed to save rules to config"));
      return;
    }

    // Process entity if instance is running
    if (optInfo.value().running) {
      if (isZone) {
        RuleProcessor::processZone(instanceId, newEntity);
      } else {
        RuleProcessor::processLine(instanceId, newEntity);
        applyLinesToInstance(instanceId, rules["lines"]);
      }
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] POST /v1/core/instance/" << instanceId
                << "/rules/entities - Success - " << duration.count() << "ms";
    }

    callback(createSuccessResponse(newEntity, 201));
  } catch (const std::exception &e) {
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] POST /v1/core/instance/" << instanceId
                 << "/rules/entities - Exception: " << e.what();
    }
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void RulesHandler::updateEntity(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();

  std::string instanceId = extractInstanceId(req);
  std::string entityUuid = req->getParameter("entityUuid");

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] PUT /v1/core/instance/" << instanceId
              << "/rules/entities/" << entityUuid << " - Update entity";
  }

  try {
    if (!instance_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Instance manager not initialized"));
      return;
    }

    if (instanceId.empty() || entityUuid.empty()) {
      callback(createErrorResponse(400, "Bad request",
                                   "Instance ID and entity UUID are required"));
      return;
    }

    // Check if instance exists
    auto optInfo = instance_manager_->getInstance(instanceId);
    if (!optInfo.has_value()) {
      callback(createErrorResponse(404, "Not found",
                                   "Instance not found: " + instanceId));
      return;
    }

    // Parse JSON body
    auto json = req->getJsonObject();
    if (!json) {
      callback(createErrorResponse(400, "Bad request",
                                   "Request body must be valid JSON"));
      return;
    }

    Json::Value updatedEntity = *json;

    // Ensure entity UUID matches
    if (updatedEntity.isMember("id") &&
        updatedEntity["id"].asString() != entityUuid) {
      callback(createErrorResponse(400, "Bad request",
                                   "Entity UUID in body does not match path"));
      return;
    }
    updatedEntity["id"] = entityUuid;

    // Load existing rules
    Json::Value rules = loadRulesFromConfig(instanceId);

    // Check if entity exists
    Json::Value existingEntity = findEntityByUuid(rules, entityUuid);
    if (existingEntity.isNull()) {
      callback(createErrorResponse(404, "Not found",
                                   "Entity not found: " + entityUuid));
      return;
    }

    // Determine entity type
    std::string entityType = getEntityType(rules, entityUuid);
    bool isZone = (entityType == "zone");

    // Validate updated entity
    std::vector<std::string> validationErrors;
    if (isZone) {
      if (!RuleTypeValidator::validateZone(updatedEntity, validationErrors)) {
        std::string errorMessage = "Validation failed:\n";
        for (const auto &error : validationErrors) {
          errorMessage += "  - " + error + "\n";
        }
        callback(createErrorResponse(400, "Bad request", errorMessage));
        return;
      }
    } else {
      if (!RuleTypeValidator::validateLine(updatedEntity, validationErrors)) {
        std::string errorMessage = "Validation failed:\n";
        for (const auto &error : validationErrors) {
          errorMessage += "  - " + error + "\n";
        }
        callback(createErrorResponse(400, "Bad request", errorMessage));
        return;
      }
    }

    // Update entity
    if (!updateEntityByUuid(rules, entityUuid, updatedEntity)) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Failed to update entity"));
      return;
    }

    // Save rules
    if (!saveRulesToConfig(instanceId, rules)) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Failed to save rules to config"));
      return;
    }

    // Process entity if instance is running
    if (optInfo.value().running) {
      if (isZone) {
        RuleProcessor::processZone(instanceId, updatedEntity);
      } else {
        RuleProcessor::processLine(instanceId, updatedEntity);
        applyLinesToInstance(instanceId, rules["lines"]);
      }
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] PUT /v1/core/instance/" << instanceId
                << "/rules/entities/" << entityUuid << " - Success - "
                << duration.count() << "ms";
    }

    callback(createSuccessResponse(updatedEntity, 200));
  } catch (const std::exception &e) {
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] PUT /v1/core/instance/" << instanceId
                 << "/rules/entities/" << entityUuid
                 << " - Exception: " << e.what();
    }
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void RulesHandler::deleteEntity(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();

  std::string instanceId = extractInstanceId(req);
  std::string entityUuid = req->getParameter("entityUuid");

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] DELETE /v1/core/instance/" << instanceId
              << "/rules/entities/" << entityUuid << " - Delete entity";
  }

  try {
    if (!instance_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Instance manager not initialized"));
      return;
    }

    if (instanceId.empty() || entityUuid.empty()) {
      callback(createErrorResponse(400, "Bad request",
                                   "Instance ID and entity UUID are required"));
      return;
    }

    // Check if instance exists
    auto optInfo = instance_manager_->getInstance(instanceId);
    if (!optInfo.has_value()) {
      callback(createErrorResponse(404, "Not found",
                                   "Instance not found: " + instanceId));
      return;
    }

    // Load existing rules
    Json::Value rules = loadRulesFromConfig(instanceId);

    // Check if entity exists
    Json::Value existingEntity = findEntityByUuid(rules, entityUuid);
    if (existingEntity.isNull()) {
      callback(createErrorResponse(404, "Not found",
                                   "Entity not found: " + entityUuid));
      return;
    }

    // Determine entity type
    std::string entityType = getEntityType(rules, entityUuid);

    // Remove entity
    if (!removeEntityByUuid(rules, entityUuid)) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Failed to remove entity"));
      return;
    }

    // Save rules
    if (!saveRulesToConfig(instanceId, rules)) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Failed to save rules to config"));
      return;
    }

    // If instance is running and entity was a line, update lines
    if (optInfo.value().running && entityType == "line") {
      applyLinesToInstance(instanceId, rules["lines"]);
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] DELETE /v1/core/instance/" << instanceId
                << "/rules/entities/" << entityUuid << " - Success - "
                << duration.count() << "ms";
    }

    callback(createSuccessResponse(Json::Value(Json::objectValue), 204));
  } catch (const std::exception &e) {
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] DELETE /v1/core/instance/" << instanceId
                 << "/rules/entities/" << entityUuid
                 << " - Exception: " << e.what();
    }
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void RulesHandler::toggleEntityEnabled(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto start_time = std::chrono::steady_clock::now();

  std::string instanceId = extractInstanceId(req);
  std::string entityUuid = req->getParameter("entityUuid");

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] PATCH /v1/core/instance/" << instanceId
              << "/rules/entities/" << entityUuid << "/enable";
  }

  try {
    if (!instance_manager_) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Instance manager not initialized"));
      return;
    }

    if (instanceId.empty() || entityUuid.empty()) {
      callback(createErrorResponse(400, "Bad request",
                                   "Instance ID and entity UUID are required"));
      return;
    }

    // Check if instance exists
    auto optInfo = instance_manager_->getInstance(instanceId);
    if (!optInfo.has_value()) {
      callback(createErrorResponse(404, "Not found",
                                   "Instance not found: " + instanceId));
      return;
    }

    // Parse JSON body to get enabled status
    auto json = req->getJsonObject();
    bool enabled = true; // Default to enabled
    if (json && json->isMember("enabled") && (*json)["enabled"].isBool()) {
      enabled = (*json)["enabled"].asBool();
    } else if (json && json->isMember("enabled") &&
               (*json)["enabled"].isString()) {
      std::string enabledStr = (*json)["enabled"].asString();
      enabled = (enabledStr == "true" || enabledStr == "1");
    }

    // Load existing rules
    Json::Value rules = loadRulesFromConfig(instanceId);

    // Find and update entity
    Json::Value entity = findEntityByUuid(rules, entityUuid);
    if (entity.isNull()) {
      callback(createErrorResponse(404, "Not found",
                                   "Entity not found: " + entityUuid));
      return;
    }

    // Update enabled status
    entity["enabled"] = enabled;

    // Update entity in rules
    if (!updateEntityByUuid(rules, entityUuid, entity)) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Failed to update entity"));
      return;
    }

    // Save rules
    if (!saveRulesToConfig(instanceId, rules)) {
      callback(createErrorResponse(500, "Internal server error",
                                   "Failed to save rules to config"));
      return;
    }

    // Process entity if instance is running
    std::string entityType = getEntityType(rules, entityUuid);
    if (optInfo.value().running) {
      if (entityType == "zone") {
        RuleProcessor::processZone(instanceId, entity);
      } else if (entityType == "line") {
        RuleProcessor::processLine(instanceId, entity);
        applyLinesToInstance(instanceId, rules["lines"]);
      }
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] PATCH /v1/core/instance/" << instanceId
                << "/rules/entities/" << entityUuid << "/enable - Success - "
                << duration.count() << "ms";
    }

    callback(createSuccessResponse(entity, 200));
  } catch (const std::exception &e) {
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] PATCH /v1/core/instance/" << instanceId
                 << "/rules/entities/" << entityUuid
                 << "/enable - Exception: " << e.what();
    }
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

