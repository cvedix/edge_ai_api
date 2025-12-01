#pragma once

#include "instances/instance_info.h"
#include <string>
#include <vector>
#include <optional>
#include <filesystem>

/**
 * @brief Instance Storage
 * 
 * Handles persistent storage of instances to/from JSON files.
 */
class InstanceStorage {
public:
    /**
     * @brief Constructor
     * @param storage_dir Directory to store instance JSON files (default: ./instances)
     */
    explicit InstanceStorage(const std::string& storage_dir = "./instances");
    
    /**
     * @brief Save instance to JSON file
     * @param instanceId Instance ID
     * @param info Instance information
     * @return true if successful
     */
    bool saveInstance(const std::string& instanceId, const InstanceInfo& info);
    
    /**
     * @brief Load instance from JSON file
     * @param instanceId Instance ID
     * @return Instance info if found, empty optional otherwise
     */
    std::optional<InstanceInfo> loadInstance(const std::string& instanceId);
    
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
    bool deleteInstance(const std::string& instanceId);
    
    /**
     * @brief Check if instance file exists
     * @param instanceId Instance ID
     * @return true if file exists
     */
    bool instanceExists(const std::string& instanceId) const;
    
    /**
     * @brief Get storage directory path
     */
    std::string getStorageDir() const { return storage_dir_; }
    
private:
    std::string storage_dir_;
    
    /**
     * @brief Ensure storage directory exists
     */
    void ensureStorageDir() const;
    
    /**
     * @brief Get file path for instance
     */
    std::string getInstanceFilePath(const std::string& instanceId) const;
    
    /**
     * @brief Convert InstanceInfo to JSON
     */
    std::string instanceInfoToJson(const InstanceInfo& info) const;
    
    /**
     * @brief Convert JSON to InstanceInfo
     */
    std::optional<InstanceInfo> jsonToInstanceInfo(const std::string& json) const;
};

