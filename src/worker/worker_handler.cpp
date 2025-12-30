#include "worker/worker_handler.h"
#include "core/pipeline_builder.h"
#include "models/create_instance_request.h"
#include "solutions/solution_registry.h"
#include <cstring>
#include <cvedix/nodes/ba/cvedix_ba_crossline_node.h>
#include <cvedix/nodes/common/cvedix_node.h>
#include <cvedix/nodes/src/cvedix_file_src_node.h>
#include <cvedix/nodes/src/cvedix_rtsp_src_node.h>
#include <cvedix/objects/shapes/cvedix_line.h>
#include <cvedix/objects/shapes/cvedix_point.h>
#include <getopt.h>
#include <iostream>
#include <optional>
#include <opencv2/imgcodecs.hpp>
#include <type_traits>
#include <utility>
#include <json/reader.h>
#include <sstream>

// Base64 encoding table
static const char base64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

namespace worker {

namespace {

template <typename T, typename = void>
struct has_set_lines : std::false_type {};
template <typename T>
struct has_set_lines<
    T, std::void_t<decltype(std::declval<T &>().set_lines(
           std::declval<const std::map<int, cvedix_objects::cvedix_line> &>()))>>
    : std::true_type {};

template <typename T, typename = void>
struct has_setLines : std::false_type {};
template <typename T>
struct has_setLines<
    T, std::void_t<decltype(std::declval<T &>().setLines(
           std::declval<const std::map<int, cvedix_objects::cvedix_line> &>()))>>
    : std::true_type {};

template <typename T, typename = void>
struct has_update_lines : std::false_type {};
template <typename T>
struct has_update_lines<
    T, std::void_t<decltype(std::declval<T &>().update_lines(
           std::declval<const std::map<int, cvedix_objects::cvedix_line> &>()))>>
    : std::true_type {};

template <typename T, typename = void>
struct has_updateLines : std::false_type {};
template <typename T>
struct has_updateLines<
    T, std::void_t<decltype(std::declval<T &>().updateLines(
           std::declval<const std::map<int, cvedix_objects::cvedix_line> &>()))>>
    : std::true_type {};

static bool applyLinesToCrosslineNode(
    cvedix_nodes::cvedix_ba_crossline_node &node,
    const std::map<int, cvedix_objects::cvedix_line> &lines,
    std::string &error) {
  // Best-effort: support multiple SDK API spellings if present.
  if constexpr (has_set_lines<cvedix_nodes::cvedix_ba_crossline_node>::value) {
    node.set_lines(lines);
    return true;
  } else if constexpr (has_setLines<cvedix_nodes::cvedix_ba_crossline_node>::value) {
    node.setLines(lines);
    return true;
  } else if constexpr (has_update_lines<cvedix_nodes::cvedix_ba_crossline_node>::value) {
    node.update_lines(lines);
    return true;
  } else if constexpr (has_updateLines<cvedix_nodes::cvedix_ba_crossline_node>::value) {
    node.updateLines(lines);
    return true;
  } else {
    error =
        "CVEDIX SDK does not expose a runtime line update API for "
        "cvedix_ba_crossline_node (no set_lines/setLines/update_lines/updateLines)";
    return false;
  }
}

static std::optional<std::map<int, cvedix_objects::cvedix_line>>
parseCrossingLinesJsonString(const std::string &linesJsonStr,
                             std::string &error) {
  Json::Reader reader;
  Json::Value parsedLines;
  if (!reader.parse(linesJsonStr, parsedLines)) {
    error = "Failed to parse CrossingLines JSON string";
    return std::nullopt;
  }
  if (!parsedLines.isArray()) {
    error = "CrossingLines parsed JSON is not an array";
    return std::nullopt;
  }

  std::map<int, cvedix_objects::cvedix_line> lines;
  for (Json::ArrayIndex i = 0; i < parsedLines.size(); ++i) {
    const Json::Value &lineObj = parsedLines[i];
    if (!lineObj.isObject()) {
      continue;
    }
    if (!lineObj.isMember("coordinates") || !lineObj["coordinates"].isArray()) {
      continue;
    }
    const Json::Value &coordinates = lineObj["coordinates"];
    if (coordinates.size() < 2) {
      continue;
    }
    const Json::Value &startCoord = coordinates[0];
    const Json::Value &endCoord = coordinates[coordinates.size() - 1];
    if (!startCoord.isObject() || !endCoord.isObject()) {
      continue;
    }
    if (!startCoord.isMember("x") || !startCoord.isMember("y") ||
        !endCoord.isMember("x") || !endCoord.isMember("y")) {
      continue;
    }
    if (!startCoord["x"].isNumeric() || !startCoord["y"].isNumeric() ||
        !endCoord["x"].isNumeric() || !endCoord["y"].isNumeric()) {
      continue;
    }

    int start_x = startCoord["x"].asInt();
    int start_y = startCoord["y"].asInt();
    int end_x = endCoord["x"].asInt();
    int end_y = endCoord["y"].asInt();

    cvedix_objects::cvedix_point start(start_x, start_y);
    cvedix_objects::cvedix_point end(end_x, end_y);
    int channel = static_cast<int>(i);
    lines[channel] = cvedix_objects::cvedix_line(start, end);
  }

  return lines;
}

} // namespace

WorkerHandler::WorkerHandler(const std::string &instance_id,
                             const std::string &socket_path,
                             const Json::Value &config)
    : instance_id_(instance_id), socket_path_(socket_path), config_(config) {
  start_time_ = std::chrono::steady_clock::now();
  last_fps_update_ = start_time_;
}

WorkerHandler::~WorkerHandler() {
  cleanupPipeline();
  if (server_) {
    server_->stop();
  }
}

bool WorkerHandler::initializeDependencies() {
  std::cout << "[Worker:" << instance_id_ << "] Initializing dependencies..."
            << std::endl;

  try {
    // Get solution registry singleton and initialize default solutions
    SolutionRegistry::getInstance().initializeDefaultSolutions();

    // Create pipeline builder (uses default constructor)
    pipeline_builder_ = std::make_unique<PipelineBuilder>();

    std::cout << "[Worker:" << instance_id_ << "] Dependencies initialized"
              << std::endl;
    return true;
  } catch (const std::exception &e) {
    std::cerr << "[Worker:" << instance_id_
              << "] Failed to initialize dependencies: " << e.what()
              << std::endl;
    last_error_ = e.what();
    return false;
  }
}

int WorkerHandler::run() {
  std::cout << "[Worker:" << instance_id_ << "] Starting..." << std::endl;

  // Initialize dependencies first
  if (!initializeDependencies()) {
    std::cerr << "[Worker:" << instance_id_
              << "] Failed to initialize dependencies" << std::endl;
    return 1;
  }

  // Create and start IPC server
  server_ = std::make_unique<UnixSocketServer>(socket_path_);

  if (!server_->start(
          [this](const IPCMessage &msg) { return handleMessage(msg); })) {
    std::cerr << "[Worker:" << instance_id_ << "] Failed to start IPC server"
              << std::endl;
    return 1;
  }

  // Build pipeline from initial config if provided
  if (!config_.isNull() && config_.isMember("Solution")) {
    if (!buildPipeline()) {
      std::cerr << "[Worker:" << instance_id_
                << "] Failed to build initial pipeline" << std::endl;
      // Continue anyway - supervisor can send CREATE_INSTANCE later
    }
  }

  // Send ready signal to supervisor
  sendReadySignal();

  std::cout << "[Worker:" << instance_id_ << "] Ready and listening on "
            << socket_path_ << std::endl;

  // Main loop - just wait for shutdown
  while (!shutdown_requested_.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  std::cout << "[Worker:" << instance_id_ << "] Shutting down..." << std::endl;

  // Cleanup
  stopPipeline();
  cleanupPipeline();
  server_->stop();

  std::cout << "[Worker:" << instance_id_ << "] Exited cleanly" << std::endl;
  return 0;
}

void WorkerHandler::requestShutdown() { shutdown_requested_.store(true); }

IPCMessage WorkerHandler::handleMessage(const IPCMessage &msg) {
  switch (msg.type) {
  case MessageType::PING:
    return handlePing(msg);
  case MessageType::SHUTDOWN:
    return handleShutdown(msg);
  case MessageType::CREATE_INSTANCE:
    return handleCreateInstance(msg);
  case MessageType::DELETE_INSTANCE:
    return handleDeleteInstance(msg);
  case MessageType::START_INSTANCE:
    return handleStartInstance(msg);
  case MessageType::STOP_INSTANCE:
    return handleStopInstance(msg);
  case MessageType::UPDATE_INSTANCE:
    return handleUpdateInstance(msg);
  case MessageType::GET_INSTANCE_STATUS:
    return handleGetStatus(msg);
  case MessageType::GET_STATISTICS:
    return handleGetStatistics(msg);
  case MessageType::GET_LAST_FRAME:
    return handleGetLastFrame(msg);
  default: {
    IPCMessage error;
    error.type = MessageType::ERROR_RESPONSE;
    error.payload = createErrorResponse("Unknown message type",
                                        ResponseStatus::INVALID_REQUEST);
    return error;
  }
  }
}

IPCMessage WorkerHandler::handlePing(const IPCMessage & /*msg*/) {
  IPCMessage response;
  response.type = MessageType::PONG;
  response.payload["instance_id"] = instance_id_;
  response.payload["state"] = current_state_;
  response.payload["uptime_ms"] = static_cast<Json::Int64>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - start_time_)
          .count());
  return response;
}

IPCMessage WorkerHandler::handleShutdown(const IPCMessage & /*msg*/) {
  IPCMessage response;
  response.type = MessageType::SHUTDOWN_ACK;
  response.payload = createResponse(ResponseStatus::OK, "Shutting down");

  // Request shutdown after sending response
  shutdown_requested_.store(true);

  return response;
}

IPCMessage WorkerHandler::handleCreateInstance(const IPCMessage &msg) {
  IPCMessage response;
  response.type = MessageType::CREATE_INSTANCE_RESPONSE;

  if (!pipeline_nodes_.empty()) {
    response.payload = createErrorResponse("Instance already exists",
                                           ResponseStatus::ALREADY_EXISTS);
    return response;
  }

  // Update config from message
  if (msg.payload.isMember("config")) {
    config_ = msg.payload["config"];
  }

  if (!buildPipeline()) {
    response.payload =
        createErrorResponse("Failed to build pipeline: " + last_error_,
                            ResponseStatus::INTERNAL_ERROR);
    return response;
  }

  response.payload = createResponse(ResponseStatus::OK, "Instance created");
  response.payload["data"]["instance_id"] = instance_id_;
  return response;
}

IPCMessage WorkerHandler::handleDeleteInstance(const IPCMessage & /*msg*/) {
  IPCMessage response;
  response.type = MessageType::DELETE_INSTANCE_RESPONSE;

  stopPipeline();
  cleanupPipeline();

  response.payload = createResponse(ResponseStatus::OK, "Instance deleted");

  // Request shutdown after delete
  shutdown_requested_.store(true);

  return response;
}

IPCMessage WorkerHandler::handleStartInstance(const IPCMessage & /*msg*/) {
  IPCMessage response;
  response.type = MessageType::START_INSTANCE_RESPONSE;

  if (pipeline_nodes_.empty()) {
    response.payload = createErrorResponse("No pipeline configured",
                                           ResponseStatus::NOT_FOUND);
    return response;
  }

  if (pipeline_running_) {
    response.payload = createErrorResponse("Pipeline already running",
                                           ResponseStatus::ALREADY_EXISTS);
    return response;
  }

  if (!startPipeline()) {
    response.payload =
        createErrorResponse("Failed to start pipeline: " + last_error_,
                            ResponseStatus::INTERNAL_ERROR);
    return response;
  }

  response.payload = createResponse(ResponseStatus::OK, "Instance started");
  return response;
}

IPCMessage WorkerHandler::handleStopInstance(const IPCMessage & /*msg*/) {
  IPCMessage response;
  response.type = MessageType::STOP_INSTANCE_RESPONSE;

  if (!pipeline_running_) {
    response.payload =
        createErrorResponse("Pipeline not running", ResponseStatus::NOT_FOUND);
    return response;
  }

  stopPipeline();

  response.payload = createResponse(ResponseStatus::OK, "Instance stopped");
  return response;
}

IPCMessage WorkerHandler::handleUpdateInstance(const IPCMessage &msg) {
  IPCMessage response;
  response.type = MessageType::UPDATE_INSTANCE_RESPONSE;

  // Merge config
  if (msg.payload.isMember("config")) {
    for (const auto &key : msg.payload["config"].getMemberNames()) {
      config_[key] = msg.payload["config"][key];
    }
  }

  // Best-effort: apply CrossingLines update to running pipeline without restart.
  if (msg.payload.isMember("config")) {
    std::string applyError;
    (void)applyCrossingLinesRuntimeUpdate(msg.payload["config"], applyError);
    if (!applyError.empty()) {
      std::cerr << "[Worker:" << instance_id_
                << "] CrossingLines runtime update warning: " << applyError
                << std::endl;
    }
  }

  response.payload = createResponse(ResponseStatus::OK, "Instance updated");
  return response;
}

bool WorkerHandler::applyCrossingLinesRuntimeUpdate(const Json::Value &configUpdate,
                                                    std::string &error) {
  error.clear();

  // If pipeline isn't running, we don't need runtime update; config will apply
  // on next start/build.
  if (!pipeline_running_) {
    return true;
  }

  // Extract stringified JSON array from config update.
  std::string linesJsonStr;
  if (configUpdate.isMember("AdditionalParams") &&
      configUpdate["AdditionalParams"].isObject() &&
      configUpdate["AdditionalParams"].isMember("CrossingLines") &&
      configUpdate["AdditionalParams"]["CrossingLines"].isString()) {
    linesJsonStr = configUpdate["AdditionalParams"]["CrossingLines"].asString();
  } else if (configUpdate.isMember("additionalParams") &&
             configUpdate["additionalParams"].isObject() &&
             configUpdate["additionalParams"].isMember("CrossingLines") &&
             configUpdate["additionalParams"]["CrossingLines"].isString()) {
    // Backward compatibility if caller uses camelCase.
    linesJsonStr = configUpdate["additionalParams"]["CrossingLines"].asString();
  } else {
    return false; // no applicable update in this message
  }

  std::string parseError;
  auto optLines = parseCrossingLinesJsonString(linesJsonStr, parseError);
  if (!optLines.has_value()) {
    error = parseError;
    return false;
  }

  // Find BA crossline node in pipeline and apply.
  for (const auto &nodeBase : pipeline_nodes_) {
    auto crosslineNode =
        std::dynamic_pointer_cast<cvedix_nodes::cvedix_ba_crossline_node>(
            nodeBase);
    if (!crosslineNode) {
      continue;
    }

    if (!applyLinesToCrosslineNode(*crosslineNode, optLines.value(), error)) {
      return false;
    }

    std::cout << "[Worker:" << instance_id_
              << "] Applied CrossingLines runtime update ("
              << optLines->size() << " line(s))" << std::endl;
    return true;
  }

  error = "Running pipeline does not contain a cvedix_ba_crossline_node";
  return false;
}

IPCMessage WorkerHandler::handleGetStatus(const IPCMessage & /*msg*/) {
  IPCMessage response;
  response.type = MessageType::GET_INSTANCE_STATUS_RESPONSE;

  Json::Value data;
  data["instance_id"] = instance_id_;
  data["state"] = current_state_;
  data["running"] = pipeline_running_;
  data["has_pipeline"] = !pipeline_nodes_.empty();
  if (!last_error_.empty()) {
    data["last_error"] = last_error_;
  }

  response.payload = createResponse(ResponseStatus::OK, "", data);
  return response;
}

IPCMessage WorkerHandler::handleGetStatistics(const IPCMessage & /*msg*/) {
  IPCMessage response;
  response.type = MessageType::GET_STATISTICS_RESPONSE;

  auto now = std::chrono::steady_clock::now();
  auto uptime =
      std::chrono::duration_cast<std::chrono::seconds>(now - start_time_)
          .count();
  auto start_unix = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();

  Json::Value data;
  data["instance_id"] = instance_id_;
  data["frames_processed"] =
      static_cast<Json::UInt64>(frames_processed_.load());
  data["dropped_frames_count"] =
      static_cast<Json::UInt64>(dropped_frames_.load());
  data["start_time"] = static_cast<Json::Int64>(start_unix - uptime);
  data["current_framerate"] = current_fps_.load();
  data["source_framerate"] = 0.0; // TODO: Get from source node
  data["latency"] = 0.0;          // TODO: Calculate actual latency
  data["input_queue_size"] = static_cast<Json::UInt64>(queue_size_.load());
  data["resolution"] = resolution_;
  data["source_resolution"] = source_resolution_;
  data["format"] = "BGR";
  data["state"] = current_state_;

  response.payload = createResponse(ResponseStatus::OK, "", data);
  return response;
}

IPCMessage WorkerHandler::handleGetLastFrame(const IPCMessage & /*msg*/) {
  IPCMessage response;
  response.type = MessageType::GET_LAST_FRAME_RESPONSE;

  Json::Value data;
  data["instance_id"] = instance_id_;

  std::lock_guard<std::mutex> lock(frame_mutex_);
  if (has_frame_ && !last_frame_.empty()) {
    data["frame"] = encodeFrameToBase64(last_frame_);
    data["has_frame"] = true;
  } else {
    data["frame"] = "";
    data["has_frame"] = false;
  }

  response.payload = createResponse(ResponseStatus::OK, "", data);
  return response;
}

CreateInstanceRequest
WorkerHandler::parseCreateRequest(const Json::Value &config) const {
  CreateInstanceRequest req;

  req.name = config.get("Name", instance_id_).asString();
  req.group = config.get("Group", "").asString();

  // Get solution ID
  if (config.isMember("Solution")) {
    if (config["Solution"].isString()) {
      req.solution = config["Solution"].asString();
    } else if (config["Solution"].isObject()) {
      req.solution = config["Solution"].get("SolutionId", "").asString();
    }
  }
  req.solution = config.get("SolutionId", req.solution).asString();

  // Flags
  req.persistent = config.get("Persistent", false).asBool();
  req.autoStart = config.get("AutoStart", false).asBool();
  req.autoRestart = config.get("AutoRestart", false).asBool();
  req.frameRateLimit = config.get("FrameRateLimit", 0).asInt();

  // Additional parameters (source URLs, model paths, etc.)
  if (config.isMember("AdditionalParams")) {
    const auto &params = config["AdditionalParams"];
    for (const auto &key : params.getMemberNames()) {
      req.additionalParams[key] = params[key].asString();
    }
  }

  // Direct URL parameters
  if (config.isMember("RtspUrl")) {
    req.additionalParams["RTSP_URL"] = config["RtspUrl"].asString();
  }
  if (config.isMember("RtmpUrl")) {
    req.additionalParams["RTMP_URL"] = config["RtmpUrl"].asString();
  }
  if (config.isMember("FilePath")) {
    req.additionalParams["FILE_PATH"] = config["FilePath"].asString();
  }

  return req;
}

bool WorkerHandler::buildPipeline() {
  std::cout << "[Worker:" << instance_id_
            << "] Building pipeline from config..." << std::endl;

  if (!pipeline_builder_) {
    last_error_ = "Pipeline builder not initialized";
    return false;
  }

  try {
    // Parse config to CreateInstanceRequest
    CreateInstanceRequest req = parseCreateRequest(config_);

    if (req.solution.empty()) {
      last_error_ = "No solution specified in config";
      return false;
    }

    // Get solution config from singleton
    auto optSolution =
        SolutionRegistry::getInstance().getSolution(req.solution);
    if (!optSolution.has_value()) {
      last_error_ = "Solution not found: " + req.solution;
      return false;
    }

    // Build pipeline
    pipeline_nodes_ = pipeline_builder_->buildPipeline(optSolution.value(), req,
                                                       instance_id_);

    if (pipeline_nodes_.empty()) {
      last_error_ = "Pipeline builder returned empty pipeline";
      return false;
    }

    current_state_ = "created";
    std::cout << "[Worker:" << instance_id_ << "] Pipeline built with "
              << pipeline_nodes_.size() << " nodes" << std::endl;
    return true;

  } catch (const std::exception &e) {
    last_error_ = e.what();
    std::cerr << "[Worker:" << instance_id_
              << "] Failed to build pipeline: " << e.what() << std::endl;
    return false;
  }
}

bool WorkerHandler::startPipeline() {
  if (pipeline_nodes_.empty()) {
    last_error_ = "No pipeline to start";
    return false;
  }

  std::cout << "[Worker:" << instance_id_ << "] Starting pipeline..."
            << std::endl;

  try {
    // Find and start source node (first node in pipeline)
    // Try RTSP source first
    auto rtspNode =
        std::dynamic_pointer_cast<cvedix_nodes::cvedix_rtsp_src_node>(
            pipeline_nodes_[0]);
    if (rtspNode) {
      std::cout << "[Worker:" << instance_id_ << "] Starting RTSP source node"
                << std::endl;
      rtspNode->start();
    } else {
      // Try file source
      auto fileNode =
          std::dynamic_pointer_cast<cvedix_nodes::cvedix_file_src_node>(
              pipeline_nodes_[0]);
      if (fileNode) {
        std::cout << "[Worker:" << instance_id_ << "] Starting file source node"
                  << std::endl;
        fileNode->start();
      } else {
        last_error_ = "No supported source node found in pipeline";
        return false;
      }
    }

    // Setup frame capture hook for statistics
    setupFrameCaptureHook();

    pipeline_running_ = true;
    current_state_ = "running";
    start_time_ = std::chrono::steady_clock::now();
    last_fps_update_ = start_time_;
    frames_processed_.store(0);
    dropped_frames_.store(0);

    std::cout << "[Worker:" << instance_id_ << "] Pipeline started"
              << std::endl;
    return true;

  } catch (const std::exception &e) {
    last_error_ = e.what();
    std::cerr << "[Worker:" << instance_id_
              << "] Failed to start pipeline: " << e.what() << std::endl;
    return false;
  }
}

void WorkerHandler::stopPipeline() {
  if (!pipeline_running_) {
    return;
  }

  std::cout << "[Worker:" << instance_id_ << "] Stopping pipeline..."
            << std::endl;

  try {
    // Stop source nodes first
    for (const auto &node : pipeline_nodes_) {
      if (node) {
        // Detach recursively to stop the pipeline
        node->detach_recursively();
        break;
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "[Worker:" << instance_id_
              << "] Error stopping pipeline: " << e.what() << std::endl;
  }

  pipeline_running_ = false;
  current_state_ = "stopped";
  std::cout << "[Worker:" << instance_id_ << "] Pipeline stopped" << std::endl;
}

void WorkerHandler::cleanupPipeline() {
  stopPipeline();
  pipeline_nodes_.clear();
  current_state_ = "stopped";
}

void WorkerHandler::sendReadySignal() {
  std::cout << "[Worker:" << instance_id_ << "] Sending ready signal"
            << std::endl;
}

void WorkerHandler::setupFrameCaptureHook() {
  // TODO: Setup hooks on pipeline nodes to capture frames and statistics
  // This would involve finding the appropriate node (e.g., OSD or DES node)
  // and setting up a callback to capture frames
}

void WorkerHandler::updateFrameCache(const cv::Mat &frame) {
  std::lock_guard<std::mutex> lock(frame_mutex_);
  last_frame_ = frame.clone();
  has_frame_ = true;
  frames_processed_.fetch_add(1);

  // Update FPS calculation
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                     now - last_fps_update_)
                     .count();
  if (elapsed >= 1000) {
    // Calculate FPS over the last second
    // This is a simplified calculation
    last_fps_update_ = now;
  }

  // Update resolution
  if (!frame.empty()) {
    resolution_ = std::to_string(frame.cols) + "x" + std::to_string(frame.rows);
  }
}

std::string WorkerHandler::encodeFrameToBase64(const cv::Mat &frame,
                                               int quality) const {
  if (frame.empty()) {
    return "";
  }

  // Encode to JPEG
  std::vector<uchar> buffer;
  std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, quality};
  if (!cv::imencode(".jpg", frame, buffer, params)) {
    return "";
  }

  // Base64 encode
  std::string result;
  result.reserve(((buffer.size() + 2) / 3) * 4);

  size_t i = 0;
  while (i < buffer.size()) {
    uint32_t octet_a = i < buffer.size() ? buffer[i++] : 0;
    uint32_t octet_b = i < buffer.size() ? buffer[i++] : 0;
    uint32_t octet_c = i < buffer.size() ? buffer[i++] : 0;

    uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;

    result += base64_chars[(triple >> 18) & 0x3F];
    result += base64_chars[(triple >> 12) & 0x3F];
    result += base64_chars[(triple >> 6) & 0x3F];
    result += base64_chars[triple & 0x3F];
  }

  // Add padding
  size_t padding = (3 - (buffer.size() % 3)) % 3;
  for (size_t p = 0; p < padding; ++p) {
    result[result.size() - 1 - p] = '=';
  }

  return result;
}

// ============================================================================
// WorkerArgs parsing
// ============================================================================

WorkerArgs WorkerArgs::parse(int argc, char *argv[]) {
  WorkerArgs args;

  static struct option long_options[] = {
      {"instance-id", required_argument, 0, 'i'},
      {"socket", required_argument, 0, 's'},
      {"config", required_argument, 0, 'c'},
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0}};

  int opt;
  int option_index = 0;

  // Reset getopt
  optind = 1;

  while ((opt = getopt_long(argc, argv, "i:s:c:h", long_options,
                            &option_index)) != -1) {
    switch (opt) {
    case 'i':
      args.instance_id = optarg;
      break;
    case 's':
      args.socket_path = optarg;
      break;
    case 'c': {
      // Parse JSON config
      Json::CharReaderBuilder builder;
      std::istringstream stream(optarg);
      std::string errors;
      if (!Json::parseFromStream(builder, stream, &args.config, &errors)) {
        args.error = "Failed to parse config JSON: " + errors;
        return args;
      }
      break;
    }
    case 'h':
      args.error = "Usage: edge_ai_worker --instance-id <id> --socket <path> "
                   "[--config <json>]";
      return args;
    default:
      args.error = "Unknown option";
      return args;
    }
  }

  // Validate required arguments
  if (args.instance_id.empty()) {
    args.error = "Missing required argument: --instance-id";
    return args;
  }

  if (args.socket_path.empty()) {
    args.error = "Missing required argument: --socket";
    return args;
  }

  args.valid = true;
  return args;
}

} // namespace worker
