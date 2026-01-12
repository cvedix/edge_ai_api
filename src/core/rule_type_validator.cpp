#include "core/rule_type_validator.h"
#include <algorithm>
#include <sstream>

// Zone rule types
const std::vector<std::string> RuleTypeValidator::ZONE_TYPES = {
    "armedPersonAreas",    "crossingAreas",      "crowdEstimationAreas",
    "crowdingAreas",       "dwellingAreas",      "fallenPersonAreas",
    "intrusionAreas",      "loiteringAreas",     "objectLeftAreas",
    "objectRemovedAreas",  "occupancyAreas",     "stopZone",
    "jamZone",             "custom"};

// Line rule types
const std::vector<std::string> RuleTypeValidator::LINE_TYPES = {
    "countingLines", "crossingLines", "tailgatingLines"};

// All valid rule types
const std::vector<std::string> RuleTypeValidator::ALL_RULE_TYPES = {
    // Zones
    "armedPersonAreas",    "crossingAreas",      "crowdEstimationAreas",
    "crowdingAreas",       "dwellingAreas",      "fallenPersonAreas",
    "intrusionAreas",      "loiteringAreas",     "objectLeftAreas",
    "objectRemovedAreas",  "occupancyAreas",     "stopZone",
    "jamZone",             "custom",
    // Lines
    "countingLines",       "crossingLines",      "tailgatingLines"};

bool RuleTypeValidator::isValidRuleType(const std::string &ruleType) {
  return std::find(ALL_RULE_TYPES.begin(), ALL_RULE_TYPES.end(), ruleType) !=
         ALL_RULE_TYPES.end();
}

bool RuleTypeValidator::isZoneType(const std::string &ruleType) {
  return std::find(ZONE_TYPES.begin(), ZONE_TYPES.end(), ruleType) !=
         ZONE_TYPES.end();
}

bool RuleTypeValidator::isLineType(const std::string &ruleType) {
  return std::find(LINE_TYPES.begin(), LINE_TYPES.end(), ruleType) !=
         LINE_TYPES.end();
}

std::vector<std::string> RuleTypeValidator::getRequiredZoneFields(const std::string &ruleType) {
  std::vector<std::string> fields = {"id", "name", "polygon", "properties"};

  // Add type-specific required fields
  if (ruleType == "loiteringAreas") {
    fields.push_back("duration"); // Duration in seconds
  } else if (ruleType == "crowdingAreas") {
    fields.push_back("threshold"); // Minimum number of people
  } else if (ruleType == "dwellingAreas") {
    fields.push_back("minDuration"); // Minimum duration in seconds
  } else if (ruleType == "stopZone") {
    fields.push_back("min_stop_seconds");
    fields.push_back("check_interval_frames");
    fields.push_back("check_min_hit_frames");
    fields.push_back("check_max_distance");
  } else if (ruleType == "jamZone") {
    fields.push_back("threshold");
    fields.push_back("duration_seconds");
  }

  return fields;
}

std::vector<std::string> RuleTypeValidator::getRequiredLineFields(const std::string &ruleType) {
  std::vector<std::string> fields = {"id", "name", "coordinates", "direction", "classes"};

  // Add type-specific required fields
  if (ruleType == "tailgatingLines") {
    fields.push_back("alarmAfterSeconds"); // Time threshold for tailgating detection
  }

  return fields;
}

bool RuleTypeValidator::validateZone(const Json::Value &zone, std::vector<std::string> &errors) {
  errors.clear();

  if (!zone.isObject()) {
    errors.push_back("Zone must be a JSON object");
    return false;
  }

  // Required fields for all zones
  if (!zone.isMember("id") || !zone["id"].isString() || zone["id"].asString().empty()) {
    errors.push_back("Zone 'id' is required and must be a non-empty string");
  }

  if (!zone.isMember("name") || !zone["name"].isString()) {
    errors.push_back("Zone 'name' is required and must be a string");
  }

  // Validate polygon
  if (!zone.isMember("polygon") || !zone["polygon"].isArray()) {
    errors.push_back("Zone 'polygon' is required and must be an array");
  } else {
    const Json::Value &polygon = zone["polygon"];
    if (polygon.size() < 3) {
      errors.push_back("Zone 'polygon' must have at least 3 points");
    } else {
      for (Json::ArrayIndex i = 0; i < polygon.size(); ++i) {
        const Json::Value &point = polygon[i];
        if (!point.isArray() || point.size() != 2) {
          errors.push_back("Zone polygon point " + std::to_string(i) +
                          " must be an array of 2 numbers [x, y]");
        } else {
          if (!point[0].isNumeric() || !point[1].isNumeric()) {
            errors.push_back("Zone polygon point " + std::to_string(i) +
                            " coordinates must be numbers");
          }
        }
      }
    }
  }

  // Validate properties
  if (!zone.isMember("properties") || !zone["properties"].isObject()) {
    errors.push_back("Zone 'properties' is required and must be an object");
  } else {
    const Json::Value &properties = zone["properties"];
    if (!properties.isMember("classes") || !properties["classes"].isArray()) {
      errors.push_back("Zone 'properties.classes' is required and must be an array");
    } else {
      const Json::Value &classes = properties["classes"];
      if (classes.empty()) {
        errors.push_back("Zone 'properties.classes' must contain at least one class");
      }
      // Validate class values
      std::vector<std::string> validClasses = {"Person", "Vehicle", "Animal", "Face", "Unknown"};
      for (Json::ArrayIndex i = 0; i < classes.size(); ++i) {
        if (!classes[i].isString()) {
          errors.push_back("Zone class at index " + std::to_string(i) + " must be a string");
        } else {
          std::string className = classes[i].asString();
          if (std::find(validClasses.begin(), validClasses.end(), className) ==
              validClasses.end()) {
            errors.push_back("Zone class '" + className + "' is not valid. Valid classes: Person, Vehicle, Animal, Face, Unknown");
          }
        }
      }
    }
  }

  // Validate rule type if provided
  std::string ruleType = "intrusionAreas"; // Default
  if (zone.isMember("type") && zone["type"].isString()) {
    ruleType = zone["type"].asString();
    if (!isValidRuleType(ruleType) || !isZoneType(ruleType)) {
      errors.push_back("Zone type '" + ruleType + "' is not a valid zone rule type");
    }
  }

  // Validate type-specific fields
  if (!errors.empty()) {
    return false; // Don't validate type-specific if basic validation failed
  }

  return validateZoneTypeSpecific(zone, ruleType, errors);
}

bool RuleTypeValidator::validateLine(const Json::Value &line, std::vector<std::string> &errors) {
  errors.clear();

  if (!line.isObject()) {
    errors.push_back("Line must be a JSON object");
    return false;
  }

  // Required fields for all lines
  if (!line.isMember("id") || !line["id"].isString() || line["id"].asString().empty()) {
    errors.push_back("Line 'id' is required and must be a non-empty string");
  }

  if (!line.isMember("name") || !line["name"].isString()) {
    errors.push_back("Line 'name' is required and must be a string");
  }

  // Validate coordinates
  if (!line.isMember("coordinates") || !line["coordinates"].isArray()) {
    errors.push_back("Line 'coordinates' is required and must be an array");
  } else {
    const Json::Value &coordinates = line["coordinates"];
    if (coordinates.size() < 2) {
      errors.push_back("Line 'coordinates' must have at least 2 points");
    } else {
      for (Json::ArrayIndex i = 0; i < coordinates.size(); ++i) {
        const Json::Value &coord = coordinates[i];
        if (!coord.isObject()) {
          errors.push_back("Line coordinate " + std::to_string(i) + " must be an object");
        } else {
          if (!coord.isMember("x") || !coord["x"].isNumeric()) {
            errors.push_back("Line coordinate " + std::to_string(i) + " must have numeric 'x'");
          }
          if (!coord.isMember("y") || !coord["y"].isNumeric()) {
            errors.push_back("Line coordinate " + std::to_string(i) + " must have numeric 'y'");
          }
        }
      }
    }
  }

  // Validate direction
  if (!line.isMember("direction") || !line["direction"].isString()) {
    errors.push_back("Line 'direction' is required and must be a string");
  } else {
    std::string direction = line["direction"].asString();
    std::vector<std::string> validDirections = {"Up", "Down", "Both"};
    if (std::find(validDirections.begin(), validDirections.end(), direction) ==
        validDirections.end()) {
      errors.push_back("Line 'direction' must be one of: Up, Down, Both");
    }
  }

  // Validate classes
  if (!line.isMember("classes") || !line["classes"].isArray()) {
    errors.push_back("Line 'classes' is required and must be an array");
  } else {
    const Json::Value &classes = line["classes"];
    if (classes.empty()) {
      errors.push_back("Line 'classes' must contain at least one class");
    }
    // Validate class values
    std::vector<std::string> validClasses = {"Person", "Vehicle", "Animal", "Face", "Unknown"};
    for (Json::ArrayIndex i = 0; i < classes.size(); ++i) {
      if (!classes[i].isString()) {
        errors.push_back("Line class at index " + std::to_string(i) + " must be a string");
      } else {
        std::string className = classes[i].asString();
        if (std::find(validClasses.begin(), validClasses.end(), className) ==
            validClasses.end()) {
          errors.push_back("Line class '" + className + "' is not valid. Valid classes: Person, Vehicle, Animal, Face, Unknown");
        }
      }
    }
  }

  // Validate color (optional but if present must be valid)
  if (line.isMember("color")) {
    if (!line["color"].isArray() || line["color"].size() != 4) {
      errors.push_back("Line 'color' must be an array of 4 integers [R, G, B, A]");
    } else {
      for (Json::ArrayIndex i = 0; i < 4; ++i) {
        if (!line["color"][i].isInt()) {
          errors.push_back("Line 'color'[" + std::to_string(i) + "] must be an integer");
        } else {
          int value = line["color"][i].asInt();
          if (value < 0 || value > 255) {
            errors.push_back("Line 'color'[" + std::to_string(i) + "] must be between 0 and 255");
          }
        }
      }
    }
  }

  // Determine rule type (default to crossingLines)
  std::string ruleType = "crossingLines";
  // Note: Lines don't typically have a "type" field, but we can infer from context
  // For now, we'll validate all lines the same way

  // Validate type-specific fields
  if (!errors.empty()) {
    return false; // Don't validate type-specific if basic validation failed
  }

  return validateLineTypeSpecific(line, ruleType, errors);
}

bool RuleTypeValidator::validateZoneTypeSpecific(const Json::Value &zone,
                                                  const std::string &ruleType,
                                                  std::vector<std::string> &errors) {
  // Validate type-specific required fields
  if (ruleType == "loiteringAreas") {
    if (!zone.isMember("duration") || !zone["duration"].isNumeric() ||
        zone["duration"].asDouble() <= 0) {
      errors.push_back("loiteringAreas requires 'duration' (seconds) > 0");
    }
  } else if (ruleType == "crowdingAreas") {
    if (!zone.isMember("threshold") || !zone["threshold"].isNumeric() ||
        zone["threshold"].asInt() < 1) {
      errors.push_back("crowdingAreas requires 'threshold' (minimum people count) >= 1");
    }
  } else if (ruleType == "dwellingAreas") {
    if (!zone.isMember("minDuration") || !zone["minDuration"].isNumeric() ||
        zone["minDuration"].asDouble() <= 0) {
      errors.push_back("dwellingAreas requires 'minDuration' (seconds) > 0");
    }
  } else if (ruleType == "stopZone") {
    if (!zone.isMember("min_stop_seconds") || !zone["min_stop_seconds"].isNumeric() ||
        zone["min_stop_seconds"].asDouble() < 0) {
      errors.push_back("stopZone requires 'min_stop_seconds' >= 0");
    }
    if (!zone.isMember("check_interval_frames") || !zone["check_interval_frames"].isInt() ||
        zone["check_interval_frames"].asInt() < 1) {
      errors.push_back("stopZone requires 'check_interval_frames' >= 1");
    }
    if (!zone.isMember("check_min_hit_frames") || !zone["check_min_hit_frames"].isInt() ||
        zone["check_min_hit_frames"].asInt() < 1) {
      errors.push_back("stopZone requires 'check_min_hit_frames' >= 1");
    }
    if (!zone.isMember("check_max_distance") || !zone["check_max_distance"].isNumeric() ||
        zone["check_max_distance"].asDouble() < 0) {
      errors.push_back("stopZone requires 'check_max_distance' >= 0");
    }
  } else if (ruleType == "jamZone") {
    if (!zone.isMember("threshold") || !zone["threshold"].isInt() ||
        zone["threshold"].asInt() < 1) {
      errors.push_back("jamZone requires 'threshold' (minimum objects) >= 1");
    }
    if (!zone.isMember("duration_seconds") || !zone["duration_seconds"].isNumeric() ||
        zone["duration_seconds"].asDouble() < 0) {
      errors.push_back("jamZone requires 'duration_seconds' >= 0");
    }
  }

  return errors.empty();
}

bool RuleTypeValidator::validateLineTypeSpecific(const Json::Value &line,
                                                  const std::string &ruleType,
                                                  std::vector<std::string> &errors) {
  // Validate type-specific required fields
  if (ruleType == "tailgatingLines") {
    if (!line.isMember("alarmAfterSeconds") || !line["alarmAfterSeconds"].isNumeric() ||
        line["alarmAfterSeconds"].asDouble() < 0) {
      errors.push_back("tailgatingLines requires 'alarmAfterSeconds' >= 0");
    }
  }

  return errors.empty();
}

