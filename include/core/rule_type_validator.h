#pragma once

#include <json/json.h>
#include <string>
#include <vector>

/**
 * @brief Rule Type Validator
 * 
 * Validates rule types and their formats according to USC rule type specifications.
 * Supports all rule types: zones (intrusionAreas, loiteringAreas, etc.) and lines (crossingLines, countingLines, tailgatingLines).
 */
class RuleTypeValidator {
public:
  /**
   * @brief Validate a zone rule
   * @param zone Zone JSON object
   * @param errors Output vector for validation errors
   * @return true if valid, false otherwise
   */
  static bool validateZone(const Json::Value &zone, std::vector<std::string> &errors);

  /**
   * @brief Validate a line rule
   * @param line Line JSON object
   * @param errors Output vector for validation errors
   * @return true if valid, false otherwise
   */
  static bool validateLine(const Json::Value &line, std::vector<std::string> &errors);

  /**
   * @brief Check if a rule type is valid
   * @param ruleType Rule type string (e.g., "intrusionAreas", "crossingLines")
   * @return true if valid rule type
   */
  static bool isValidRuleType(const std::string &ruleType);

  /**
   * @brief Check if a rule type is a zone type
   * @param ruleType Rule type string
   * @return true if zone type
   */
  static bool isZoneType(const std::string &ruleType);

  /**
   * @brief Check if a rule type is a line type
   * @param ruleType Rule type string
   * @return true if line type
   */
  static bool isLineType(const std::string &ruleType);

  /**
   * @brief Get required fields for a zone rule type
   * @param ruleType Zone rule type
   * @return Vector of required field names
   */
  static std::vector<std::string> getRequiredZoneFields(const std::string &ruleType);

  /**
   * @brief Get required fields for a line rule type
   * @param ruleType Line rule type
   * @return Vector of required field names
   */
  static std::vector<std::string> getRequiredLineFields(const std::string &ruleType);

  /**
   * @brief Validate zone-specific properties based on rule type
   * @param zone Zone JSON object
   * @param ruleType Zone rule type
   * @param errors Output vector for validation errors
   * @return true if valid
   */
  static bool validateZoneTypeSpecific(const Json::Value &zone, const std::string &ruleType,
                                       std::vector<std::string> &errors);

  /**
   * @brief Validate line-specific properties based on rule type
   * @param line Line JSON object
   * @param ruleType Line rule type
   * @param errors Output vector for validation errors
   * @return true if valid
   */
  static bool validateLineTypeSpecific(const Json::Value &line, const std::string &ruleType,
                                       std::vector<std::string> &errors);

private:
  // Zone rule types
  static const std::vector<std::string> ZONE_TYPES;
  // Line rule types
  static const std::vector<std::string> LINE_TYPES;
  // All valid rule types
  static const std::vector<std::string> ALL_RULE_TYPES;
};

