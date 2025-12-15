#pragma once

#include <string>
#include <vector>
#include <optional>
#include "models/group_info.h"
#include <json/json.h>

/**
 * @brief Group Storage
 * Handles persistence of groups to/from filesystem
 */
class GroupStorage {
public:
    /**
     * @brief Constructor
     * @param storageDir Directory to store group files
     */
    explicit GroupStorage(const std::string& storageDir);
    
    /**
     * @brief Save a group to file
     * @param group Group information
     * @return true if successful
     */
    bool saveGroup(const GroupInfo& group);
    
    /**
     * @brief Load a group from file
     * @param groupId Group ID
     * @return GroupInfo if found, nullopt otherwise
     */
    std::optional<GroupInfo> loadGroup(const std::string& groupId);
    
    /**
     * @brief Load all groups from storage
     * @return Vector of GroupInfo
     */
    std::vector<GroupInfo> loadAllGroups();
    
    /**
     * @brief Delete a group file
     * @param groupId Group ID
     * @return true if successful
     */
    bool deleteGroup(const std::string& groupId);
    
    /**
     * @brief Check if group file exists
     * @param groupId Group ID
     * @return true if file exists
     */
    bool groupFileExists(const std::string& groupId) const;
    
    /**
     * @brief Convert GroupInfo to JSON
     * @param group Group information
     * @param error Optional error message output
     * @return JSON value
     */
    Json::Value groupInfoToJson(const GroupInfo& group, std::string* error = nullptr) const;
    
    /**
     * @brief Convert JSON to GroupInfo
     * @param json JSON value
     * @param error Optional error message output
     * @return GroupInfo if valid, nullopt otherwise
     */
    std::optional<GroupInfo> jsonToGroupInfo(const Json::Value& json, std::string* error = nullptr) const;
    
private:
    std::string storage_dir_;
    
    /**
     * @brief Get file path for a group
     * @param groupId Group ID
     * @return File path
     */
    std::string getGroupFilePath(const std::string& groupId) const;
    
    /**
     * @brief Ensure storage directory exists (with fallback if needed)
     */
    void ensureStorageDir();
};

