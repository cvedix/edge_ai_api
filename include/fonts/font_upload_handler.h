#pragma once

#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <filesystem>
#include <json/json.h>
#include <string>

using namespace drogon;

/**
 * @brief Font Upload Handler
 *
 * Handles font file uploads for AI instances.
 *
 * Endpoints:
 * - POST /v1/core/fonts/upload - Upload a font file
 * - GET /v1/core/fonts/list - List uploaded fonts
 * - PUT /v1/core/fonts/{fontName} - Rename a font file
 * - DELETE /v1/core/fonts/{fontName} - Delete a font file
 */
class FontUploadHandler : public drogon::HttpController<FontUploadHandler> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(FontUploadHandler::uploadFont, "/v1/core/fonts/upload", Post);
  ADD_METHOD_TO(FontUploadHandler::listFonts, "/v1/core/fonts/list", Get);
  ADD_METHOD_TO(FontUploadHandler::renameFont, "/v1/core/fonts/{fontName}",
                Put);
  ADD_METHOD_TO(FontUploadHandler::deleteFont, "/v1/core/fonts/{fontName}",
                Delete);
  ADD_METHOD_TO(FontUploadHandler::handleOptions, "/v1/core/fonts/upload",
                Options);
  METHOD_LIST_END

  /**
   * @brief Handle POST /v1/core/fonts/upload
   * Uploads a font file (multipart/form-data)
   */
  void uploadFont(const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle GET /v1/core/fonts/list
   * Lists all uploaded font files
   */
  void listFonts(const HttpRequestPtr &req,
                 std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle PUT /v1/core/fonts/{fontName}
   * Renames a font file
   */
  void renameFont(const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle DELETE /v1/core/fonts/{fontName}
   * Deletes a font file
   */
  void deleteFont(const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Handle OPTIONS request for CORS preflight
   */
  void handleOptions(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Set fonts directory (dependency injection)
   */
  static void setFontsDirectory(const std::string &dir);

private:
  static std::string fonts_dir_;

  /**
   * @brief Get fonts directory path
   */
  std::string getFontsDirectory() const;

  /**
   * @brief Validate font file extension
   */
  bool isValidFontFile(const std::string &filename) const;

  /**
   * @brief Sanitize filename to prevent path traversal
   */
  std::string sanitizeFilename(const std::string &filename) const;

  /**
   * @brief Extract font name from request path
   */
  std::string extractFontName(const HttpRequestPtr &req) const;

  /**
   * @brief Create error response
   */
  HttpResponsePtr createErrorResponse(int statusCode, const std::string &error,
                                      const std::string &message) const;
};
