#pragma once

#include <chrono>
#include <drogon/HttpFilter.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

/**
 * @brief Middleware to track request metrics for endpoint monitoring
 *
 * This middleware automatically tracks:
 * - Request count per endpoint
 * - Response time per endpoint
 * - Error count per endpoint
 */
class RequestMetricsMiddleware
    : public drogon::HttpFilter<RequestMetricsMiddleware> {
public:
  RequestMetricsMiddleware() {}

  void doFilter(const drogon::HttpRequestPtr &req, drogon::FilterCallback &&fcb,
                drogon::FilterChainCallback &&fccb) override;
};
