#pragma once

#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <json/json.h>
#include <string>
#include <filesystem>

using namespace drogon;

/**
 * @brief Model Upload Handler
 * 
 * Handles model file uploads for AI instances.
 * 
 * Endpoints:
 * - POST /v1/core/models/upload - Upload a model file
 * - GET /v1/core/models/list - List uploaded models
 * - PUT /v1/core/models/{modelName} - Rename a model file
 * - DELETE /v1/core/models/{modelName} - Delete a model file
 */
class ModelUploadHandler : public drogon::HttpController<ModelUploadHandler> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(ModelUploadHandler::uploadModel, "/v1/core/models/upload", Post);
        ADD_METHOD_TO(ModelUploadHandler::listModels, "/v1/core/models/list", Get);
        ADD_METHOD_TO(ModelUploadHandler::renameModel, "/v1/core/models/{modelName}", Put);
        ADD_METHOD_TO(ModelUploadHandler::deleteModel, "/v1/core/models/{modelName}", Delete);
        ADD_METHOD_TO(ModelUploadHandler::handleOptions, "/v1/core/models/upload", Options);
    METHOD_LIST_END
    
    /**
     * @brief Handle POST /v1/core/models/upload
     * Uploads a model file (multipart/form-data)
     */
    void uploadModel(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle GET /v1/core/models/list
     * Lists all uploaded model files
     */
    void listModels(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle PUT /v1/core/models/{modelName}
     * Renames a model file
     */
    void renameModel(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle DELETE /v1/core/models/{modelName}
     * Deletes a model file
     */
    void deleteModel(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle OPTIONS request for CORS preflight
     */
    void handleOptions(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Set models directory (dependency injection)
     */
    static void setModelsDirectory(const std::string& dir);
    
private:
    static std::string models_dir_;
    
    /**
     * @brief Get models directory path
     */
    std::string getModelsDirectory() const;
    
    /**
     * @brief Validate model file extension
     */
    bool isValidModelFile(const std::string& filename) const;
    
    /**
     * @brief Sanitize filename to prevent path traversal
     */
    std::string sanitizeFilename(const std::string& filename) const;
    
    /**
     * @brief Extract model name from request path
     */
    std::string extractModelName(const HttpRequestPtr &req) const;
    
    /**
     * @brief Create error response
     */
    HttpResponsePtr createErrorResponse(int statusCode,
                                       const std::string& error,
                                       const std::string& message) const;
};

