#pragma once

#include "models/create_instance_request.h"
#include <memory>
#include <map>
#include <string>

// Forward declarations
namespace cvedix_nodes {
class cvedix_node;
}

/**
 * @brief Behavior Analysis Node Factory for PipelineBuilder
 * 
 * Handles creation of all behavior analysis nodes (BA crossline, jam, stop) and their OSD nodes
 */
class PipelineBuilderBehaviorAnalysisNodes {
public:
  // ========== Behavior Analysis Nodes ==========

  /**
   * @brief Create BA crossline node
   */
  static std::shared_ptr<cvedix_nodes::cvedix_node>
  createBACrosslineNode(const std::string &nodeName,
                        const std::map<std::string, std::string> &params,
                        const CreateInstanceRequest &req);

  /**
   * @brief Create BA jam node
   */
  static std::shared_ptr<cvedix_nodes::cvedix_node>
  createBAJamNode(const std::string &nodeName,
                  const std::map<std::string, std::string> &params,
                  const CreateInstanceRequest &req);

  /**
   * @brief Create BA stop node
   */
  static std::shared_ptr<cvedix_nodes::cvedix_node>
  createBAStopNode(const std::string &nodeName,
                   const std::map<std::string, std::string> &params,
                   const CreateInstanceRequest &req);

  // ========== Behavior Analysis OSD Nodes ==========

  /**
   * @brief Create BA crossline OSD node
   */
  static std::shared_ptr<cvedix_nodes::cvedix_node>
  createBACrosslineOSDNode(const std::string &nodeName,
                           const std::map<std::string, std::string> &params,
                           const CreateInstanceRequest &req);

  /**
   * @brief Create BA jam OSD node
   */
  static std::shared_ptr<cvedix_nodes::cvedix_node>
  createBAJamOSDNode(const std::string &nodeName,
                     const std::map<std::string, std::string> &params);

  /**
   * @brief Create BA stop OSD node
   */
  static std::shared_ptr<cvedix_nodes::cvedix_node>
  createBAStopOSDNode(const std::string &nodeName,
                      const std::map<std::string, std::string> &params);
};

