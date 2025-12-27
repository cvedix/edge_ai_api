#include "api/ai_websocket.h"
#include "core/logging_flags.h"
#include "instances/instance_manager.h"
#include <chrono>
#include <iostream>
#include <json/json.h>
#include <map>
#include <mutex>
#include <thread>

std::atomic<size_t> AIWebSocketController::active_connections_{0};
IInstanceManager *AIWebSocketController::instance_manager_ = nullptr;

// Map to store instanceId for each WebSocket connection
static std::map<void *, std::string> connection_instance_map;
static std::mutex connection_map_mutex;

void AIWebSocketController::setInstanceManager(IInstanceManager *manager) {
  instance_manager_ = manager;
}

void AIWebSocketController::handleNewConnection(
    const HttpRequestPtr &req, const WebSocketConnectionPtr &wsConnPtr) {
  active_connections_++;

  // Extract instanceId from path if this is an instance stream connection
  std::string path = req->getPath();
  std::string instanceId;
  if (path.find("/instance/") != std::string::npos) {
    size_t instancePos = path.find("/instance/");
    size_t start = instancePos + 10; // length of "/instance/"
    size_t end = path.find("/stream", start);
    if (end == std::string::npos) {
      end = path.length();
    }
    instanceId = path.substr(start, end - start);

    // Store instanceId for this connection
    std::lock_guard<std::mutex> lock(connection_map_mutex);
    connection_instance_map[wsConnPtr.get()] = instanceId;
  }

  std::cout << "[WebSocket] New connection. Total: "
            << active_connections_.load() << std::endl;

  Json::Value welcome;
  welcome["type"] = "connected";
  welcome["message"] = "WebSocket connection established";
  if (!instanceId.empty()) {
    welcome["instanceId"] = instanceId;
  }

  Json::StreamWriterBuilder builder;
  std::string message = Json::writeString(builder, welcome);
  wsConnPtr->send(message);
}

void AIWebSocketController::handleConnectionClosed(
    const WebSocketConnectionPtr &wsConnPtr) {
  active_connections_--;

  // Remove instanceId mapping
  {
    std::lock_guard<std::mutex> lock(connection_map_mutex);
    connection_instance_map.erase(wsConnPtr.get());
  }

  std::cout << "[WebSocket] Connection closed. Total: "
            << active_connections_.load() << std::endl;
}

void AIWebSocketController::handleNewMessage(
    const WebSocketConnectionPtr &wsConnPtr, std::string &&message,
    const WebSocketMessageType &type) {
  if (type == WebSocketMessageType::Text) {
    // Check if this is an instance stream connection
    std::string instanceId;
    {
      std::lock_guard<std::mutex> lock(connection_map_mutex);
      auto it = connection_instance_map.find(wsConnPtr.get());
      if (it != connection_instance_map.end()) {
        instanceId = it->second;
      }
    }

    if (!instanceId.empty()) {
      processInstanceStreamMessage(wsConnPtr, message, instanceId);
    } else {
      processStreamMessage(wsConnPtr, message);
    }
  } else if (type == WebSocketMessageType::Binary) {
    // Handle binary data (e.g., image frames)
    // For now, just acknowledge
    Json::Value response;
    response["type"] = "ack";
    response["message"] = "Binary data received";

    Json::StreamWriterBuilder builder;
    std::string response_str = Json::writeString(builder, response);
    wsConnPtr->send(response_str);
  }
}

void AIWebSocketController::processStreamMessage(
    const WebSocketConnectionPtr &wsConnPtr, const std::string &message) {
  try {
    Json::Value json;
    Json::Reader reader;

    if (!reader.parse(message, json)) {
      Json::Value error;
      error["type"] = "error";
      error["message"] = "Invalid JSON";
      sendResult(wsConnPtr,
                 Json::writeString(Json::StreamWriterBuilder(), error));
      return;
    }

    std::string msg_type = json.get("type", "").asString();

    if (msg_type == "process") {
      // Process AI request
      std::string image_data = json.get("image", "").asString();
      std::string config = json.get("config", "").asString();

      // TODO: Integrate with AI processor
      // For now, send dummy response
      Json::Value result;
      result["type"] = "result";
      result["status"] = "processing";
      result["message"] = "AI processing started";

      sendResult(wsConnPtr,
                 Json::writeString(Json::StreamWriterBuilder(), result));
    } else if (msg_type == "ping") {
      Json::Value pong;
      pong["type"] = "pong";
      sendResult(wsConnPtr,
                 Json::writeString(Json::StreamWriterBuilder(), pong));
    }
  } catch (const std::exception &e) {
    Json::Value error;
    error["type"] = "error";
    error["message"] = e.what();
    sendResult(wsConnPtr,
               Json::writeString(Json::StreamWriterBuilder(), error));
  }
}

void AIWebSocketController::processInstanceStreamMessage(
    const WebSocketConnectionPtr &wsConnPtr, const std::string &message,
    const std::string &instanceId) {
  try {
    Json::Value json;
    Json::Reader reader;

    if (!reader.parse(message, json)) {
      Json::Value error;
      error["type"] = "error";
      error["message"] = "Invalid JSON";
      sendResult(wsConnPtr,
                 Json::writeString(Json::StreamWriterBuilder(), error));
      return;
    }

    std::string msg_type = json.get("type", "").asString();

    if (msg_type == "subscribe") {
      // Start sending updates for this instance
      // Store connection info (in production, use a proper connection manager)
      Json::Value response;
      response["type"] = "subscribed";
      response["instanceId"] = instanceId;
      response["message"] = "Subscribed to instance updates";
      sendResult(wsConnPtr,
                 Json::writeString(Json::StreamWriterBuilder(), response));

      // Start periodic updates (in production, use a proper thread pool)
      std::thread([this, wsConnPtr, instanceId]() {
        while (wsConnPtr->connected()) {
          sendInstanceUpdate(wsConnPtr, instanceId);
          std::this_thread::sleep_for(
              std::chrono::milliseconds(1000)); // 1 second
        }
      }).detach();
    } else if (msg_type == "ping") {
      Json::Value pong;
      pong["type"] = "pong";
      pong["instanceId"] = instanceId;
      sendResult(wsConnPtr,
                 Json::writeString(Json::StreamWriterBuilder(), pong));
    }
  } catch (const std::exception &e) {
    Json::Value error;
    error["type"] = "error";
    error["message"] = e.what();
    sendResult(wsConnPtr,
               Json::writeString(Json::StreamWriterBuilder(), error));
  }
}

void AIWebSocketController::sendInstanceUpdate(
    const WebSocketConnectionPtr &wsConnPtr, const std::string &instanceId) {
  if (!instance_manager_) {
    return;
  }

  try {
    // Get instance info
    auto optInfo = instance_manager_->getInstance(instanceId);
    if (!optInfo.has_value()) {
      Json::Value error;
      error["type"] = "error";
      error["message"] = "Instance not found";
      error["instanceId"] = instanceId;
      sendResult(wsConnPtr,
                 Json::writeString(Json::StreamWriterBuilder(), error));
      return;
    }

    const auto &info = optInfo.value();

    // Get statistics
    auto optStats = instance_manager_->getInstanceStatistics(instanceId);

    // Build update message
    Json::Value update;
    update["type"] = "update";
    update["instanceId"] = instanceId;
    update["running"] = info.running;
    update["fps"] = info.fps;
    update["timestamp"] =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();

    if (optStats.has_value()) {
      const auto &stats = optStats.value();
      Json::Value statsJson;
      statsJson["frames_processed"] =
          static_cast<Json::UInt64>(stats.frames_processed);
      statsJson["current_framerate"] = stats.current_framerate;
      statsJson["source_framerate"] = stats.source_framerate;
      statsJson["latency"] = stats.latency;
      statsJson["input_queue_size"] =
          static_cast<Json::UInt64>(stats.input_queue_size);
      statsJson["dropped_frames_count"] =
          static_cast<Json::UInt64>(stats.dropped_frames_count);
      statsJson["resolution"] = stats.resolution;
      statsJson["source_resolution"] = stats.source_resolution;
      statsJson["format"] = stats.format;
      update["statistics"] = statsJson;
    }

    sendResult(wsConnPtr,
               Json::writeString(Json::StreamWriterBuilder(), update));
  } catch (const std::exception &e) {
    // Silently ignore errors to avoid spamming
  }
}

void AIWebSocketController::sendResult(const WebSocketConnectionPtr &wsConnPtr,
                                       const std::string &result) {
  if (wsConnPtr->connected()) {
    wsConnPtr->send(result);
  }
}
