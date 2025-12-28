#pragma once

#include "core/node_pool_manager.h"
#include <map>
#include <string>
#include <vector>

/**
 * @brief Node Template Registry
 *
 * Provides a centralized registry of node type metadata imported from SDK.
 * This allows automatic template generation from SDK node types without
 * hardcoding each template individually.
 */
class NodeTemplateRegistry {
public:
  /**
   * @brief Node type metadata structure
   */
  struct NodeTypeMetadata {
    std::string
        category; // "source", "detector", "processor", "destination", "broker"
    std::vector<std::string> requiredParameters;
    std::vector<std::string> optionalParameters;
    std::string displayName;
    std::string description;
  };

  /**
   * @brief Import templates from SDK node types
   * Automatically generates NodeTemplate objects from SDK node type metadata
   * @return Vector of NodeTemplate objects
   */
  static std::vector<NodePoolManager::NodeTemplate> importTemplatesFromSDK();

private:
  /**
   * @brief Node type metadata registry
   * Maps node type string to metadata (category, parameters, etc.)
   * This is populated from SDK node types supported in PipelineBuilder
   */
  static const std::map<std::string, NodeTypeMetadata> nodeTypeMetadata;

  /**
   * @brief Default parameters for node types
   * Maps node type to default parameter values
   */
  static const std::map<std::string, std::map<std::string, std::string>>
      defaultParameters;
};
