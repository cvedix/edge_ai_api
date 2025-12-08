#include "api/version_handler.h"
#include <drogon/HttpResponse.h>
#include <json/json.h>

// Version information - can be set via CMake or build system
#ifndef PROJECT_VERSION
#define PROJECT_VERSION "2025.0.1.1"
#endif

#ifndef BUILD_TIME
#define BUILD_TIME __DATE__ " " __TIME__
#endif

#ifndef GIT_COMMIT
#define GIT_COMMIT "unknown"
#endif

void VersionHandler::getVersion(const HttpRequestPtr & /*req*/,
                                std::function<void(const HttpResponsePtr &)> &&callback)
{
    try {
        Json::Value response;
        
        // Version information
        response["version"] = PROJECT_VERSION;
        response["build_time"] = BUILD_TIME;
        response["git_commit"] = GIT_COMMIT;
        response["api_version"] = "v1";
        response["service"] = "edge_ai_api";

        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k200OK);
        
        // Add CORS headers
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
        
        callback(resp);
    } catch (const std::exception& e) {
        // Error handling
        Json::Value errorResponse;
        errorResponse["error"] = "Internal server error";
        errorResponse["message"] = e.what();
        
        auto resp = HttpResponse::newHttpJsonResponse(errorResponse);
        resp->setStatusCode(k500InternalServerError);
        callback(resp);
    }
}

