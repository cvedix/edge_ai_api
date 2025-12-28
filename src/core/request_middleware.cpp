#include "core/request_middleware.h"
#include "core/endpoint_monitor.h"
#include "core/performance_monitor.h"
#include <chrono>
#include <mutex>
#include <unordered_map>

// Map to store start times for each request
// Key: request pointer, Value: start time
// Note: These are not static so they can be accessed from
// metrics_interceptor.cpp
std::unordered_map<const void *, std::chrono::steady_clock::time_point>
    request_start_times;
std::mutex start_times_mutex;

void RequestMetricsMiddleware::doFilter(const drogon::HttpRequestPtr &req,
                                        drogon::FilterCallback && /*fcb*/,
                                        drogon::FilterChainCallback &&fccb) {
  auto start_time = std::chrono::steady_clock::now();
  std::string endpoint = req->path();

  // DEBUG: Log when middleware is called
  std::cerr << "[RequestMetricsMiddleware] Processing request: " << endpoint
            << ", req_ptr=" << req.get() << std::endl;

  // Store start time using request pointer as key
  // This will be used to calculate response time when response is created
  {
    std::lock_guard<std::mutex> lock(start_times_mutex);
    request_start_times[req.get()] = start_time;
    std::cerr << "[RequestMetricsMiddleware] Stored start time for req_ptr="
              << req.get() << ", map_size=" << request_start_times.size()
              << std::endl;
  }

  // In Drogon, we can't wrap the filter callback (fcb) because:
  // 1. fcb is an rvalue reference, so we can't modify it
  // 2. fccb() doesn't take parameters, so we can't pass a wrapped callback
  //
  // Solution: Store the start time in a map. Handlers should call
  // MetricsInterceptor::callWithMetrics() to record metrics automatically.
  // This is the safest and simplest approach that works with Drogon's
  // architecture.

  // Continue filter chain
  // The start time is stored in request_start_times map.
  // Handlers should use MetricsInterceptor::callWithMetrics(req, resp,
  // callback) to automatically record metrics when sending responses.
  fccb();
}
