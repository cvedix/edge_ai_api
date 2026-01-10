#include "instances/uri_parser.h"
#include <algorithm>
#include <regex>

namespace InstanceUriParser {

std::string extractUrlFromPipeline(const std::string &uri) {
  if (uri.empty()) {
    return "";
  }

  // Try to extract from GStreamer pipeline formats
  // Pattern 1: rtspsrc location=URL ! ...
  std::regex rtspPattern(R"(rtspsrc\s+location=([^\s!]+))");
  std::smatch rtspMatch;
  if (std::regex_search(uri, rtspMatch, rtspPattern)) {
    return rtspMatch[1].str();
  }

  // Pattern 2: rtmpsrc location=URL ! ...
  std::regex rtmpPattern(R"(rtmpsrc\s+location=([^\s!]+))");
  std::smatch rtmpMatch;
  if (std::regex_search(uri, rtmpMatch, rtmpPattern)) {
    return rtmpMatch[1].str();
  }

  // Pattern 3: souphttpsrc location=URL ! ... (for HLS/HTTP)
  std::regex httpPattern(R"(souphttpsrc\s+location=([^\s!]+))");
  std::smatch httpMatch;
  if (std::regex_search(uri, httpMatch, httpPattern)) {
    return httpMatch[1].str();
  }

  // Pattern 4: filesrc location=PATH ! ...
  std::regex filePattern(R"(filesrc\s+location=([^\s!]+))");
  std::smatch fileMatch;
  if (std::regex_search(uri, fileMatch, filePattern)) {
    return fileMatch[1].str();
  }

  // Pattern 5: gstreamer:///urisourcebin uri=URL ! ... (old format)
  std::regex uriSourcePattern(R"(gstreamer:///urisourcebin\s+uri=([^\s!]+))");
  std::smatch uriSourceMatch;
  if (std::regex_search(uri, uriSourceMatch, uriSourcePattern)) {
    return uriSourceMatch[1].str();
  }

  // Pattern 6: Direct URL or path (no GStreamer pipeline)
  // Check if it's a URL with protocol
  if (uri.find("://") != std::string::npos) {
    // It's a direct URL (rtsp://, rtmp://, http://, https://, etc.)
    size_t spacePos = uri.find(" ");
    size_t exclamationPos = uri.find("!");
    size_t endPos = std::min(spacePos, exclamationPos);
    if (endPos == std::string::npos) {
      return uri;
    }
    return uri.substr(0, endPos);
  }

  // Pattern 7: Direct file path (no protocol, no GStreamer pipeline)
  // Check if it looks like a file path
  size_t spacePos = uri.find(" ");
  size_t exclamationPos = uri.find("!");
  if (spacePos == std::string::npos && exclamationPos == std::string::npos) {
    return uri; // Likely a direct file path
  }

  // If we can't parse it, return empty (will be handled by caller)
  return "";
}

void parseInputUriToAdditionalParams(const std::string &uri,
                                     std::map<std::string, std::string> &additionalParams) {
  std::string extractedUrl = extractUrlFromPipeline(uri);
  
  if (!extractedUrl.empty()) {
    // Determine input type based on extracted URL
    std::string lowerUrl = extractedUrl;
    std::transform(lowerUrl.begin(), lowerUrl.end(), lowerUrl.begin(), ::tolower);
    
    if (lowerUrl.find("rtsp://") == 0 || lowerUrl.find("rtsps://") == 0) {
      // RTSP URL
      if (!additionalParams.count("RTSP_URL")) {
        additionalParams["RTSP_URL"] = extractedUrl;
      }
    } else if (lowerUrl.find("rtmp://") == 0) {
      // RTMP URL
      if (!additionalParams.count("RTMP_URL")) {
        additionalParams["RTMP_URL"] = extractedUrl;
      }
    } else if (lowerUrl.find("http://") == 0 || lowerUrl.find("https://") == 0) {
      // HTTP/HTTPS URL - check if it's HLS
      if (lowerUrl.find(".m3u8") != std::string::npos) {
        // HLS URL
        if (!additionalParams.count("HLS_URL")) {
          additionalParams["HLS_URL"] = extractedUrl;
        }
      } else {
        // HTTP URL
        if (!additionalParams.count("HTTP_URL")) {
          additionalParams["HTTP_URL"] = extractedUrl;
        }
      }
    } else if (lowerUrl.find("udp://") == 0) {
      // UDP URL
      if (!additionalParams.count("UDP_URL")) {
        additionalParams["UDP_URL"] = extractedUrl;
      }
    } else {
      // File path (no protocol or local file)
      if (!additionalParams.count("FILE_PATH")) {
        additionalParams["FILE_PATH"] = extractedUrl;
      }
    }
  } else {
    // Fallback: if extraction failed, try old parsing method
    size_t rtspPos = uri.find("location=");
    if (rtspPos != std::string::npos) {
      size_t start = rtspPos + 9;
      size_t end = uri.find(" ", start);
      if (end == std::string::npos) {
        end = uri.find(" !", start);
      }
      if (end == std::string::npos) {
        end = uri.length();
      }
      std::string url = uri.substr(start, end - start);
      std::string lowerUrl = url;
      std::transform(lowerUrl.begin(), lowerUrl.end(), lowerUrl.begin(), ::tolower);
      if (lowerUrl.find("rtsp://") == 0 || lowerUrl.find("rtsps://") == 0) {
        if (!additionalParams.count("RTSP_URL")) {
          additionalParams["RTSP_URL"] = url;
        }
      } else {
        if (!additionalParams.count("FILE_PATH")) {
          additionalParams["FILE_PATH"] = url;
        }
      }
    } else if (uri.find("://") == std::string::npos) {
      // Direct file path (no protocol)
      if (!additionalParams.count("FILE_PATH")) {
        additionalParams["FILE_PATH"] = uri;
      }
    } else {
      // URL with protocol - try to detect type
      std::string lowerUri = uri;
      std::transform(lowerUri.begin(), lowerUri.end(), lowerUri.begin(), ::tolower);
      if (lowerUri.find("rtsp://") == 0 || lowerUri.find("rtsps://") == 0) {
        if (!additionalParams.count("RTSP_URL")) {
          additionalParams["RTSP_URL"] = uri;
        }
      } else {
        if (!additionalParams.count("FILE_PATH")) {
          additionalParams["FILE_PATH"] = uri;
        }
      }
    }
  }
}

} // namespace InstanceUriParser

