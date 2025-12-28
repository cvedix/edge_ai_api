#include "core/node_storage.h"
#include "core/env_config.h"
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <json/json.h>
#include <set>
#include <sstream>

NodeStorage::NodeStorage(const std::string &storage_dir)
    : storage_dir_(storage_dir) {
  ensureStorageDir();
}

void NodeStorage::ensureStorageDir() {
  // Extract subdir name from storage_dir_ for fallback
  std::filesystem::path path(storage_dir_);
  std::string subdir = path.filename().string();
  if (subdir.empty()) {
    subdir = "nodes"; // Default fallback subdir
  }

  // Use resolveDirectory with 3-tier fallback strategy
  std::string resolved_dir = EnvConfig::resolveDirectory(storage_dir_, subdir);

  // Update storage_dir_ if fallback was used
  if (resolved_dir != storage_dir_) {
    std::cerr << "[NodeStorage] ⚠ Storage directory changed from "
              << storage_dir_ << " to " << resolved_dir << " (fallback)"
              << std::endl;
    storage_dir_ = resolved_dir;
  }
}

std::string NodeStorage::getNodesFilePath() const {
  return storage_dir_ + "/nodes.json";
}

Json::Value NodeStorage::loadNodesFile() const {
  Json::Value root(Json::objectValue);

  // Extract subdir for checking all tiers
  std::filesystem::path path(storage_dir_);
  std::string subdir = path.filename().string();
  if (subdir.empty()) {
    subdir = "nodes";
  }

  // Get all possible directories in priority order
  std::vector<std::string> allDirs =
      EnvConfig::getAllPossibleDirectories(subdir);

  // Try to load from all tiers, merge data (later tiers override earlier ones)
  for (const auto &dir : allDirs) {
    std::string filepath = dir + "/nodes.json";
    if (!std::filesystem::exists(filepath)) {
      continue; // Skip if file doesn't exist
    }

    try {
      std::ifstream file(filepath);
      if (!file.is_open()) {
        continue;
      }

      Json::CharReaderBuilder builder;
      std::string errors;
      Json::Value tierData(Json::objectValue);
      if (Json::parseFromStream(builder, file, &tierData, &errors)) {
        // For nodes, we need to merge the "nodes" array
        if (tierData.isMember("nodes") && tierData["nodes"].isArray()) {
          if (!root.isMember("nodes")) {
            root["nodes"] = Json::Value(Json::arrayValue);
          }
          // Append nodes from this tier (avoid duplicates by nodeId)
          std::set<std::string> existingNodeIds;
          for (const auto &existingNode : root["nodes"]) {
            if (existingNode.isMember("nodeId") &&
                existingNode["nodeId"].isString()) {
              existingNodeIds.insert(existingNode["nodeId"].asString());
            }
          }
          for (const auto &newNode : tierData["nodes"]) {
            if (newNode.isMember("nodeId") && newNode["nodeId"].isString()) {
              std::string nodeId = newNode["nodeId"].asString();
              if (existingNodeIds.find(nodeId) == existingNodeIds.end()) {
                root["nodes"].append(newNode);
                existingNodeIds.insert(nodeId);
              }
            }
          }
        }
        // Merge other fields (version, total, etc.)
        for (const auto &key : tierData.getMemberNames()) {
          if (key != "nodes") {
            root[key] = tierData[key];
          }
        }
        std::cerr << "[NodeStorage] Loaded data from tier: " << dir
                  << std::endl;
      }
    } catch (const std::exception &e) {
      // Continue to next tier
      continue;
    }
  }

  return root;
}

bool NodeStorage::saveNodesFile(const Json::Value &nodes) {
  try {
    ensureStorageDir();

    std::string filepath = getNodesFilePath();
    std::cerr << "[NodeStorage] Saving nodes to: " << filepath << std::endl;

    std::ofstream file(filepath);
    if (!file.is_open()) {
      std::cerr << "[NodeStorage] Error: Failed to open file for writing: "
                << filepath << std::endl;
      return false;
    }

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "    "; // 4 spaces for indentation
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    writer->write(nodes, &file);
    file.close();

    std::cerr << "[NodeStorage] Successfully saved nodes file" << std::endl;
    return true;
  } catch (const std::exception &e) {
    std::cerr << "[NodeStorage] Exception saving nodes file: " << e.what()
              << std::endl;
    return false;
  } catch (...) {
    std::cerr << "[NodeStorage] Unknown exception saving nodes file"
              << std::endl;
    return false;
  }
}

Json::Value
NodeStorage::nodeToJson(const NodePoolManager::PreConfiguredNode &node) const {
  Json::Value json(Json::objectValue);

  json["nodeId"] = node.nodeId;
  json["templateId"] = node.templateId;
  json["inUse"] = node.inUse;

  // Parameters
  Json::Value params(Json::objectValue);
  for (const auto &[key, value] : node.parameters) {
    params[key] = value;
  }
  json["parameters"] = params;

  // Timestamp - convert steady_clock to string representation
  // We'll store as ISO 8601 string
  auto steady_now = std::chrono::steady_clock::now();
  auto elapsed = steady_now - node.createdAt;
  auto system_now = std::chrono::system_clock::now();
  auto system_time = system_now - elapsed;
  auto time_t = std::chrono::system_clock::to_time_t(system_time);
  std::stringstream ss;
  ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
  json["createdAt"] = ss.str();

  return json;
}

std::optional<NodePoolManager::PreConfiguredNode>
NodeStorage::jsonToNode(const Json::Value &json) const {
  try {
    NodePoolManager::PreConfiguredNode node;

    if (!json.isMember("nodeId") || !json["nodeId"].isString()) {
      return std::nullopt;
    }
    node.nodeId = json["nodeId"].asString();

    if (!json.isMember("templateId") || !json["templateId"].isString()) {
      return std::nullopt;
    }
    node.templateId = json["templateId"].asString();

    // Parse parameters
    if (json.isMember("parameters") && json["parameters"].isObject()) {
      const auto &paramsObj = json["parameters"];
      for (const auto &key : paramsObj.getMemberNames()) {
        node.parameters[key] = paramsObj[key].asString();
      }
    }

    // Parse inUse flag
    if (json.isMember("inUse") && json["inUse"].isBool()) {
      node.inUse = json["inUse"].asBool();
    } else {
      node.inUse = false; // Default to not in use
    }

    // Parse createdAt timestamp
    if (json.isMember("createdAt") && json["createdAt"].isString()) {
      std::string timeStr = json["createdAt"].asString();
      // Parse ISO 8601 format: YYYY-MM-DDTHH:MM:SSZ
      std::tm tm = {};
      std::istringstream ss(timeStr);
      ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
      if (!ss.fail()) {
        auto time_t = std::mktime(&tm);
        auto system_time = std::chrono::system_clock::from_time_t(time_t);
        auto steady_now = std::chrono::steady_clock::now();
        auto system_now = std::chrono::system_clock::now();
        auto elapsed = system_now - system_time;
        node.createdAt = steady_now - elapsed;
      } else {
        // If parsing fails, use current time
        node.createdAt = std::chrono::steady_clock::now();
      }
    } else {
      // If not present, use current time
      node.createdAt = std::chrono::steady_clock::now();
    }

    // Node instance is not stored (will be created when needed)
    node.node = nullptr;

    return node;
  } catch (const std::exception &e) {
    std::cerr << "[NodeStorage] Exception converting JSON to node: " << e.what()
              << std::endl;
    return std::nullopt;
  }
}

bool NodeStorage::saveAllNodes(
    const std::vector<NodePoolManager::PreConfiguredNode> &nodes) {
  try {
    Json::Value root(Json::objectValue);
    Json::Value nodesArray(Json::arrayValue);

    for (const auto &node : nodes) {
      nodesArray.append(nodeToJson(node));
    }

    root["nodes"] = nodesArray;
    root["version"] = "1.0";
    root["total"] = static_cast<Json::Int64>(nodes.size());

    return saveNodesFile(root);
  } catch (const std::exception &e) {
    std::cerr << "[NodeStorage] Exception saving nodes: " << e.what()
              << std::endl;
    return false;
  }
}

std::vector<NodePoolManager::PreConfiguredNode> NodeStorage::loadAllNodes() {
  std::vector<NodePoolManager::PreConfiguredNode> nodes;

  try {
    Json::Value root = loadNodesFile();

    if (root.isNull() || !root.isMember("nodes") || !root["nodes"].isArray()) {
      return nodes; // Return empty vector if no nodes file or invalid format
    }

    const auto &nodesArray = root["nodes"];
    for (const auto &nodeJson : nodesArray) {
      auto nodeOpt = jsonToNode(nodeJson);
      if (nodeOpt.has_value()) {
        nodes.push_back(nodeOpt.value());
      }
    }

    std::cerr << "[NodeStorage] Loaded " << nodes.size()
              << " pre-configured nodes from storage" << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "[NodeStorage] Exception loading nodes: " << e.what()
              << std::endl;
  }

  return nodes;
}
