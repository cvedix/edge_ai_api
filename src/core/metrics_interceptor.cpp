#include "core/metrics_interceptor.h"
#include "core/endpoint_monitor.h"
#include "core/performance_monitor.h"
#include <chrono>
#include <mutex>
#include <unordered_map>

// Map to store start times for each request (shared with middleware)
// These are defined in request_middleware.cpp
extern std::unordered_map<const void *, std::chrono::steady_clock::time_point>
    request_start_times;
extern std::mutex start_times_mutex;

// Map to store handler start times (set when handler begins processing)
static std::unordered_map<const void *, std::chrono::steady_clock::time_point>
    handler_start_times;
static std::mutex handler_start_times_mutex;

// Helper to set handler start time (should be called at the beginning of
// handler)
void MetricsInterceptor::setHandlerStartTime(
    const drogon::HttpRequestPtr &req) {
  auto start_time = std::chrono::steady_clock::now();
  std::lock_guard<std::mutex> lock(handler_start_times_mutex);
  handler_start_times[req.get()] = start_time;
}

void MetricsInterceptor::intercept(const drogon::HttpRequestPtr &req,
                                   const drogon::HttpResponsePtr &resp) {
  try {
    // DEBUG: Log when interceptor is called
    std::cerr << "[MetricsInterceptor] Intercepting response for: "
              << req->path() << ", req_ptr=" << req.get() << std::endl;

    // Get start time from map using request pointer as key
    std::chrono::steady_clock::time_point start_time;
    bool found = false;

    {
      std::lock_guard<std::mutex> lock(start_times_mutex);
      std::cerr << "[MetricsInterceptor] Looking for req_ptr=" << req.get()
                << ", map_size=" << request_start_times.size() << std::endl;
      auto it = request_start_times.find(req.get());
      if (it != request_start_times.end()) {
        start_time = it->second;
        found = true;
        // Remove from map after reading
        request_start_times.erase(it);
        std::cerr << "[MetricsInterceptor] Found start time for req_ptr="
                  << req.get() << std::endl;
      } else {
        std::cerr
            << "[MetricsInterceptor] WARNING: Start time NOT found for req_ptr="
            << req.get() << std::endl;
        // Try to get from request attributes as fallback
        // Drogon stores request creation time, but we can't access it directly
        // So we'll use current time minus a small estimate, or skip if not
        // critical
      }
    }

    if (!found) {
      // If no start time found, use current time as fallback
      // This means we can't calculate accurate response time, but we can still
      // record the request
      std::cerr << "[MetricsInterceptor] WARNING: No start time found, using "
                   "current time as fallback"
                << std::endl;
      start_time = std::chrono::steady_clock::now();
      // Use 0ms as response time since we don't have accurate timing
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    uint64_t response_time_ms =
        found ? duration.count() : 0; // Use 0 if we don't have accurate timing

    std::string endpoint = req->path();

    // Determine if error (4xx, 5xx)
    bool is_error = resp->statusCode() >= 400;
    bool is_success = !is_error;

    // Record metrics in EndpointMonitor
    EndpointMonitor::getInstance().recordRequest(endpoint, response_time_ms,
                                                 is_error);

    // Record metrics in PerformanceMonitor
    PerformanceMonitor::getInstance().recordRequest(endpoint, duration,
                                                    is_success);
  } catch (...) {
    // Silently ignore errors in metrics recording to not affect request
    // handling
  }
}

void MetricsInterceptor::callWithMetrics(
    const drogon::HttpRequestPtr &req, const drogon::HttpResponsePtr &resp,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  // Try to get start time from middleware map first (most accurate)
  std::chrono::steady_clock::time_point start_time;
  bool found = false;

  {
    std::lock_guard<std::mutex> lock(start_times_mutex);
    auto it = request_start_times.find(req.get());
    if (it != request_start_times.end()) {
      start_time = it->second;
      found = true;
      request_start_times.erase(it);
    }
  }

  // If not found, try handler start times (set when handler begins)
  if (!found) {
    std::lock_guard<std::mutex> lock(handler_start_times_mutex);
    auto it = handler_start_times.find(req.get());
    if (it != handler_start_times.end()) {
      start_time = it->second;
      found = true;
      handler_start_times.erase(it);
    }
  }

  // If still not found, use current time minus small estimate
  if (!found) {
    start_time =
        std::chrono::steady_clock::now() - std::chrono::milliseconds(1);
  }

  // Calculate response time (use microseconds for accuracy, then convert to ms)
  auto end_time = std::chrono::steady_clock::now();
  auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time - start_time);
  // Convert to milliseconds, rounding up if < 1ms to avoid 0
  uint64_t response_time_ms =
      (duration_us.count() + 500) / 1000; // Round to nearest ms
  if (response_time_ms == 0 && duration_us.count() > 0) {
    response_time_ms = 1; // At least 1ms if there's any measurable time
  }
  auto duration = std::chrono::milliseconds(response_time_ms);

  std::string endpoint = req->path();
  int status_code = resp->statusCode();
  bool is_error = status_code >= 400;
  bool is_success = !is_error;

  // Get HTTP method
  std::string method;
  switch (req->method()) {
  case drogon::Get:
    method = "GET";
    break;
  case drogon::Post:
    method = "POST";
    break;
  case drogon::Put:
    method = "PUT";
    break;
  case drogon::Delete:
    method = "DELETE";
    break;
  case drogon::Patch:
    method = "PATCH";
    break;
  case drogon::Options:
    method = "OPTIONS";
    break;
  case drogon::Head:
    method = "HEAD";
    break;
  default:
    method = "UNKNOWN";
    break;
  }

  // Convert duration to seconds
  double duration_seconds = duration_us.count() / 1000000.0;

  // Record metrics with new format (method, endpoint, status, duration_seconds)
  PerformanceMonitor::getInstance().recordRequest(method, endpoint, status_code,
                                                  duration_seconds);

  // Also record legacy format for backward compatibility
  EndpointMonitor::getInstance().recordRequest(endpoint, response_time_ms,
                                               is_error);
  PerformanceMonitor::getInstance().recordRequest(endpoint, duration,
                                                  is_success);

  // Call the original callback
  callback(resp);
}
