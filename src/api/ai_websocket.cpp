#include "api/ai_websocket.h"
#include <iostream>
#include <json/json.h>

std::atomic<size_t> AIWebSocketController::active_connections_{0};

void AIWebSocketController::handleNewConnection(
    const HttpRequestPtr &req, const WebSocketConnectionPtr &wsConnPtr) {
  active_connections_++;
  std::cout << "[WebSocket] New connection. Total: "
            << active_connections_.load() << std::endl;

  Json::Value welcome;
  welcome["type"] = "connected";
  welcome["message"] = "WebSocket connection established";

  Json::StreamWriterBuilder builder;
  std::string message = Json::writeString(builder, welcome);
  wsConnPtr->send(message);
}

void AIWebSocketController::handleConnectionClosed(
    const WebSocketConnectionPtr &wsConnPtr) {
  active_connections_--;
  std::cout << "[WebSocket] Connection closed. Total: "
            << active_connections_.load() << std::endl;
}

void AIWebSocketController::handleNewMessage(
    const WebSocketConnectionPtr &wsConnPtr, std::string &&message,
    const WebSocketMessageType &type) {
  if (type == WebSocketMessageType::Text) {
    processStreamMessage(wsConnPtr, message);
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

void AIWebSocketController::sendResult(const WebSocketConnectionPtr &wsConnPtr,
                                       const std::string &result) {
  wsConnPtr->send(result);
}
