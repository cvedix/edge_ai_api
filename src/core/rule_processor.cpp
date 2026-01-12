#include "core/rule_processor.h"
#include "core/rule_type_validator.h"
#include "instances/instance_manager.h"
#include "core/logger.h"
#include "core/logging_flags.h"
#include <json/writer.h>
#include <sstream>
#include <map>

IInstanceManager *RuleProcessor::instance_manager_ = nullptr;

void RuleProcessor::setInstanceManager(IInstanceManager *manager) {
  instance_manager_ = manager;
}

bool RuleProcessor::processZones(const std::string &instanceId, const Json::Value &zones) {
  if (!zones.isArray()) {
    return false;
  }

  bool allSuccess = true;
  for (Json::ArrayIndex i = 0; i < zones.size(); ++i) {
    if (!processZone(instanceId, zones[i])) {
      allSuccess = false;
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[RuleProcessor] Failed to process zone at index " << i;
      }
    }
  }

  return allSuccess;
}

bool RuleProcessor::processLines(const std::string &instanceId, const Json::Value &lines) {
  if (!lines.isArray()) {
    return false;
  }

  bool allSuccess = true;
  for (Json::ArrayIndex i = 0; i < lines.size(); ++i) {
    if (!processLine(instanceId, lines[i])) {
      allSuccess = false;
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[RuleProcessor] Failed to process line at index " << i;
      }
    }
  }

  return allSuccess;
}

bool RuleProcessor::processZone(const std::string &instanceId, const Json::Value &zone) {
  if (!zone.isObject()) {
    return false;
  }

  std::string ruleType = "intrusionAreas"; // Default
  if (zone.isMember("type") && zone["type"].isString()) {
    ruleType = zone["type"].asString();
  }

  // Route to specific processor based on type
  if (ruleType == "intrusionAreas") {
    return processIntrusionArea(instanceId, zone);
  } else if (ruleType == "loiteringAreas") {
    return processLoiteringArea(instanceId, zone);
  } else if (ruleType == "dwellingAreas") {
    return processDwellingArea(instanceId, zone);
  } else if (ruleType == "crowdingAreas") {
    return processCrowdingArea(instanceId, zone);
  } else if (ruleType == "stopZone") {
    return processStopZone(instanceId, zone);
  } else if (ruleType == "jamZone") {
    return processJamZone(instanceId, zone);
  } else {
    // Generic zone processor for other types
    return processGenericZone(instanceId, zone);
  }
}

bool RuleProcessor::processLine(const std::string &instanceId, const Json::Value &line) {
  if (!line.isObject()) {
    return false;
  }

  // Lines typically don't have a "type" field, but we can infer from context
  // For now, all lines are processed as crossing lines (ba_crossline_node)
  // In the future, we can add type field or infer from instance solution type
  return processCrossingLine(instanceId, line);
}

std::map<std::string, std::string>
RuleProcessor::convertRulesToAdditionalParams(const Json::Value &zones,
                                              const Json::Value &lines) {
  std::map<std::string, std::string> additionalParams;

  // Convert zones to RulesZones
  if (zones.isArray() && !zones.empty()) {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::string zonesJson = Json::writeString(builder, zones);
    additionalParams["RulesZones"] = zonesJson;
  }

  // Convert lines to RulesLines (or CrossingLines for backward compatibility)
  if (lines.isArray() && !lines.empty()) {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::string linesJson = Json::writeString(builder, lines);
    additionalParams["RulesLines"] = linesJson;
    // Also set CrossingLines for backward compatibility with ba_crossline_node
    additionalParams["CrossingLines"] = linesJson;
  }

  return additionalParams;
}

// Zone type processors
bool RuleProcessor::processIntrusionArea(const std::string &instanceId,
                                         const Json::Value &zone) {
  // Intrusion areas are generic zones - just save to config
  // Pipeline nodes will process them based on zone type
  if (isApiLoggingEnabled()) {
    PLOG_DEBUG << "[RuleProcessor] Processing intrusion area for instance " << instanceId;
  }
  return true; // Success - rules are saved to config, pipeline will process
}

bool RuleProcessor::processLoiteringArea(const std::string &instanceId,
                                         const Json::Value &zone) {
  // Loitering areas require duration parameter
  // Pipeline nodes will use this for loitering detection
  if (isApiLoggingEnabled()) {
    PLOG_DEBUG << "[RuleProcessor] Processing loitering area for instance " << instanceId;
    if (zone.isMember("duration")) {
      PLOG_DEBUG << "[RuleProcessor] Loitering duration: " << zone["duration"].asDouble()
                 << " seconds";
    }
  }
  return true;
}

bool RuleProcessor::processDwellingArea(const std::string &instanceId,
                                        const Json::Value &zone) {
  // Dwelling areas require minDuration parameter
  if (isApiLoggingEnabled()) {
    PLOG_DEBUG << "[RuleProcessor] Processing dwelling area for instance " << instanceId;
    if (zone.isMember("minDuration")) {
      PLOG_DEBUG << "[RuleProcessor] Dwelling minDuration: " << zone["minDuration"].asDouble()
                 << " seconds";
    }
  }
  return true;
}

bool RuleProcessor::processCrowdingArea(const std::string &instanceId,
                                        const Json::Value &zone) {
  // Crowding areas require threshold parameter
  if (isApiLoggingEnabled()) {
    PLOG_DEBUG << "[RuleProcessor] Processing crowding area for instance " << instanceId;
    if (zone.isMember("threshold")) {
      PLOG_DEBUG << "[RuleProcessor] Crowding threshold: " << zone["threshold"].asInt()
                 << " people";
    }
  }
  return true;
}

bool RuleProcessor::processStopZone(const std::string &instanceId, const Json::Value &zone) {
  // Stop zones have specific parameters: min_stop_seconds, check_interval_frames, etc.
  if (isApiLoggingEnabled()) {
    PLOG_DEBUG << "[RuleProcessor] Processing stop zone for instance " << instanceId;
    if (zone.isMember("min_stop_seconds")) {
      PLOG_DEBUG << "[RuleProcessor] Stop zone min_stop_seconds: "
                 << zone["min_stop_seconds"].asDouble();
    }
  }
  return true;
}

bool RuleProcessor::processJamZone(const std::string &instanceId, const Json::Value &zone) {
  // Jam zones have specific parameters: threshold, duration_seconds
  if (isApiLoggingEnabled()) {
    PLOG_DEBUG << "[RuleProcessor] Processing jam zone for instance " << instanceId;
    if (zone.isMember("threshold")) {
      PLOG_DEBUG << "[RuleProcessor] Jam zone threshold: " << zone["threshold"].asInt()
                 << " objects";
    }
    if (zone.isMember("duration_seconds")) {
      PLOG_DEBUG << "[RuleProcessor] Jam zone duration: " << zone["duration_seconds"].asDouble()
                 << " seconds";
    }
  }
  return true;
}

bool RuleProcessor::processGenericZone(const std::string &instanceId, const Json::Value &zone) {
  // Generic processor for other zone types
  if (isApiLoggingEnabled()) {
    std::string ruleType = "unknown";
    if (zone.isMember("type") && zone["type"].isString()) {
      ruleType = zone["type"].asString();
    }
    PLOG_DEBUG << "[RuleProcessor] Processing generic zone type '" << ruleType
               << "' for instance " << instanceId;
  }
  return true;
}

// Line type processors
bool RuleProcessor::processCrossingLine(const std::string &instanceId, const Json::Value &line) {
  // Crossing lines are processed by ba_crossline_node
  // Lines are already saved to CrossingLines in additionalParams
  if (isApiLoggingEnabled()) {
    PLOG_DEBUG << "[RuleProcessor] Processing crossing line for instance " << instanceId;
  }
  return true;
}

bool RuleProcessor::processCountingLine(const std::string &instanceId, const Json::Value &line) {
  // Counting lines are similar to crossing lines but with counting logic
  // For now, process same as crossing lines
  if (isApiLoggingEnabled()) {
    PLOG_DEBUG << "[RuleProcessor] Processing counting line for instance " << instanceId;
  }
  return processCrossingLine(instanceId, line);
}

bool RuleProcessor::processTailgatingLine(const std::string &instanceId,
                                          const Json::Value &line) {
  // Tailgating lines require alarmAfterSeconds parameter
  if (isApiLoggingEnabled()) {
    PLOG_DEBUG << "[RuleProcessor] Processing tailgating line for instance " << instanceId;
    if (line.isMember("alarmAfterSeconds")) {
      PLOG_DEBUG << "[RuleProcessor] Tailgating alarmAfterSeconds: "
                 << line["alarmAfterSeconds"].asDouble() << " seconds";
    }
  }
  return processCrossingLine(instanceId, line);
}

bool RuleProcessor::processGenericLine(const std::string &instanceId, const Json::Value &line) {
  // Generic processor for other line types
  if (isApiLoggingEnabled()) {
    PLOG_DEBUG << "[RuleProcessor] Processing generic line for instance " << instanceId;
  }
  return processCrossingLine(instanceId, line);
}

