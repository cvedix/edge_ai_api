#pragma once

#include <chrono>
#include <string>
#include <vector>

/**
 * @brief Group information structure
 * Represents a group that can contain multiple instances
 */
struct GroupInfo {
  std::string groupId;     // Unique identifier for the group
  std::string groupName;   // Display name of the group
  std::string description; // Optional description
  bool isDefault = false;  // If true, group cannot be deleted
  bool readOnly = false;   // If true, group cannot be modified
  int instanceCount = 0;   // Number of instances in this group

  // Timestamps
  std::string createdAt; // Creation timestamp
  std::string updatedAt; // Last update timestamp

  /**
   * @brief Validate group information
   */
  bool validate(std::string &error) const;
};
