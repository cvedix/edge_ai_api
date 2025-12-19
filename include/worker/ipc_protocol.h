#pragma once

#include <cstdint>
#include <json/json.h>
#include <string>

namespace worker {

/**
 * @brief IPC Message Types for Unix Socket communication
 */
enum class MessageType : uint8_t {
  // Worker lifecycle
  PING = 0,
  PONG = 1,
  SHUTDOWN = 2,
  SHUTDOWN_ACK = 3,

  // Instance management
  CREATE_INSTANCE = 10,
  CREATE_INSTANCE_RESPONSE = 11,
  DELETE_INSTANCE = 12,
  DELETE_INSTANCE_RESPONSE = 13,
  START_INSTANCE = 14,
  START_INSTANCE_RESPONSE = 15,
  STOP_INSTANCE = 16,
  STOP_INSTANCE_RESPONSE = 17,
  UPDATE_INSTANCE = 18,
  UPDATE_INSTANCE_RESPONSE = 19,

  // Query
  GET_INSTANCE_STATUS = 20,
  GET_INSTANCE_STATUS_RESPONSE = 21,
  GET_STATISTICS = 22,
  GET_STATISTICS_RESPONSE = 23,
  GET_LAST_FRAME = 24,
  GET_LAST_FRAME_RESPONSE = 25,

  // Events (worker -> supervisor)
  INSTANCE_STATE_CHANGED = 30,
  INSTANCE_ERROR = 31,
  WORKER_READY = 32,
  WORKER_MEMORY_WARNING = 33,

  // Error
  ERROR_RESPONSE = 255
};

/**
 * @brief IPC Message Header (fixed size: 16 bytes)
 *
 * Wire format:
 * [0-3]   magic (4 bytes): "EDGE"
 * [4]     version (1 byte)
 * [5]     type (1 byte): MessageType
 * [6-7]   reserved (2 bytes)
 * [8-15]  payload_size (8 bytes): little-endian uint64
 */
struct MessageHeader {
  static constexpr char MAGIC[4] = {'E', 'D', 'G', 'E'};
  static constexpr uint8_t VERSION = 1;
  static constexpr size_t HEADER_SIZE = 16;

  uint8_t type;
  uint64_t payload_size;

  // Serialize to bytes
  std::string serialize() const;

  // Deserialize from bytes (returns true if valid)
  static bool deserialize(const char *data, size_t len, MessageHeader &out);
};

/**
 * @brief IPC Message (header + JSON payload)
 */
struct IPCMessage {
  MessageType type;
  Json::Value payload;

  // Serialize entire message (header + payload)
  std::string serialize() const;

  // Deserialize from raw bytes
  static bool deserialize(const std::string &data, IPCMessage &out);
};

/**
 * @brief Response status codes
 */
enum class ResponseStatus : int {
  OK = 0,
  ERROR = 1,
  NOT_FOUND = 2,
  ALREADY_EXISTS = 3,
  INVALID_REQUEST = 4,
  INTERNAL_ERROR = 5,
  TIMEOUT = 6
};

/**
 * @brief Create standard response payload
 */
Json::Value createResponse(ResponseStatus status,
                           const std::string &message = "",
                           const Json::Value &data = Json::Value());

/**
 * @brief Create error response payload
 */
Json::Value createErrorResponse(const std::string &error,
                                ResponseStatus status = ResponseStatus::ERROR);

} // namespace worker
