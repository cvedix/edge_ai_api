#pragma once

#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <filesystem>
#include <json/json.h>
#include <string>

using namespace drogon;

/**
 * @brief Model Upload Handler
 *
 * Handles model file uploads for AI instances.
 *
 * Endpoints:
 * - POST /v1/core/model/upload - Upload a model file
 * - GET /v1/core/model/list - List uploaded models
 * - PUT /v1/core/model/{modelName} - Rename a model file
 * - DELETE /v1/core/model/{modelName} - Delete a model file
 */
class ModelUploadHandler : public drogon::HttpController<ModelUploadHandler> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(ModelUploadHandler::uploadModel, "/v1/core/model/upload", Post);
  ADD_METHOD_TO(ModelUploadHandler::listModels, "/v1/core/model/list", Get);
  ADD_METHOD_TO(ModelUploadHandler::renameModel, "/v1/core/model/{modelName}",
                Put);
  ADD_METHOD_TO(ModelUploadHandler::deleteModel, "/v1/core/model/{modelName}",
                Delete);
  ADD_METHOD_TO(ModelUploadHandler::handleOptions, "/v1/core/model/upload",
                Options);
  METHOD_LIST_END

  /**
   * @brief Handle POST /v1/core/model/upload
   * Uploads a model file (multipart/form-data)
   */
  void uploadModel(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle GET /v1/core/model/list
   * Lists all uploaded model files
   */
  void listModels(const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle PUT /v1/core/model/{modelName}
   * Renames a model file
   */
  void renameModel(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle DELETE /v1/core/model/{modelName}
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
  static void setModelsDirectory(const std::string &dir);

private:
  static std::string models_dir_;

  /**
   * @brief Get models directory path
   */
  std::string getModelsDirectory() const;

  /**
   * @brief Validate model file extension
   */
  bool isValidModelFile(const std::string &filename) const;

  /**
   * @brief Sanitize filename to prevent path traversal
   */
  std::string sanitizeFilename(const std::string &filename) const;

  /**
   * @brief Extract model name from request path
   */
  std::string extractModelName(const HttpRequestPtr &req) const;

  /**
   * @brief Create error response
   */
  HttpResponsePtr createErrorResponse(int statusCode, const std::string &error,
                                      const std::string &message) const;
};
