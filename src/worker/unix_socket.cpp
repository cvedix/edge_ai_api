#include "worker/unix_socket.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <filesystem>
#include <cstdlib>

namespace worker {

// ============================================================================
// UnixSocketServer
// ============================================================================

UnixSocketServer::UnixSocketServer(const std::string& socket_path)
    : socket_path_(socket_path) {}

UnixSocketServer::~UnixSocketServer() {
    stop();
}

bool UnixSocketServer::start(MessageHandler handler) {
    if (running_.load()) {
        return false;
    }
    
    handler_ = std::move(handler);
    
    // Clean up existing socket
    cleanupSocket(socket_path_);
    
    // Create socket
    server_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "[Worker] Failed to create socket: " << strerror(errno) << std::endl;
        return false;
    }
    
    // Bind
    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);
    
    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[Worker] Failed to bind socket: " << strerror(errno) << std::endl;
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }
    
    // Listen
    if (listen(server_fd_, 5) < 0) {
        std::cerr << "[Worker] Failed to listen: " << strerror(errno) << std::endl;
        close(server_fd_);
        server_fd_ = -1;
        cleanupSocket(socket_path_);
        return false;
    }
    
    running_.store(true);
    accept_thread_ = std::thread(&UnixSocketServer::acceptLoop, this);
    
    std::cout << "[Worker] Server listening on " << socket_path_ << std::endl;
    return true;
}

void UnixSocketServer::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    // Close server socket to interrupt accept()
    if (server_fd_ >= 0) {
        shutdown(server_fd_, SHUT_RDWR);
        close(server_fd_);
        server_fd_ = -1;
    }
    
    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }
    
    cleanupSocket(socket_path_);
}

void UnixSocketServer::acceptLoop() {
    while (running_.load()) {
        struct pollfd pfd;
        pfd.fd = server_fd_;
        pfd.events = POLLIN;
        
        int ret = poll(&pfd, 1, 1000);  // 1 second timeout
        if (ret <= 0) {
            continue;
        }
        
        int client_fd = accept(server_fd_, nullptr, nullptr);
        if (client_fd < 0) {
            if (running_.load()) {
                std::cerr << "[Worker] Accept failed: " << strerror(errno) << std::endl;
            }
            continue;
        }
        
        // Handle client in same thread (single client expected per worker)
        handleClient(client_fd);
    }
}

void UnixSocketServer::handleClient(int client_fd) {
    while (running_.load()) {
        // Read header
        char header_buf[MessageHeader::HEADER_SIZE];
        ssize_t n = recv(client_fd, header_buf, MessageHeader::HEADER_SIZE, MSG_WAITALL);
        if (n <= 0) {
            break;
        }
        
        MessageHeader header;
        if (!MessageHeader::deserialize(header_buf, n, header)) {
            std::cerr << "[Worker] Invalid message header" << std::endl;
            break;
        }
        
        // Read payload
        std::string payload_buf(header.payload_size, '\0');
        if (header.payload_size > 0) {
            n = recv(client_fd, &payload_buf[0], header.payload_size, MSG_WAITALL);
            if (n != static_cast<ssize_t>(header.payload_size)) {
                std::cerr << "[Worker] Failed to read payload" << std::endl;
                break;
            }
        }
        
        // Deserialize message
        std::string full_msg = std::string(header_buf, MessageHeader::HEADER_SIZE) + payload_buf;
        IPCMessage request;
        if (!IPCMessage::deserialize(full_msg, request)) {
            std::cerr << "[Worker] Failed to deserialize message" << std::endl;
            continue;
        }
        
        // Handle message
        IPCMessage response = handler_(request);
        
        // Send response
        std::string response_data = response.serialize();
        ssize_t sent = send(client_fd, response_data.data(), response_data.size(), 0);
        if (sent != static_cast<ssize_t>(response_data.size())) {
            std::cerr << "[Worker] Failed to send response" << std::endl;
            break;
        }
    }
    
    close(client_fd);
}

// ============================================================================
// UnixSocketClient
// ============================================================================

UnixSocketClient::UnixSocketClient(const std::string& socket_path)
    : socket_path_(socket_path) {}

UnixSocketClient::~UnixSocketClient() {
    disconnect();
}

bool UnixSocketClient::connect(int timeout_ms) {
    if (connected_.load()) {
        return true;
    }
    
    socket_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        return false;
    }
    
    // Set non-blocking for connect with timeout
    int flags = fcntl(socket_fd_, F_GETFL, 0);
    fcntl(socket_fd_, F_SETFL, flags | O_NONBLOCK);
    
    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);
    
    int ret = ::connect(socket_fd_, (struct sockaddr*)&addr, sizeof(addr));
    if (ret < 0 && errno != EINPROGRESS) {
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    
    if (ret < 0) {
        // Wait for connection
        struct pollfd pfd;
        pfd.fd = socket_fd_;
        pfd.events = POLLOUT;
        
        ret = poll(&pfd, 1, timeout_ms);
        if (ret <= 0) {
            close(socket_fd_);
            socket_fd_ = -1;
            return false;
        }
        
        // Check for connection error
        int error = 0;
        socklen_t len = sizeof(error);
        getsockopt(socket_fd_, SOL_SOCKET, SO_ERROR, &error, &len);
        if (error != 0) {
            close(socket_fd_);
            socket_fd_ = -1;
            return false;
        }
    }
    
    // Set back to blocking
    fcntl(socket_fd_, F_SETFL, flags);
    
    connected_.store(true);
    return true;
}

void UnixSocketClient::disconnect() {
    if (!connected_.load()) {
        return;
    }
    
    connected_.store(false);
    
    if (socket_fd_ >= 0) {
        shutdown(socket_fd_, SHUT_RDWR);
        close(socket_fd_);
        socket_fd_ = -1;
    }
}

IPCMessage UnixSocketClient::sendAndReceive(const IPCMessage& msg, int timeout_ms) {
    std::lock_guard<std::mutex> send_lock(send_mutex_);
    std::lock_guard<std::mutex> recv_lock(recv_mutex_);
    
    if (!connected_.load()) {
        IPCMessage error;
        error.type = MessageType::ERROR_RESPONSE;
        error.payload = createErrorResponse("Not connected");
        return error;
    }
    
    // Send
    std::string data = msg.serialize();
    if (!sendRaw(data)) {
        IPCMessage error;
        error.type = MessageType::ERROR_RESPONSE;
        error.payload = createErrorResponse("Send failed");
        return error;
    }
    
    // Receive header
    std::string header_data = receiveRaw(MessageHeader::HEADER_SIZE, timeout_ms);
    if (header_data.empty()) {
        IPCMessage error;
        error.type = MessageType::ERROR_RESPONSE;
        error.payload = createErrorResponse("Receive timeout");
        return error;
    }
    
    MessageHeader header;
    if (!MessageHeader::deserialize(header_data.data(), header_data.size(), header)) {
        IPCMessage error;
        error.type = MessageType::ERROR_RESPONSE;
        error.payload = createErrorResponse("Invalid response header");
        return error;
    }
    
    // Receive payload
    std::string payload_data;
    if (header.payload_size > 0) {
        payload_data = receiveRaw(header.payload_size, timeout_ms);
        if (payload_data.empty()) {
            IPCMessage error;
            error.type = MessageType::ERROR_RESPONSE;
            error.payload = createErrorResponse("Receive payload timeout");
            return error;
        }
    }
    
    // Deserialize
    IPCMessage response;
    std::string full_data = header_data + payload_data;
    if (!IPCMessage::deserialize(full_data, response)) {
        IPCMessage error;
        error.type = MessageType::ERROR_RESPONSE;
        error.payload = createErrorResponse("Failed to deserialize response");
        return error;
    }
    
    return response;
}

bool UnixSocketClient::send(const IPCMessage& msg) {
    std::lock_guard<std::mutex> lock(send_mutex_);
    if (!connected_.load()) {
        return false;
    }
    return sendRaw(msg.serialize());
}

IPCMessage UnixSocketClient::receive(int timeout_ms) {
    std::lock_guard<std::mutex> lock(recv_mutex_);
    
    if (!connected_.load()) {
        IPCMessage error;
        error.type = MessageType::ERROR_RESPONSE;
        error.payload = createErrorResponse("Not connected");
        return error;
    }
    
    // Receive header
    std::string header_data = receiveRaw(MessageHeader::HEADER_SIZE, timeout_ms);
    if (header_data.empty()) {
        IPCMessage error;
        error.type = MessageType::ERROR_RESPONSE;
        error.payload = createErrorResponse("Receive timeout");
        return error;
    }
    
    MessageHeader header;
    if (!MessageHeader::deserialize(header_data.data(), header_data.size(), header)) {
        IPCMessage error;
        error.type = MessageType::ERROR_RESPONSE;
        error.payload = createErrorResponse("Invalid header");
        return error;
    }
    
    // Receive payload
    std::string payload_data;
    if (header.payload_size > 0) {
        payload_data = receiveRaw(header.payload_size, timeout_ms);
    }
    
    IPCMessage response;
    std::string full_data = header_data + payload_data;
    if (!IPCMessage::deserialize(full_data, response)) {
        IPCMessage error;
        error.type = MessageType::ERROR_RESPONSE;
        error.payload = createErrorResponse("Deserialize failed");
        return error;
    }
    
    return response;
}

bool UnixSocketClient::sendRaw(const std::string& data) {
    size_t total_sent = 0;
    while (total_sent < data.size()) {
        ssize_t sent = ::send(socket_fd_, data.data() + total_sent, 
                              data.size() - total_sent, MSG_NOSIGNAL);
        if (sent <= 0) {
            return false;
        }
        total_sent += sent;
    }
    return true;
}

std::string UnixSocketClient::receiveRaw(size_t expected_size, int timeout_ms) {
    std::string result(expected_size, '\0');
    size_t total_received = 0;
    
    while (total_received < expected_size) {
        struct pollfd pfd;
        pfd.fd = socket_fd_;
        pfd.events = POLLIN;
        
        int ret = poll(&pfd, 1, timeout_ms);
        if (ret <= 0) {
            return "";
        }
        
        ssize_t received = recv(socket_fd_, &result[total_received], 
                                expected_size - total_received, 0);
        if (received <= 0) {
            return "";
        }
        total_received += received;
    }
    
    return result;
}

// ============================================================================
// Utility functions
// ============================================================================

std::string generateSocketPath(const std::string& instance_id) {
    // Check environment variable first
    const char* socket_dir = std::getenv("EDGE_AI_SOCKET_DIR");
    std::string dir;
    
    if (socket_dir && strlen(socket_dir) > 0) {
        dir = std::string(socket_dir);
    } else {
        // Default to /opt/edge_ai_api/run
        dir = "/opt/edge_ai_api/run";
    }
    
    // Create directory if it doesn't exist
    try {
        if (!std::filesystem::exists(dir)) {
            std::filesystem::create_directories(dir);
            std::cout << "[Socket] Created socket directory: " << dir << std::endl;
        }
    } catch (const std::filesystem::filesystem_error& e) {
        // If can't create in /opt, fallback to /tmp
        if (dir == "/opt/edge_ai_api/run") {
            std::cerr << "[Socket] ⚠ Cannot create " << dir 
                      << " (permission denied), falling back to /tmp" << std::endl;
            dir = "/tmp";
        } else {
            std::cerr << "[Socket] ⚠ Error creating socket directory " << dir 
                      << ": " << e.what() << std::endl;
            // Fallback to /tmp as last resort
            dir = "/tmp";
        }
    }
    
    // Return full socket path
    return dir + "/edge_ai_worker_" + instance_id + ".sock";
}

void cleanupSocket(const std::string& socket_path) {
    std::error_code ec;
    std::filesystem::remove(socket_path, ec);
}

} // namespace worker

