#pragma once

#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <string>
#include <mutex>
#include <unordered_map>
#include <chrono>

using namespace drogon;

/**
 * @brief Swagger UI handler
 * 
 * Endpoints:
 * - GET /swagger - Swagger UI interface (all versions)
 * - GET /v1/swagger - Swagger UI for API v1
 * - GET /v2/swagger - Swagger UI for API v2
 * - GET /openapi.yaml - OpenAPI specification file (all versions)
 * - GET /v1/openapi.yaml - OpenAPI specification for v1
 * - GET /v2/openapi.yaml - OpenAPI specification for v2
 */
class SwaggerHandler : public drogon::HttpController<SwaggerHandler>
{
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(SwaggerHandler::getSwaggerUI, "/swagger", Get);
        ADD_METHOD_TO(SwaggerHandler::getSwaggerUI, "/v1/swagger", Get);
        ADD_METHOD_TO(SwaggerHandler::getSwaggerUI, "/v2/swagger", Get);
        ADD_METHOD_TO(SwaggerHandler::getOpenAPISpec, "/openapi.yaml", Get);
        ADD_METHOD_TO(SwaggerHandler::getOpenAPISpec, "/v1/openapi.yaml", Get);
        ADD_METHOD_TO(SwaggerHandler::getOpenAPISpec, "/v2/openapi.yaml", Get);
        ADD_METHOD_TO(SwaggerHandler::getOpenAPISpec, "/api-docs", Get);
    METHOD_LIST_END

    /**
     * @brief Serve Swagger UI HTML page
     */
    void getSwaggerUI(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback);

    /**
     * @brief Serve OpenAPI specification file
     */
    void getOpenAPISpec(const HttpRequestPtr &req,
                       std::function<void(const HttpResponsePtr &)> &&callback);

    /**
     * @brief Validate version format (e.g., "v1", "v2")
     * @param version Version string to validate
     * @return true if valid, false otherwise
     * @note Public for testing
     */
    bool validateVersionFormat(const std::string& version) const;

    /**
     * @brief Sanitize file path to prevent path traversal
     * @param path Path to sanitize
     * @return Sanitized path or empty if invalid
     * @note Public for testing
     */
    std::string sanitizePath(const std::string& path) const;

private:
    /**
     * @brief Extract API version from request path
     * @return Version string (e.g., "v1", "v2") or empty string for all versions
     */
    std::string extractVersionFromPath(const std::string& path) const;

    /**
     * @brief Generate Swagger UI HTML content
     * @param version API version (e.g., "v1", "v2") or empty for all versions
     */
    std::string generateSwaggerUIHTML(const std::string& version = "") const;

    /**
     * @brief Read OpenAPI YAML file
     * @param version API version to filter (e.g., "v1", "v2") or empty for all versions
     */
    std::string readOpenAPIFile(const std::string& version = "") const;

    /**
     * @brief Filter OpenAPI YAML to only include paths for specified version
     * @param yamlContent Original YAML content
     * @param version Version to filter (e.g., "v1", "v2")
     * @return Filtered YAML content
     */
    std::string filterOpenAPIByVersion(const std::string& yamlContent, const std::string& version) const;

    // Cache for OpenAPI file content
    struct CacheEntry {
        std::string content;
        std::chrono::steady_clock::time_point timestamp;
        std::chrono::seconds ttl;
    };
    
    static std::unordered_map<std::string, CacheEntry> cache_;
    static std::mutex cache_mutex_;
    static const std::chrono::seconds cache_ttl_;
};

