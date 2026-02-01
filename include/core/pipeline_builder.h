#pragma once

#include "instances/instance_info.h"
#include "models/create_instance_request.h"
#include "models/solution_config.h"
#include "core/pipeline_builder_model_resolver.h"
#include "core/pipeline_builder_request_utils.h"
#include <memory>
#include <set>
#include <string>
#include <vector>

// Forward declarations
namespace cvedix_nodes {
class cvedix_node;
}
class AreaManager;
class SecuRTLineManager;

/**
 * @brief Pipeline Builder
 *
 * Builds CVEDIX SDK pipelines from solution configurations and instance
 * requests.
 */
class PipelineBuilder {
public:
  /**
   * @brief Set Area Manager for SecuRT integration
   * @param manager Area manager instance
   */
  static void setAreaManager(AreaManager *manager);

  /**
   * @brief Set Line Manager for SecuRT integration
   * @param manager Line manager instance
   */
  static void setLineManager(SecuRTLineManager *manager);
  /**
   * @brief Build pipeline from solution config and request
   * @param solution Solution configuration
   * @param req Create instance request
   * @param instanceId Instance ID for node naming
   * @return Vector of pipeline nodes (connected in order)
   */
  std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>>
  buildPipeline(const SolutionConfig &solution,
                const CreateInstanceRequest &req,
                const std::string &instanceId,
                const std::set<std::string> &existingRTMPStreamKeys = {});


private:
  // Static pointers for SecuRT integration
  static AreaManager *area_manager_;
  static SecuRTLineManager *line_manager_;
  /**
   * @brief Create a node from node configuration
   * @param nodeConfig Node configuration
   * @param req Create instance request
   * @param instanceId Instance ID
   * @return Created node
   */
  std::shared_ptr<cvedix_nodes::cvedix_node>
  createNode(const SolutionConfig::NodeConfig &nodeConfig,
             const CreateInstanceRequest &req, const std::string &instanceId,
             const std::set<std::string> &existingRTMPStreamKeys = {});

  // Note: Source node methods have been moved to PipelineBuilderSourceNodes

  // Note: Source node methods have been moved to PipelineBuilderSourceNodes
  // Note: Destination node methods have been moved to PipelineBuilderDestinationNodes
  // Note: Detector node methods have been moved to PipelineBuilderDetectorNodes

  // Note: OSD and Tracking node methods have been moved to PipelineBuilderOtherNodes

  // Note: Source node methods have been moved to PipelineBuilderSourceNodes
  // Note: Destination node methods have been moved to PipelineBuilderDestinationNodes
  // Note: Detector node methods have been moved to PipelineBuilderDetectorNodes
  // Note: Broker node methods have been moved to PipelineBuilderBrokerNodes
  // Note: Behavior Analysis node methods have been moved to PipelineBuilderBehaviorAnalysisNodes


  // Note: Utility methods have been moved to separate utility classes:
  // - PipelineBuilderModelResolver: resolveModelPath, resolveModelByName, 
  //   listAvailableModels, mapDetectionSensitivity
  // - PipelineBuilderRequestUtils: getFilePath, getRTMPUrl, getRTSPUrl
};
