#include "api/swagger_handler.h"
#include <drogon/HttpResponse.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <stdexcept>
#include <regex>
#include <algorithm>

void SwaggerHandler::getSwaggerUI(const HttpRequestPtr &req,
                                  std::function<void(const HttpResponsePtr &)> &&callback)
{
    try {
        std::string path = req->path();
        std::string version = extractVersionFromPath(path);
        std::string html = generateSwaggerUIHTML(version);
        
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k200OK);
        resp->setContentTypeCode(CT_TEXT_HTML);
        resp->setBody(html);
        
        callback(resp);
    } catch (const std::exception& e) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k500InternalServerError);
        resp->setBody("Error generating Swagger UI: " + std::string(e.what()));
        callback(resp);
    }
}

void SwaggerHandler::getOpenAPISpec(const HttpRequestPtr &req,
                                    std::function<void(const HttpResponsePtr &)> &&callback)
{
    try {
        std::string path = req->path();
        std::string version = extractVersionFromPath(path);
        std::string yaml = readOpenAPIFile(version);
        
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k200OK);
        resp->setContentTypeCode(CT_TEXT_PLAIN);
        resp->setBody(yaml);
        
        // Add CORS headers
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
        
        callback(resp);
    } catch (const std::exception& e) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k500InternalServerError);
        resp->setBody("Error reading OpenAPI spec: " + std::string(e.what()));
        callback(resp);
    }
}

std::string SwaggerHandler::extractVersionFromPath(const std::string& path) const
{
    // Match patterns like /v1/swagger, /v2/openapi.yaml, etc.
    std::regex versionPattern(R"(/(v\d+)/)");
    std::smatch match;
    
    if (std::regex_search(path, match, versionPattern)) {
        return match[1].str(); // Returns "v1", "v2", etc.
    }
    
    return ""; // No version found, return all versions
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
    // Try multiple possible paths relative to executable and current directory
    std::vector<std::string> possiblePaths = {
        "openapi.yaml",
        "./openapi.yaml",
        "../openapi.yaml",
        "../../openapi.yaml",
        "../../../openapi.yaml"
    };
    
    // Add current working directory path
    try {
        std::string basePath = std::filesystem::current_path().string();
        possiblePaths.insert(possiblePaths.begin(), basePath + "/openapi.yaml");
        
        // Try parent directories
        std::filesystem::path current(basePath);
        for (int i = 0; i < 3; ++i) {
            if (current.has_parent_path()) {
                current = current.parent_path();
                possiblePaths.insert(possiblePaths.begin(), 
                                    current.string() + "/openapi.yaml");
            }
        }
    } catch (...) {
        // Ignore filesystem errors, continue with relative paths
    }
    
    // Try to read from each path
    std::string yamlContent;
    for (const auto& path : possiblePaths) {
        std::ifstream file(path);
        if (file.is_open() && file.good()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            file.close();
            yamlContent = buffer.str();
            if (!yamlContent.empty()) {
                break;
            }
        }
    }
    
    if (yamlContent.empty()) {
        // If file not found, throw error
        std::string errorMsg = "OpenAPI file not found. Searched paths:\n";
        for (const auto& path : possiblePaths) {
            errorMsg += "  - " + path + "\n";
        }
        throw std::runtime_error(errorMsg);
    }
    
    // If version is specified, filter the content
    if (!version.empty()) {
        return filterOpenAPIByVersion(yamlContent, version);
    }
    
    return yamlContent;
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

