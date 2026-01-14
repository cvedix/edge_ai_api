#include "videos/video_preview_handler.h"
#include "core/cors_helper.h"
#include "core/env_config.h"
#include "core/metrics_interceptor.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <drogon/HttpResponse.h>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <iomanip>

namespace fs = std::filesystem;

// Static members
std::string VideoPreviewHandler::videos_dir_ = "./videos";
std::mutex VideoPreviewHandler::streams_mutex_;
std::unordered_map<std::string, PreviewStreamInfo> VideoPreviewHandler::active_streams_;

// Preview timeout: 30 minutes
constexpr auto PREVIEW_TIMEOUT_MINUTES = std::chrono::minutes(30);

void VideoPreviewHandler::setVideosDirectory(const std::string &dir) {
  videos_dir_ = dir;
}

std::string VideoPreviewHandler::getVideosDirectory() const {
  return videos_dir_;
}

std::string VideoPreviewHandler::extractVideoName(const HttpRequestPtr &req) const {
  std::string videoName = req->getParameter("videoName");
  
  if (videoName.empty()) {
    std::string path = req->getPath();
    size_t videoPos = path.find("/v1/core/video/");
    if (videoPos != std::string::npos) {
      size_t start = videoPos + 15; // length of "/v1/core/video/"
      size_t end = path.find("/", start);
      if (end != std::string::npos) {
        videoName = path.substr(start, end - start);
      } else {
        videoName = path.substr(start);
      }
    }
  }
  
  // URL decode
  if (!videoName.empty()) {
    std::string decoded;
    decoded.reserve(videoName.length());
    for (size_t i = 0; i < videoName.length(); ++i) {
      if (videoName[i] == '%' && i + 2 < videoName.length()) {
        char hex[3] = {videoName[i + 1], videoName[i + 2], '\0'};
        char *end;
        unsigned long value = std::strtoul(hex, &end, 16);
        if (*end == '\0' && value <= 255) {
          decoded += static_cast<char>(value);
          i += 2;
        } else {
          decoded += videoName[i];
        }
      } else {
        decoded += videoName[i];
      }
    }
    videoName = decoded;
  }
  
  return videoName;
}

std::string VideoPreviewHandler::extractPreviewId(const HttpRequestPtr &req) const {
  std::string previewId = req->getParameter("previewId");
  
  if (previewId.empty()) {
    std::string path = req->getPath();
    size_t previewPos = path.find("/preview/");
    if (previewPos != std::string::npos) {
      size_t start = previewPos + 9; // length of "/preview/"
      size_t end = path.find("/", start);
      if (end != std::string::npos) {
        previewId = path.substr(start, end - start);
      } else {
        previewId = path.substr(start);
      }
    }
  }
  
  return previewId;
}

std::string VideoPreviewHandler::findVideoFilePath(const std::string &videoName) const {
  std::string videosDir = getVideosDirectory();
  videosDir = EnvConfig::resolveDirectory(videosDir, "videos");
  fs::path videosPath(videosDir);
  
  // Try direct path first
  fs::path filePath = videosPath / videoName;
  if (fs::exists(filePath) && fs::is_regular_file(filePath)) {
    return fs::canonical(filePath).string();
  }
  
  // Try recursive search
  try {
    for (const auto &entry : fs::recursive_directory_iterator(videosPath)) {
      if (entry.is_regular_file() && entry.path().filename().string() == videoName) {
        return fs::canonical(entry.path()).string();
      }
    }
  } catch (const fs::filesystem_error &e) {
    // Ignore errors, return empty
  }
  
  return "";
}

std::vector<std::string> VideoPreviewHandler::buildFFmpegCommand(
    const std::string &videoPath, const std::string &outputFile) const {
  std::vector<std::string> command;
  command.push_back("ffmpeg");
  
  // Input file
  command.push_back("-i");
  command.push_back(videoPath);
  
  // Video codec options - transcode to H.264 + AAC for browser compatibility
  command.push_back("-c:v");
  command.push_back("libx264");
  command.push_back("-preset");
  command.push_back("ultrafast");
  command.push_back("-tune");
  command.push_back("zerolatency");
  command.push_back("-profile:v");
  command.push_back("baseline");
  command.push_back("-level");
  command.push_back("3.0");
  command.push_back("-pix_fmt");
  command.push_back("yuv420p");
  
  // Audio codec
  command.push_back("-c:a");
  command.push_back("aac");
  command.push_back("-b:a");
  command.push_back("128k");
  command.push_back("-ar");
  command.push_back("44100");
  
  // Fix timestamp issues
  command.push_back("-avoid_negative_ts");
  command.push_back("make_zero");
  
  // HLS output options
  command.push_back("-f");
  command.push_back("hls");
  command.push_back("-hls_time");
  command.push_back("2");
  command.push_back("-hls_list_size");
  command.push_back("10");
  command.push_back("-hls_flags");
  command.push_back("delete_segments");
  command.push_back("-hls_segment_type");
  command.push_back("mpegts");
  
  // Segment filename pattern
  std::string segmentPattern = outputFile;
  size_t lastDot = segmentPattern.find_last_of('.');
  if (lastDot != std::string::npos) {
    segmentPattern = segmentPattern.substr(0, lastDot) + "_%03d.ts";
  } else {
    segmentPattern += "_%03d.ts";
  }
  command.push_back("-hls_segment_filename");
  command.push_back(segmentPattern);
  
  command.push_back("-start_number");
  command.push_back("0");
  command.push_back("-hls_allow_cache");
  command.push_back("0");
  
  // Logging
  command.push_back("-loglevel");
  command.push_back("warning");
  
  // Overwrite output
  command.push_back("-y");
  command.push_back(outputFile);
  
  return command;
}

pid_t VideoPreviewHandler::startFFmpegProcess(
    const std::string &videoPath, const std::string &outputFile) const {
  auto command = buildFFmpegCommand(videoPath, outputFile);
  
  // Convert to char* array for execvp
  std::vector<char*> args;
  for (const auto &arg : command) {
    args.push_back(const_cast<char*>(arg.c_str()));
  }
  args.push_back(nullptr);
  
  pid_t pid = fork();
  if (pid == 0) {
    // Child process: redirect output and exec
    int devNull = open("/dev/null", O_WRONLY);
    if (devNull >= 0) {
      dup2(devNull, STDOUT_FILENO);
      dup2(devNull, STDERR_FILENO);
      close(devNull);
    }
    execvp("ffmpeg", args.data());
    exit(1);
  } else if (pid > 0) {
    return pid;
  } else {
    return -1;
  }
}

void VideoPreviewHandler::cleanupOutputDir(const std::string &outputDir) {
  try {
    fs::path dir(outputDir);
    if (fs::exists(dir)) {
      fs::remove_all(dir);
    }
  } catch (const fs::filesystem_error &e) {
    // Ignore cleanup errors
  }
}

void VideoPreviewHandler::startPreview(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  MetricsInterceptor::setHandlerStartTime(req);
  
  try {
    std::string videoName = extractVideoName(req);
    if (videoName.empty()) {
      callback(createErrorResponse(400, "Bad request", "Video name is required"));
      return;
    }
    
    // Find video file
    std::string videoPath = findVideoFilePath(videoName);
    if (videoPath.empty()) {
      callback(createErrorResponse(404, "Not found", "Video file not found: " + videoName));
      return;
    }
    
    // Generate preview ID (UUID-like format)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    std::uniform_int_distribution<> dis2(8, 11);
    
    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 8; i++) {
      ss << dis(gen);
    }
    ss << "-";
    for (int i = 0; i < 4; i++) {
      ss << dis(gen);
    }
    ss << "-4";
    for (int i = 0; i < 3; i++) {
      ss << dis(gen);
    }
    ss << "-";
    ss << dis2(gen);
    for (int i = 0; i < 3; i++) {
      ss << dis(gen);
    }
    ss << "-";
    for (int i = 0; i < 12; i++) {
      ss << dis(gen);
    }
    std::string previewId = ss.str();
    
    // Create output directory
    std::string hlsBaseDir = "/tmp/edge_ai_api_preview_hls";
    fs::path outputDir = fs::path(hlsBaseDir) / previewId;
    fs::create_directories(outputDir);
    
    // HLS output file
    fs::path hlsOutputPath = outputDir / "stream.m3u8";
    std::string hlsOutputFile = hlsOutputPath.string();
    
    // Start FFmpeg process
    pid_t processId = startFFmpegProcess(videoPath, hlsOutputFile);
    if (processId <= 0) {
      cleanupOutputDir(outputDir.string());
      callback(createErrorResponse(500, "Internal server error", "Failed to start FFmpeg process"));
      return;
    }
    
    // Wait a bit for FFmpeg to create the file
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Check if process is still alive
    int status;
    if (waitpid(processId, &status, WNOHANG) != 0) {
      // Process already exited
      cleanupOutputDir(outputDir.string());
      callback(createErrorResponse(500, "Internal server error", "FFmpeg process exited immediately"));
      return;
    }
    
    // Store stream info
    PreviewStreamInfo streamInfo;
    streamInfo.preview_id = previewId;
    streamInfo.video_name = videoName;
    streamInfo.video_path = videoPath;
    streamInfo.output_dir = outputDir.string();
    streamInfo.hls_output_file = hlsOutputFile;
    streamInfo.preview_url = "/v1/core/video/" + videoName + "/preview/" + previewId + "/stream.m3u8";
    streamInfo.process_id = processId;
    streamInfo.start_time = std::chrono::steady_clock::now();
    streamInfo.process_dead = false;
    
    {
      std::lock_guard<std::mutex> lock(streams_mutex_);
      active_streams_[previewId] = streamInfo;
    }
    
    // Build response
    Json::Value response;
    response["success"] = true;
    response["previewId"] = previewId;
    response["previewUrl"] = streamInfo.preview_url;
    response["message"] = "Preview stream started successfully";
    
    auto resp = createSuccessResponse(response);
    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
    
  } catch (const std::exception &e) {
    callback(createErrorResponse(500, "Internal server error", e.what()));
  }
}

void VideoPreviewHandler::stopPreview(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  MetricsInterceptor::setHandlerStartTime(req);
  
  try {
    std::string previewId = extractPreviewId(req);
    if (previewId.empty()) {
      callback(createErrorResponse(400, "Bad request", "Preview ID is required"));
      return;
    }
    
    std::lock_guard<std::mutex> lock(streams_mutex_);
    auto it = active_streams_.find(previewId);
    if (it == active_streams_.end()) {
      callback(createErrorResponse(404, "Not found", "Preview stream not found"));
      return;
    }
    
    PreviewStreamInfo &streamInfo = it->second;
    
    // Kill FFmpeg process
    if (streamInfo.process_id > 0) {
      kill(streamInfo.process_id, SIGTERM);
      // Wait a bit, then force kill if still alive
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      int status;
      if (waitpid(streamInfo.process_id, &status, WNOHANG) == 0) {
        kill(streamInfo.process_id, SIGKILL);
        waitpid(streamInfo.process_id, &status, 0);
      }
    }
    
    // Cleanup output directory
    cleanupOutputDir(streamInfo.output_dir);
    
    // Remove from active streams
    active_streams_.erase(it);
    
    Json::Value response;
    response["success"] = true;
    response["message"] = "Preview stream stopped";
    
    auto resp = createSuccessResponse(response);
    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
    
  } catch (const std::exception &e) {
    callback(createErrorResponse(500, "Internal server error", e.what()));
  }
}

void VideoPreviewHandler::serveHlsFile(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  MetricsInterceptor::setHandlerStartTime(req);
  
  try {
    std::string previewId = extractPreviewId(req);
    std::string filename = req->getParameter("filename");
    
    if (previewId.empty() || filename.empty()) {
      callback(createErrorResponse(400, "Bad request", "Preview ID and filename are required"));
      return;
    }
    
    std::lock_guard<std::mutex> lock(streams_mutex_);
    auto it = active_streams_.find(previewId);
    if (it == active_streams_.end()) {
      callback(createErrorResponse(404, "Not found", "Preview stream not found"));
      return;
    }
    
    PreviewStreamInfo &streamInfo = it->second;
    
    // Check if process is still alive
    int status;
    if (waitpid(streamInfo.process_id, &status, WNOHANG) != 0) {
      streamInfo.process_dead = true;
    }
    
    if (streamInfo.process_dead) {
      callback(createErrorResponse(410, "Gone", "Preview stream has stopped"));
      return;
    }
    
    // Build file path
    fs::path filePath = fs::path(streamInfo.output_dir) / filename;
    
    if (!fs::exists(filePath) || !fs::is_regular_file(filePath)) {
      callback(createErrorResponse(404, "Not found", "HLS file not found"));
      return;
    }
    
    // Read file content
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
      callback(createErrorResponse(500, "Internal server error", "Failed to open HLS file"));
      return;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    // Determine content type
    std::string contentType;
    if (filename.length() >= 5 && filename.substr(filename.length() - 5) == ".m3u8") {
      contentType = "application/vnd.apple.mpegurl";
    } else if (filename.length() >= 3 && filename.substr(filename.length() - 3) == ".ts") {
      contentType = "video/mp2t";
    } else {
      contentType = "application/octet-stream";
    }
    
    // Create response
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    resp->setContentTypeCode(CT_CUSTOM);
    resp->addHeader("Content-Type", contentType);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
    resp->setBody(content);
    
    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
    
  } catch (const std::exception &e) {
    callback(createErrorResponse(500, "Internal server error", e.what()));
  }
}

void VideoPreviewHandler::handleOptions(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  MetricsInterceptor::setHandlerStartTime(req);
  auto resp = CorsHelper::createOptionsResponse();
  MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
}

void VideoPreviewHandler::cleanupOldStreams() {
  auto now = std::chrono::steady_clock::now();
  auto timeout = std::chrono::duration_cast<std::chrono::milliseconds>(
      PREVIEW_TIMEOUT_MINUTES).count();
  
  std::lock_guard<std::mutex> lock(streams_mutex_);
  
  for (auto it = active_streams_.begin(); it != active_streams_.end();) {
    const auto &previewId = it->first;
    auto &streamInfo = it->second;
    
    // Check timeout
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - streamInfo.start_time).count();
    
    if (elapsed > timeout) {
      // Timeout - cleanup
      if (streamInfo.process_id > 0) {
        kill(streamInfo.process_id, SIGTERM);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        int status;
        if (waitpid(streamInfo.process_id, &status, WNOHANG) == 0) {
          kill(streamInfo.process_id, SIGKILL);
          waitpid(streamInfo.process_id, &status, 0);
        }
      }
      cleanupOutputDir(streamInfo.output_dir);
      it = active_streams_.erase(it);
      continue;
    }
    
    // Check if process is dead
    int status;
    if (waitpid(streamInfo.process_id, &status, WNOHANG) != 0) {
      streamInfo.process_dead = true;
      // Cleanup after a grace period
      if (elapsed > 60000) { // 60 seconds grace period
        cleanupOutputDir(streamInfo.output_dir);
        it = active_streams_.erase(it);
        continue;
      }
    }
    
    ++it;
  }
}

HttpResponsePtr VideoPreviewHandler::createErrorResponse(
    int statusCode, const std::string &error, const std::string &message) const {
  Json::Value errorJson;
  errorJson["success"] = false;
  errorJson["error"] = error;
  if (!message.empty()) {
    errorJson["message"] = message;
  }
  
  auto resp = HttpResponse::newHttpJsonResponse(errorJson);
  resp->setStatusCode(static_cast<HttpStatusCode>(statusCode));
  CorsHelper::addAllowAllHeaders(resp);
  return resp;
}

HttpResponsePtr VideoPreviewHandler::createSuccessResponse(
    const Json::Value &data, int statusCode) const {
  auto resp = HttpResponse::newHttpJsonResponse(data);
  resp->setStatusCode(static_cast<HttpStatusCode>(statusCode));
  CorsHelper::addAllowAllHeaders(resp);
  return resp;
}

