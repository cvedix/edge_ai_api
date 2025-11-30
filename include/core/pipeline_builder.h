#pragma once

#include "models/solution_config.h"
#include "models/create_instance_request.h"
#include "instances/instance_info.h"
#include <memory>
#include <vector>
#include <string>

// Forward declarations for CVEDIX SDK nodes
namespace cvedix_nodes {
    class cvedix_node;
}

/**
 * @brief Pipeline Builder
 * 
 * Builds CVEDIX SDK pipelines from solution configurations and instance requests.
 */
class PipelineBuilder {
public:
    /**
     * @brief Build pipeline from solution config and request
     * @param solution Solution configuration
     * @param req Create instance request
     * @param instanceId Instance ID for node naming
     * @return Vector of pipeline nodes (connected in order)
     */
    std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>> buildPipeline(
        const SolutionConfig& solution,
        const CreateInstanceRequest& req,
        const std::string& instanceId
    );
    
private:
    /**
     * @brief Create a node from node configuration
     * @param nodeConfig Node configuration
     * @param req Create instance request
     * @param instanceId Instance ID
     * @return Created node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createNode(
        const SolutionConfig::NodeConfig& nodeConfig,
        const CreateInstanceRequest& req,
        const std::string& instanceId
    );
    
    /**
     * @brief Create RTSP source node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createRTSPSourceNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create face detector node (YuNet)
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createFaceDetectorNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create file destination node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createFileDestinationNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const std::string& instanceId
    );
    
    /**
     * @brief Map detection sensitivity to threshold value
     * @param sensitivity "Low", "Medium", or "High"
     * @return Threshold value (0.0-1.0)
     */
    double mapDetectionSensitivity(const std::string& sensitivity) const;
    
    /**
     * @brief Get RTSP URL from request
     */
    std::string getRTSPUrl(const CreateInstanceRequest& req) const;
};

