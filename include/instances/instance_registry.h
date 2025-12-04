#pragma once

#include "instances/instance_info.h"
#include "models/create_instance_request.h"
#include "core/solution_registry.h"
#include "core/pipeline_builder.h"
#include "instances/instance_storage.h"
#include <string>
#include <unordered_map>
#include <optional>
#include <mutex>
#include <vector>
#include <memory>

// Forward declarations
namespace cvedix_nodes {
    class cvedix_node;
}

/**
 * @brief Instance Registry
 * 
 * Manages AI instances and their pipelines.
 * Handles creation, deletion, starting, and stopping of instances.
 */
class InstanceRegistry {
public:
    /**
     * @brief Constructor
     * @param solutionRegistry Reference to solution registry
     * @param pipelineBuilder Reference to pipeline builder
     * @param instanceStorage Reference to instance storage
     */
    InstanceRegistry(
        SolutionRegistry& solutionRegistry,
        PipelineBuilder& pipelineBuilder,
        InstanceStorage& instanceStorage
    );
    
    /**
     * @brief Create a new instance
     * @param req Create instance request
     * @return Instance ID if successful, empty string otherwise
     */
    std::string createInstance(const CreateInstanceRequest& req);
    
    /**
     * @brief Delete an instance
     * @param instanceId Instance ID
     * @return true if successful
     */
    bool deleteInstance(const std::string& instanceId);
    
    /**
     * @brief Get instance information
     * @param instanceId Instance ID
     * @return Instance info if found, empty optional otherwise
     */
    std::optional<InstanceInfo> getInstance(const std::string& instanceId) const;
    
    /**
     * @brief Start an instance (start pipeline)
     * @param instanceId Instance ID
     * @param skipAutoStop If true, skip auto-stop of running instance (for restart scenario)
     * @return true if successful
     */
    bool startInstance(const std::string& instanceId, bool skipAutoStop = false);
    
    /**
     * @brief Stop an instance (stop pipeline)
     * @param instanceId Instance ID
     * @return true if successful
     */
    bool stopInstance(const std::string& instanceId);
    
    /**
     * @brief Update instance information
     * @param instanceId Instance ID
     * @param req Update instance request
     * @return true if successful
     */
    bool updateInstance(const std::string& instanceId, const class UpdateInstanceRequest& req);
    
    /**
     * @brief List all instance IDs
     * @return Vector of instance IDs
     */
    std::vector<std::string> listInstances() const;
    
    /**
     * @brief Get all instances info in one lock acquisition (optimized for list operations)
     * @return Map of instance ID to InstanceInfo
     */
    std::unordered_map<std::string, InstanceInfo> getAllInstances() const;
    
    /**
     * @brief Check if instance exists
     * @param instanceId Instance ID
     * @return true if instance exists
     */
    bool hasInstance(const std::string& instanceId) const;
    
    /**
     * @brief Load all persistent instances from storage
     */
    void loadPersistentInstances();
    
    /**
     * @brief Check if instance has RTMP output
     * @param instanceId Instance ID
     * @return true if instance has RTMP output
     */
    bool hasRTMPOutput(const std::string& instanceId) const;
    
    /**
     * @brief Get source nodes from all running instances (for debug/analysis board)
     * @return Vector of source nodes from running instances
     */
    std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>> getSourceNodesFromRunningInstances() const;
    
private:
    SolutionRegistry& solution_registry_;
    PipelineBuilder& pipeline_builder_;
    InstanceStorage& instance_storage_;
    
    mutable std::mutex mutex_;
    std::unordered_map<std::string, InstanceInfo> instances_;
    std::unordered_map<std::string, std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>>> pipelines_;
    
    /**
     * @brief Create InstanceInfo from request
     */
    InstanceInfo createInstanceInfo(
        const std::string& instanceId,
        const CreateInstanceRequest& req,
        const SolutionConfig* solution
    ) const;
    
    /**
     * @brief Wait for DNN models to be ready using exponential backoff
     * @param nodes Pipeline nodes to check
     * @param maxWaitMs Maximum wait time in milliseconds. Use -1 for unlimited wait (no timeout)
     */
    void waitForModelsReady(const std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>>& nodes, int maxWaitMs);
    
    /**
     * @brief Start pipeline nodes
     * @param nodes Pipeline nodes to start
     * @param isRestart If true, this is a restart (use longer delays for model initialization)
     */
    bool startPipeline(const std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>>& nodes, bool isRestart = false);
    
    /**
     * @brief Stop and cleanup pipeline nodes
     * @param nodes Pipeline nodes to stop
     * @param isDeletion If true, this is for deletion (full cleanup). If false, just stop (can restart).
     */
    void stopPipeline(const std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>>& nodes, bool isDeletion = false);
    
    /**
     * @brief Rebuild pipeline from instance info (for instances loaded from storage)
     * @param instanceId Instance ID
     * @return true if pipeline was rebuilt successfully
     */
    bool rebuildPipelineFromInstanceInfo(const std::string& instanceId);
    
    /**
     * @brief Log processing results for instance (for instances without RTMP output)
     * @param instanceId Instance ID
     */
    void logProcessingResults(const std::string& instanceId) const;
};

