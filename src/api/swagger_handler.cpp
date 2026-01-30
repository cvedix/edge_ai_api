#include "api/swagger_handler.h"
#include "core/env_config.h"
#include "core/logger.h"
#include "core/metrics_interceptor.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <drogon/HttpResponse.h>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <vector>

// Static cache members
std::unordered_map<std::string, SwaggerHandler::CacheEntry>
    SwaggerHandler::cache_;
std::mutex SwaggerHandler::cache_mutex_;
const std::chrono::seconds SwaggerHandler::cache_ttl_(300); // 5 minutes cache

void SwaggerHandler::getSwaggerUI(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  // Set handler start time for accurate metrics
  MetricsInterceptor::setHandlerStartTime(req);

  try {
    std::string path = req->path();
    std::string version = extractVersionFromPath(path);

    // Validate version format if provided
    if (!version.empty() && !validateVersionFormat(version)) {
      auto resp = HttpResponse::newHttpResponse();
      resp->setStatusCode(k400BadRequest);
      resp->setBody("Invalid API version format");
      MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
      return;
    }

    // Build absolute URL for OpenAPI spec based on request
    std::string host = req->getHeader("Host");
    if (host.empty()) {
      host = "localhost:8080";
    }
    std::string scheme = req->getHeader("X-Forwarded-Proto");
    if (scheme.empty()) {
      // Check if request is secure
      scheme = req->isOnSecureConnection() ? "https" : "http";
    }

    std::string baseUrl = scheme + "://" + host;
    std::string html = generateSwaggerUIHTML(version, baseUrl);

    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    resp->setContentTypeCode(CT_TEXT_HTML);
    resp->setBody(html);

    // Add CORS headers
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");

    // Record metrics and call callback
    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
  } catch (const std::exception &e) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k500InternalServerError);
    resp->setBody("Internal server error");
    // Add CORS headers
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
  } catch (...) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k500InternalServerError);
    resp->setBody("Internal server error");
    // Add CORS headers
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
  }
}

void SwaggerHandler::getScalarDocument(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  // Set handler start time for accurate metrics
  MetricsInterceptor::setHandlerStartTime(req);

  try {
    std::string path = req->path();
    std::string version = extractVersionFromPath(path);

    // Validate version format if provided
    if (!version.empty() && !validateVersionFormat(version)) {
      auto resp = HttpResponse::newHttpResponse();
      resp->setStatusCode(k400BadRequest);
      resp->setBody("Invalid API version format");
      MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
      return;
    }

    // Build absolute URL for OpenAPI spec based on request
    std::string host = req->getHeader("Host");
    if (host.empty()) {
      host = "localhost:8080";
    }
    std::string scheme = req->getHeader("X-Forwarded-Proto");
    if (scheme.empty()) {
      // Check if request is secure
      scheme = req->isOnSecureConnection() ? "https" : "http";
    }

    std::string baseUrl = scheme + "://" + host;
    
    // Get language from URL parameter if present
    std::string language = req->getParameter("lang");
    PLOG_INFO << "[Scalar] Request path: " << path;
    PLOG_INFO << "[Scalar] Language from getParameter('lang'): " << (language.empty() ? "(empty)" : language);
    
    if (language.empty()) {
      // Try to get from query string
      auto queryParams = req->getParameters();
      PLOG_INFO << "[Scalar] Query params count: " << queryParams.size();
      for (const auto& [key, value] : queryParams) {
        PLOG_INFO << "[Scalar] Query param: " << key << " = " << value;
      }
      auto it = queryParams.find("lang");
      if (it != queryParams.end()) {
        language = it->second;
        PLOG_INFO << "[Scalar] Language from query params: " << language;
      }
    }
    
    // Validate language
    if (!language.empty() && language != "en" && language != "vi") {
      PLOG_WARNING << "[Scalar] Invalid language: " << language << ", defaulting to 'en'";
      language = "en"; // Default to English if invalid
    }
    if (language.empty()) {
      PLOG_INFO << "[Scalar] No language specified, defaulting to 'en'";
      language = "en"; // Default to English
    }
    
    PLOG_INFO << "[Scalar] Final language: " << language;
    PLOG_INFO << "[Scalar] Base URL: " << baseUrl;
    PLOG_INFO << "[Scalar] Version: " << (version.empty() ? "(empty)" : version);
    
    std::string html = generateScalarDocumentHTML(version, baseUrl, language);
    
    PLOG_INFO << "[Scalar] Generated HTML length: " << html.length();
    // Log if initial spec URL was replaced
    if (html.find("{{INITIAL_SPEC_URL}}") != std::string::npos) {
      PLOG_WARNING << "[Scalar] WARNING: {{INITIAL_SPEC_URL}} placeholder not replaced!";
    } else {
      PLOG_INFO << "[Scalar] {{INITIAL_SPEC_URL}} placeholder replaced successfully";
    }

    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    resp->setContentTypeCode(CT_TEXT_HTML);
    resp->setBody(html);

    // Add CORS headers
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");

    // Record metrics and call callback
    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
  } catch (const std::exception &e) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k500InternalServerError);
    resp->setBody("Internal server error");
    // Add CORS headers
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
  } catch (...) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k500InternalServerError);
    resp->setBody("Internal server error");
    // Add CORS headers
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
  }
}

void SwaggerHandler::getOpenAPISpec(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  // Set handler start time for accurate metrics
  MetricsInterceptor::setHandlerStartTime(req);
  try {
    std::string path = req->path();
    std::string version = extractVersionFromPath(path);

    // Validate version format if provided
    if (!version.empty() && !validateVersionFormat(version)) {
      auto resp = HttpResponse::newHttpResponse();
      resp->setStatusCode(k400BadRequest);
      resp->setBody("Invalid API version format");
      // Add CORS headers
      resp->addHeader("Access-Control-Allow-Origin", "*");
      resp->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
      resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
      MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
      return;
    }

    // Get host from request for browser-accessible URL
    std::string requestHost = req->getHeader("Host");
    if (requestHost.empty()) {
      requestHost = "localhost:8080";
    }

    std::string yaml = readOpenAPIFile(version, requestHost, "");

    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    resp->setContentTypeCode(CT_TEXT_PLAIN);
    resp->addHeader("Content-Type", "text/yaml; charset=utf-8");
    resp->setBody(yaml);

    // Add CORS headers (restrictive for production)
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");

    // Record metrics and call callback
    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
  } catch (const std::exception &e) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k500InternalServerError);
    resp->setBody("Internal server error");
    // Add CORS headers
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
  } catch (...) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k500InternalServerError);
    resp->setBody("Internal server error");
    // Add CORS headers
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
  }
}

void SwaggerHandler::getOpenAPISpecWithLang(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  // Set handler start time for accurate metrics
  MetricsInterceptor::setHandlerStartTime(req);
  try {
    std::string path = req->path();
    std::string version = extractVersionFromPath(path);
    // Get language from path parameter (Drogon uses {lang} in route)
    std::string language = req->getParameter("lang");
    // Fallback to extracting from path if parameter not found
    if (language.empty()) {
      language = extractLanguageFromPath(path);
    }

    // Validate version format if provided
    if (!version.empty() && !validateVersionFormat(version)) {
      auto resp = HttpResponse::newHttpResponse();
      resp->setStatusCode(k400BadRequest);
      resp->setBody("Invalid API version format");
      // Add CORS headers
      resp->addHeader("Access-Control-Allow-Origin", "*");
      resp->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
      resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
      MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
      return;
    }

    // Validate language code
    if (!language.empty() && !validateLanguageCode(language)) {
      auto resp = HttpResponse::newHttpResponse();
      resp->setStatusCode(k400BadRequest);
      resp->setBody("Invalid language code. Supported languages: en, vi");
      // Add CORS headers
      resp->addHeader("Access-Control-Allow-Origin", "*");
      resp->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
      resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
      MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
      return;
    }

    // Get host from request for browser-accessible URL
    std::string requestHost = req->getHeader("Host");
    if (requestHost.empty()) {
      requestHost = "localhost:8080";
    }

    std::string yaml = readOpenAPIFile(version, requestHost, language);

    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    resp->setContentTypeCode(CT_TEXT_PLAIN);
    resp->addHeader("Content-Type", "text/yaml; charset=utf-8");
    resp->setBody(yaml);

    // Add CORS headers (restrictive for production)
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");

    // Record metrics and call callback
    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
  } catch (const std::exception &e) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k500InternalServerError);
    resp->setBody("Internal server error");
    // Add CORS headers
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
  } catch (...) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k500InternalServerError);
    resp->setBody("Internal server error");
    // Add CORS headers
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
  }
}

std::string
SwaggerHandler::extractVersionFromPath(const std::string &path) const {
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

bool SwaggerHandler::validateVersionFormat(const std::string &version) const {
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

std::string SwaggerHandler::extractLanguageFromPath(const std::string &path) const {
  // Match patterns like /v1/openapi/en/openapi.yaml or /v1/openapi/vi/openapi.yaml
  // Extract the language code (en or vi) from the path
  std::regex langPattern(R"(/openapi/(en|vi)/openapi\.yaml)");
  std::smatch match;

  if (std::regex_search(path, match, langPattern)) {
    return match[1].str();
  }

  return ""; // No language found
}

bool SwaggerHandler::validateLanguageCode(const std::string &lang) const {
  // Only support "en" and "vi"
  return lang == "en" || lang == "vi";
}

std::string SwaggerHandler::sanitizePath(const std::string &path) const {
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
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_' &&
        c != '.') {
      return "";
    }
  }

  return path;
}

std::string
SwaggerHandler::generateSwaggerUIHTML(const std::string &version,
                                      const std::string &baseUrl) const {
  // Determine the OpenAPI spec URL based on version
  std::string specUrl = "/openapi.yaml";
  std::string title = "Edge AI API - Swagger UI";

  if (!version.empty()) {
    specUrl = "/" + version + "/openapi.yaml";
    title = "Edge AI API " + version + " - Swagger UI";
  }

  // Use absolute URL if baseUrl is provided, otherwise use relative URL
  std::string fullSpecUrl = specUrl;
  if (!baseUrl.empty()) {
    // Remove trailing slash from baseUrl if present
    std::string cleanBaseUrl = baseUrl;
    if (cleanBaseUrl.back() == '/') {
      cleanBaseUrl.pop_back();
    }
    fullSpecUrl = cleanBaseUrl + specUrl;
  }

  // Swagger UI HTML with embedded CDN and fallback mechanism
  std::string html = R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>)" + title +
                     R"(</title>
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
        #loading-message {
            text-align: center;
            padding: 20px;
            font-family: Arial, sans-serif;
            color: #333;
        }
    </style>
    <script>
        // CDN fallback configuration - multiple CDN providers for redundancy
        const CDN_CONFIGS = {
            css: [
                'https://cdn.jsdelivr.net/npm/swagger-ui-dist@5.9.0/swagger-ui.css',
                'https://unpkg.com/swagger-ui-dist@5.9.0/swagger-ui.css',
                'https://fastly.jsdelivr.net/npm/swagger-ui-dist@5.9.0/swagger-ui.css'
            ],
            bundle: [
                'https://cdn.jsdelivr.net/npm/swagger-ui-dist@5.9.0/swagger-ui-bundle.js',
                'https://unpkg.com/swagger-ui-dist@5.9.0/swagger-ui-bundle.js',
                'https://fastly.jsdelivr.net/npm/swagger-ui-dist@5.9.0/swagger-ui-bundle.js'
            ],
            standalone: [
                'https://cdn.jsdelivr.net/npm/swagger-ui-dist@5.9.0/swagger-ui-standalone-preset.js',
                'https://unpkg.com/swagger-ui-dist@5.9.0/swagger-ui-standalone-preset.js',
                'https://fastly.jsdelivr.net/npm/swagger-ui-dist@5.9.0/swagger-ui-standalone-preset.js'
            ]
        };

        // Load resource with fallback
        function loadResource(urls, type, onSuccess, onError, index = 0) {
            if (index >= urls.length) {
                console.error('All CDN fallbacks failed for', type);
                if (onError) onError();
                return;
            }

            const url = urls[index];
            console.log('Trying to load', type, 'from:', url);

            if (type === 'css') {
                const link = document.createElement('link');
                link.rel = 'stylesheet';
                link.type = 'text/css';
                link.href = url;
                link.onerror = function() {
                    console.warn('Failed to load CSS from', url, ', trying next CDN...');
                    loadResource(urls, type, onSuccess, onError, index + 1);
                };
                link.onload = function() {
                    console.log('Successfully loaded CSS from', url);
                    if (onSuccess) onSuccess();
                };
                document.head.appendChild(link);
            } else {
                const script = document.createElement('script');
                script.src = url;
                script.onerror = function() {
                    console.warn('Failed to load script from', url, ', trying next CDN...');
                    loadResource(urls, type, onSuccess, onError, index + 1);
                };
                script.onload = function() {
                    console.log('Successfully loaded script from', url);
                    if (onSuccess) onSuccess();
                };
                document.head.appendChild(script);
            }
        }

        // Initialize Swagger UI after all resources are loaded
        let cssLoaded = false;
        let bundleLoaded = false;
        let standaloneLoaded = false;
        let swaggerInitialized = false;

        function checkAndInitSwagger() {
            // Check if DOM is ready
            if (document.readyState === 'loading') {
                document.addEventListener('DOMContentLoaded', checkAndInitSwagger);
                return;
            }

            // Check if all resources are loaded and Swagger is available
            if (!swaggerInitialized && cssLoaded && bundleLoaded && standaloneLoaded && 
                typeof SwaggerUIBundle !== 'undefined' && typeof SwaggerUIStandalonePreset !== 'undefined') {
                
                const swaggerContainer = document.getElementById('swagger-ui');
                if (!swaggerContainer) {
                    console.error('Swagger UI container not found');
                    return;
                }

                const loadingMsg = document.getElementById('loading-message');
                if (loadingMsg) loadingMsg.remove();

                try {
                    swaggerInitialized = true;
                    const ui = SwaggerUIBundle({
                        url: ')" +
                     fullSpecUrl + R"(',
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
                        validatorUrl: null,
                        tryItOutEnabled: true,
                        requestInterceptor: function(request) {
                            console.log('Swagger request:', request);
                            return request;
                        },
                        onComplete: function() {
                            console.log("Swagger UI loaded successfully");
                        },
                        onFailure: function(data) {
                            console.error("Swagger UI failed to load:", data);
                            const container = document.getElementById('swagger-ui');
                            if (container) {
                                container.innerHTML = '<div style="padding: 20px; color: red;">Error loading Swagger UI. Please check console for details.</div>';
                            }
                        }
                    });
                } catch (error) {
                    console.error("Error initializing Swagger UI:", error);
                    const container = document.getElementById('swagger-ui');
                    if (container) {
                        container.innerHTML = '<div style="padding: 20px; color: red;">Error loading Swagger UI: ' + error.message + '. Please check console for details.</div>';
                    }
                    swaggerInitialized = false;
                }
            }
        }

        // Wait for DOM to be ready before loading resources
        if (document.readyState === 'loading') {
            document.addEventListener('DOMContentLoaded', function() {
                initializeSwaggerResources();
            });
        } else {
            initializeSwaggerResources();
        }

        function initializeSwaggerResources() {
            // Load CSS first
            loadResource(CDN_CONFIGS.css, 'css', function() {
                cssLoaded = true;
                checkAndInitSwagger();
            });

            // Load bundle script
            loadResource(CDN_CONFIGS.bundle, 'script', function() {
                bundleLoaded = true;
                checkAndInitSwagger();
            });

            // Load standalone preset script
            loadResource(CDN_CONFIGS.standalone, 'script', function() {
                standaloneLoaded = true;
                checkAndInitSwagger();
            });
        }
    </script>
</head>
<body>
    <div id="loading-message">Loading Swagger UI...</div>
    <div id="swagger-ui"></div>
</body>
</html>)";

  return html;
}

std::string
SwaggerHandler::readScalarHTMLFile() const {
  // Try to find api-specs/scalar/index.html
  std::vector<std::filesystem::path> possiblePaths;

  // 1. Check api-specs/scalar/index.html first (project structure)
  try {
    std::filesystem::path currentDir = std::filesystem::current_path();
    std::filesystem::path testPath = currentDir / "api-specs" / "scalar" / "index.html";
    if (std::filesystem::exists(testPath) &&
        std::filesystem::is_regular_file(testPath)) {
      possiblePaths.push_back(testPath);
    }
  } catch (...) {
    // Ignore errors
  }

  // 2. Check environment variable for installation directory
  const char *installDir = std::getenv("EDGE_AI_API_INSTALL_DIR");
  if (installDir && installDir[0] != '\0') {
    try {
      std::filesystem::path installPath(installDir);
      std::filesystem::path testPath = installPath / "api-specs" / "scalar" / "index.html";
      if (std::filesystem::exists(testPath) &&
          std::filesystem::is_regular_file(testPath)) {
        possiblePaths.push_back(testPath);
      }
    } catch (...) {
      // Ignore errors
    }
  }

  // 3. Search up from current directory (for development)
  std::filesystem::path basePath;
  try {
    basePath = std::filesystem::canonical(std::filesystem::current_path());
  } catch (...) {
    basePath = std::filesystem::current_path();
  }

  std::filesystem::path current = basePath;
  for (int i = 0; i < 4; ++i) {
    std::filesystem::path testPath = current / "api-specs" / "scalar" / "index.html";
    if (std::filesystem::exists(testPath) &&
        std::filesystem::is_regular_file(testPath)) {
      possiblePaths.push_back(testPath);
    }

    if (current.has_parent_path() && current != current.parent_path()) {
      current = current.parent_path();
    } else {
      break;
    }
  }

  // Try to read from found paths
  for (const auto &path : possiblePaths) {
    try {
      std::filesystem::path canonicalPath = std::filesystem::canonical(path);
      std::ifstream file(canonicalPath);
      if (file.is_open() && file.good()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();
        std::string htmlContent = buffer.str();
        if (!htmlContent.empty()) {
          return htmlContent;
        }
      }
    } catch (...) {
      // Continue to next path
    }
  }

  // Return empty string if file not found (fallback to hardcoded HTML)
  return "";
}

std::string
SwaggerHandler::generateScalarDocumentHTML(const std::string &version,
                                           const std::string &baseUrl,
                                           const std::string &language) const {
  // Try to read HTML from file first
  std::string html = readScalarHTMLFile();
  
  if (!html.empty()) {
    // Determine the OpenAPI spec URL based on version
    // Default to English, but will be updated by JavaScript based on language selector
    std::string specUrl = "/openapi.yaml";
    if (!version.empty()) {
      specUrl = "/" + version + "/openapi.yaml";
    }

    // Use absolute URL if baseUrl is provided, otherwise use relative URL
    std::string fullSpecUrl = specUrl;
    std::string cleanBaseUrl = baseUrl;
    if (!baseUrl.empty()) {
      if (cleanBaseUrl.back() == '/') {
        cleanBaseUrl.pop_back();
      }
      fullSpecUrl = cleanBaseUrl + specUrl;
    }

    // Build initial spec URL based on language parameter
    std::string lang = language.empty() ? "en" : language;
    if (lang != "en" && lang != "vi") {
      PLOG_WARNING << "[Scalar] Invalid language in generateScalarDocumentHTML: " << lang << ", defaulting to 'en'";
      lang = "en"; // Default to English if invalid
    }
    std::string initialSpecUrl = fullSpecUrl;
    if (!version.empty()) {
      initialSpecUrl = cleanBaseUrl + "/" + version + "/openapi/" + lang + "/openapi.yaml";
    } else {
      initialSpecUrl = cleanBaseUrl + "/openapi/" + lang + "/openapi.yaml";
    }
    
    PLOG_INFO << "[Scalar] Initial spec URL: " << initialSpecUrl;
    PLOG_INFO << "[Scalar] Language for initial URL: " << lang;

    // Replace placeholders in HTML
    // For JSON script tag, we need proper JSON strings
    std::string baseUrlJson = baseUrl.empty() ? "window.location.origin" : "\"" + cleanBaseUrl + "\"";
    std::string versionJson = version.empty() ? "\"\"" : "\"" + version + "\"";
    std::string fallbackUrlJson = "\"" + fullSpecUrl + "\"";
    std::string initialSpecUrlJson = "\"" + initialSpecUrl + "\"";
    
    // Replace placeholders - use replace_all equivalent
    std::string placeholder;
    
    // Replace {{BASE_URL}}
    placeholder = "{{BASE_URL}}";
    size_t pos = 0;
    while ((pos = html.find(placeholder, pos)) != std::string::npos) {
      html.replace(pos, placeholder.length(), baseUrlJson);
      pos += baseUrlJson.length();
    }
    
    // Replace {{VERSION}}
    placeholder = "{{VERSION}}";
    pos = 0;
    while ((pos = html.find(placeholder, pos)) != std::string::npos) {
      html.replace(pos, placeholder.length(), versionJson);
      pos += versionJson.length();
    }
    
    // Replace {{FALLBACK_URL}}
    placeholder = "{{FALLBACK_URL}}";
    pos = 0;
    while ((pos = html.find(placeholder, pos)) != std::string::npos) {
      html.replace(pos, placeholder.length(), fallbackUrlJson);
      pos += fallbackUrlJson.length();
    }
    
    // Replace {{INITIAL_SPEC_URL}} - this is critical for Scalar auto-initialization
    placeholder = "{{INITIAL_SPEC_URL}}";
    pos = 0;
    while ((pos = html.find(placeholder, pos)) != std::string::npos) {
      html.replace(pos, placeholder.length(), initialSpecUrlJson);
      pos += initialSpecUrlJson.length();
    }
    
    return html;
  }

  // Fallback to hardcoded HTML if file not found
  // Determine the OpenAPI spec URL based on version
  std::string specUrl = "/openapi.yaml";
  std::string title = "Edge AI API - Scalar Documentation";

  if (!version.empty()) {
    specUrl = "/" + version + "/openapi.yaml";
    title = "Edge AI API " + version + " - Scalar Documentation";
  }

  // Use absolute URL if baseUrl is provided, otherwise use relative URL
  std::string fullSpecUrl = specUrl;
  std::string cleanBaseUrl = baseUrl;
  if (!baseUrl.empty()) {
    // Remove trailing slash from baseUrl if present
    if (cleanBaseUrl.back() == '/') {
      cleanBaseUrl.pop_back();
    }
    fullSpecUrl = cleanBaseUrl + specUrl;
  }

  // Build initial spec URL based on language parameter
  std::string lang = language.empty() ? "en" : language;
  if (lang != "en" && lang != "vi") {
    lang = "en"; // Default to English if invalid
  }
  std::string initialSpecUrl = fullSpecUrl;
  if (!version.empty() && !cleanBaseUrl.empty()) {
    initialSpecUrl = cleanBaseUrl + "/" + version + "/openapi/" + lang + "/openapi.yaml";
  } else if (!cleanBaseUrl.empty()) {
    initialSpecUrl = cleanBaseUrl + "/openapi/" + lang + "/openapi.yaml";
  }

  // Scalar API Reference HTML - using data-configuration attribute like the original file
  std::string fallbackHtml = R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>)" + title +
                     R"(</title>
    <style>
        body {
            margin: 0;
            padding: 0;
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
        }
        .language-selector {
            position: fixed;
            top: 20px;
            right: 20px;
            z-index: 10000;
            background: white;
            padding: 10px 15px;
            border-radius: 8px;
            box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
            display: flex;
            align-items: center;
            gap: 10px;
        }
        .language-selector label {
            font-weight: 600;
            font-size: 14px;
            color: #333;
        }
        .language-selector select {
            padding: 6px 12px;
            border: 1px solid #ddd;
            border-radius: 4px;
            font-size: 14px;
            cursor: pointer;
            background: white;
        }
        .language-selector select:hover {
            border-color: #999;
        }
        .language-selector select:focus {
            outline: none;
            border-color: #0066cc;
            box-shadow: 0 0 0 2px rgba(0, 102, 204, 0.1);
        }
        #loading-message {
            text-align: center;
            padding: 40px 20px;
            color: #333;
        }
        #error-message {
            display: none;
            text-align: center;
            padding: 40px 20px;
            color: #d32f2f;
            background: #ffebee;
            margin: 20px;
            border-radius: 4px;
        }
    </style>
    <script>
        console.log('[Scalar Fallback] Script block initialized at:', new Date().toISOString());
        console.log('[Scalar Fallback] Document readyState:', document.readyState);
        
        // Global reference to Scalar instance for cleanup
        window.scalarInstance = null;
        
        // CDN fallback configuration for Scalar
        // Using version 1.24.0 (pinned for stability)
        const SCALAR_CDN_URLS = [
            'https://cdn.jsdelivr.net/npm/@scalar/api-reference@1.24.0/dist/browser/standalone.js',
            'https://unpkg.com/@scalar/api-reference@1.24.0/dist/browser/standalone.js',
            'https://fastly.jsdelivr.net/npm/@scalar/api-reference@1.24.0/dist/browser/standalone.js'
        ];

        let scriptLoaded = false;
        
        // Check if element exists immediately
        console.log('[Scalar Fallback] Checking for #api-reference element immediately...');
        const immediateCheck = document.getElementById('api-reference');
        console.log('[Scalar Fallback] Immediate check result:', immediateCheck ? 'FOUND' : 'NOT FOUND');
        if (immediateCheck) {
            console.log('[Scalar Fallback] Immediate element data-spec-url:', immediateCheck.getAttribute('data-spec-url'));
        }

        function loadScalarScript(urls, index = 0) {
            console.log('[Scalar Fallback] loadScalarScript called, index:', index, 'at:', new Date().toISOString());
            console.log('[Scalar Fallback] Document readyState:', document.readyState);
            
            // Check element before loading script
            const apiRefBefore = document.getElementById('api-reference');
            console.log('[Scalar Fallback] Element check before script load:', apiRefBefore ? 'FOUND' : 'NOT FOUND');
            if (apiRefBefore) {
                console.log('[Scalar Fallback] Element data-spec-url before script load:', apiRefBefore.getAttribute('data-spec-url'));
                console.log('[Scalar Fallback] Element innerHTML length:', apiRefBefore.innerHTML.length);
            }
            
            if (index >= urls.length) {
                console.error('[Scalar Fallback] All CDN fallbacks failed for Scalar API Reference');
                document.getElementById('loading-message').style.display = 'none';
                document.getElementById('error-message').style.display = 'block';
                document.getElementById('error-message').innerHTML = 
                    '<h2>Failed to load Scalar API Reference</h2>' +
                    '<p>Unable to load from CDN. Please check your internet connection.</p>' +
                    '<p>You can try:</p>' +
                    '<ul style="text-align: left; display: inline-block;">' +
                    '<li>Refresh the page</li>' +
                    '<li>Check your internet connection</li>' +
                    '<li>Check browser console for errors</li>' +
                    '</ul>';
                return;
            }

            const url = urls[index];
            console.log('[Scalar Fallback] Loading Scalar from:', url, 'at:', new Date().toISOString());

            const script = document.createElement('script');
            script.src = url;
            script.onerror = function() {
                console.warn('Failed to load Scalar from', url, ', trying next CDN...');
                loadScalarScript(urls, index + 1);
            };
            script.onload = function() {
                console.log('[Scalar Fallback] Script onload fired at:', new Date().toISOString());
                console.log('[Scalar Fallback] Successfully loaded Scalar from:', url);
                console.log('[Scalar Fallback] Document readyState:', document.readyState);
                scriptLoaded = true;
                
                // Check element immediately after script load
                const apiRefImmediate = document.getElementById('api-reference');
                console.log('[Scalar Fallback] Element check immediately after script load:', apiRefImmediate ? 'FOUND' : 'NOT FOUND');
                if (apiRefImmediate) {
                    console.log('[Scalar Fallback] Element data-spec-url immediately after script load:', apiRefImmediate.getAttribute('data-spec-url'));
                }
                
                // Check if ScalarApiReference is available
                console.log('[Scalar Fallback] typeof ScalarApiReference:', typeof ScalarApiReference);
                console.log('[Scalar Fallback] typeof window.ScalarApiReference:', typeof window.ScalarApiReference);
                if (typeof ScalarApiReference !== 'undefined') {
                    console.log('[Scalar Fallback] ScalarApiReference keys:', Object.keys(ScalarApiReference));
                    if (ScalarApiReference.default) {
                        console.log('[Scalar Fallback] ScalarApiReference.default is available');
                    }
                }
                
                // Đợi một chút để đảm bảo element đã được render hoàn toàn
                setTimeout(function() {
                    console.log('[Scalar Fallback] setTimeout callback fired at:', new Date().toISOString());
                    const apiRef = document.getElementById('api-reference');
                    console.log('[Scalar Fallback] Element check in setTimeout:', apiRef ? 'FOUND' : 'NOT FOUND');
                    if (apiRef) {
                        const specUrl = apiRef.getAttribute('data-spec-url');
                        console.log('[Scalar Fallback] Element data-spec-url in setTimeout:', specUrl);
                        console.log('[Scalar Fallback] Element innerHTML length:', apiRef.innerHTML.length);
                        
                        if (specUrl) {
                            // Nếu Scalar chưa tự động khởi tạo, khởi tạo thủ công
                            if (typeof ScalarApiReference !== 'undefined' && ScalarApiReference.default) {
                                console.log('[Scalar Fallback] Manually initializing Scalar with URL:', specUrl);
                                try {
                                    ScalarApiReference.default({
                                        spec: { url: specUrl },
                                        theme: 'default',
                                        layout: 'modern'
                                    });
                                    console.log('[Scalar Fallback] Scalar initialization call completed');
                                } catch (e) {
                                    console.error('[Scalar Fallback] Error initializing Scalar:', e);
                                }
                            } else {
                                console.warn('[Scalar Fallback] ScalarApiReference.default not available, waiting for auto-init');
                            }
                        } else {
                            console.warn('[Scalar Fallback] Element #api-reference missing data-spec-url');
                        }
                    } else {
                        console.error('[Scalar Fallback] Element #api-reference not found in setTimeout');
                    }
                    document.getElementById('loading-message').style.display = 'none';
                }, 100);
            };
            // Load script với defer để đảm bảo DOM đã sẵn sàng
            script.defer = true;
            document.head.appendChild(script);
        }

R"(        // Check if spec URL is accessible
        function checkSpecUrl() {
            const specUrl = ")" + fullSpecUrl + R"(";
            fetch(specUrl)
                .then(response => {
                    if (!response.ok) {
                        throw new Error('HTTP ' + response.status + ': ' + response.statusText);
                    }
                    return response.text();
                })
                .then(data => {
                    console.log('OpenAPI spec loaded successfully');
                })
                .catch(error => {
                    console.error('Failed to load OpenAPI spec:', error);
                    document.getElementById('loading-message').style.display = 'none';
                    document.getElementById('error-message').style.display = 'block';
                    document.getElementById('error-message').innerHTML = 
                        '<h2>Error loading API documentation</h2>' +
                        '<p>Failed to load OpenAPI specification from: <a href="' + specUrl + '" target="_blank">' + specUrl + '</a></p>' +
                        '<p>Error: ' + error.message + '</p>' +
                        '<p>Please check:</p>' +
                        '<ul style="text-align: left; display: inline-block;">' +
                        '<li>That the server is running</li>' +
                        '<li>That the endpoint is accessible</li>' +
                        '<li>Browser console for more details</li>' +
                        '</ul>';
                });
        }

        // Start loading when page loads - đảm bảo element đã có trong DOM
        console.log('[Scalar Fallback] Setting up DOMContentLoaded listener at:', new Date().toISOString());
        window.addEventListener('DOMContentLoaded', function() {
            console.log('[Scalar Fallback] DOMContentLoaded fired at:', new Date().toISOString());
            console.log('[Scalar Fallback] Document readyState:', document.readyState);
            
            // Check element immediately
            const apiRefImmediate = document.getElementById('api-reference');
            console.log('[Scalar Fallback] Element check in DOMContentLoaded:', apiRefImmediate ? 'FOUND' : 'NOT FOUND');
            if (apiRefImmediate) {
                console.log('[Scalar Fallback] Element data-spec-url in DOMContentLoaded:', apiRefImmediate.getAttribute('data-spec-url'));
            }
            
            // Đợi một chút để đảm bảo tất cả elements đã được render
            setTimeout(function() {
                console.log('[Scalar Fallback] setTimeout in DOMContentLoaded fired at:', new Date().toISOString());
                const apiRef = document.getElementById('api-reference');
                console.log('[Scalar Fallback] Element check in setTimeout:', apiRef ? 'FOUND' : 'NOT FOUND');
                if (apiRef) {
                    const specUrl = apiRef.getAttribute('data-spec-url');
                    console.log('[Scalar Fallback] Element #api-reference found, data-spec-url:', specUrl);
                    console.log('[Scalar Fallback] Loading Scalar script...');
                    loadScalarScript(SCALAR_CDN_URLS);
                } else {
                    console.error('[Scalar Fallback] Element #api-reference not found in setTimeout!');
                }
                // Check spec URL after a short delay
                setTimeout(checkSpecUrl, 1000);
            }, 50); // Small delay to ensure DOM is fully ready
        });
        
        // Also check if DOM is already loaded
        if (document.readyState === 'loading') {
            console.log('[Scalar Fallback] Document is still loading, waiting for DOMContentLoaded');
        } else {
            console.log('[Scalar Fallback] Document already loaded, readyState:', document.readyState);
            // If already loaded, trigger immediately
            const apiRefNow = document.getElementById('api-reference');
            console.log('[Scalar Fallback] Element check now (already loaded):', apiRefNow ? 'FOUND' : 'NOT FOUND');
            if (apiRefNow) {
                console.log('[Scalar Fallback] Element data-spec-url now:', apiRefNow.getAttribute('data-spec-url'));
            }
        }

        // Fallback: if script doesn't load after 10 seconds, show error
        setTimeout(function() {
            if (!scriptLoaded) {
                console.warn('Scalar script loading timeout');
                document.getElementById('loading-message').style.display = 'none';
                document.getElementById('error-message').style.display = 'block';
                document.getElementById('error-message').innerHTML = 
                    '<h2>Timeout loading Scalar API Reference</h2>' +
                    '<p>The script is taking too long to load. Please refresh the page.</p>';
            }
        }, 10000);
    </script>
</head>
<body>
    <div class="language-selector">
        <label for="language-select">Language / Ngôn ngữ:</label>
        <select id="language-select">
            <option value="en">English</option>
            <option value="vi">Tiếng Việt</option>
        </select>
    </div>
    <div id="loading-message">
        <h2>Loading API Documentation...</h2>
        <p>Please wait while we load the Scalar API Reference.</p>
    </div>
    <div id="error-message"></div>
    <div id="api-reference" data-spec-url=")" + initialSpecUrl + R"("></div>
    <script>
        // Global reference to Scalar instance for cleanup
        window.scalarInstance = null;
        
        // Lấy ngôn ngữ từ URL parameter hoặc localStorage
        function getLanguage() {
            const urlParams = new URLSearchParams(window.location.search);
            const langFromUrl = urlParams.get('lang');
            if (langFromUrl && (langFromUrl === 'en' || langFromUrl === 'vi')) {
                return langFromUrl;
            }
            const langFromStorage = localStorage.getItem('api-docs-language');
            if (langFromStorage && (langFromStorage === 'en' || langFromStorage === 'vi')) {
                return langFromStorage;
            }
            return 'en'; // Default to English
        }

        // Lưu ngôn ngữ đã chọn
        function saveLanguage(lang) {
            localStorage.setItem('api-docs-language', lang);
            const url = new URL(window.location);
            url.searchParams.set('lang', lang);
            window.history.replaceState({}, '', url);
        }

        // Khởi tạo Scalar với ngôn ngữ đã chọn
        function initializeScalar(language, forceRefresh) {
            const baseUrl = ")" + baseUrl + R"(";
            const versionStr = ")" + version + R"(";
            let specUrl;
            
            // Xây dựng URL cho file OpenAPI theo ngôn ngữ
            // File OpenAPI được serve từ endpoint: /{version}/openapi/{lang}/openapi.yaml
            if (versionStr && versionStr !== '') {
                specUrl = baseUrl + '/' + versionStr + '/openapi/' + language + '/openapi.yaml';
            } else {
                specUrl = baseUrl + '/openapi/' + language + '/openapi.yaml';
            }
            
            // Add cache-busting parameter when force refresh is needed (e.g., language change)
            if (forceRefresh) {
                specUrl += '?_t=' + Date.now();
            }
            
            // Fallback: nếu không tìm thấy file theo ngôn ngữ, dùng file mặc định
            const fallbackUrl = ")" + fullSpecUrl + R"(";
            
            // Thử load file theo ngôn ngữ, nếu fail thì dùng fallback
            fetch(specUrl, { 
                cache: 'no-store',
                headers: {
                    'Cache-Control': 'no-cache, no-store, must-revalidate',
                    'Pragma': 'no-cache',
                    'Expires': '0'
                }
            })
                .then(response => {
                    if (!response.ok) {
                        throw new Error('File not found');
                    }
                    return specUrl;
                })
                .catch(() => {
                    console.warn('Language-specific file not found, using fallback');
                    return fallbackUrl;
                })
                .then(finalUrl => {
                    // Update data-spec-url attribute trước khi khởi tạo
                    const apiRef = document.getElementById('api-reference');
                    if (apiRef) {
                        apiRef.setAttribute('data-spec-url', finalUrl);
                    }
                    
                    // Get server URL for API testing
                    const serverUrl = baseUrl || window.location.origin;
                    
                    const configuration = {
                        spec: {
                            url: finalUrl
                        },
                        theme: 'default',
                        layout: 'modern',
                        hideDownloadButton: false,
                        hideModels: false,
                        hideSidebar: false,
                        // Enable API testing with proper configuration
                        defaultHttpClient: {
                            targetKey: 'javascript',
                            clientKey: 'fetch'
                        },
                        // Configure server for API testing
                        withCredentials: false,
                        // Custom CSS để cải thiện định dạng response body
                        customCss: `
                            .scalar-api-reference .response-body {
                                font-family: 'Monaco', 'Menlo', 'Ubuntu Mono', 'Consolas', 'source-code-pro', monospace !important;
                                font-size: 13px !important;
                                line-height: 1.6 !important;
                                background: #f8f9fa !important;
                                border-radius: 6px !important;
                                padding: 16px !important;
                                overflow-x: auto !important;
                            }
                            .scalar-api-reference .response-body pre {
                                margin: 0 !important;
                                padding: 0 !important;
                                background: transparent !important;
                                white-space: pre-wrap !important;
                                word-wrap: break-word !important;
                            }
                            .scalar-api-reference .response-body code {
                                color: #24292e !important;
                                background: transparent !important;
                                padding: 0 !important;
                                font-size: 13px !important;
                            }
                            .scalar-api-reference .response-section {
                                padding: 20px !important;
                                background: #ffffff !important;
                                border: 1px solid #e1e4e8 !important;
                                border-radius: 6px !important;
                                margin-top: 16px !important;
                            }
                            .scalar-api-reference .response-body::-webkit-scrollbar {
                                width: 8px;
                                height: 8px;
                            }
                            .scalar-api-reference .response-body::-webkit-scrollbar-track {
                                background: #f1f1f1;
                                border-radius: 4px;
                            }
                            .scalar-api-reference .response-body::-webkit-scrollbar-thumb {
                                background: #888;
                                border-radius: 4px;
                            }
                            .scalar-api-reference .response-body::-webkit-scrollbar-thumb:hover {
                                background: #555;
                            }
                        `
                    };

                    // Xóa api-reference cũ nếu có
                    const oldRef = document.getElementById('api-reference');
                    if (oldRef) {
                        oldRef.innerHTML = '';
                    }

                    // Destroy previous instance if exists
                    if (window.scalarInstance) {
                        try {
                            console.log('[Scalar] Destroying previous instance...');
                            if (window.scalarInstance.destroy && typeof window.scalarInstance.destroy === 'function') {
                                window.scalarInstance.destroy();
                            } else if (window.scalarInstance.unmount && typeof window.scalarInstance.unmount === 'function') {
                                window.scalarInstance.unmount();
                            } else if (window.scalarInstance.app && window.scalarInstance.app.unmount) {
                                window.scalarInstance.app.unmount();
                            }
                        } catch (destroyError) {
                            console.warn('[Scalar] Error destroying instance:', destroyError);
                        }
                        window.scalarInstance = null;
                    }
                    
                    // Đợi Scalar script load xong rồi mới khởi tạo
                    if (typeof ScalarApiReference !== 'undefined') {
                        // Scalar đã load, khởi tạo trực tiếp
                        const result = ScalarApiReference.default({
                            ...configuration,
                            target: '#api-reference'
                        });
                        
                        // Store instance reference
                        if (result) {
                            window.scalarInstance = result;
                        }
                    } else {
                        // Scalar chưa load, tạo script element để tự động khởi tạo
                        const script = document.createElement('script');
                        script.id = 'api-reference-config';
                        script.setAttribute('data-configuration', JSON.stringify(configuration));
                        document.body.appendChild(script);
                    }
                });
        }

        // Xử lý thay đổi ngôn ngữ
        async function handleLanguageChange() {
            const select = document.getElementById('language-select');
            const selectedLang = select.value;
            saveLanguage(selectedLang);
            
            console.log('[Language Change] Switching to language:', selectedLang);
            
            // Reload page to ensure fresh content (most reliable solution)
            // This ensures:
            // 1. Server renders HTML with correct language parameter
            // 2. Scalar initializes fresh with correct data-spec-url
            // 3. No cache issues or stale content
            // The URL already has ?lang= parameter, so reload will fetch correct content
            window.location.reload();
        }

        // Khởi tạo
        document.addEventListener('DOMContentLoaded', function() {
            const currentLang = getLanguage();
            const select = document.getElementById('language-select');
            if (select) {
                select.value = currentLang;
                select.addEventListener('change', handleLanguageChange);
            }
            
            // Kiểm tra xem data-spec-url đã được set đúng chưa
            const apiRef = document.getElementById('api-reference');
            const currentUrl = apiRef ? apiRef.getAttribute('data-spec-url') : '';
            
            // Chỉ khởi tạo lại nếu URL không khớp với language hiện tại
            if (!currentUrl || !currentUrl.includes('/openapi/' + currentLang + '/')) {
                // Khởi tạo Scalar với ngôn ngữ hiện tại (sẽ update data-spec-url trong initializeScalar)
                initializeScalar(currentLang);
            } else {
                // URL đã đúng, chỉ cần đảm bảo Scalar đã khởi tạo
                console.log('Scalar URL already correct for language:', currentLang);
            }
        });
    </script>
</body>
</html>)";

  return html;
}

std::string
SwaggerHandler::readOpenAPIFile(const std::string &version,
                                const std::string &requestHost,
                                const std::string &language) const {
  // Check cache first - include language in cache key
  std::string cacheKey = version.empty() ? "all" : version;
  if (!language.empty()) {
    cacheKey += "_" + language;
  }

  {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = cache_.find(cacheKey);
    if (it != cache_.end()) {
      auto now = std::chrono::steady_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
          now - it->second.timestamp);

      // Check if cache is still valid (not expired)
      if (elapsed < it->second.ttl) {
        // Check if file has been modified since cache was created
        try {
          if (!it->second.filePath.empty() &&
              std::filesystem::exists(it->second.filePath)) {
            auto currentModTime =
                std::filesystem::last_write_time(it->second.filePath);
            if (currentModTime <= it->second.fileModTime) {
              // File hasn't changed, return cached content
              return it->second.content;
            }
            // File has changed, invalidate cache
          }
        } catch (...) {
          // If we can't check file time, use cache anyway (fallback)
          return it->second.content;
        }
      }
      // Cache expired or file changed, remove it
      cache_.erase(it);
    }
  }

  // Cache miss or expired, read from file
  // Priority: 1) api-specs/openapi/{lang}/openapi.yaml (if language specified), 2) api-specs/openapi.yaml (project structure), 3) Current working directory (service install dir), 4)
  // Environment variable, 5) Search up from current dir
  std::vector<std::filesystem::path> possiblePaths;

  // 1. If language is specified, check language-specific file first
  if (!language.empty() && validateLanguageCode(language)) {
    // Try multiple possible locations for the file
    std::vector<std::filesystem::path> testLocations;
    
    // Location 1: Project root (relative to source/build)
    try {
      std::filesystem::path currentDir = std::filesystem::current_path();
      // If we're in build directory, go up to project root
      if (currentDir.filename() == "build") {
        testLocations.push_back(currentDir.parent_path() / "api-specs" / "openapi" / language / "openapi.yaml");
      }
      // Also try current directory
      testLocations.push_back(currentDir / "api-specs" / "openapi" / language / "openapi.yaml");
    } catch (...) {
      // Ignore
    }
    
    // Location 2: Search up from current directory (for development)
    try {
      std::filesystem::path basePath = std::filesystem::canonical(std::filesystem::current_path());
      std::filesystem::path current = basePath;
      for (int i = 0; i < 4; ++i) {
        std::filesystem::path testPath = current / "api-specs" / "openapi" / language / "openapi.yaml";
        testLocations.push_back(testPath);
        if (current.has_parent_path() && current != current.parent_path()) {
          current = current.parent_path();
        } else {
          break;
        }
      }
    } catch (...) {
      // Ignore
    }
    
    // Check all test locations
    for (const auto& testPath : testLocations) {
      try {
        PLOG_INFO << "[OpenAPI] Checking language-specific file: " << testPath;
        if (std::filesystem::exists(testPath) &&
            std::filesystem::is_regular_file(testPath)) {
          PLOG_INFO << "[OpenAPI] Found language-specific file: " << testPath << " for language: " << language;
          possiblePaths.push_back(testPath);
          break; // Found it, no need to check other locations
        }
      } catch (const std::exception& e) {
        PLOG_WARNING << "[OpenAPI] Error checking " << testPath << ": " << e.what();
      } catch (...) {
        // Ignore
      }
    }
    
    if (possiblePaths.empty()) {
      PLOG_WARNING << "[OpenAPI] Language-specific file not found for language: " << language;
    }

    // Also check in install directory
    const char *installDir = std::getenv("EDGE_AI_API_INSTALL_DIR");
    if (installDir && installDir[0] != '\0') {
      try {
        std::filesystem::path installPath(installDir);
        std::filesystem::path testPath = installPath / "api-specs" / "openapi" / language / "openapi.yaml";
        if (std::filesystem::exists(testPath) &&
            std::filesystem::is_regular_file(testPath)) {
          possiblePaths.push_back(testPath);
        }
      } catch (...) {
        // Ignore errors
      }
    }
  }

  // 2. Check api-specs/openapi.yaml (project structure) - fallback if language-specific not found
  try {
    std::filesystem::path currentDir = std::filesystem::current_path();
    std::filesystem::path testPath = currentDir / "api-specs" / "openapi.yaml";
    if (std::filesystem::exists(testPath) &&
        std::filesystem::is_regular_file(testPath)) {
      possiblePaths.push_back(testPath);
    }
  } catch (...) {
    // Ignore errors
  }

  // 3. Check current working directory (for service installations)
  try {
    std::filesystem::path currentDir = std::filesystem::current_path();
    std::filesystem::path testPath = currentDir / "openapi.yaml";
    if (std::filesystem::exists(testPath) &&
        std::filesystem::is_regular_file(testPath)) {
      possiblePaths.push_back(testPath);
    }
  } catch (...) {
    // Ignore errors
  }

  // 4. Check environment variable for installation directory
  const char *installDir = std::getenv("EDGE_AI_API_INSTALL_DIR");
  if (installDir && installDir[0] != '\0') {
    try {
      std::filesystem::path installPath(installDir);
      std::filesystem::path testPath = installPath / "openapi.yaml";
      if (std::filesystem::exists(testPath) &&
          std::filesystem::is_regular_file(testPath)) {
        possiblePaths.push_back(testPath);
      }
      // Also check api-specs subdirectory
      std::filesystem::path testPathApiSpecs = installPath / "api-specs" / "openapi.yaml";
      if (std::filesystem::exists(testPathApiSpecs) &&
          std::filesystem::is_regular_file(testPathApiSpecs)) {
        possiblePaths.push_back(testPathApiSpecs);
      }
    } catch (...) {
      // Ignore errors
    }
  }

  // 5. Search up from current directory (for development)
  std::filesystem::path basePath;
  try {
    basePath = std::filesystem::canonical(std::filesystem::current_path());
  } catch (...) {
    basePath = std::filesystem::current_path();
  }

  std::filesystem::path current = basePath;
  for (int i = 0; i < 4; ++i) {
    // Check api-specs/openapi.yaml first
    std::filesystem::path testPathApiSpecs = current / "api-specs" / "openapi.yaml";
    if (std::filesystem::exists(testPathApiSpecs)) {
      if (std::filesystem::is_regular_file(testPathApiSpecs)) {
        // Avoid duplicates
        bool alreadyAdded = false;
        for (const auto &existing : possiblePaths) {
          try {
            if (std::filesystem::equivalent(testPathApiSpecs, existing)) {
              alreadyAdded = true;
              break;
            }
          } catch (...) {
            if (testPathApiSpecs == existing) {
              alreadyAdded = true;
              break;
            }
          }
        }
        if (!alreadyAdded) {
          possiblePaths.push_back(testPathApiSpecs);
        }
      }
    }

    // Check openapi.yaml in current directory
    std::filesystem::path testPath = current / "openapi.yaml";
    if (std::filesystem::exists(testPath)) {
      // Verify it's a regular file and not a symlink to prevent attacks
      if (std::filesystem::is_regular_file(testPath)) {
        // Avoid duplicates
        bool alreadyAdded = false;
        for (const auto &existing : possiblePaths) {
          try {
            if (std::filesystem::equivalent(testPath, existing)) {
              alreadyAdded = true;
              break;
            }
          } catch (...) {
            // If equivalent fails, compare as strings
            if (testPath == existing) {
              alreadyAdded = true;
              break;
            }
          }
        }
        if (!alreadyAdded) {
          possiblePaths.push_back(testPath);
        }
      }
    }

    if (current.has_parent_path() && current != current.parent_path()) {
      current = current.parent_path();
    } else {
      break;
    }
  }

  // Try to read from found paths
  PLOG_INFO << "[OpenAPI] Found " << possiblePaths.size() << " possible paths for version: " << version << ", language: " << language;
  for (size_t i = 0; i < possiblePaths.size(); ++i) {
    PLOG_INFO << "[OpenAPI] Path " << (i+1) << ": " << possiblePaths[i];
  }
  
  std::string yamlContent;
  std::filesystem::path actualFilePath;
  std::filesystem::file_time_type fileModTime;

  for (const auto &path : possiblePaths) {
    try {
      // Use canonical path to ensure no symlink attacks
      std::filesystem::path canonicalPath = std::filesystem::canonical(path);
      PLOG_INFO << "[OpenAPI] Trying to read file: " << canonicalPath;

      std::ifstream file(canonicalPath);
      if (file.is_open() && file.good()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();
        yamlContent = buffer.str();
        if (!yamlContent.empty()) {
          PLOG_INFO << "[OpenAPI] Successfully read file: " << canonicalPath << ", size: " << yamlContent.length();
          // Log first few characters to verify language
          if (yamlContent.length() > 200) {
            std::string preview = yamlContent.substr(0, 200);
            PLOG_INFO << "[OpenAPI] File preview (first 200 chars): " << preview;
            // Check if it contains Vietnamese text
            if (yamlContent.find("Kiểm tra") != std::string::npos || 
                yamlContent.find("trạng thái") != std::string::npos) {
              PLOG_INFO << "[OpenAPI] File contains Vietnamese text - language: " << language;
            } else if (yamlContent.find("Health check") != std::string::npos ||
                       yamlContent.find("Returns the health") != std::string::npos) {
              PLOG_WARNING << "[OpenAPI] File contains English text but language requested is: " << language;
            }
          }
          actualFilePath = canonicalPath;
          fileModTime = std::filesystem::last_write_time(canonicalPath);
          break;
        } else {
          PLOG_WARNING << "[OpenAPI] File is empty: " << canonicalPath;
        }
      } else {
        PLOG_WARNING << "[OpenAPI] Cannot open file: " << canonicalPath;
      }
    } catch (const std::exception& e) {
      PLOG_ERROR << "[OpenAPI] Error reading file " << path << ": " << e.what();
    } catch (...) {
      PLOG_ERROR << "[OpenAPI] Unknown error reading file: " << path;
    }
  }

  if (yamlContent.empty()) {
    throw std::runtime_error("OpenAPI specification file not found");
  }

  // Update server URLs from environment variables
  // Use requestHost if provided (for browser-accessible URL), otherwise use env
  // vars
  std::string updatedContent =
      updateOpenAPIServerURLs(yamlContent, requestHost);

  // If version is specified, filter the content
  std::string finalContent = updatedContent;
  if (!version.empty()) {
    finalContent = filterOpenAPIByVersion(updatedContent, version);
  }

  // Update cache with file path and modification time
  {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    CacheEntry entry;
    entry.content = finalContent;
    entry.timestamp = std::chrono::steady_clock::now();
    entry.ttl = cache_ttl_;
    entry.filePath = actualFilePath;
    entry.fileModTime = fileModTime;
    cache_[cacheKey] = entry;
  }

  return finalContent;
}

std::string
SwaggerHandler::filterOpenAPIByVersion(const std::string &yamlContent,
                                       const std::string &version) const {
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
        currentPath =
            line.substr(2, colonPos - 2); // Extract path without leading spaces

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

std::string
SwaggerHandler::updateOpenAPIServerURLs(const std::string &yamlContent,
                                        const std::string &requestHost) const {
  std::string serverUrl;

  // If requestHost is provided, use it (browser-accessible URL)
  if (!requestHost.empty()) {
    // Extract host and port from requestHost (format: "host:port" or "host")
    std::string host = requestHost;
    std::string portStr = "";

    size_t colonPos = requestHost.find(':');
    if (colonPos != std::string::npos) {
      host = requestHost.substr(0, colonPos);
      portStr = requestHost.substr(colonPos + 1);
    }

    // Determine scheme (http or https)
    std::string scheme = "http";
    const char *https_env = std::getenv("API_HTTPS");
    if (https_env &&
        (std::string(https_env) == "1" || std::string(https_env) == "true")) {
      scheme = "https";
    }

    // Build server URL from request host
    serverUrl = scheme + "://" + host;
    if (!portStr.empty()) {
      serverUrl += ":" + portStr;
    }
  } else {
    // Fallback to environment variables
    std::string host = EnvConfig::getString("API_HOST", "0.0.0.0");
    uint16_t port =
        static_cast<uint16_t>(EnvConfig::getInt("API_PORT", 8080, 1, 65535));

    // If host is 0.0.0.0, use localhost for browser compatibility
    if (host == "0.0.0.0") {
      host = "localhost";
    }

    // Determine scheme (http or https)
    std::string scheme = "http";
    const char *https_env = std::getenv("API_HTTPS");
    if (https_env &&
        (std::string(https_env) == "1" || std::string(https_env) == "true")) {
      scheme = "https";
    }

    // Build server URL
    serverUrl = scheme + "://" + host;
    if ((scheme == "http" && port != 80) ||
        (scheme == "https" && port != 443)) {
      serverUrl += ":" + std::to_string(port);
    }
  }

  // Replace server URLs in OpenAPI spec
  // Pattern: url: http://localhost:8080 or url: http://0.0.0.0:8080
  std::string result = yamlContent;

  // Replace all server URL patterns
  // Match: "  - url: http://..." or "  url: http://..."
  std::regex urlPattern(R"((\s+-\s+url:\s+)(https?://[^\s]+))");
  result = std::regex_replace(result, urlPattern, "$1" + serverUrl);

  // Also handle case without dash (single server)
  std::regex urlPattern2(R"((\s+url:\s+)(https?://[^\s]+))");
  result = std::regex_replace(result, urlPattern2, "$1" + serverUrl);

  return result;
}

void SwaggerHandler::handleOptions(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  // Set handler start time for accurate metrics
  MetricsInterceptor::setHandlerStartTime(req);

  auto resp = HttpResponse::newHttpResponse();
  resp->setStatusCode(k200OK);
  resp->addHeader("Access-Control-Allow-Origin", "*");
  resp->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
  resp->addHeader("Access-Control-Max-Age", "3600");

  // Record metrics and call callback
  MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
}

