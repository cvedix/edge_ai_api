#pragma once

#include <drogon/WebSocketController.h>
#include <drogon/HttpRequest.h>
#include <string>
#include <memory>
#include <atomic>

using namespace drogon;

/**
 * @brief WebSocket controller for real-time AI streaming
 * 
 * Endpoint: /v1/core/ai/stream
 * Supports bidirectional communication for streaming AI results
 */
class AIWebSocketController : public drogon::WebSocketController<AIWebSocketController> {
public:
    void handleNewMessage(const WebSocketConnectionPtr& wsConnPtr,
                         std::string&& message,
                         const WebSocketMessageType& type) override;

    void handleNewConnection(const HttpRequestPtr& req,
                            const WebSocketConnectionPtr& wsConnPtr) override;

    void handleConnectionClosed(const WebSocketConnectionPtr& wsConnPtr) override;

    WS_PATH_LIST_BEGIN
        WS_PATH_ADD("/v1/core/ai/stream", drogon::Get);
    WS_PATH_LIST_END

private:
    void processStreamMessage(const WebSocketConnectionPtr& wsConnPtr,
                             const std::string& message);
    
    void sendResult(const WebSocketConnectionPtr& wsConnPtr,
                   const std::string& result);
    
    static std::atomic<size_t> active_connections_;
};

