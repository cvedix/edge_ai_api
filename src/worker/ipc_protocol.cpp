#include "worker/ipc_protocol.h"
#include <cstring>
#include <sstream>

namespace worker {

std::string MessageHeader::serialize() const {
    std::string result(HEADER_SIZE, '\0');
    
    // Magic
    std::memcpy(&result[0], MAGIC, 4);
    
    // Version
    result[4] = VERSION;
    
    // Type
    result[5] = type;
    
    // Reserved
    result[6] = 0;
    result[7] = 0;
    
    // Payload size (little-endian)
    uint64_t size = payload_size;
    for (int i = 0; i < 8; ++i) {
        result[8 + i] = static_cast<char>(size & 0xFF);
        size >>= 8;
    }
    
    return result;
}

bool MessageHeader::deserialize(const char* data, size_t len, MessageHeader& out) {
    if (len < HEADER_SIZE) {
        return false;
    }
    
    // Check magic
    if (std::memcmp(data, MAGIC, 4) != 0) {
        return false;
    }
    
    // Check version
    if (static_cast<uint8_t>(data[4]) != VERSION) {
        return false;
    }
    
    // Type
    out.type = static_cast<uint8_t>(data[5]);
    
    // Payload size (little-endian)
    out.payload_size = 0;
    for (int i = 7; i >= 0; --i) {
        out.payload_size = (out.payload_size << 8) | static_cast<uint8_t>(data[8 + i]);
    }
    
    return true;
}

std::string IPCMessage::serialize() const {
    // Serialize payload to JSON string
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";  // Compact JSON
    std::string payload_str = Json::writeString(builder, payload);
    
    // Create header
    MessageHeader header;
    header.type = static_cast<uint8_t>(type);
    header.payload_size = payload_str.size();
    
    // Combine header + payload
    return header.serialize() + payload_str;
}

bool IPCMessage::deserialize(const std::string& data, IPCMessage& out) {
    if (data.size() < MessageHeader::HEADER_SIZE) {
        return false;
    }
    
    // Parse header
    MessageHeader header;
    if (!MessageHeader::deserialize(data.data(), data.size(), header)) {
        return false;
    }
    
    // Check payload size
    if (data.size() < MessageHeader::HEADER_SIZE + header.payload_size) {
        return false;
    }
    
    out.type = static_cast<MessageType>(header.type);
    
    // Parse payload JSON
    if (header.payload_size > 0) {
        std::string payload_str(data.data() + MessageHeader::HEADER_SIZE, header.payload_size);
        Json::CharReaderBuilder reader_builder;
        std::istringstream stream(payload_str);
        std::string errors;
        if (!Json::parseFromStream(reader_builder, stream, &out.payload, &errors)) {
            return false;
        }
    } else {
        out.payload = Json::Value();
    }
    
    return true;
}

Json::Value createResponse(ResponseStatus status, const std::string& message, 
                           const Json::Value& data) {
    Json::Value response;
    response["status"] = static_cast<int>(status);
    response["success"] = (status == ResponseStatus::OK);
    if (!message.empty()) {
        response["message"] = message;
    }
    if (!data.isNull()) {
        response["data"] = data;
    }
    return response;
}

Json::Value createErrorResponse(const std::string& error, ResponseStatus status) {
    Json::Value response;
    response["status"] = static_cast<int>(status);
    response["success"] = false;
    response["error"] = error;
    return response;
}

} // namespace worker

