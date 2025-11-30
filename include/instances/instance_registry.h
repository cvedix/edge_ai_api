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
     * @return true if successful
     */
    bool startInstance(const std::string& instanceId);
    
    /**
     * @brief Stop an instance (stop pipeline)
     * @param instanceId Instance ID
     * @return true if successful
     */
    bool stopInstance(const std::string& instanceId);
    
    /**
     * @brief List all instance IDs
     * @return Vector of instance IDs
     */
    std::vector<std::string> listInstances() const;
    
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
     * @brief Start pipeline nodes
     */
    bool startPipeline(const std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>>& nodes);
    
    /**
     * @brief Stop and cleanup pipeline nodes
     */
    void stopPipeline(const std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>>& nodes);
};

