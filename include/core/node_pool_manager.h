#pragma once

#include "models/solution_config.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <optional>
#include <map>
#include <chrono>

// Forward declarations
namespace cvedix_nodes {
    class cvedix_node;
}
class SolutionRegistry;
class NodeStorage;

/**
 * @brief Node Pool Manager
 * 
 * Manages a pool of pre-configured nodes that users can select from
 * to build custom pipeline solutions.
 */
class NodePoolManager {
public:
    /**
     * @brief Node template configuration
     * Defines a reusable node template that can be instantiated
     */
    struct NodeTemplate {
        std::string templateId;           // Unique template ID
        std::string nodeType;             // Node type (rtsp_src, yunet_face_detector, etc.)
        std::string displayName;          // Human-readable name
        std::string description;          // Description of what this node does
        std::string category;             // Category: "source", "detector", "processor", "destination", "broker"
        std::map<std::string, std::string> defaultParameters;  // Default parameters
        std::vector<std::string> requiredParameters;           // Required parameters that must be provided
        std::vector<std::string> optionalParameters;           // Optional parameters
        bool isPreConfigured;             // If true, node is pre-configured and ready to use
    };
    
    /**
     * @brief Pre-configured node instance
     * An actual node instance that has been created and configured
     */
    struct PreConfiguredNode {
        std::string nodeId;                // Unique node ID
        std::string templateId;           // Reference to template
        std::shared_ptr<cvedix_nodes::cvedix_node> node;  // Actual node instance
        std::map<std::string, std::string> parameters;     // Configured parameters
        bool inUse;                        // Whether node is currently in use
        std::chrono::steady_clock::time_point createdAt;
    };
    
    /**
     * @brief Get singleton instance
     */
    static NodePoolManager& getInstance();
    
    /**
     * @brief Initialize node pool with default templates
     */
    void initializeDefaultTemplates();
    
    /**
     * @brief Register a node template
     */
    bool registerTemplate(const NodeTemplate& nodeTemplate);
    
    /**
     * @brief Get all available node templates
     */
    std::vector<NodeTemplate> getAllTemplates() const;
    
    /**
     * @brief Get templates by category
     */
    std::vector<NodeTemplate> getTemplatesByCategory(const std::string& category) const;
    
    /**
     * @brief Get a specific template by ID
     */
    std::optional<NodeTemplate> getTemplate(const std::string& templateId) const;
    
    /**
     * @brief Create a pre-configured node from template
     * @param templateId Template ID
     * @param parameters Parameters to override defaults
     * @return Node ID if successful, empty string otherwise
     */
    std::string createPreConfiguredNode(
        const std::string& templateId,
        const std::map<std::string, std::string>& parameters = {}
    );
    
    /**
     * @brief Get a pre-configured node by ID
     */
    std::optional<PreConfiguredNode> getPreConfiguredNode(const std::string& nodeId) const;
    
    /**
     * @brief Get all pre-configured nodes
     */
    std::vector<PreConfiguredNode> getAllPreConfiguredNodes() const;
    
    /**
     * @brief Get available (not in use) pre-configured nodes
     */
    std::vector<PreConfiguredNode> getAvailableNodes() const;
    
    /**
     * @brief Mark node as in use
     */
    bool markNodeInUse(const std::string& nodeId);
    
    /**
     * @brief Mark node as available
     */
    bool markNodeAvailable(const std::string& nodeId);
    
    /**
     * @brief Remove a pre-configured node
     */
    bool removePreConfiguredNode(const std::string& nodeId);
    
    /**
     * @brief Get node count statistics
     */
    struct NodeStats {
        size_t totalTemplates;
        size_t totalPreConfiguredNodes;
        size_t availableNodes;
        size_t inUseNodes;
        std::map<std::string, size_t> nodesByCategory;
    };
    
    NodeStats getStats() const;
    
    /**
     * @brief Build a solution config from selected node IDs
     * @param nodeIds Vector of node IDs in pipeline order
     * @param solutionId Solution ID
     * @param solutionName Solution name
     * @return Solution config if successful
     */
    std::optional<SolutionConfig> buildSolutionFromNodes(
        const std::vector<std::string>& nodeIds,
        const std::string& solutionId,
        const std::string& solutionName
    );
    
    /**
     * @brief Check if default nodes exist (by checking node types from solutions)
     * @param solutionRegistry Reference to solution registry
     * @return true if all default node types already exist
     */
    bool hasDefaultNodes(SolutionRegistry& solutionRegistry) const;
    
    /**
     * @brief Create default nodes from all available templates
     * Creates nodes for all templates that can be created (have all required parameters or defaults)
     * Only creates nodes for types that don't already exist
     * @return Number of nodes created
     */
    size_t createDefaultNodesFromTemplates();
    
    /**
     * @brief Create pre-configured nodes from default solutions
     * Extracts unique node types from all default solutions and creates nodes
     * Only creates nodes for types that don't already exist
     * @param solutionRegistry Reference to solution registry
     * @return Number of nodes created
     */
    size_t createNodesFromDefaultSolutions(SolutionRegistry& solutionRegistry);
    
    /**
     * @brief Create pre-configured nodes from a specific solution
     * Extracts unique node types from the solution and creates default nodes
     * Only creates nodes for types that don't already exist
     * @param solutionConfig Solution configuration
     * @return Number of nodes created
     */
    size_t createNodesFromSolution(const SolutionConfig& solutionConfig);
    
    /**
     * @brief Load pre-configured nodes from storage and merge with existing nodes
     * @param nodeStorage Reference to node storage
     * @return Number of nodes loaded and added
     */
    size_t loadNodesFromStorage(NodeStorage& nodeStorage);
    
    /**
     * @brief Save all pre-configured nodes to storage
     * @param nodeStorage Reference to node storage
     * @return true if successful
     */
    bool saveNodesToStorage(NodeStorage& nodeStorage) const;
    
private:
    NodePoolManager() = default;
    ~NodePoolManager() = default;
    NodePoolManager(const NodePoolManager&) = delete;
    NodePoolManager& operator=(const NodePoolManager&) = delete;
    
    mutable std::shared_timed_mutex mutex_;
    std::unordered_map<std::string, NodeTemplate> templates_;
    std::unordered_map<std::string, PreConfiguredNode> preConfiguredNodes_;
    
    /**
     * @brief Create actual node instance from template
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createNodeInstance(
        const NodeTemplate& nodeTemplate,
        const std::map<std::string, std::string>& parameters
    );
    
    /**
     * @brief Generate unique node ID
     */
    std::string generateNodeId() const;
    
    /**
     * @brief Generate node ID for default/preconfigured nodes based on nodeType
     * @param nodeType The node type (e.g., "broker", "file_des", "app_src")
     * @return Node ID in format: node_<nodeType>_default
     */
    std::string generateDefaultNodeId(const std::string& nodeType) const;
};

