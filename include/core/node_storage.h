#pragma once

#include "core/node_pool_manager.h"
#include <json/json.h>
#include <string>
#include <vector>
#include <optional>
#include <filesystem>

/**
 * @brief Node Storage
 * 
 * Handles persistent storage of pre-configured nodes to/from JSON files.
 * Nodes created from default solutions are stored, as well as user-created nodes.
 */
class NodeStorage {
public:
    /**
     * @brief Constructor
     * @param storage_dir Directory to store node JSON files (default: ./nodes)
     */
    explicit NodeStorage(const std::string& storage_dir = "./nodes");
    
    /**
     * @brief Save all pre-configured nodes to JSON file
     * @param nodes Vector of pre-configured nodes
     * @return true if successful
     */
    bool saveAllNodes(const std::vector<NodePoolManager::PreConfiguredNode>& nodes);
    
    /**
     * @brief Load all pre-configured nodes from storage directory
     * @return Vector of pre-configured nodes that were loaded
     */
    std::vector<NodePoolManager::PreConfiguredNode> loadAllNodes();
    
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
     * @brief Get nodes file path
     */
    std::string getNodesFilePath() const;
    
    /**
     * @brief Load nodes file
     */
    Json::Value loadNodesFile() const;
    
    /**
     * @brief Save nodes file
     */
    bool saveNodesFile(const Json::Value& nodes) const;
    
    /**
     * @brief Convert PreConfiguredNode to JSON
     */
    Json::Value nodeToJson(const NodePoolManager::PreConfiguredNode& node) const;
    
    /**
     * @brief Convert JSON to PreConfiguredNode
     */
    std::optional<NodePoolManager::PreConfiguredNode> jsonToNode(const Json::Value& json) const;
};

