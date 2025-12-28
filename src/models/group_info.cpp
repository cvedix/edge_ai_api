#include "models/group_info.h"
#include <regex>

bool GroupInfo::validate(std::string &error) const {
  if (groupId.empty()) {
    error = "Group ID cannot be empty";
    return false;
  }

  // Validate groupId format: alphanumeric, underscore, hyphen only
  std::regex pattern("^[A-Za-z0-9_-]+$");
  if (!std::regex_match(groupId, pattern)) {
    error = "Group ID must contain only alphanumeric characters, underscores, "
            "and hyphens";
    return false;
  }

  if (groupName.empty()) {
    error = "Group name cannot be empty";
    return false;
  }

  // Validate groupName format: alphanumeric, space, underscore, hyphen
  std::regex namePattern("^[A-Za-z0-9 -_]+$");
  if (!std::regex_match(groupName, namePattern)) {
    error = "Group name must match pattern: ^[A-Za-z0-9 -_]+$";
    return false;
  }

  if (instanceCount < 0) {
    error = "Instance count cannot be negative";
    return false;
  }

  return true;
}
