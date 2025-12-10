#pragma once

#include <drogon/HttpResponse.h>

/**
 * @brief CORS Helper
 * 
 * Utility functions to add CORS headers with "allow all" configuration
 */
namespace CorsHelper {
    /**
     * @brief Add CORS headers with "allow all" to response
     * 
     * This adds permissive CORS headers to allow requests from any origin
     * 
     * @param resp HTTP response to add headers to
     */
    void addAllowAllHeaders(drogon::HttpResponsePtr& resp);
    
    /**
     * @brief Create OPTIONS preflight response with "allow all"
     * 
     * @return HTTP response with CORS headers for OPTIONS preflight
     */
    drogon::HttpResponsePtr createOptionsResponse();
}

