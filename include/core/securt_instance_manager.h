#pragma once

#include "core/securt_instance.h"
#include "core/securt_instance_registry.h"
#include "core/securt_statistics_collector.h"
#include "instances/instance_manager.h"
#include "models/create_instance_request.h"
#include <json/json.h>
#include <memory>
#include <optional>
#include <string>

/**
 * @brief SecuRT Instance Manager
 *
 * Manages SecuRT instances, integrating with core instance system.
 * Handles CRUD operations and statistics collection.
 */
class SecuRTInstanceManager {
public:
  /**
   * @brief Constructor
   * @param instanceManager Core instance manager
   */
  explicit SecuRTInstanceManager(IInstanceManager *instanceManager);

  /**
   * @brief Create a new SecuRT instance
   * @param instanceId Optional instance ID (if empty, will be generated)
   * @param write SecuRTInstanceWrite with instance data
   * @return Instance ID if successful, empty string otherwise
   */
  std::string createInstance(const std::string &instanceId,
                             const SecuRTInstanceWrite &write);

  /**
   * @brief Update SecuRT instance
   * @param instanceId Instance ID
   * @param write SecuRTInstanceWrite with updates
   * @return true if successful
   */
  bool updateInstance(const std::string &instanceId,
                      const SecuRTInstanceWrite &write);

  /**
   * @brief Delete SecuRT instance
   * @param instanceId Instance ID
   * @return true if successful
   */
  bool deleteInstance(const std::string &instanceId);

  /**
   * @brief Get SecuRT instance
   * @param instanceId Instance ID
   * @return Optional SecuRT instance if found
   */
  std::optional<SecuRTInstance> getInstance(const std::string &instanceId) const;

  /**
   * @brief Get instance statistics
   * @param instanceId Instance ID
   * @return Statistics if found
   */
  SecuRTInstanceStats getStatistics(const std::string &instanceId) const;

  /**
   * @brief Check if instance exists
   * @param instanceId Instance ID
   * @return true if exists
   */
  bool hasInstance(const std::string &instanceId) const;

private:
  IInstanceManager *instance_manager_;
  SecuRTInstanceRegistry registry_;
  SecuRTStatisticsCollector statistics_collector_;

  /**
   * @brief Generate instance ID if not provided
   * @return Generated UUID
   */
  std::string generateInstanceId() const;

  /**
   * @brief Create core instance request from SecuRT instance
   * @param instanceId Instance ID
   * @param write SecuRTInstanceWrite
   * @return CreateInstanceRequest
   */
  CreateInstanceRequest createCoreInstanceRequest(
      const std::string &instanceId, const SecuRTInstanceWrite &write) const;
};

