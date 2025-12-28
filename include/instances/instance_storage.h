#pragma once

#include "instances/instance_info.h"
#include <filesystem>
#include <json/json.h>
#include <optional>
#include <string>
#include <vector>

/**
 * @brief Instance Storage
 *
 * Handles persistent storage of instances to/from JSON files.
 */
class InstanceStorage {
public:
  /**
   * @brief Constructor
   * @param storage_dir Directory to store instance JSON files (default:
   * ./instances)
   */
  explicit InstanceStorage(const std::string &storage_dir = "./instances");

  /**
   * @brief Save instance to JSON file
   * @param instanceId Instance ID
   * @param info Instance information
   * @return true if successful
   */
  bool saveInstance(const std::string &instanceId, const InstanceInfo &info);

  /**
   * @brief Load instance from JSON file
   * @param instanceId Instance ID
   * @return Instance info if found, empty optional otherwise
   */
  std::optional<InstanceInfo> loadInstance(const std::string &instanceId);

  /**
   * @brief Load all instances from storage directory
   * @return Vector of instance IDs that were loaded
   */
  std::vector<std::string> loadAllInstances();

  /**
   * @brief Delete instance JSON file
   * @param instanceId Instance ID
   * @return true if successful
   */
  bool deleteInstance(const std::string &instanceId);

  /**
   * @brief Check if instance file exists
   * @param instanceId Instance ID
   * @return true if file exists
   */
  bool instanceExists(const std::string &instanceId) const;

  /**
   * @brief Get storage directory path
   */
  std::string getStorageDir() const { return storage_dir_; }

  /**
   * @brief Validate InstanceInfo before conversion
   * @param info InstanceInfo to validate
   * @param error Error message output
   * @return true if valid, false otherwise
   */
  bool validateInstanceInfo(const InstanceInfo &info, std::string &error) const;

  /**
   * @brief Validate JSON config object before conversion
   * @param config JSON config to validate
   * @param error Error message output
   * @return true if valid, false otherwise
   */
  bool validateConfigJson(const Json::Value &config, std::string &error) const;

  /**
   * @brief Merge new config into existing config, preserving complex nested
   * structures
   * @param existingConfig Existing config (will be modified)
   * @param newConfig New config to merge
   * @param preserveKeys List of keys to preserve from existing config (e.g.,
   * "TensorRT", "Zone", etc.)
   * @return true if merge successful
   */
  bool mergeConfigs(Json::Value &existingConfig, const Json::Value &newConfig,
                    const std::vector<std::string> &preserveKeys = {}) const;

  /**
   * @brief Convert InstanceInfo to JSON config object (new format)
   * @param info InstanceInfo to convert
   * @param error Optional error message output
   * @return JSON config object, or empty object on error
   */
  Json::Value instanceInfoToConfigJson(const InstanceInfo &info,
                                       std::string *error = nullptr) const;

  /**
   * @brief Convert JSON config object to InstanceInfo
   * @param config JSON config object to convert
   * @param error Optional error message output
   * @return InstanceInfo if successful, empty optional on error
   */
  std::optional<InstanceInfo>
  configJsonToInstanceInfo(const Json::Value &config,
                           std::string *error = nullptr) const;

private:
  std::string storage_dir_;

  /**
   * @brief Ensure storage directory exists (with fallback if needed)
   */
  void ensureStorageDir();

  /**
   * @brief Get file path for instances.json
   */
  std::string getInstancesFilePath() const;

  /**
   * @brief Load entire instances.json file
   */
  Json::Value loadInstancesFile() const;

  /**
   * @brief Save entire instances.json file
   */
  bool saveInstancesFile(const Json::Value &instances);
};
