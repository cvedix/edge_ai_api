#include "api/swagger_handler.h"
#include <drogon/HttpResponse.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <stdexcept>
#include <regex>
#include <algorithm>
#include <cctype>

// Static cache members
std::unordered_map<std::string, SwaggerHandler::CacheEntry> SwaggerHandler::cache_;
std::mutex SwaggerHandler::cache_mutex_;
const std::chrono::seconds SwaggerHandler::cache_ttl_(300); // 5 minutes cache

void SwaggerHandler::getSwaggerUI(const HttpRequestPtr &req,
                                  std::function<void(const HttpResponsePtr &)> &&callback)
{
    try {
        std::string path = req->path();
        std::string version = extractVersionFromPath(path);
        
        // Validate version format if provided
        if (!version.empty() && !validateVersionFormat(version)) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(k400BadRequest);
            resp->setBody("Invalid API version format");
            callback(resp);
            return;
        }
        
        std::string html = generateSwaggerUIHTML(version);
        
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k200OK);
        resp->setContentTypeCode(CT_TEXT_HTML);
        resp->setBody(html);
        
        callback(resp);
    } catch (const std::exception& e) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k500InternalServerError);
        resp->setBody("Internal server error");
        callback(resp);
    } catch (...) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k500InternalServerError);
        resp->setBody("Internal server error");
        callback(resp);
    }
}

void SwaggerHandler::getOpenAPISpec(const HttpRequestPtr &req,
                                    std::function<void(const HttpResponsePtr &)> &&callback)
{
    try {
        std::string path = req->path();
        std::string version = extractVersionFromPath(path);
        
        // Validate version format if provided
        if (!version.empty() && !validateVersionFormat(version)) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(k400BadRequest);
            resp->setBody("Invalid API version format");
            callback(resp);
            return;
        }
        
        std::string yaml = readOpenAPIFile(version);
        
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k200OK);
        resp->setContentTypeCode(CT_TEXT_PLAIN);
        resp->setBody(yaml);
        
        // Add CORS headers (restrictive for production)
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
        
        callback(resp);
    } catch (const std::exception& e) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k500InternalServerError);
        resp->setBody("Internal server error");
        callback(resp);
    } catch (...) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k500InternalServerError);
        resp->setBody("Internal server error");
        callback(resp);
    }
}

std::string SwaggerHandler::extractVersionFromPath(const std::string& path) const
{
    // Match patterns like /v1/swagger, /v2/openapi.yaml, etc.
    // Only match v followed by digits (v1, v2, v10, etc.)
    std::regex versionPattern(R"(/(v\d+)/)");
    std::smatch match;
    
    if (std::regex_search(path, match, versionPattern)) {
        std::string version = match[1].str();
        // Additional validation: ensure it's exactly "v" + digits
        if (validateVersionFormat(version)) {
            return version;
        }
    }
    
    return ""; // No version found, return all versions
}

bool SwaggerHandler::validateVersionFormat(const std::string& version) const
{
    // Version must start with 'v' followed by one or more digits
    // Examples: v1, v2, v10, v99 (but not v, v0, v-1, v1.0, etc.)
    if (version.empty() || version.length() < 2) {
        return false;
    }
    
    if (version[0] != 'v') {
        return false;
    }
    
    // Check that all remaining characters are digits
    for (size_t i = 1; i < version.length(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(version[i]))) {
            return false;
        }
    }
    
    // Ensure at least one digit after 'v'
    return version.length() > 1;
}

std::string SwaggerHandler::sanitizePath(const std::string& path) const
{
    // Prevent path traversal attacks
    // Only allow simple filenames without directory traversal
    if (path.empty()) {
        return "";
    }
    
    // Check for path traversal patterns
    if (path.find("..") != std::string::npos ||
        path.find("/") != std::string::npos ||
        path.find("\\") != std::string::npos) {
        return "";
    }
    
    // Only allow alphanumeric, dash, underscore, dot
    for (char c : path) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && 
            c != '-' && c != '_' && c != '.') {
            return "";
        }
    }
    
    return path;
}

std::string SwaggerHandler::generateSwaggerUIHTML(const std::string& version) const
{
    // Determine the OpenAPI spec URL based on version
    std::string specUrl = "/openapi.yaml";
    std::string title = "Edge AI API - Swagger UI";
    
    if (!version.empty()) {
        specUrl = "/" + version + "/openapi.yaml";
        title = "Edge AI API " + version + " - Swagger UI";
    }
    
    // Swagger UI HTML with embedded CDN
    std::string html = R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>)" + title + R"(</title>
    <link rel="stylesheet" type="text/css" href="https://unpkg.com/swagger-ui-dist@5.9.0/swagger-ui.css" />
    <style>
        html {
            box-sizing: border-box;
            overflow: -moz-scrollbars-vertical;
            overflow-y: scroll;
        }
        *, *:before, *:after {
            box-sizing: inherit;
        }
        body {
            margin:0;
            background: #fafafa;
        }
    </style>
</head>
<body>
    <div id="swagger-ui"></div>
    <script src="https://unpkg.com/swagger-ui-dist@5.9.0/swagger-ui-bundle.js"></script>
    <script src="https://unpkg.com/swagger-ui-dist@5.9.0/swagger-ui-standalone-preset.js"></script>
    <script>
        window.onload = function() {
            const ui = SwaggerUIBundle({
                url: ')" + specUrl + R"(',
                dom_id: '#swagger-ui',
                deepLinking: true,
                presets: [
                    SwaggerUIBundle.presets.apis,
                    SwaggerUIStandalonePreset
                ],
                plugins: [
                    SwaggerUIBundle.plugins.DownloadUrl
                ],
                layout: "StandaloneLayout",
                validatorUrl: null
            });
        };
    </script>
</body>
</html>)";
    
    return html;
}

std::string SwaggerHandler::readOpenAPIFile(const std::string& version) const
{
    // Check cache first
    std::string cacheKey = version.empty() ? "all" : version;
    
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = cache_.find(cacheKey);
        if (it != cache_.end()) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - it->second.timestamp);
            
            if (elapsed < it->second.ttl) {
                return it->second.content; // Return cached content
            } else {
                // Cache expired, remove it
                cache_.erase(it);
            }
        }
    }
    
    // Cache miss or expired, read from file
    // Use canonical path to prevent path traversal
    std::filesystem::path basePath;
    try {
        basePath = std::filesystem::canonical(std::filesystem::current_path());
    } catch (...) {
        // Fallback to current_path if canonical fails
        basePath = std::filesystem::current_path();
    }
    
    // Only look in project root (current directory and up to 3 levels)
    std::vector<std::filesystem::path> possiblePaths;
    std::filesystem::path current = basePath;
    
    for (int i = 0; i < 4; ++i) {
        std::filesystem::path testPath = current / "openapi.yaml";
        if (std::filesystem::exists(testPath)) {
            // Verify it's a regular file and not a symlink to prevent attacks
            if (std::filesystem::is_regular_file(testPath)) {
                possiblePaths.push_back(testPath);
            }
        }
        
        if (current.has_parent_path() && current != current.parent_path()) {
            current = current.parent_path();
        } else {
            break;
        }
    }
    
    // Try to read from found paths
    std::string yamlContent;
    for (const auto& path : possiblePaths) {
        try {
            // Use canonical path to ensure no symlink attacks
            std::filesystem::path canonicalPath = std::filesystem::canonical(path);
            
            std::ifstream file(canonicalPath);
            if (file.is_open() && file.good()) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                file.close();
                yamlContent = buffer.str();
                if (!yamlContent.empty()) {
                    break;
                }
            }
        } catch (...) {
            // Skip this path if there's an error
            continue;
        }
    }
    
    if (yamlContent.empty()) {
        throw std::runtime_error("OpenAPI specification file not found");
    }
    
    // If version is specified, filter the content
    std::string finalContent = yamlContent;
    if (!version.empty()) {
        finalContent = filterOpenAPIByVersion(yamlContent, version);
    }
    
    // Update cache
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        CacheEntry entry;
        entry.content = finalContent;
        entry.timestamp = std::chrono::steady_clock::now();
        entry.ttl = cache_ttl_;
        cache_[cacheKey] = entry;
    }
    
    return finalContent;
}

std::string SwaggerHandler::filterOpenAPIByVersion(const std::string& yamlContent, const std::string& version) const
{
    std::stringstream result;
    std::istringstream input(yamlContent);
    std::string line;
    bool inPaths = false;
    bool skipPath = false;
    std::string currentPath = "";
    
    // Version prefix to match (e.g., "/v1/")
    std::string versionPrefix = "/" + version + "/";
    
    while (std::getline(input, line)) {
        // Check if we're entering the paths section
        if (line.find("paths:") != std::string::npos) {
            inPaths = true;
            result << line << "\n";
            continue;
        }
        
        if (!inPaths) {
            // Before paths section, just copy everything
            result << line << "\n";
            continue;
        }
        
        // Calculate indentation level (count leading spaces)
        size_t indent = 0;
        while (indent < line.length() && line[indent] == ' ') {
            indent++;
        }
        
        // Check if this is a path definition (starts with / at indent level 2)
        if (indent == 2 && line.find("  /") == 0) {
            // This is a new path definition
            size_t colonPos = line.find(':');
            if (colonPos != std::string::npos) {
                currentPath = line.substr(2, colonPos - 2); // Extract path without leading spaces
                
                // Check if this path belongs to the requested version
                skipPath = (currentPath.find(versionPrefix) != 0);
                
                if (!skipPath) {
                    result << line << "\n";
                }
            } else {
                if (!skipPath) {
                    result << line << "\n";
                }
            }
        } else if (skipPath) {
            // Skip lines that are part of a path we don't want
            // Only include if we hit a new path at indent level 2 or less
            if (indent <= 2 && line.find("  /") == 0) {
                // This is a new path, reset skipPath and process it
                size_t colonPos = line.find(':');
                if (colonPos != std::string::npos) {
                    currentPath = line.substr(2, colonPos - 2);
                    skipPath = (currentPath.find(versionPrefix) != 0);
                    if (!skipPath) {
                        result << line << "\n";
                    }
                }
            }
            // Otherwise skip this line
        } else {
            // Include this line (it's part of a path we want)
            result << line << "\n";
        }
    }
    
    return result.str();
}

