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
 * @brief Other Node Factory for PipelineBuilder
 * 
 * Handles creation of other nodes (OSD, Tracking, etc.)
 */
class PipelineBuilderOtherNodes {
public:
  // ========== Tracking Nodes ==========

  /**
   * @brief Create SORT tracker node
   */
  static std::shared_ptr<cvedix_nodes::cvedix_node>
  createSORTTrackNode(const std::string &nodeName,
                      const std::map<std::string, std::string> &params);

  // ========== OSD Nodes ==========

  /**
   * @brief Create face OSD v2 node
   */
  static std::shared_ptr<cvedix_nodes::cvedix_node>
  createFaceOSDNode(const std::string &nodeName,
                    const std::map<std::string, std::string> &params);

  /**
   * @brief Create OSD v3 node (for masks and labels)
   */
  static std::shared_ptr<cvedix_nodes::cvedix_node>
  createOSDv3Node(const std::string &nodeName,
                  const std::map<std::string, std::string> &params,
                  const CreateInstanceRequest &req);
};

