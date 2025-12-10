#include "core/cors_helper.h"
#include <drogon/HttpResponse.h>

namespace CorsHelper {
    void addAllowAllHeaders(drogon::HttpResponsePtr& resp) {
        // ALLOW ALL - Permissive CORS configuration
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, PATCH, OPTIONS, HEAD");
        resp->addHeader("Access-Control-Allow-Headers", "*");
        resp->addHeader("Access-Control-Expose-Headers", "*");
        resp->addHeader("Access-Control-Max-Age", "86400"); // 24 hours
    }
    
    drogon::HttpResponsePtr createOptionsResponse() {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k200OK);
        addAllowAllHeaders(resp);
        return resp;
    }
}

