#pragma once

#include <drogon/HttpFilter.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

/**
 * @brief CORS Filter
 *
 * Handles CORS preflight (OPTIONS) requests and adds CORS headers to all
 * responses
 */
class CorsFilter : public drogon::HttpFilter<CorsFilter, false> {
public:
  CorsFilter() {}

  void doFilter(const drogon::HttpRequestPtr &req, drogon::FilterCallback &&fcb,
                drogon::FilterChainCallback &&fccb) override;
};
