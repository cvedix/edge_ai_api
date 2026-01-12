#pragma once

#include <json/json.h>
#include <string>

class IInstanceManager;

/**
 * @brief Rule Processor
 * 
 * Processes rules and applies them to instance pipeline based on rule type.
 * Each rule type may have specific processing logic.
 */
class RuleProcessor {
public:
  /**
   * @brief Set instance manager
   */
  static void setInstanceManager(IInstanceManager *manager);

  /**
   * @brief Process and apply zones to instance
   * @param instanceId Instance ID
   * @param zones Zones array
   * @return true if successful
   */
  static bool processZones(const std::string &instanceId, const Json::Value &zones);

  /**
   * @brief Process and apply lines to instance
   * @param instanceId Instance ID
   * @param lines Lines array
   * @return true if successful
   */
  static bool processLines(const std::string &instanceId, const Json::Value &lines);

  /**
   * @brief Process a specific zone based on its type
   * @param instanceId Instance ID
   * @param zone Zone JSON object
   * @return true if successful
   */
  static bool processZone(const std::string &instanceId, const Json::Value &zone);

  /**
   * @brief Process a specific line based on its type
   * @param instanceId Instance ID
   * @param line Line JSON object
   * @return true if successful
   */
  static bool processLine(const std::string &instanceId, const Json::Value &line);

  /**
   * @brief Convert rules to instance additionalParams format
   * @param zones Zones array
   * @param lines Lines array
   * @return Map of additionalParams keys and values
   */
  static std::map<std::string, std::string>
  convertRulesToAdditionalParams(const Json::Value &zones, const Json::Value &lines);

private:
  static IInstanceManager *instance_manager_;

  // Zone type processors
  static bool processIntrusionArea(const std::string &instanceId, const Json::Value &zone);
  static bool processLoiteringArea(const std::string &instanceId, const Json::Value &zone);
  static bool processDwellingArea(const std::string &instanceId, const Json::Value &zone);
  static bool processCrowdingArea(const std::string &instanceId, const Json::Value &zone);
  static bool processStopZone(const std::string &instanceId, const Json::Value &zone);
  static bool processJamZone(const std::string &instanceId, const Json::Value &zone);
  static bool processGenericZone(const std::string &instanceId, const Json::Value &zone);

  // Line type processors
  static bool processCrossingLine(const std::string &instanceId, const Json::Value &line);
  static bool processCountingLine(const std::string &instanceId, const Json::Value &line);
  static bool processTailgatingLine(const std::string &instanceId, const Json::Value &line);
  static bool processGenericLine(const std::string &instanceId, const Json::Value &line);
};

