#include "api/solution_handler.h"
#include "core/logger.h"
#include "core/logging_flags.h"
#include "core/node_pool_manager.h"
#include "core/node_template_registry.h"
#include "models/solution_config.h"
#include "solutions/solution_registry.h"
#include "solutions/solution_storage.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <drogon/HttpResponse.h>
#include <map>
#include <regex>
#include <set>

SolutionRegistry *SolutionHandler::solution_registry_ = nullptr;
SolutionStorage *SolutionHandler::solution_storage_ = nullptr;

void SolutionHandler::setSolutionRegistry(SolutionRegistry *registry) {
  solution_registry_ = registry;
}

void SolutionHandler::setSolutionStorage(SolutionStorage *storage) {
  solution_storage_ = storage;
}

std::string
SolutionHandler::extractSolutionId(const HttpRequestPtr &req) const {
  // Try getParameter first (standard way)
  std::string solutionId = req->getParameter("solutionId");

  // Fallback: extract from path if getParameter doesn't work
  if (solutionId.empty()) {
    std::string path = req->getPath();
    size_t solutionsPos = path.find("/solutions/");
    if (solutionsPos != std::string::npos) {
      size_t start = solutionsPos + 11; // length of "/solutions/"
      size_t end = path.find("/", start);
      if (end == std::string::npos) {
        end = path.length();
      }
      solutionId = path.substr(start, end - start);
    }
  }

  return solutionId;
}

bool SolutionHandler::validateSolutionId(const std::string &solutionId,
                                         std::string &error) const {
  if (solutionId.empty()) {
    error = "Solution ID cannot be empty";
    return false;
  }

  // Validate format: alphanumeric, underscore, hyphen only
  std::regex pattern("^[A-Za-z0-9_-]+$");
  if (!std::regex_match(solutionId, pattern)) {
    error = "Solution ID must contain only alphanumeric characters, "
            "underscores, and hyphens";
    return false;
  }

  return true;
}

Json::Value
SolutionHandler::solutionConfigToJson(const SolutionConfig &config) const {
  Json::Value json(Json::objectValue);

  json["solutionId"] = config.solutionId;
  json["solutionName"] = config.solutionName;
  json["solutionType"] = config.solutionType;
  json["isDefault"] = config.isDefault;

  // Convert pipeline
  Json::Value pipeline(Json::arrayValue);
  for (const auto &node : config.pipeline) {
    Json::Value nodeJson(Json::objectValue);
    nodeJson["nodeType"] = node.nodeType;
    nodeJson["nodeName"] = node.nodeName;

    Json::Value params(Json::objectValue);
    for (const auto &param : node.parameters) {
      params[param.first] = param.second;
    }
    nodeJson["parameters"] = params;

    pipeline.append(nodeJson);
  }
  json["pipeline"] = pipeline;

  // Convert defaults
  Json::Value defaults(Json::objectValue);
  for (const auto &def : config.defaults) {
    defaults[def.first] = def.second;
  }
  json["defaults"] = defaults;

  return json;
}

std::optional<SolutionConfig>
SolutionHandler::parseSolutionConfig(const Json::Value &json,
                                     std::string &error) const {
  try {
    SolutionConfig config;

    // Required: solutionId
    if (!json.isMember("solutionId") || !json["solutionId"].isString()) {
      error = "Missing required field: solutionId";
      return std::nullopt;
    }
    config.solutionId = json["solutionId"].asString();

    // Validate solutionId format
    std::string validationError;
    if (!validateSolutionId(config.solutionId, validationError)) {
      error = validationError;
      return std::nullopt;
    }

    // Required: solutionName
    if (!json.isMember("solutionName") || !json["solutionName"].isString()) {
      error = "Missing required field: solutionName";
      return std::nullopt;
    }
    config.solutionName = json["solutionName"].asString();

    if (config.solutionName.empty()) {
      error = "solutionName cannot be empty";
      return std::nullopt;
    }

    // Required: solutionType
    if (!json.isMember("solutionType") || !json["solutionType"].isString()) {
      error = "Missing required field: solutionType";
      return std::nullopt;
    }
    config.solutionType = json["solutionType"].asString();

    // SECURITY: Ignore isDefault from user input - users cannot create default
    // solutions Default solutions are hardcoded in the application and cannot
    // be created via API We explicitly ignore this field if provided by the
    // user
    config.isDefault = false;

    // Required: pipeline
    if (!json.isMember("pipeline") || !json["pipeline"].isArray()) {
      error = "Missing required field: pipeline (must be an array)";
      return std::nullopt;
    }

    if (json["pipeline"].size() == 0) {
      error = "pipeline cannot be empty";
      return std::nullopt;
    }

    // Parse pipeline nodes
    for (const auto &nodeJson : json["pipeline"]) {
      if (!nodeJson.isObject()) {
        error = "Pipeline nodes must be objects";
        return std::nullopt;
      }

      SolutionConfig::NodeConfig node;

      // Required: nodeType
      if (!nodeJson.isMember("nodeType") || !nodeJson["nodeType"].isString()) {
        error = "Pipeline node missing required field: nodeType";
        return std::nullopt;
      }
      node.nodeType = nodeJson["nodeType"].asString();

      // Required: nodeName
      if (!nodeJson.isMember("nodeName") || !nodeJson["nodeName"].isString()) {
        error = "Pipeline node missing required field: nodeName";
        return std::nullopt;
      }
      node.nodeName = nodeJson["nodeName"].asString();

      // Optional: parameters
      if (nodeJson.isMember("parameters") &&
          nodeJson["parameters"].isObject()) {
        for (const auto &key : nodeJson["parameters"].getMemberNames()) {
          if (nodeJson["parameters"][key].isString()) {
            node.parameters[key] = nodeJson["parameters"][key].asString();
          } else {
            error = "Pipeline node parameters must be strings";
            return std::nullopt;
          }
        }
      }

      config.pipeline.push_back(node);
    }

    // Optional: defaults
    if (json.isMember("defaults") && json["defaults"].isObject()) {
      for (const auto &key : json["defaults"].getMemberNames()) {
        if (json["defaults"][key].isString()) {
          config.defaults[key] = json["defaults"][key].asString();
        }
      }
    }

    return config;
  } catch (const std::exception &e) {
    error = std::string("Error parsing solution config: ") + e.what();
    return std::nullopt;
  }
}

HttpResponsePtr
SolutionHandler::createErrorResponse(int statusCode, const std::string &error,
                                     const std::string &message) const {
  Json::Value response(Json::objectValue);
  response["error"] = error;
  if (!message.empty()) {
    response["message"] = message;
  }

  auto resp = HttpResponse::newHttpJsonResponse(response);
  resp->setStatusCode(static_cast<HttpStatusCode>(statusCode));
  resp->addHeader("Access-Control-Allow-Origin", "*");
  resp->addHeader("Access-Control-Allow-Methods",
                  "GET, POST, PUT, DELETE, OPTIONS");
  resp->addHeader("Access-Control-Allow-Headers",
                  "Content-Type, Authorization");

  return resp;
}

HttpResponsePtr SolutionHandler::createSuccessResponse(const Json::Value &data,
                                                       int statusCode) const {
  auto resp = HttpResponse::newHttpJsonResponse(data);
  resp->setStatusCode(static_cast<HttpStatusCode>(statusCode));
  resp->addHeader("Access-Control-Allow-Origin", "*");
  resp->addHeader("Access-Control-Allow-Methods",
                  "GET, POST, PUT, DELETE, OPTIONS");
  resp->addHeader("Access-Control-Allow-Headers",
                  "Content-Type, Authorization");
  return resp;
}

void SolutionHandler::listSolutions(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {

  auto start_time = std::chrono::steady_clock::now();

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] GET /v1/core/solution - List solutions";
    PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
  }

  try {
    if (!solution_registry_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] GET /v1/core/solution - Error: Solution registry "
                      "not initialized";
      }
      callback(createErrorResponse(500, "Internal server error",
                                   "Solution registry not initialized"));
      return;
    }

    // Get all solutions
    auto allSolutions = solution_registry_->getAllSolutions();

    // Build response
    Json::Value response;
    Json::Value solutions(Json::arrayValue);

    int totalCount = 0;
    int defaultCount = 0;
    int customCount = 0;

    for (const auto &[solutionId, config] : allSolutions) {
      Json::Value solution;
      solution["solutionId"] = config.solutionId;
      solution["solutionName"] = config.solutionName;
      solution["solutionType"] = config.solutionType;
      solution["isDefault"] = config.isDefault;
      solution["pipelineNodeCount"] = static_cast<int>(config.pipeline.size());

      solutions.append(solution);
      totalCount++;
      if (config.isDefault) {
        defaultCount++;
      } else {
        customCount++;
      }
    }

    response["solutions"] = solutions;
    response["total"] = totalCount;
    response["default"] = defaultCount;
    response["custom"] = customCount;

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] GET /v1/core/solution - Success: " << totalCount
                << " solutions (default: " << defaultCount
                << ", custom: " << customCount << ") - " << duration.count()
                << "ms";
    }

    callback(createSuccessResponse(response));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] GET /v1/core/solution - Exception: " << e.what()
                 << " - " << duration.count() << "ms";
    }
    std::cerr << "[SolutionHandler] Exception: " << e.what() << std::endl;
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] GET /v1/core/solution - Unknown exception - "
                 << duration.count() << "ms";
    }
    std::cerr << "[SolutionHandler] Unknown exception" << std::endl;
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void SolutionHandler::getSolution(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {

  auto start_time = std::chrono::steady_clock::now();

  std::string solutionId = extractSolutionId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] GET /v1/core/solution/" << solutionId
              << " - Get solution";
  }

  try {
    if (!solution_registry_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] GET /v1/core/solution/" << solutionId
                   << " - Error: Solution registry not initialized";
      }
      callback(createErrorResponse(500, "Internal server error",
                                   "Solution registry not initialized"));
      return;
    }

    if (solutionId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] GET /v1/core/solution/{id} - Error: Solution "
                        "ID is empty";
      }
      callback(createErrorResponse(400, "Invalid request",
                                   "Solution ID is required"));
      return;
    }

    auto optConfig = solution_registry_->getSolution(solutionId);
    if (!optConfig.has_value()) {
      auto end_time = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
          end_time - start_time);
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] GET /v1/core/solution/" << solutionId
                     << " - Not found - " << duration.count() << "ms";
      }
      callback(createErrorResponse(404, "Not found",
                                   "Solution not found: " + solutionId));
      return;
    }

    Json::Value response = solutionConfigToJson(optConfig.value());

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] GET /v1/core/solution/" << solutionId
                << " - Success - " << duration.count() << "ms";
    }

    callback(createSuccessResponse(response));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] GET /v1/core/solution/" << solutionId
                 << " - Exception: " << e.what() << " - " << duration.count()
                 << "ms";
    }
    std::cerr << "[SolutionHandler] Exception: " << e.what() << std::endl;
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] GET /v1/core/solution/" << solutionId
                 << " - Unknown exception - " << duration.count() << "ms";
    }
    std::cerr << "[SolutionHandler] Unknown exception" << std::endl;
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void SolutionHandler::createSolution(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {

  auto start_time = std::chrono::steady_clock::now();

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] POST /v1/core/solution - Create solution";
    PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
  }

  try {
    if (!solution_registry_ || !solution_storage_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] POST /v1/core/solution - Error: Solution "
                      "registry or storage not initialized";
      }
      callback(
          createErrorResponse(500, "Internal server error",
                              "Solution registry or storage not initialized"));
      return;
    }

    // Parse JSON body
    auto json = req->getJsonObject();
    if (!json) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING
            << "[API] POST /v1/core/solution - Error: Invalid JSON body";
      }
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }

    // Parse solution config
    std::string parseError;
    auto optConfig = parseSolutionConfig(*json, parseError);
    if (!optConfig.has_value()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] POST /v1/core/solution - Parse error: "
                     << parseError;
      }
      callback(createErrorResponse(400, "Invalid request", parseError));
      return;
    }

    SolutionConfig config = optConfig.value();

    // Ensure isDefault is false for custom solutions
    config.isDefault = false;

    // Check if solution already exists
    if (solution_registry_->hasSolution(config.solutionId)) {
      // Check if it's a default solution - cannot override default solutions
      if (solution_registry_->isDefaultSolution(config.solutionId)) {
        if (isApiLoggingEnabled()) {
          PLOG_WARNING << "[API] POST /v1/core/solution - Error: Cannot "
                          "override default solution: "
                       << config.solutionId;
        }
        callback(createErrorResponse(
            403, "Forbidden",
            "Cannot create solution with ID '" + config.solutionId +
                "': This ID is reserved for a default system solution. Please "
                "use a different solution ID."));
        return;
      }

      // Solution exists and is not default - return conflict
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] POST /v1/core/solution - Error: Solution "
                        "already exists: "
                     << config.solutionId;
      }
      callback(createErrorResponse(
          409, "Conflict",
          "Solution with ID '" + config.solutionId +
              "' already exists. Use PUT /v1/core/solution/" +
              config.solutionId + " to update it."));
      return;
    }

    // Validate node types in solution - ensure all node types are supported
    auto &nodePool = NodePoolManager::getInstance();
    auto allTemplates = nodePool.getAllTemplates();
    std::set<std::string> supportedNodeTypes;
    for (const auto &template_ : allTemplates) {
      supportedNodeTypes.insert(template_.nodeType);
    }

    std::vector<std::string> unsupportedNodeTypes;
    for (const auto &nodeConfig : config.pipeline) {
      if (supportedNodeTypes.find(nodeConfig.nodeType) ==
          supportedNodeTypes.end()) {
        unsupportedNodeTypes.push_back(nodeConfig.nodeType);
      }
    }

    if (!unsupportedNodeTypes.empty()) {
      std::string errorMsg = "Solution contains unsupported node types: ";
      for (size_t i = 0; i < unsupportedNodeTypes.size(); ++i) {
        errorMsg += unsupportedNodeTypes[i];
        if (i < unsupportedNodeTypes.size() - 1) {
          errorMsg += ", ";
        }
      }
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] POST /v1/core/solution - Error: " << errorMsg;
      }
      callback(createErrorResponse(400, "Invalid request", errorMsg));
      return;
    }

    // Register solution
    solution_registry_->registerSolution(config);

    // Create nodes for node types in this custom solution (if they don't exist)
    // Note: These are user-created nodes, not default nodes
    size_t nodesCreated = nodePool.createNodesFromSolution(config);
    if (nodesCreated > 0) {
      std::cerr << "[SolutionHandler] Created " << nodesCreated
                << " nodes for custom solution: " << config.solutionId
                << std::endl;
    }

    // Save to storage
    std::cerr << "[SolutionHandler] Attempting to save solution to storage: "
              << config.solutionId << std::endl;
    if (!solution_storage_->saveSolution(config)) {
      std::cerr
          << "[SolutionHandler] Warning: Failed to save solution to storage: "
          << config.solutionId << std::endl;
    } else {
      std::cerr
          << "[SolutionHandler] ✓ Solution saved successfully to storage: "
          << config.solutionId << std::endl;
    }

    Json::Value response = solutionConfigToJson(config);
    response["message"] = "Solution created successfully";

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] POST /v1/core/solution - Success: Created solution "
                << config.solutionId << " (" << config.solutionName << ") - "
                << duration.count() << "ms";
    }

    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k201Created);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers",
                    "Content-Type, Authorization");

    callback(resp);

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] POST /v1/core/solution - Exception: " << e.what()
                 << " - " << duration.count() << "ms";
    }
    std::cerr << "[SolutionHandler] Exception: " << e.what() << std::endl;
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] POST /v1/core/solution - Unknown exception - "
                 << duration.count() << "ms";
    }
    std::cerr << "[SolutionHandler] Unknown exception" << std::endl;
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void SolutionHandler::updateSolution(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {

  auto start_time = std::chrono::steady_clock::now();

  std::string solutionId = extractSolutionId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] PUT /v1/core/solution/" << solutionId
              << " - Update solution";
  }

  try {
    if (!solution_registry_ || !solution_storage_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] PUT /v1/core/solution/" << solutionId
                   << " - Error: Solution registry or storage not initialized";
      }
      callback(
          createErrorResponse(500, "Internal server error",
                              "Solution registry or storage not initialized"));
      return;
    }

    if (solutionId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] PUT /v1/core/solution/{id} - Error: Solution "
                        "ID is empty";
      }
      callback(createErrorResponse(400, "Invalid request",
                                   "Solution ID is required"));
      return;
    }

    // Check if solution exists
    if (!solution_registry_->hasSolution(solutionId)) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] PUT /v1/core/solution/" << solutionId
                     << " - Not found";
      }
      callback(createErrorResponse(404, "Not found",
                                   "Solution not found: " + solutionId));
      return;
    }

    // Check if it's a default solution
    if (solution_registry_->isDefaultSolution(solutionId)) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] PUT /v1/core/solution/" << solutionId
                     << " - Cannot update default solution";
      }
      callback(createErrorResponse(
          403, "Forbidden", "Cannot update default solution: " + solutionId));
      return;
    }

    // Parse JSON body
    auto json = req->getJsonObject();
    if (!json) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] PUT /v1/core/solution/" << solutionId
                     << " - Error: Invalid JSON body";
      }
      callback(createErrorResponse(400, "Invalid request",
                                   "Request body must be valid JSON"));
      return;
    }

    // Parse solution config
    std::string parseError;
    auto optConfig = parseSolutionConfig(*json, parseError);
    if (!optConfig.has_value()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] PUT /v1/core/solution/" << solutionId
                     << " - Parse error: " << parseError;
      }
      callback(createErrorResponse(400, "Invalid request", parseError));
      return;
    }

    SolutionConfig config = optConfig.value();

    // Ensure solutionId matches
    if (config.solutionId != solutionId) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] PUT /v1/core/solution/" << solutionId
                     << " - Solution ID mismatch";
      }
      callback(
          createErrorResponse(400, "Invalid request",
                              "Solution ID in body must match URL parameter"));
      return;
    }

    // Ensure isDefault is false for custom solutions
    config.isDefault = false;

    // Validate node types in solution - ensure all node types are supported
    auto &nodePool = NodePoolManager::getInstance();
    auto allTemplates = nodePool.getAllTemplates();
    std::set<std::string> supportedNodeTypes;
    for (const auto &template_ : allTemplates) {
      supportedNodeTypes.insert(template_.nodeType);
    }

    std::vector<std::string> unsupportedNodeTypes;
    for (const auto &nodeConfig : config.pipeline) {
      if (supportedNodeTypes.find(nodeConfig.nodeType) ==
          supportedNodeTypes.end()) {
        unsupportedNodeTypes.push_back(nodeConfig.nodeType);
      }
    }

    if (!unsupportedNodeTypes.empty()) {
      std::string errorMsg = "Solution contains unsupported node types: ";
      for (size_t i = 0; i < unsupportedNodeTypes.size(); ++i) {
        errorMsg += unsupportedNodeTypes[i];
        if (i < unsupportedNodeTypes.size() - 1) {
          errorMsg += ", ";
        }
      }
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] PUT /v1/core/solution/" << solutionId
                     << " - Error: " << errorMsg;
      }
      callback(createErrorResponse(400, "Invalid request", errorMsg));
      return;
    }

    // Update solution
    if (!solution_registry_->updateSolution(config)) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] PUT /v1/core/solution/" << solutionId
                   << " - Failed to update";
      }
      callback(createErrorResponse(500, "Internal server error",
                                   "Failed to update solution"));
      return;
    }

    // Create nodes for node types in this custom solution (if they don't exist)
    // Note: These are user-created nodes, not default nodes
    size_t nodesCreated = nodePool.createNodesFromSolution(config);
    if (nodesCreated > 0) {
      std::cerr << "[SolutionHandler] Created " << nodesCreated
                << " nodes for custom solution: " << config.solutionId
                << std::endl;
    }

    // Save to storage
    std::cerr
        << "[SolutionHandler] Attempting to save updated solution to storage: "
        << config.solutionId << std::endl;
    if (!solution_storage_->saveSolution(config)) {
      std::cerr
          << "[SolutionHandler] Warning: Failed to save solution to storage: "
          << config.solutionId << std::endl;
    } else {
      std::cerr << "[SolutionHandler] ✓ Solution updated and saved "
                   "successfully to storage: "
                << config.solutionId << std::endl;
    }

    Json::Value response = solutionConfigToJson(config);
    response["message"] = "Solution updated successfully";

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] PUT /v1/core/solution/" << solutionId
                << " - Success - " << duration.count() << "ms";
    }

    callback(createSuccessResponse(response));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] PUT /v1/core/solution/" << solutionId
                 << " - Exception: " << e.what() << " - " << duration.count()
                 << "ms";
    }
    std::cerr << "[SolutionHandler] Exception: " << e.what() << std::endl;
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] PUT /v1/core/solution/" << solutionId
                 << " - Unknown exception - " << duration.count() << "ms";
    }
    std::cerr << "[SolutionHandler] Unknown exception" << std::endl;
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void SolutionHandler::deleteSolution(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {

  auto start_time = std::chrono::steady_clock::now();

  std::string solutionId = extractSolutionId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] DELETE /v1/core/solution/" << solutionId
              << " - Delete solution";
  }

  try {
    if (!solution_registry_ || !solution_storage_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] DELETE /v1/core/solution/" << solutionId
                   << " - Error: Solution registry or storage not initialized";
      }
      callback(
          createErrorResponse(500, "Internal server error",
                              "Solution registry or storage not initialized"));
      return;
    }

    if (solutionId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] DELETE /v1/core/solution/{id} - Error: "
                        "Solution ID is empty";
      }
      callback(createErrorResponse(400, "Invalid request",
                                   "Solution ID is required"));
      return;
    }

    // Check if solution exists
    if (!solution_registry_->hasSolution(solutionId)) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] DELETE /v1/core/solution/" << solutionId
                     << " - Not found";
      }
      callback(createErrorResponse(404, "Not found",
                                   "Solution not found: " + solutionId));
      return;
    }

    // Check if it's a default solution
    if (solution_registry_->isDefaultSolution(solutionId)) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] DELETE /v1/core/solution/" << solutionId
                     << " - Cannot delete default solution";
      }
      callback(createErrorResponse(
          403, "Forbidden", "Cannot delete default solution: " + solutionId));
      return;
    }

    // Delete from registry
    if (!solution_registry_->deleteSolution(solutionId)) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] DELETE /v1/core/solution/" << solutionId
                   << " - Failed to delete from registry";
      }
      callback(createErrorResponse(500, "Internal server error",
                                   "Failed to delete solution"));
      return;
    }

    // Delete from storage
    if (!solution_storage_->deleteSolution(solutionId)) {
      std::cerr
          << "[SolutionHandler] Warning: Failed to delete solution from storage"
          << std::endl;
    }

    Json::Value response(Json::objectValue);
    response["message"] = "Solution deleted successfully";
    response["solutionId"] = solutionId;

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] DELETE /v1/core/solution/" << solutionId
                << " - Success - " << duration.count() << "ms";
    }

    callback(createSuccessResponse(response));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] DELETE /v1/core/solution/" << solutionId
                 << " - Exception: " << e.what() << " - " << duration.count()
                 << "ms";
    }
    std::cerr << "[SolutionHandler] Exception: " << e.what() << std::endl;
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] DELETE /v1/core/solution/" << solutionId
                 << " - Unknown exception - " << duration.count() << "ms";
    }
    std::cerr << "[SolutionHandler] Unknown exception" << std::endl;
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void SolutionHandler::getSolutionParameters(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {

  auto start_time = std::chrono::steady_clock::now();

  std::string solutionId = extractSolutionId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] GET /v1/core/solution/" << solutionId
              << "/parameters - Get solution parameters";
  }

  try {
    if (!solution_registry_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR << "[API] GET /v1/core/solution/" << solutionId
                   << "/parameters - Error: Solution registry not initialized";
      }
      callback(createErrorResponse(500, "Internal server error",
                                   "Solution registry not initialized"));
      return;
    }

    if (solutionId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] GET /v1/core/solution/{id}/parameters - Error: "
                        "Solution ID is empty";
      }
      callback(createErrorResponse(400, "Invalid request",
                                   "Solution ID is required"));
      return;
    }

    auto optConfig = solution_registry_->getSolution(solutionId);
    if (!optConfig.has_value()) {
      auto end_time = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
          end_time - start_time);
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] GET /v1/core/solution/" << solutionId
                     << "/parameters - Not found - " << duration.count()
                     << "ms";
      }
      callback(createErrorResponse(404, "Not found",
                                   "Solution not found: " + solutionId));
      return;
    }

    const SolutionConfig &config = optConfig.value();

    // Extract parameters from solution
    Json::Value response(Json::objectValue);
    response["solutionId"] = config.solutionId;
    response["solutionName"] = config.solutionName;
    response["solutionType"] = config.solutionType;

    // Get node templates for detailed parameter information
    auto &nodePool = NodePoolManager::getInstance();
    auto allTemplates = nodePool.getAllTemplates();
    std::map<std::string, NodePoolManager::NodeTemplate> templatesByType;
    for (const auto &template_ : allTemplates) {
      templatesByType[template_.nodeType] = template_;
    }

    // Extract variables from pipeline nodes and defaults
    std::set<std::string> allParams;      // All parameters found
    std::set<std::string> requiredParams; // Parameters that are required
    std::map<std::string, std::string> paramDefaults;
    std::map<std::string, std::string> paramDescriptions;
    std::map<std::string, std::string> paramTypes; // Parameter types
    std::map<std::string, std::vector<std::string>>
        paramUsedInNodes; // Which nodes use this param

    // Extract from pipeline node parameters
    std::regex varPattern("\\$\\{([A-Za-z0-9_]+)\\}");
    Json::Value pipelineNodes(Json::arrayValue);

    for (const auto &node : config.pipeline) {
      Json::Value nodeInfo(Json::objectValue);
      nodeInfo["nodeType"] = node.nodeType;
      nodeInfo["nodeName"] = node.nodeName;

      // Get template info for this node type
      auto templateIt = templatesByType.find(node.nodeType);
      if (templateIt != templatesByType.end()) {
        const auto &nodeTemplate = templateIt->second;
        nodeInfo["displayName"] = nodeTemplate.displayName;
        nodeInfo["description"] = nodeTemplate.description;
        nodeInfo["category"] = nodeTemplate.category;

        // Add required and optional parameters from template
        Json::Value requiredParamsArray(Json::arrayValue);
        for (const auto &param : nodeTemplate.requiredParameters) {
          requiredParamsArray.append(param);
        }
        nodeInfo["requiredParameters"] = requiredParamsArray;

        Json::Value optionalParamsArray(Json::arrayValue);
        for (const auto &param : nodeTemplate.optionalParameters) {
          optionalParamsArray.append(param);
        }
        nodeInfo["optionalParameters"] = optionalParamsArray;

        // Add default parameters
        Json::Value defaultParams(Json::objectValue);
        for (const auto &[key, value] : nodeTemplate.defaultParameters) {
          defaultParams[key] = value;
        }
        nodeInfo["defaultParameters"] = defaultParams;
      }

      // Extract variables from node parameters
      Json::Value nodeParams(Json::objectValue);
      for (const auto &param : node.parameters) {
        nodeParams[param.first] = param.second;

        std::string value = param.second;
        std::sregex_iterator iter(value.begin(), value.end(), varPattern);
        std::sregex_iterator end;

        for (; iter != end; ++iter) {
          std::string varName = (*iter)[1].str();
          allParams.insert(varName);
          // Initially mark as required (will be overridden if has default)
          requiredParams.insert(varName);

          // Track which nodes use this parameter
          paramUsedInNodes[varName].push_back(node.nodeType);

          // Try to infer description from parameter name and node template
          if (paramDescriptions.find(varName) == paramDescriptions.end()) {
            std::string desc = "Parameter for " + node.nodeType + " node";
            if (templateIt != templatesByType.end()) {
              const auto &nodeTemplate = templateIt->second;
              // Check if this parameter is in required/optional list
              bool isRequired =
                  std::find(nodeTemplate.requiredParameters.begin(),
                            nodeTemplate.requiredParameters.end(),
                            param.first) !=
                  nodeTemplate.requiredParameters.end();
              if (isRequired) {
                desc = "Required parameter for " + node.nodeType + " (" +
                       param.first + ")";
              } else {
                desc = "Optional parameter for " + node.nodeType + " (" +
                       param.first + ")";
              }
            } else if (param.first == "url" ||
                       varName.find("URL") != std::string::npos) {
              desc = "URL for " + node.nodeType;
            } else if (varName.find("MODEL") != std::string::npos ||
                       varName.find("PATH") != std::string::npos) {
              desc = "File path for " + node.nodeType;
            }
            paramDescriptions[varName] = desc;
          }
        }
      }
      nodeInfo["parameters"] = nodeParams;
      pipelineNodes.append(nodeInfo);
    }

    response["pipeline"] = pipelineNodes;

    // Extract from defaults
    // If a parameter has a default value (literal, not containing variables),
    // it's optional If default value contains variables, those variables are
    // still required
    for (const auto &def : config.defaults) {
      std::string paramName = def.first;
      std::string defaultValue = def.second;
      allParams.insert(paramName);
      paramDefaults[paramName] = defaultValue;

      // Check if default value contains variables
      std::sregex_iterator iter(defaultValue.begin(), defaultValue.end(),
                                varPattern);
      std::sregex_iterator end;
      bool hasVariables = false;
      for (; iter != end; ++iter) {
        std::string varName = (*iter)[1].str();
        allParams.insert(varName);
        requiredParams.insert(varName); // Variables in defaults are required
        hasVariables = true;
      }

      // If default value is literal (no variables), the parameter is optional
      if (!hasVariables) {
        requiredParams.erase(paramName);
      }
    }

    // Build additionalParams schema
    Json::Value additionalParams(Json::objectValue);
    Json::Value required(Json::arrayValue);

    for (const auto &param : allParams) {
      Json::Value paramInfo(Json::objectValue);
      paramInfo["name"] = param;
      paramInfo["type"] = "string";

      // Check if has default
      auto defIt = paramDefaults.find(param);
      bool isRequired = (requiredParams.find(param) != requiredParams.end());

      if (defIt != paramDefaults.end()) {
        std::string defaultValue = defIt->second;
        // Only set default if it's a literal value (doesn't contain variables)
        std::sregex_iterator iter(defaultValue.begin(), defaultValue.end(),
                                  varPattern);
        std::sregex_iterator end;
        if (iter == end) {
          // No variables in default, it's a literal value
          paramInfo["default"] = defaultValue;
          paramInfo["required"] = false;
        } else {
          // Default contains variables, parameter is still required
          paramInfo["required"] = true;
          required.append(param);
        }
      } else {
        paramInfo["required"] = isRequired;
        if (isRequired) {
          required.append(param);
        }
      }

      // Add description
      auto descIt = paramDescriptions.find(param);
      if (descIt != paramDescriptions.end()) {
        paramInfo["description"] = descIt->second;
      } else {
        // Generate default description
        if (param.find("URL") != std::string::npos) {
          paramInfo["description"] = "URL parameter";
        } else if (param.find("PATH") != std::string::npos ||
                   param.find("MODEL") != std::string::npos) {
          paramInfo["description"] = "File path parameter";
        } else {
          paramInfo["description"] = "Solution parameter";
        }
      }

      // Add information about which nodes use this parameter
      auto nodesIt = paramUsedInNodes.find(param);
      if (nodesIt != paramUsedInNodes.end()) {
        Json::Value usedInNodes(Json::arrayValue);
        for (const auto &nodeType : nodesIt->second) {
          usedInNodes.append(nodeType);
        }
        paramInfo["usedInNodes"] = usedInNodes;
      }

      additionalParams[param] = paramInfo;
    }

    response["additionalParams"] = additionalParams;
    response["requiredAdditionalParams"] = required;

    // Add standard instance creation fields
    Json::Value standardFields(Json::objectValue);

    // Required fields
    Json::Value requiredFields(Json::arrayValue);
    requiredFields.append("name");
    standardFields["name"] = Json::Value(Json::objectValue);
    standardFields["name"]["type"] = "string";
    standardFields["name"]["required"] = true;
    standardFields["name"]["description"] =
        "Instance name (pattern: ^[A-Za-z0-9 -_]+$)";
    standardFields["name"]["pattern"] = "^[A-Za-z0-9 -_]+$";

    // Optional fields
    standardFields["group"] = Json::Value(Json::objectValue);
    standardFields["group"]["type"] = "string";
    standardFields["group"]["required"] = false;
    standardFields["group"]["description"] =
        "Group name (pattern: ^[A-Za-z0-9 -_]+$)";
    standardFields["group"]["pattern"] = "^[A-Za-z0-9 -_]+$";

    standardFields["solution"] = Json::Value(Json::objectValue);
    standardFields["solution"]["type"] = "string";
    standardFields["solution"]["required"] = false;
    standardFields["solution"]["description"] =
        "Solution ID (must match existing solution)";
    standardFields["solution"]["default"] = solutionId;

    standardFields["persistent"] = Json::Value(Json::objectValue);
    standardFields["persistent"]["type"] = "boolean";
    standardFields["persistent"]["required"] = false;
    standardFields["persistent"]["description"] = "Save instance to JSON file";
    standardFields["persistent"]["default"] = false;

    standardFields["autoStart"] = Json::Value(Json::objectValue);
    standardFields["autoStart"]["type"] = "boolean";
    standardFields["autoStart"]["required"] = false;
    standardFields["autoStart"]["description"] =
        "Automatically start instance when created";
    standardFields["autoStart"]["default"] = false;

    standardFields["frameRateLimit"] = Json::Value(Json::objectValue);
    standardFields["frameRateLimit"]["type"] = "integer";
    standardFields["frameRateLimit"]["required"] = false;
    standardFields["frameRateLimit"]["description"] = "Frame rate limit (FPS)";
    standardFields["frameRateLimit"]["default"] = 0;
    standardFields["frameRateLimit"]["minimum"] = 0;

    standardFields["detectionSensitivity"] = Json::Value(Json::objectValue);
    standardFields["detectionSensitivity"]["type"] = "string";
    standardFields["detectionSensitivity"]["required"] = false;
    standardFields["detectionSensitivity"]["description"] =
        "Detection sensitivity level";
    standardFields["detectionSensitivity"]["enum"] =
        Json::Value(Json::arrayValue);
    standardFields["detectionSensitivity"]["enum"].append("Low");
    standardFields["detectionSensitivity"]["enum"].append("Medium");
    standardFields["detectionSensitivity"]["enum"].append("High");
    standardFields["detectionSensitivity"]["enum"].append("Normal");
    standardFields["detectionSensitivity"]["enum"].append("Slow");
    standardFields["detectionSensitivity"]["default"] = "Low";

    standardFields["detectorMode"] = Json::Value(Json::objectValue);
    standardFields["detectorMode"]["type"] = "string";
    standardFields["detectorMode"]["required"] = false;
    standardFields["detectorMode"]["description"] = "Detector mode";
    standardFields["detectorMode"]["enum"] = Json::Value(Json::arrayValue);
    standardFields["detectorMode"]["enum"].append("SmartDetection");
    standardFields["detectorMode"]["enum"].append("FullRegionInference");
    standardFields["detectorMode"]["enum"].append("MosaicInference");
    standardFields["detectorMode"]["default"] = "SmartDetection";

    standardFields["metadataMode"] = Json::Value(Json::objectValue);
    standardFields["metadataMode"]["type"] = "boolean";
    standardFields["metadataMode"]["required"] = false;
    standardFields["metadataMode"]["description"] = "Enable metadata mode";
    standardFields["metadataMode"]["default"] = false;

    standardFields["statisticsMode"] = Json::Value(Json::objectValue);
    standardFields["statisticsMode"]["type"] = "boolean";
    standardFields["statisticsMode"]["required"] = false;
    standardFields["statisticsMode"]["description"] = "Enable statistics mode";
    standardFields["statisticsMode"]["default"] = false;

    standardFields["diagnosticsMode"] = Json::Value(Json::objectValue);
    standardFields["diagnosticsMode"]["type"] = "boolean";
    standardFields["diagnosticsMode"]["required"] = false;
    standardFields["diagnosticsMode"]["description"] =
        "Enable diagnostics mode";
    standardFields["diagnosticsMode"]["default"] = false;

    standardFields["debugMode"] = Json::Value(Json::objectValue);
    standardFields["debugMode"]["type"] = "boolean";
    standardFields["debugMode"]["required"] = false;
    standardFields["debugMode"]["description"] = "Enable debug mode";
    standardFields["debugMode"]["default"] = false;

    response["standardFields"] = standardFields;
    response["requiredStandardFields"] = requiredFields;

    // Add flexible input/output parameters info
    // These are auto-detected by pipeline builder and can be added to any
    // instance
    Json::Value flexibleIO(Json::objectValue);

    // Input options
    Json::Value inputOptions(Json::objectValue);
    inputOptions["description"] =
        "Choose ONE input source. Pipeline builder auto-detects input type.";
    Json::Value inputParams(Json::objectValue);

    inputParams["FILE_PATH"] = Json::Value(Json::objectValue);
    inputParams["FILE_PATH"]["type"] = "string";
    inputParams["FILE_PATH"]["description"] =
        "Local video file path or URL (rtsp://, rtmp://, http://)";
    inputParams["FILE_PATH"]["example"] =
        "./cvedix_data/test_video/example.mp4";

    inputParams["RTSP_SRC_URL"] = Json::Value(Json::objectValue);
    inputParams["RTSP_SRC_URL"]["type"] = "string";
    inputParams["RTSP_SRC_URL"]["description"] =
        "RTSP stream URL (overrides FILE_PATH)";
    inputParams["RTSP_SRC_URL"]["example"] = "rtsp://camera-ip:8554/stream";

    inputParams["RTMP_SRC_URL"] = Json::Value(Json::objectValue);
    inputParams["RTMP_SRC_URL"]["type"] = "string";
    inputParams["RTMP_SRC_URL"]["description"] = "RTMP input stream URL";
    inputParams["RTMP_SRC_URL"]["example"] =
        "rtmp://input-server:1935/live/input";

    inputParams["HLS_URL"] = Json::Value(Json::objectValue);
    inputParams["HLS_URL"]["type"] = "string";
    inputParams["HLS_URL"]["description"] = "HLS stream URL (.m3u8)";
    inputParams["HLS_URL"]["example"] = "http://example.com/stream.m3u8";

    inputOptions["parameters"] = inputParams;
    flexibleIO["input"] = inputOptions;

    // Output options (can combine multiple)
    Json::Value outputOptions(Json::objectValue);
    outputOptions["description"] =
        "Add any combination of outputs. Pipeline builder auto-adds nodes.";
    Json::Value outputParams(Json::objectValue);

    // MQTT
    outputParams["MQTT_BROKER_URL"] = Json::Value(Json::objectValue);
    outputParams["MQTT_BROKER_URL"]["type"] = "string";
    outputParams["MQTT_BROKER_URL"]["description"] =
        "MQTT broker address (enables MQTT output)";
    outputParams["MQTT_BROKER_URL"]["example"] = "localhost";

    outputParams["MQTT_PORT"] = Json::Value(Json::objectValue);
    outputParams["MQTT_PORT"]["type"] = "string";
    outputParams["MQTT_PORT"]["description"] = "MQTT broker port";
    outputParams["MQTT_PORT"]["default"] = "1883";

    outputParams["MQTT_TOPIC"] = Json::Value(Json::objectValue);
    outputParams["MQTT_TOPIC"]["type"] = "string";
    outputParams["MQTT_TOPIC"]["description"] = "MQTT topic for events";
    outputParams["MQTT_TOPIC"]["default"] = "events";

    // RTMP output
    outputParams["RTMP_URL"] = Json::Value(Json::objectValue);
    outputParams["RTMP_URL"]["type"] = "string";
    outputParams["RTMP_URL"]["description"] =
        "RTMP destination URL (enables RTMP streaming output)";
    outputParams["RTMP_URL"]["example"] = "rtmp://server:1935/live/stream";

    // Screen display
    outputParams["ENABLE_SCREEN_DES"] = Json::Value(Json::objectValue);
    outputParams["ENABLE_SCREEN_DES"]["type"] = "string";
    outputParams["ENABLE_SCREEN_DES"]["description"] =
        "Enable screen display (true/false)";
    outputParams["ENABLE_SCREEN_DES"]["default"] = "false";

    // Recording
    outputParams["RECORD_PATH"] = Json::Value(Json::objectValue);
    outputParams["RECORD_PATH"]["type"] = "string";
    outputParams["RECORD_PATH"]["description"] =
        "Path for video recording output";
    outputParams["RECORD_PATH"]["example"] = "./output/recordings";

    outputOptions["parameters"] = outputParams;
    flexibleIO["output"] = outputOptions;

    // Zone info for BA solutions
    Json::Value zoneParams(Json::objectValue);
    zoneParams["ZONE_ID"] = Json::Value(Json::objectValue);
    zoneParams["ZONE_ID"]["type"] = "string";
    zoneParams["ZONE_ID"]["description"] = "Zone identifier for BA events";
    zoneParams["ZONE_ID"]["default"] = "zone_1";

    zoneParams["ZONE_NAME"] = Json::Value(Json::objectValue);
    zoneParams["ZONE_NAME"]["type"] = "string";
    zoneParams["ZONE_NAME"]["description"] = "Zone display name for BA events";
    zoneParams["ZONE_NAME"]["default"] = "Default Zone";

    flexibleIO["zoneInfo"] = zoneParams;

    response["flexibleInputOutput"] = flexibleIO;

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] GET /v1/core/solution/" << solutionId
                << "/parameters - Success - " << duration.count() << "ms";
    }

    callback(createSuccessResponse(response));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] GET /v1/core/solution/" << solutionId
                 << "/parameters - Exception: " << e.what() << " - "
                 << duration.count() << "ms";
    }
    std::cerr << "[SolutionHandler] Exception: " << e.what() << std::endl;
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] GET /v1/core/solution/" << solutionId
                 << "/parameters - Unknown exception - " << duration.count()
                 << "ms";
    }
    std::cerr << "[SolutionHandler] Unknown exception" << std::endl;
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void SolutionHandler::getSolutionInstanceBody(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {

  auto start_time = std::chrono::steady_clock::now();

  std::string solutionId = extractSolutionId(req);

  if (isApiLoggingEnabled()) {
    PLOG_INFO << "[API] GET /v1/core/solution/" << solutionId
              << "/instance-body - Get instance body";
  }

  try {
    if (!solution_registry_) {
      if (isApiLoggingEnabled()) {
        PLOG_ERROR
            << "[API] GET /v1/core/solution/" << solutionId
            << "/instance-body - Error: Solution registry not initialized";
      }
      callback(createErrorResponse(500, "Internal server error",
                                   "Solution registry not initialized"));
      return;
    }

    if (solutionId.empty()) {
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] GET /v1/core/solution/{id}/instance-body - "
                        "Error: Solution ID is empty";
      }
      callback(createErrorResponse(400, "Invalid request",
                                   "Solution ID is required"));
      return;
    }

    auto optConfig = solution_registry_->getSolution(solutionId);
    if (!optConfig.has_value()) {
      auto end_time = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
          end_time - start_time);
      if (isApiLoggingEnabled()) {
        PLOG_WARNING << "[API] GET /v1/core/solution/" << solutionId
                     << "/instance-body - Not found - " << duration.count()
                     << "ms";
      }
      callback(createErrorResponse(404, "Not found",
                                   "Solution not found: " + solutionId));
      return;
    }

    const SolutionConfig &config = optConfig.value();

    // Build example request body
    Json::Value body(Json::objectValue);

    // Standard fields
    body["name"] = "example_instance";
    body["group"] = "default";
    body["solution"] = solutionId;
    body["persistent"] = false;
    body["autoStart"] = false;
    body["frameRateLimit"] = 0;
    body["detectionSensitivity"] = "Low";
    body["detectorMode"] = "SmartDetection";
    body["metadataMode"] = false;
    body["statisticsMode"] = false;
    body["diagnosticsMode"] = false;
    body["debugMode"] = false;

    // Get node templates for additional parameter information
    auto &nodePool = NodePoolManager::getInstance();
    auto allTemplates = nodePool.getAllTemplates();
    std::map<std::string, NodePoolManager::NodeTemplate> templatesByType;
    for (const auto &template_ : allTemplates) {
      templatesByType[template_.nodeType] = template_;
    }

    // Extract variables from pipeline and defaults
    std::set<std::string> allParams;
    std::map<std::string, std::string> paramDefaults;
    std::regex varPattern("\\$\\{([A-Za-z0-9_]+)\\}");

    // Extract from pipeline nodes
    for (const auto &node : config.pipeline) {
      // Extract variables from node parameters
      for (const auto &param : node.parameters) {
        std::string value = param.second;
        std::sregex_iterator iter(value.begin(), value.end(), varPattern);
        std::sregex_iterator end;
        for (; iter != end; ++iter) {
          std::string varName = (*iter)[1].str();
          allParams.insert(varName);
        }
      }

      // Extract variables from nodeName template (e.g., {instanceId})
      std::string nodeName = node.nodeName;
      std::regex instanceIdPattern("\\{([A-Za-z0-9_]+)\\}");
      std::sregex_iterator iter(nodeName.begin(), nodeName.end(),
                                instanceIdPattern);
      std::sregex_iterator end;
      for (; iter != end; ++iter) {
        std::string varName = (*iter)[1].str();
        // Note: {instanceId} is handled automatically, but we track it for
        // completeness
        if (varName != "instanceId") {
          // Convert {VAR} to ${VAR} format for consistency
          allParams.insert(varName);
        }
      }

      // Add required parameters from node template if not already in pipeline
      // parameters
      auto templateIt = templatesByType.find(node.nodeType);
      if (templateIt != templatesByType.end()) {
        const auto &nodeTemplate = templateIt->second;

        // Check required parameters from template
        for (const auto &requiredParam : nodeTemplate.requiredParameters) {
          // Check if this required parameter is used in node.parameters
          bool foundInParams = false;
          for (const auto &param : node.parameters) {
            if (param.first == requiredParam) {
              foundInParams = true;
              // Check if value contains variable placeholder
              std::string value = param.second;
              std::sregex_iterator iter(value.begin(), value.end(), varPattern);
              std::sregex_iterator end;
              if (iter != end) {
                // Value contains variable, extract it
                for (; iter != end; ++iter) {
                  std::string varName = (*iter)[1].str();
                  allParams.insert(varName);
                }
              }
              break;
            }
          }

          // If required parameter not found in node.parameters,
          // check if it should be provided via additionalParams
          if (!foundInParams) {
            // Convert parameter name to uppercase with underscores (common
            // pattern)
            std::string upperParam = requiredParam;
            std::transform(upperParam.begin(), upperParam.end(),
                           upperParam.begin(), ::toupper);
            std::replace(upperParam.begin(), upperParam.end(), '-', '_');
            allParams.insert(upperParam);
          }
        }
      }
    }

    // Extract from defaults
    for (const auto &def : config.defaults) {
      std::string paramName = def.first;
      std::string defaultValue = def.second;
      allParams.insert(paramName);
      paramDefaults[paramName] = defaultValue;

      // Also extract variables from default values
      std::sregex_iterator iter(defaultValue.begin(), defaultValue.end(),
                                varPattern);
      std::sregex_iterator end;
      for (; iter != end; ++iter) {
        std::string varName = (*iter)[1].str();
        allParams.insert(varName);
      }
    }

    // Helper function to generate example value based on parameter name
    auto generateExampleValue = [](const std::string &param) -> std::string {
      std::string upperParam = param;
      std::transform(upperParam.begin(), upperParam.end(), upperParam.begin(),
                     ::toupper);

      // Flexible Input parameters (auto-detected by pipeline builder)
      if (upperParam == "RTSP_SRC_URL") {
        return "rtsp://camera-ip:8554/stream";
      } else if (upperParam == "RTMP_SRC_URL") {
        return "rtmp://input-server:1935/live/input";
      } else if (upperParam == "HLS_URL") {
        return "http://example.com/stream.m3u8";
      } else if (upperParam == "HTTP_URL") {
        return "http://example.com/video.mp4";
      }

      // URL parameters (legacy)
      if (upperParam.find("RTSP_URL") != std::string::npos ||
          upperParam == "RTSP_URL") {
        return "rtsp://example.com/stream";
      } else if (upperParam.find("RTMP_URL") != std::string::npos ||
                 upperParam == "RTMP_URL") {
        return "rtmp://example.com/live/stream";
      } else if (upperParam.find("URL") != std::string::npos) {
        return "http://example.com";
      }

      // Flexible Output - MQTT parameters
      if (upperParam == "MQTT_BROKER_URL") {
        return "localhost";
      } else if (upperParam == "MQTT_PORT") {
        return "1883";
      } else if (upperParam == "MQTT_TOPIC") {
        return "events";
      } else if (upperParam == "MQTT_USERNAME") {
        return "";
      } else if (upperParam == "MQTT_PASSWORD") {
        return "";
      }

      // Flexible Output - RTMP destination
      if (upperParam == "RTMP_URL") {
        return "rtmp://server:1935/live/stream";
      }

      // Flexible Output - Screen display
      if (upperParam == "ENABLE_SCREEN_DES") {
        return "false";
      }

      // Crossline specific parameters
      if (upperParam == "ZONE_ID") {
        return "zone_1";
      } else if (upperParam == "ZONE_NAME") {
        return "Default Zone";
      } else if (upperParam.find("CROSSLINE_START_X") != std::string::npos) {
        return "0";
      } else if (upperParam.find("CROSSLINE_START_Y") != std::string::npos) {
        return "250";
      } else if (upperParam.find("CROSSLINE_END_X") != std::string::npos) {
        return "700";
      } else if (upperParam.find("CROSSLINE_END_Y") != std::string::npos) {
        return "220";
      }

      // Model and file paths
      if (upperParam.find("MODEL_PATH") != std::string::npos ||
          upperParam == "MODEL_PATH") {
        return "./cvedix_data/models/example.model";
      } else if (upperParam.find("MODEL_CONFIG_PATH") != std::string::npos ||
                 upperParam == "MODEL_CONFIG_PATH") {
        return "./cvedix_data/models/example.config";
      } else if (upperParam.find("WEIGHTS_PATH") != std::string::npos ||
                 upperParam == "WEIGHTS_PATH") {
        return "./cvedix_data/models/example.weights";
      } else if (upperParam.find("CONFIG_PATH") != std::string::npos ||
                 upperParam == "CONFIG_PATH") {
        return "./cvedix_data/models/example.cfg";
      } else if (upperParam.find("LABELS_PATH") != std::string::npos ||
                 upperParam == "LABELS_PATH") {
        return "./cvedix_data/models/example.labels";
      } else if (upperParam.find("FILE_PATH") != std::string::npos ||
                 upperParam == "FILE_PATH") {
        return "./cvedix_data/test_video/example.mp4";
      } else if (upperParam.find("PATH") != std::string::npos ||
                 upperParam.find("MODEL") != std::string::npos) {
        return "./cvedix_data/models/example.model";
      }

      // Directory parameters
      if (upperParam.find("DIR") != std::string::npos ||
          upperParam.find("OUTPUT") != std::string::npos) {
        return "./output";
      }

      // Numeric parameters
      if (upperParam.find("WIDTH") != std::string::npos) {
        return "416";
      } else if (upperParam.find("HEIGHT") != std::string::npos) {
        return "416";
      } else if (upperParam.find("THRESHOLD") != std::string::npos ||
                 upperParam.find("SCORE") != std::string::npos) {
        return "0.5";
      } else if (upperParam.find("RATIO") != std::string::npos) {
        return "1.0";
      } else if (upperParam.find("CHANNEL") != std::string::npos) {
        return "0";
      }

      // Boolean-like parameters
      if (upperParam.find("ENABLE") != std::string::npos) {
        return "true";
      } else if (upperParam.find("DISABLE") != std::string::npos) {
        return "false";
      }

      // Default
      return "example_value";
    };

    // List of standard fields that should NOT be in additionalParams
    // These are already handled at root level
    std::set<std::string> standardFields = {"detectionSensitivity",
                                            "DETECTION_SENSITIVITY",
                                            "detectorMode",
                                            "DETECTOR_MODE",
                                            "sensorModality",
                                            "SENSOR_MODALITY",
                                            "movementSensitivity",
                                            "MOVEMENT_SENSITIVITY",
                                            "frameRateLimit",
                                            "FRAME_RATE_LIMIT",
                                            "metadataMode",
                                            "METADATA_MODE",
                                            "statisticsMode",
                                            "STATISTICS_MODE",
                                            "diagnosticsMode",
                                            "DIAGNOSTICS_MODE",
                                            "debugMode",
                                            "DEBUG_MODE",
                                            "autoStart",
                                            "AUTO_START",
                                            "persistent",
                                            "PERSISTENT",
                                            "name",
                                            "NAME",
                                            "group",
                                            "GROUP",
                                            "solution",
                                            "SOLUTION"};

    // Define output-related parameters
    std::set<std::string> outputParams = {
        "MQTT_BROKER_URL", "MQTT_PORT",         "MQTT_TOPIC",
        "MQTT_USERNAME",   "MQTT_PASSWORD",     "RTMP_URL",
        "RTMP_DES_URL",    "ENABLE_SCREEN_DES", "RECORD_PATH"};

    // Build additionalParams with input/output structure
    Json::Value additionalParams(Json::objectValue);
    Json::Value inputParams(Json::objectValue);
    Json::Value outputParamsObj(Json::objectValue);

    for (const auto &param : allParams) {
      // Skip standard fields that are already at root level
      std::string upperParam = param;
      std::transform(upperParam.begin(), upperParam.end(), upperParam.begin(),
                     ::toupper);
      if (standardFields.find(param) != standardFields.end() ||
          standardFields.find(upperParam) != standardFields.end()) {
        continue; // Skip this parameter
      }

      // Determine example value
      std::string exampleValue;
      auto defIt = paramDefaults.find(param);
      if (defIt != paramDefaults.end()) {
        std::string defaultValue = defIt->second;
        // Check if default contains variables
        std::sregex_iterator iter(defaultValue.begin(), defaultValue.end(),
                                  varPattern);
        std::sregex_iterator end;
        if (iter == end) {
          // Literal default value - use it
          exampleValue = defaultValue;
        } else {
          // Default contains variables, use generated example value
          exampleValue = generateExampleValue(param);
        }
      } else {
        // No default, use generated example value based on parameter name
        exampleValue = generateExampleValue(param);
      }

      // Put in output or input based on parameter type
      if (outputParams.find(upperParam) != outputParams.end()) {
        outputParamsObj[param] = exampleValue;
      } else {
        inputParams[param] = exampleValue;
      }
    }

    // Add default output parameters if not already present
    if (!outputParamsObj.isMember("ENABLE_SCREEN_DES")) {
      outputParamsObj["ENABLE_SCREEN_DES"] = "false";
    }
    if (!outputParamsObj.isMember("MQTT_BROKER_URL")) {
      outputParamsObj["MQTT_BROKER_URL"] = "";
    }
    if (!outputParamsObj.isMember("MQTT_PORT")) {
      outputParamsObj["MQTT_PORT"] = "1883";
    }
    if (!outputParamsObj.isMember("MQTT_TOPIC")) {
      outputParamsObj["MQTT_TOPIC"] = "events";
    }
    if (!outputParamsObj.isMember("MQTT_USERNAME")) {
      outputParamsObj["MQTT_USERNAME"] = "";
    }
    if (!outputParamsObj.isMember("MQTT_PASSWORD")) {
      outputParamsObj["MQTT_PASSWORD"] = "";
    }

    additionalParams["input"] = inputParams;
    additionalParams["output"] = outputParamsObj;

    body["additionalParams"] = additionalParams;

    // Add detailed schema metadata for UI
    Json::Value schema(Json::objectValue);

    // Standard fields schema
    Json::Value standardFieldsSchema(Json::objectValue);
    addStandardFieldSchema(standardFieldsSchema, "name", "string", true,
                           "Instance name (pattern: ^[A-Za-z0-9 -_]+$)",
                           "^[A-Za-z0-9 -_]+$", "example_instance");
    addStandardFieldSchema(standardFieldsSchema, "group", "string", false,
                           "Group name (pattern: ^[A-Za-z0-9 -_]+$)",
                           "^[A-Za-z0-9 -_]+$", "default");
    addStandardFieldSchema(standardFieldsSchema, "solution", "string", false,
                           "Solution ID (must match existing solution)", "",
                           solutionId);
    addStandardFieldSchema(standardFieldsSchema, "persistent", "boolean", false,
                           "Save instance to JSON file", "", false);
    addStandardFieldSchema(standardFieldsSchema, "autoStart", "boolean", false,
                           "Automatically start instance when created", "",
                           false);
    addStandardFieldSchema(standardFieldsSchema, "frameRateLimit", "integer",
                           false, "Frame rate limit (FPS, 0 = unlimited)", "",
                           0, 0);
    addStandardFieldSchema(standardFieldsSchema, "detectionSensitivity",
                           "string", false, "Detection sensitivity level", "",
                           "Low", -1, -1,
                           {"Low", "Medium", "High", "Normal", "Slow"});
    addStandardFieldSchema(
        standardFieldsSchema, "detectorMode", "string", false, "Detector mode",
        "", "SmartDetection", -1, -1,
        {"SmartDetection", "FullRegionInference", "MosaicInference"});
    addStandardFieldSchema(
        standardFieldsSchema, "metadataMode", "boolean", false,
        "Enable metadata mode (output detection results as JSON)", "", false);
    addStandardFieldSchema(standardFieldsSchema, "statisticsMode", "boolean",
                           false, "Enable statistics mode", "", false);
    addStandardFieldSchema(standardFieldsSchema, "diagnosticsMode", "boolean",
                           false, "Enable diagnostics mode", "", false);
    addStandardFieldSchema(standardFieldsSchema, "debugMode", "boolean", false,
                           "Enable debug mode", "", false);

    schema["standardFields"] = standardFieldsSchema;

    // Additional parameters schema (input/output)
    Json::Value additionalParamsSchema(Json::objectValue);
    Json::Value inputParamsSchema(Json::objectValue);
    Json::Value outputParamsSchema(Json::objectValue);

    // Process input parameters
    for (const auto &paramName : inputParams.getMemberNames()) {
      Json::Value paramSchema = buildParameterSchema(
          paramName, inputParams[paramName].asString(), allParams,
          paramDefaults, templatesByType, config);
      inputParamsSchema[paramName] = paramSchema;
    }

    // Process output parameters
    for (const auto &paramName : outputParamsObj.getMemberNames()) {
      Json::Value paramSchema = buildParameterSchema(
          paramName, outputParamsObj[paramName].asString(), allParams,
          paramDefaults, templatesByType, config);
      outputParamsSchema[paramName] = paramSchema;
    }

    additionalParamsSchema["input"] = inputParamsSchema;
    additionalParamsSchema["output"] = outputParamsSchema;
    schema["additionalParams"] = additionalParamsSchema;

    // Add flexible input/output schema
    Json::Value flexibleIOSchema(Json::objectValue);
    flexibleIOSchema["description"] =
        "Flexible input/output options that can be added to any instance";
    flexibleIOSchema["input"] = buildFlexibleInputSchema();
    flexibleIOSchema["output"] = buildFlexibleOutputSchema();
    schema["flexibleInputOutput"] = flexibleIOSchema;

    body["schema"] = schema;

    // Return body directly (no wrapper) - can be used directly to create
    // instance
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    if (isApiLoggingEnabled()) {
      PLOG_INFO << "[API] GET /v1/core/solution/" << solutionId
                << "/instance-body - Success - " << duration.count() << "ms";
    }

    callback(createSuccessResponse(body));

  } catch (const std::exception &e) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] GET /v1/core/solution/" << solutionId
                 << "/instance-body - Exception: " << e.what() << " - "
                 << duration.count() << "ms";
    }
    std::cerr << "[SolutionHandler] Exception: " << e.what() << std::endl;
    callback(createErrorResponse(500, "Internal server error", e.what()));
  } catch (...) {
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    if (isApiLoggingEnabled()) {
      PLOG_ERROR << "[API] GET /v1/core/solution/" << solutionId
                 << "/instance-body - Unknown exception - " << duration.count()
                 << "ms";
    }
    std::cerr << "[SolutionHandler] Unknown exception" << std::endl;
    callback(createErrorResponse(500, "Internal server error",
                                 "Unknown error occurred"));
  }
}

void SolutionHandler::handleOptions(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto resp = HttpResponse::newHttpResponse();
  resp->setStatusCode(k200OK);
  resp->addHeader("Access-Control-Allow-Origin", "*");
  resp->addHeader("Access-Control-Allow-Methods",
                  "GET, POST, PUT, DELETE, OPTIONS");
  resp->addHeader("Access-Control-Allow-Headers",
                  "Content-Type, Authorization");
  resp->addHeader("Access-Control-Max-Age", "3600");
  callback(resp);
}

// Helper functions for building parameter schema metadata
void SolutionHandler::addStandardFieldSchema(
    Json::Value &schema, const std::string &fieldName, const std::string &type,
    bool required, const std::string &description, const std::string &pattern,
    const Json::Value &defaultValue, int min, int max,
    const std::vector<std::string> &enumValues) const {
  Json::Value fieldSchema(Json::objectValue);
  fieldSchema["name"] = fieldName;
  fieldSchema["type"] = type;
  fieldSchema["required"] = required;
  fieldSchema["description"] = description;

  if (!defaultValue.isNull()) {
    fieldSchema["default"] = defaultValue;
  }

  if (!pattern.empty()) {
    fieldSchema["pattern"] = pattern;
    if (pattern == "^[A-Za-z0-9 -_]+$") {
      fieldSchema["patternDescription"] =
          "Alphanumeric characters, spaces, hyphens, and underscores only";
    }
  }

  if (min >= 0) {
    fieldSchema["minimum"] = min;
  }
  if (max >= 0) {
    fieldSchema["maximum"] = max;
  }

  if (!enumValues.empty()) {
    Json::Value enumArray(Json::arrayValue);
    for (const auto &val : enumValues) {
      enumArray.append(val);
    }
    fieldSchema["enum"] = enumArray;
  }

  // Add UI hints
  Json::Value uiHints(Json::objectValue);
  if (type == "boolean") {
    uiHints["inputType"] = "checkbox";
    uiHints["widget"] = "switch";
  } else if (type == "integer") {
    uiHints["inputType"] = "number";
    uiHints["widget"] = "input";
  } else if (!enumValues.empty()) {
    uiHints["inputType"] = "select";
    uiHints["widget"] = "select";
  } else {
    uiHints["inputType"] = "text";
    uiHints["widget"] = "input";
  }
  fieldSchema["uiHints"] = uiHints;

  // Add examples
  Json::Value examples(Json::arrayValue);
  if (fieldName == "name") {
    examples.append("my_instance");
    examples.append("camera_01");
  } else if (fieldName == "group") {
    examples.append("production");
    examples.append("testing");
  } else if (fieldName == "frameRateLimit") {
    examples.append("0");
    examples.append("10");
    examples.append("30");
  }
  if (examples.size() > 0) {
    fieldSchema["examples"] = examples;
  }

  schema[fieldName] = fieldSchema;
}

Json::Value SolutionHandler::buildParameterSchema(
    const std::string &paramName, const std::string &exampleValue,
    const std::set<std::string> &allParams,
    const std::map<std::string, std::string> &paramDefaults,
    const std::map<std::string, class NodePoolManager::NodeTemplate>
        &templatesByType,
    const class SolutionConfig &config) const {
  Json::Value paramSchema(Json::objectValue);

  paramSchema["name"] = paramName;
  std::string paramType = inferParameterType(paramName);
  paramSchema["type"] = paramType;

  // Check if required
  bool isRequired = false;
  // Check if parameter is used in pipeline with ${VAR} placeholder
  std::regex varPattern("\\$\\{([A-Za-z0-9_]+)\\}");
  for (const auto &node : config.pipeline) {
    for (const auto &param : node.parameters) {
      std::string value = param.second;
      std::sregex_iterator iter(value.begin(), value.end(), varPattern);
      std::sregex_iterator end;
      for (; iter != end; ++iter) {
        std::string varName = (*iter)[1].str();
        if (varName == paramName) {
          isRequired = true;
          break;
        }
      }
      if (isRequired)
        break;
    }
    if (isRequired)
      break;
  }

  paramSchema["required"] = isRequired;
  paramSchema["example"] = exampleValue;

  // Get default value
  auto defIt = paramDefaults.find(paramName);
  if (defIt != paramDefaults.end()) {
    std::string defaultValue = defIt->second;
    // Check if default contains variables
    std::sregex_iterator iter(defaultValue.begin(), defaultValue.end(),
                              varPattern);
    std::sregex_iterator end;
    if (iter == end) {
      // Literal default
      paramSchema["default"] = defaultValue;
    }
  }

  // Add UI hints
  Json::Value uiHints(Json::objectValue);
  uiHints["inputType"] = getInputType(paramName, paramType);
  uiHints["widget"] = getWidgetType(paramName, paramType);
  std::string placeholder = getPlaceholder(paramName);
  if (!placeholder.empty()) {
    uiHints["placeholder"] = placeholder;
  }
  paramSchema["uiHints"] = uiHints;

  // Add validation rules
  Json::Value validation(Json::objectValue);
  addValidationRules(validation, paramName, paramType);
  if (validation.size() > 0) {
    paramSchema["validation"] = validation;
  }

  // Add description
  paramSchema["description"] = getParameterDescription(paramName);

  // Add examples
  Json::Value examples(Json::arrayValue);
  auto exampleValues = getParameterExamples(paramName);
  for (const auto &ex : exampleValues) {
    examples.append(ex);
  }
  if (examples.size() > 0) {
    paramSchema["examples"] = examples;
  }

  // Add category
  paramSchema["category"] = getParameterCategory(paramName);

  // Find which nodes use this parameter
  Json::Value usedInNodes(Json::arrayValue);
  for (const auto &node : config.pipeline) {
    for (const auto &param : node.parameters) {
      std::string value = param.second;
      std::sregex_iterator iter(value.begin(), value.end(), varPattern);
      std::sregex_iterator end;
      for (; iter != end; ++iter) {
        std::string varName = (*iter)[1].str();
        if (varName == paramName) {
          usedInNodes.append(node.nodeType);
          break;
        }
      }
    }
  }
  if (usedInNodes.size() > 0) {
    paramSchema["usedInNodes"] = usedInNodes;
  }

  return paramSchema;
}

Json::Value SolutionHandler::buildFlexibleInputSchema() const {
  Json::Value inputSchema(Json::objectValue);
  inputSchema["description"] =
      "Choose ONE input source. Pipeline builder auto-detects input type.";
  inputSchema["mutuallyExclusive"] = true;

  Json::Value params(Json::objectValue);

  // Helper to build simple parameter schema for flexible params
  auto buildFlexibleParam = [this](const std::string &name,
                                   const std::string &example,
                                   const std::string &desc) -> Json::Value {
    Json::Value param(Json::objectValue);
    param["name"] = name;
    param["type"] = inferParameterType(name);
    param["required"] = false;
    param["example"] = example;
    param["description"] = desc;

    Json::Value uiHints(Json::objectValue);
    uiHints["inputType"] = getInputType(name, param["type"].asString());
    uiHints["widget"] = getWidgetType(name, param["type"].asString());
    std::string placeholder = getPlaceholder(name);
    if (!placeholder.empty()) {
      uiHints["placeholder"] = placeholder;
    }
    param["uiHints"] = uiHints;

    Json::Value validation(Json::objectValue);
    addValidationRules(validation, name, param["type"].asString());
    if (validation.size() > 0) {
      param["validation"] = validation;
    }

    auto examples = getParameterExamples(name);
    if (!examples.empty()) {
      Json::Value examplesArray(Json::arrayValue);
      for (const auto &ex : examples) {
        examplesArray.append(ex);
      }
      param["examples"] = examplesArray;
    }

    param["category"] = getParameterCategory(name);
    return param;
  };

  // FILE_PATH
  params["FILE_PATH"] = buildFlexibleParam(
      "FILE_PATH", "./cvedix_data/test_video/example.mp4",
      "Local video file path or URL (supports file://, rtsp://, rtmp://, "
      "http://, https://). Pipeline builder auto-detects input type.");

  // RTSP_SRC_URL
  params["RTSP_SRC_URL"] = buildFlexibleParam(
      "RTSP_SRC_URL", "rtsp://camera-ip:8554/stream",
      "RTSP stream URL (overrides FILE_PATH if both provided)");

  // RTMP_SRC_URL
  params["RTMP_SRC_URL"] =
      buildFlexibleParam("RTMP_SRC_URL", "rtmp://input-server:1935/live/input",
                         "RTMP input stream URL");

  // HLS_URL
  params["HLS_URL"] = buildFlexibleParam(
      "HLS_URL", "http://example.com/stream.m3u8", "HLS stream URL (.m3u8)");

  inputSchema["parameters"] = params;
  return inputSchema;
}

Json::Value SolutionHandler::buildFlexibleOutputSchema() const {
  Json::Value outputSchema(Json::objectValue);
  outputSchema["description"] =
      "Add any combination of outputs. Pipeline builder auto-adds nodes.";
  outputSchema["mutuallyExclusive"] = false;

  Json::Value params(Json::objectValue);

  // Helper to build simple parameter schema for flexible params
  auto buildFlexibleParam = [this](const std::string &name,
                                   const std::string &example,
                                   const std::string &desc) -> Json::Value {
    Json::Value param(Json::objectValue);
    param["name"] = name;
    param["type"] = inferParameterType(name);
    param["required"] = false;
    param["example"] = example;
    param["description"] = desc;

    Json::Value uiHints(Json::objectValue);
    uiHints["inputType"] = getInputType(name, param["type"].asString());
    uiHints["widget"] = getWidgetType(name, param["type"].asString());
    std::string placeholder = getPlaceholder(name);
    if (!placeholder.empty()) {
      uiHints["placeholder"] = placeholder;
    }
    param["uiHints"] = uiHints;

    Json::Value validation(Json::objectValue);
    addValidationRules(validation, name, param["type"].asString());
    if (validation.size() > 0) {
      param["validation"] = validation;
    }

    auto examples = getParameterExamples(name);
    if (!examples.empty()) {
      Json::Value examplesArray(Json::arrayValue);
      for (const auto &ex : examples) {
        examplesArray.append(ex);
      }
      param["examples"] = examplesArray;
    }

    param["category"] = getParameterCategory(name);
    return param;
  };

  // MQTT
  params["MQTT_BROKER_URL"] = buildFlexibleParam(
      "MQTT_BROKER_URL", "localhost",
      "MQTT broker address (enables MQTT output). Leave empty to disable.");
  params["MQTT_PORT"] =
      buildFlexibleParam("MQTT_PORT", "1883", "MQTT broker port");
  params["MQTT_TOPIC"] = buildFlexibleParam("MQTT_TOPIC", "events",
                                            "MQTT topic for publishing events");

  // RTMP
  params["RTMP_URL"] = buildFlexibleParam(
      "RTMP_URL", "rtmp://server:1935/live/stream",
      "RTMP destination URL (enables RTMP streaming output)");

  // Screen
  params["ENABLE_SCREEN_DES"] = buildFlexibleParam(
      "ENABLE_SCREEN_DES", "false", "Enable screen display (true/false)");

  // Recording
  params["RECORD_PATH"] = buildFlexibleParam(
      "RECORD_PATH", "./output/recordings", "Path for video recording output");

  outputSchema["parameters"] = params;
  return outputSchema;
}

// Helper functions for parameter metadata (similar to NodeHandler)
std::string
SolutionHandler::inferParameterType(const std::string &paramName) const {
  std::string upperParam = paramName;
  std::transform(upperParam.begin(), upperParam.end(), upperParam.begin(),
                 ::toupper);

  if (upperParam.find("THRESHOLD") != std::string::npos ||
      upperParam.find("RATIO") != std::string::npos ||
      upperParam.find("SCORE") != std::string::npos) {
    return "number";
  }
  if (upperParam.find("PORT") != std::string::npos ||
      upperParam.find("CHANNEL") != std::string::npos ||
      upperParam.find("WIDTH") != std::string::npos ||
      upperParam.find("HEIGHT") != std::string::npos ||
      upperParam.find("TOP_K") != std::string::npos) {
    return "integer";
  }
  if (upperParam.find("ENABLE") != std::string::npos || upperParam == "OSD" ||
      upperParam.find("DISABLE") != std::string::npos) {
    return "boolean";
  }
  return "string";
}

std::string SolutionHandler::getInputType(const std::string &paramName,
                                          const std::string &paramType) const {
  if (paramType == "number" || paramType == "integer") {
    return "number";
  }
  if (paramType == "boolean") {
    return "checkbox";
  }
  std::string upperParam = paramName;
  std::transform(upperParam.begin(), upperParam.end(), upperParam.begin(),
                 ::toupper);
  if (upperParam.find("URL") != std::string::npos) {
    return "url";
  }
  if (upperParam.find("PATH") != std::string::npos ||
      upperParam.find("DIR") != std::string::npos) {
    return "file";
  }
  return "text";
}

std::string SolutionHandler::getWidgetType(const std::string &paramName,
                                           const std::string &paramType) const {
  if (paramType == "boolean") {
    return "switch";
  }
  if (paramName.find("threshold") != std::string::npos ||
      paramName.find("ratio") != std::string::npos) {
    return "slider";
  }
  std::string upperParam = paramName;
  std::transform(upperParam.begin(), upperParam.end(), upperParam.begin(),
                 ::toupper);
  if (upperParam.find("URL") != std::string::npos) {
    return "url-input";
  }
  if (upperParam.find("PATH") != std::string::npos) {
    return "file-picker";
  }
  return "input";
}

std::string
SolutionHandler::getPlaceholder(const std::string &paramName) const {
  std::string upperParam = paramName;
  std::transform(upperParam.begin(), upperParam.end(), upperParam.begin(),
                 ::toupper);

  if (upperParam == "RTSP_SRC_URL" || upperParam == "RTSP_URL") {
    return "rtsp://camera-ip:8554/stream";
  }
  if (upperParam == "RTMP_SRC_URL" || upperParam == "RTMP_URL") {
    return "rtmp://localhost:1935/live/stream";
  }
  if (upperParam == "FILE_PATH") {
    return "/path/to/video.mp4";
  }
  if (upperParam.find("MODEL_PATH") != std::string::npos) {
    return "/opt/cvedix/models/example.onnx";
  }
  if (upperParam == "MQTT_BROKER_URL") {
    return "localhost";
  }
  if (upperParam == "MQTT_PORT") {
    return "1883";
  }
  if (upperParam == "MQTT_TOPIC") {
    return "events";
  }
  return "";
}

void SolutionHandler::addValidationRules(Json::Value &validation,
                                         const std::string &paramName,
                                         const std::string &paramType) const {
  std::string upperParam = paramName;
  std::transform(upperParam.begin(), upperParam.end(), upperParam.begin(),
                 ::toupper);

  if (paramType == "number" || paramType == "integer") {
    if (upperParam.find("THRESHOLD") != std::string::npos ||
        upperParam.find("SCORE") != std::string::npos) {
      validation["min"] = 0.0;
      validation["max"] = 1.0;
      validation["step"] = 0.01;
    }
    if (upperParam.find("RATIO") != std::string::npos) {
      validation["min"] = 0.0;
      validation["max"] = 1.0;
      validation["step"] = 0.1;
    }
    if (upperParam.find("PORT") != std::string::npos) {
      validation["min"] = 1;
      validation["max"] = 65535;
    }
    if (upperParam.find("CHANNEL") != std::string::npos) {
      validation["min"] = 0;
      validation["max"] = 15;
    }
  }

  if (paramType == "string") {
    if (upperParam.find("URL") != std::string::npos) {
      validation["pattern"] = "^(rtsp|rtmp|http|https|file|udp)://.+";
      validation["patternDescription"] =
          "Must be a valid URL (rtsp://, rtmp://, http://, https://, file://, "
          "or udp://)";
    }
    if (upperParam.find("PATH") != std::string::npos) {
      validation["pattern"] = "^[^\\0]+$";
      validation["patternDescription"] = "Must be a valid file path";
    }
  }
}

std::string
SolutionHandler::getParameterDescription(const std::string &paramName) const {
  std::string upperParam = paramName;
  std::transform(upperParam.begin(), upperParam.end(), upperParam.begin(),
                 ::toupper);

  if (upperParam == "FILE_PATH") {
    return "Path to video file or URL (supports file://, rtsp://, rtmp://, "
           "http://, https://). Pipeline builder auto-detects input type.";
  }
  if (upperParam == "RTSP_SRC_URL" || upperParam == "RTSP_URL") {
    return "RTSP stream URL (e.g., rtsp://camera-ip:8554/stream)";
  }
  if (upperParam == "RTMP_SRC_URL" || upperParam == "RTMP_URL") {
    return "RTMP stream URL (e.g., rtmp://localhost:1935/live/stream)";
  }
  if (upperParam.find("MODEL_PATH") != std::string::npos) {
    return "Path to model file (.onnx, .trt, .rknn, etc.)";
  }
  if (upperParam == "MQTT_BROKER_URL") {
    return "MQTT broker address (hostname or IP). Leave empty to disable MQTT "
           "output.";
  }
  if (upperParam == "MQTT_PORT") {
    return "MQTT broker port (default: 1883)";
  }
  if (upperParam == "MQTT_TOPIC") {
    return "MQTT topic to publish messages to";
  }
  if (upperParam == "ENABLE_SCREEN_DES") {
    return "Enable screen display (true/false)";
  }
  if (upperParam.find("THRESHOLD") != std::string::npos) {
    return "Confidence threshold (0.0-1.0). Higher values = fewer detections "
           "but more accurate";
  }
  if (upperParam.find("RATIO") != std::string::npos) {
    return "Resize ratio (0.0-1.0). 1.0 = no resize, smaller values = "
           "downscale";
  }
  return "Parameter: " + paramName;
}

std::vector<std::string>
SolutionHandler::getParameterExamples(const std::string &paramName) const {
  std::vector<std::string> examples;
  std::string upperParam = paramName;
  std::transform(upperParam.begin(), upperParam.end(), upperParam.begin(),
                 ::toupper);

  if (upperParam == "FILE_PATH") {
    examples.push_back("./cvedix_data/test_video/example.mp4");
    examples.push_back("file:///path/to/video.mp4");
    examples.push_back("rtsp://camera-ip:8554/stream");
  }
  if (upperParam == "RTSP_SRC_URL" || upperParam == "RTSP_URL") {
    examples.push_back("rtsp://192.168.1.100:8554/stream1");
    examples.push_back("rtsp://admin:password@camera-ip:554/stream");
  }
  if (upperParam == "RTMP_SRC_URL" || upperParam == "RTMP_URL") {
    examples.push_back("rtmp://localhost:1935/live/stream");
    examples.push_back("rtmp://youtube.com/live2/stream-key");
  }
  if (upperParam.find("MODEL_PATH") != std::string::npos) {
    examples.push_back("/opt/cvedix/models/yunet.onnx");
    examples.push_back("/opt/cvedix/models/yolov8.engine");
  }
  if (upperParam == "MQTT_BROKER_URL") {
    examples.push_back("localhost");
    examples.push_back("192.168.1.100");
    examples.push_back("mqtt.example.com");
  }
  if (upperParam == "MQTT_TOPIC") {
    examples.push_back("detections");
    examples.push_back("events");
    examples.push_back("camera/stream1/events");
  }

  return examples;
}

std::string
SolutionHandler::getParameterCategory(const std::string &paramName) const {
  std::string upperParam = paramName;
  std::transform(upperParam.begin(), upperParam.end(), upperParam.begin(),
                 ::toupper);

  if (upperParam.find("URL") != std::string::npos ||
      upperParam.find("PATH") != std::string::npos ||
      upperParam.find("PORT") != std::string::npos) {
    return "connection";
  }
  if (upperParam.find("THRESHOLD") != std::string::npos ||
      upperParam.find("RATIO") != std::string::npos) {
    return "performance";
  }
  if (upperParam.find("MODEL") != std::string::npos ||
      upperParam.find("WEIGHTS") != std::string::npos ||
      upperParam.find("CONFIG") != std::string::npos ||
      upperParam.find("LABELS") != std::string::npos) {
    return "model";
  }
  if (upperParam.find("MQTT") != std::string::npos ||
      upperParam.find("RTMP") != std::string::npos ||
      upperParam == "ENABLE_SCREEN_DES" || upperParam == "RECORD_PATH") {
    return "output";
  }
  return "general";
}
