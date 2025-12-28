#include "groups/group_registry.h"
#include "models/group_info.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

GroupRegistry &GroupRegistry::getInstance() {
  static GroupRegistry instance;
  return instance;
}

std::string getCurrentTimestamp() {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) %
            1000;

  std::stringstream ss;
  ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
  ss << '.' << std::setfill('0') << std::setw(3) << ms.count() << "Z";
  return ss.str();
}

bool GroupRegistry::registerGroup(const std::string &groupId,
                                  const std::string &groupName,
                                  const std::string &description) {
  std::lock_guard<std::mutex> lock(mutex_);

  // Check if group already exists
  if (groups_.find(groupId) != groups_.end()) {
    std::cerr << "[GroupRegistry] Group " << groupId << " already exists"
              << std::endl;
    return false;
  }

  // Validate groupId format
  if (groupId.empty()) {
    std::cerr << "[GroupRegistry] Group ID cannot be empty" << std::endl;
    return false;
  }

  // Create group info
  GroupInfo group;
  group.groupId = groupId;
  group.groupName = groupName.empty() ? groupId : groupName;
  group.description = description;
  group.isDefault = false;
  group.readOnly = false;
  group.instanceCount = 0;
  group.createdAt = getCurrentTimestamp();
  group.updatedAt = group.createdAt;

  // Validate
  std::string error;
  if (!group.validate(error)) {
    std::cerr << "[GroupRegistry] Validation failed: " << error << std::endl;
    return false;
  }

  groups_[groupId] = group;
  group_instances_[groupId] = std::vector<std::string>();

  std::cerr << "[GroupRegistry] Registered group: " << groupId << " ("
            << group.groupName << ")" << std::endl;
  return true;
}

std::optional<GroupInfo>
GroupRegistry::getGroup(const std::string &groupId) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = groups_.find(groupId);
  if (it != groups_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::map<std::string, GroupInfo> GroupRegistry::getAllGroups() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return groups_;
}

bool GroupRegistry::updateGroup(const std::string &groupId,
                                const std::string &groupName,
                                const std::string &description) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = groups_.find(groupId);
  if (it == groups_.end()) {
    std::cerr << "[GroupRegistry] Group " << groupId << " not found"
              << std::endl;
    return false;
  }

  GroupInfo &group = it->second;

  // Check if read-only
  if (group.readOnly) {
    std::cerr << "[GroupRegistry] Cannot update read-only group " << groupId
              << std::endl;
    return false;
  }

  // Update fields
  if (!groupName.empty()) {
    group.groupName = groupName;
  }
  if (!description.empty()) {
    group.description = description;
  }
  group.updatedAt = getCurrentTimestamp();

  // Validate
  std::string error;
  if (!group.validate(error)) {
    std::cerr << "[GroupRegistry] Validation failed: " << error << std::endl;
    return false;
  }

  std::cerr << "[GroupRegistry] Updated group: " << groupId << std::endl;
  return true;
}

bool GroupRegistry::deleteGroup(const std::string &groupId) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = groups_.find(groupId);
  if (it == groups_.end()) {
    std::cerr << "[GroupRegistry] Group " << groupId << " not found"
              << std::endl;
    return false;
  }

  const GroupInfo &group = it->second;

  // Check if default
  if (group.isDefault) {
    std::cerr << "[GroupRegistry] Cannot delete default group " << groupId
              << std::endl;
    return false;
  }

  // Check if has instances
  auto instancesIt = group_instances_.find(groupId);
  if (instancesIt != group_instances_.end() && !instancesIt->second.empty()) {
    std::cerr << "[GroupRegistry] Cannot delete group " << groupId
              << " with instances" << std::endl;
    return false;
  }

  groups_.erase(it);
  group_instances_.erase(groupId);

  std::cerr << "[GroupRegistry] Deleted group: " << groupId << std::endl;
  return true;
}

bool GroupRegistry::groupExists(const std::string &groupId) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return groups_.find(groupId) != groups_.end();
}

int GroupRegistry::getInstanceCount(const std::string &groupId) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = groups_.find(groupId);
  if (it == groups_.end()) {
    return -1;
  }
  return it->second.instanceCount;
}

void GroupRegistry::updateInstanceCountUnlocked(const std::string &groupId,
                                                int count) {
  // This function assumes mutex is already locked - do NOT lock here
  auto it = groups_.find(groupId);
  if (it != groups_.end()) {
    it->second.instanceCount = count;
    it->second.updatedAt = getCurrentTimestamp();
  }
}

void GroupRegistry::updateInstanceCount(const std::string &groupId, int count) {
  std::lock_guard<std::mutex> lock(mutex_);
  updateInstanceCountUnlocked(groupId, count);
}

std::vector<std::string>
GroupRegistry::getInstanceIds(const std::string &groupId) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = group_instances_.find(groupId);
  if (it != group_instances_.end()) {
    return it->second;
  }
  return std::vector<std::string>();
}

void GroupRegistry::setInstanceIds(
    const std::string &groupId, const std::vector<std::string> &instanceIds) {
  std::lock_guard<std::mutex> lock(mutex_);
  group_instances_[groupId] = instanceIds;
  updateInstanceCountUnlocked(groupId, static_cast<int>(instanceIds.size()));
}

void GroupRegistry::initializeDefaultGroups() {
  // No default groups for now
  // Can be added later if needed
}
