#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <functional>

/**
 * @brief Response interceptor to record metrics for all requests
 *
 * This interceptor records metrics in both EndpointMonitor and
 * PerformanceMonitor for all HTTP requests/responses.
 */
class MetricsInterceptor {
public:
  /**
   * @brief Intercept response and record metrics
   * @param req The HTTP request
   * @param resp The HTTP response
   */
  static void intercept(const drogon::HttpRequestPtr &req,
                        const drogon::HttpResponsePtr &resp);

  /**
   * @brief Helper function to wrap a callback and automatically record metrics
   *
   * Usage in handlers:
   *   auto resp = HttpResponse::newHttpJsonResponse(data);
   *   MetricsInterceptor::callWithMetrics(req, resp, callback);
   *
   * @param req The HTTP request
   * @param resp The HTTP response
   * @param callback The original callback to call after recording metrics
   */
  static void callWithMetrics(
      const drogon::HttpRequestPtr &req, const drogon::HttpResponsePtr &resp,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback);

  /**
   * @brief Set handler start time for accurate metrics
   * Should be called at the beginning of handler function
   * @param req The HTTP request
   */
  static void setHandlerStartTime(const drogon::HttpRequestPtr &req);
};
