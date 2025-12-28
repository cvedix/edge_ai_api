#pragma once

#include "models/group_info.h"
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

/**
 * @brief Group Registry
 * Manages groups and their instances
 */
class GroupRegistry {
public:
  /**
   * @brief Get singleton instance
   */
  static GroupRegistry &getInstance();

  /**
   * @brief Register a new group
   * @param groupId Group ID (must be unique)
   * @param groupName Group display name
   * @param description Optional description
   * @return true if successful, false if groupId already exists
   */
  bool registerGroup(const std::string &groupId, const std::string &groupName,
                     const std::string &description = "");

  /**
   * @brief Get group information
   * @param groupId Group ID
   * @return GroupInfo if found, nullopt otherwise
   */
  std::optional<GroupInfo> getGroup(const std::string &groupId) const;

  /**
   * @brief Get all groups
   * @return Map of groupId -> GroupInfo
   */
  std::map<std::string, GroupInfo> getAllGroups() const;

  /**
   * @brief Update group information
   * @param groupId Group ID
   * @param groupName New group name (empty to keep unchanged)
   * @param description New description (empty to keep unchanged)
   * @return true if successful, false if group not found or read-only
   */
  bool updateGroup(const std::string &groupId,
                   const std::string &groupName = "",
                   const std::string &description = "");

  /**
   * @brief Delete a group
   * @param groupId Group ID
   * @return true if successful, false if group not found, is default, or has
   * instances
   */
  bool deleteGroup(const std::string &groupId);

  /**
   * @brief Check if group exists
   * @param groupId Group ID
   * @return true if group exists
   */
  bool groupExists(const std::string &groupId) const;

  /**
   * @brief Get instance count for a group
   * @param groupId Group ID
   * @return Number of instances in the group, -1 if group not found
   */
  int getInstanceCount(const std::string &groupId) const;

  /**
   * @brief Update instance count for a group
   * @param groupId Group ID
   * @param count New instance count
   */
  void updateInstanceCount(const std::string &groupId, int count);

  /**
   * @brief Get instance IDs in a group
   * @param groupId Group ID
   * @return Vector of instance IDs
   */
  std::vector<std::string> getInstanceIds(const std::string &groupId) const;

  /**
   * @brief Set instance IDs for a group (called by InstanceRegistry)
   * @param groupId Group ID
   * @param instanceIds Vector of instance IDs
   */
  void setInstanceIds(const std::string &groupId,
                      const std::vector<std::string> &instanceIds);

  /**
   * @brief Initialize default groups
   */
  void initializeDefaultGroups();

private:
  GroupRegistry() = default;
  ~GroupRegistry() = default;
  GroupRegistry(const GroupRegistry &) = delete;
  GroupRegistry &operator=(const GroupRegistry &) = delete;

  /**
   * @brief Update instance count without locking (assumes mutex is already
   * locked)
   * @param groupId Group ID
   * @param count New instance count
   */
  void updateInstanceCountUnlocked(const std::string &groupId, int count);

  mutable std::mutex mutex_;
  std::map<std::string, GroupInfo> groups_;
  std::map<std::string, std::vector<std::string>>
      group_instances_; // groupId -> instanceIds
};
