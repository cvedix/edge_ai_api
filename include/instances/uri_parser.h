#pragma once

#include <string>
#include <map>

namespace InstanceUriParser {

/**
 * @brief Extract actual URL/path from GStreamer pipeline string
 * @param uri GStreamer pipeline string or direct URL/path
 * @return Extracted URL/path, or empty string if not found
 * 
 * Supports:
 * - RTSP: rtspsrc location=...
 * - RTMP: rtmpsrc location=...
 * - HLS/HTTP: souphttpsrc location=...
 * - File: filesrc location=...
 * - Old format: gstreamer:///urisourcebin uri=...
 * - Direct URL/path (no GStreamer pipeline)
 */
std::string extractUrlFromPipeline(const std::string &uri);

/**
 * @brief Parse Input.uri and populate additionalParams with appropriate fields
 * @param uri Input.uri string (GStreamer pipeline or direct URL/path)
 * @param additionalParams Map to populate with extracted URL/path
 * 
 * This function extracts the URL/path from Input.uri and sets the appropriate
 * field in additionalParams based on the input type:
 * - RTSP URL → RTSP_URL
 * - RTMP URL → RTMP_URL
 * - HLS URL → HLS_URL
 * - HTTP URL → HTTP_URL
 * - UDP URL → UDP_URL
 * - File path → FILE_PATH
 */
void parseInputUriToAdditionalParams(const std::string &uri,
                                     std::map<std::string, std::string> &additionalParams);

} // namespace InstanceUriParser

