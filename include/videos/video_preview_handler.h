#pragma once

#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <filesystem>
#include <json/json.h>
#include <string>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <sys/types.h>

using namespace drogon;

/**
 * @brief Preview stream info
 */
struct PreviewStreamInfo {
  std::string preview_id;
  std::string video_name;
  std::string video_path;
  std::string output_dir;
  std::string hls_output_file;
  std::string preview_url;
  pid_t process_id;
  std::chrono::steady_clock::time_point start_time;
  bool process_dead;
  
  PreviewStreamInfo() : process_id(0), process_dead(false) {}
};

/**
 * @brief Video Preview Handler
 *
 * Handles video file preview by converting to HLS stream for browser playback.
 *
 * Endpoints:
 * - POST /v1/core/video/{videoName}/preview/start - Start HLS conversion for video file
 * - DELETE /v1/core/video/{videoName}/preview/{previewId} - Stop preview
 * - GET /v1/core/video/{videoName}/preview/{previewId}/{filename} - Serve HLS files
 */
class VideoPreviewHandler : public drogon::HttpController<VideoPreviewHandler> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(VideoPreviewHandler::startPreview, "/v1/core/video/{videoName}/preview/start", Post);
  ADD_METHOD_TO(VideoPreviewHandler::stopPreview, "/v1/core/video/{videoName}/preview/{previewId}", Delete);
  ADD_METHOD_TO(VideoPreviewHandler::serveHlsFile, "/v1/core/video/{videoName}/preview/{previewId}/{filename}", Get);
  ADD_METHOD_TO(VideoPreviewHandler::handleOptions, "/v1/core/video/{videoName}/preview/start", Options);
  METHOD_LIST_END

  /**
   * @brief Handle POST /v1/core/video/{videoName}/preview/start
   * Starts HLS conversion for a video file
   */
  void startPreview(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle DELETE /v1/core/video/{videoName}/preview/{previewId}
   * Stops preview stream
   */
  void stopPreview(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle GET /v1/core/video/{videoName}/preview/{previewId}/{filename}
   * Serves HLS files (m3u8 playlist and ts segments)
   */
  void serveHlsFile(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle OPTIONS request for CORS preflight
   */
  void handleOptions(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Set videos directory (dependency injection)
   */
  static void setVideosDirectory(const std::string &dir);

  /**
   * @brief Cleanup old preview streams (called periodically)
   */
  static void cleanupOldStreams();

private:
  static std::string videos_dir_;
  static std::mutex streams_mutex_;
  static std::unordered_map<std::string, PreviewStreamInfo> active_streams_;

  /**
   * @brief Get videos directory path
   */
  std::string getVideosDirectory() const;

  /**
   * @brief Extract video name from request path
   */
  std::string extractVideoName(const HttpRequestPtr &req) const;

  /**
   * @brief Extract preview ID from request path
   */
  std::string extractPreviewId(const HttpRequestPtr &req) const;

  /**
   * @brief Find video file path
   */
  std::string findVideoFilePath(const std::string &videoName) const;

  /**
   * @brief Start FFmpeg process to convert video to HLS
   */
  pid_t startFFmpegProcess(const std::string &videoPath, const std::string &outputFile) const;

  /**
   * @brief Build FFmpeg command for video file to HLS conversion
   */
  std::vector<std::string> buildFFmpegCommand(const std::string &videoPath, const std::string &outputFile) const;

  /**
   * @brief Cleanup output directory
   */
  static void cleanupOutputDir(const std::string &outputDir);

  /**
   * @brief Create error response
   */
  HttpResponsePtr createErrorResponse(int statusCode, const std::string &error,
                                      const std::string &message) const;

  /**
   * @brief Create success response
   */
  HttpResponsePtr createSuccessResponse(const Json::Value &data, int statusCode = 200) const;
};

