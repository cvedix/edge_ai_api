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

  /**
   * @brief Create face OSD v2 node
   */
  std::shared_ptr<cvedix_nodes::cvedix_node>
  createFaceOSDNode(const std::string &nodeName,
                    const std::map<std::string, std::string> &params);

  /**
   * @brief Create OSD v3 node (for masks and labels)
   */
  std::shared_ptr<cvedix_nodes::cvedix_node>
  createOSDv3Node(const std::string &nodeName,
                  const std::map<std::string, std::string> &params,
                  const CreateInstanceRequest &req);


  // ========== Tracking Nodes ==========

  /**
   * @brief Create SORT tracker node
   */
  std::shared_ptr<cvedix_nodes::cvedix_node>
  createSORTTrackNode(const std::string &nodeName,
                      const std::map<std::string, std::string> &params);

  // ========== Behavior Analysis Nodes ==========

  /**
   * @brief Create BA crossline node
   */
  std::shared_ptr<cvedix_nodes::cvedix_node>
  createBACrosslineNode(const std::string &nodeName,
                        const std::map<std::string, std::string> &params,
                        const CreateInstanceRequest &req);

  /**
   * @brief Create BA jam node
   */
  std::shared_ptr<cvedix_nodes::cvedix_node>
  createBAJamNode(const std::string &nodeName,
                  const std::map<std::string, std::string> &params,
                  const CreateInstanceRequest &req);

  /**
   * @brief Create BA stop node
   */
  std::shared_ptr<cvedix_nodes::cvedix_node>
  createBAStopNode(const std::string &nodeName,
                   const std::map<std::string, std::string> &params,
                   const CreateInstanceRequest &req);

  /**
   * @brief Create BA crossline OSD node
   */
  std::shared_ptr<cvedix_nodes::cvedix_node>
  createBACrosslineOSDNode(const std::string &nodeName,
                           const std::map<std::string, std::string> &params);
  /**
   * @brief Create BA jam OSD node
   */
  std::shared_ptr<cvedix_nodes::cvedix_node>
  createBAJamOSDNode(const std::string &nodeName,
                     const std::map<std::string, std::string> &params);
  /**
   * @brief Create BA stop OSD node
   */
  std::shared_ptr<cvedix_nodes::cvedix_node>
  createBAStopOSDNode(const std::string &nodeName,
                      const std::map<std::string, std::string> &params);
  // ========== Broker Nodes ==========

  /**
   * @brief Create JSON console broker node
   */
  std::shared_ptr<cvedix_nodes::cvedix_node>
  createJSONConsoleBrokerNode(const std::string &nodeName,
                              const std::map<std::string, std::string> &params,
                              const CreateInstanceRequest &req);

  /**
   * @brief Create JSON enhanced console broker node
   */
  std::shared_ptr<cvedix_nodes::cvedix_node>
  createJSONEnhancedConsoleBrokerNode(
      const std::string &nodeName,
      const std::map<std::string, std::string> &params,
      const CreateInstanceRequest &req);

  /**
   * @brief Create JSON MQTT broker node
   */
  std::shared_ptr<cvedix_nodes::cvedix_node>
  createJSONMQTTBrokerNode(const std::string &nodeName,
                           const std::map<std::string, std::string> &params,
                           const CreateInstanceRequest &req);

  /**
   * @brief Create JSON Crossline MQTT broker node (custom formatting for
   * ba_crossline)
   */
  std::shared_ptr<cvedix_nodes::cvedix_node> createJSONCrosslineMQTTBrokerNode(
      const std::string &nodeName,
      const std::map<std::string, std::string> &params,
      const CreateInstanceRequest &req, const std::string &instanceId);

  /**
   * @brief Create JSON Jam MQTT broker node (custom formatting for
   * ba_jam)
   */
  std::shared_ptr<cvedix_nodes::cvedix_node> createJSONJamMQTTBrokerNode(
      const std::string &nodeName,
      const std::map<std::string, std::string> &params,
      const CreateInstanceRequest &req, const std::string &instanceId);
  /**
   * @brief Create JSON Stop MQTT broker node (custom formatting for
   * ba_stop)
   */
  std::shared_ptr<cvedix_nodes::cvedix_node> createJSONStopMQTTBrokerNode(
      const std::string &nodeName,
      const std::map<std::string, std::string> &params,
      const CreateInstanceRequest &req, const std::string &instanceId);


  /**
   * @brief Create JSON Kafka broker node
   */
  std::shared_ptr<cvedix_nodes::cvedix_node>
  createJSONKafkaBrokerNode(const std::string &nodeName,
                            const std::map<std::string, std::string> &params,
                            const CreateInstanceRequest &req);

  /**
   * @brief Create XML file broker node
   */
  std::shared_ptr<cvedix_nodes::cvedix_node>
  createXMLFileBrokerNode(const std::string &nodeName,
                          const std::map<std::string, std::string> &params,
                          const CreateInstanceRequest &req);

  /**
   * @brief Create XML socket broker node
   */
  std::shared_ptr<cvedix_nodes::cvedix_node>
  createXMLSocketBrokerNode(const std::string &nodeName,
                            const std::map<std::string, std::string> &params,
                            const CreateInstanceRequest &req);

  /**
   * @brief Create message broker node
   */
  std::shared_ptr<cvedix_nodes::cvedix_node>
  createMessageBrokerNode(const std::string &nodeName,
                          const std::map<std::string, std::string> &params,
                          const CreateInstanceRequest &req);

  /**
   * @brief Create BA socket broker node
   */
  std::shared_ptr<cvedix_nodes::cvedix_node>
  createBASocketBrokerNode(const std::string &nodeName,
                           const std::map<std::string, std::string> &params,
                           const CreateInstanceRequest &req);

  /**
   * @brief Create embeddings socket broker node
   */
  std::shared_ptr<cvedix_nodes::cvedix_node> createEmbeddingsSocketBrokerNode(
      const std::string &nodeName,
      const std::map<std::string, std::string> &params,
      const CreateInstanceRequest &req);

  /**
   * @brief Create embeddings properties socket broker node
   */
  std::shared_ptr<cvedix_nodes::cvedix_node>
  createEmbeddingsPropertiesSocketBrokerNode(
      const std::string &nodeName,
      const std::map<std::string, std::string> &params,
      const CreateInstanceRequest &req);

  /**
   * @brief Create plate socket broker node
   */
  std::shared_ptr<cvedix_nodes::cvedix_node>
  createPlateSocketBrokerNode(const std::string &nodeName,
                              const std::map<std::string, std::string> &params,
                              const CreateInstanceRequest &req);

  /**
   * @brief Create expression socket broker node
   */
  std::shared_ptr<cvedix_nodes::cvedix_node> createExpressionSocketBrokerNode(
      const std::string &nodeName,
      const std::map<std::string, std::string> &params,
      const CreateInstanceRequest &req);


  // Note: Utility methods have been moved to separate utility classes:
  // - PipelineBuilderModelResolver: resolveModelPath, resolveModelByName, 
  //   listAvailableModels, mapDetectionSensitivity
  // - PipelineBuilderRequestUtils: getFilePath, getRTMPUrl, getRTSPUrl
};
