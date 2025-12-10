#pragma once

#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <json/json.h>
#include <string>
#include <filesystem>

using namespace drogon;

/**
 * @brief Video Upload Handler
 * 
 * Handles video file uploads for AI instances.
 * 
 * Endpoints:
 * - POST /v1/core/videos/upload - Upload a video file
 * - GET /v1/core/videos/list - List uploaded videos
 * - PUT /v1/core/videos/{videoName} - Rename a video file
 * - DELETE /v1/core/videos/{videoName} - Delete a video file
 */
class VideoUploadHandler : public drogon::HttpController<VideoUploadHandler> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(VideoUploadHandler::uploadVideo, "/v1/core/videos/upload", Post);
        ADD_METHOD_TO(VideoUploadHandler::listVideos, "/v1/core/videos/list", Get);
        ADD_METHOD_TO(VideoUploadHandler::renameVideo, "/v1/core/videos/{videoName}", Put);
        ADD_METHOD_TO(VideoUploadHandler::deleteVideo, "/v1/core/videos/{videoName}", Delete);
        ADD_METHOD_TO(VideoUploadHandler::handleOptions, "/v1/core/videos/upload", Options);
    METHOD_LIST_END
    
    /**
     * @brief Handle POST /v1/core/videos/upload
     * Uploads a video file (multipart/form-data)
     */
    void uploadVideo(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle GET /v1/core/videos/list
     * Lists all uploaded video files
     */
    void listVideos(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle PUT /v1/core/videos/{videoName}
     * Renames a video file
     */
    void renameVideo(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle DELETE /v1/core/videos/{videoName}
     * Deletes a video file
     */
    void deleteVideo(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle OPTIONS request for CORS preflight
     */
    void handleOptions(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Set videos directory (dependency injection)
     */
    static void setVideosDirectory(const std::string& dir);
    
private:
    static std::string videos_dir_;
    
    /**
     * @brief Get videos directory path
     */
    std::string getVideosDirectory() const;
    
    /**
     * @brief Validate video file extension
     */
    bool isValidVideoFile(const std::string& filename) const;
    
    /**
     * @brief Sanitize filename to prevent path traversal
     */
    std::string sanitizeFilename(const std::string& filename) const;
    
    /**
     * @brief Extract video name from request path
     */
    std::string extractVideoName(const HttpRequestPtr &req) const;
    
    /**
     * @brief Create error response
     */
    HttpResponsePtr createErrorResponse(int statusCode,
                                       const std::string& error,
                                       const std::string& message) const;
};

