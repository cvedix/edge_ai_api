#pragma once

#include <chrono>
#include <cstdint>
#include <json/json.h>
#include <string>

/**
 * @brief Statistics information for an instance
 *
 * Contains real-time statistics about instance performance and processing.
 */
struct InstanceStatistics {
  uint64_t frames_processed = 0;  // Frames actually processed
  uint64_t frames_incoming = 0;   // All frames from source (including dropped)
  double source_framerate = 0.0;  // FPS from source
  double current_framerate = 0.0; // Current processing FPS
  double latency = 0.0;           // Average latency in milliseconds
  int64_t start_time = 0;         // Unix timestamp (seconds)
  size_t input_queue_size = 0;
  uint64_t dropped_frames_count = 0; // Frames dropped
  std::string resolution;            // e.g., "1280x720"
  std::string format;                // e.g., "BGR"
  std::string source_resolution;     // e.g., "1920x1080"

  /**
   * @brief Convert statistics to JSON value
   * @return Json::Value object with all statistics fields
   */
  Json::Value toJson() const {
    Json::Value json;
    json["frames_processed"] = static_cast<Json::Int64>(frames_processed);
    json["frames_incoming"] = static_cast<Json::Int64>(frames_incoming);
    json["source_framerate"] = source_framerate;
    json["current_framerate"] = current_framerate;
    json["latency"] = latency;
    json["start_time"] = static_cast<Json::Int64>(start_time);
    json["input_queue_size"] = static_cast<Json::Int64>(input_queue_size);
    json["dropped_frames_count"] =
        static_cast<Json::Int64>(dropped_frames_count);
    json["resolution"] = resolution;
    json["format"] = format;
    json["source_resolution"] = source_resolution;
    return json;
  }

  /**
   * @brief Convert statistics to JSON string
   * @return JSON string representation
   */
  std::string toJsonString() const {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    std::ostringstream oss;
    writer->write(toJson(), &oss);
    return oss.str();
  }
};
