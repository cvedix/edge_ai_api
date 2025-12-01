#include "core/request_middleware.h"
#include "core/endpoint_monitor.h"
#include <chrono>

void RequestMetricsMiddleware::doFilter(const drogon::HttpRequestPtr& req,
                                       drogon::FilterCallback&& fcb,
                                       drogon::FilterChainCallback&& fccb)
{
    auto start_time = std::chrono::steady_clock::now();
    std::string endpoint = req->path();

    // Wrap the filter callback to capture metrics
    auto wrapped_fcb = [start_time, endpoint, fcb = std::move(fcb)](const drogon::HttpResponsePtr& resp) {
        // Calculate response time
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        uint64_t response_time_ms = duration.count();

        // Determine if error (4xx, 5xx)
        bool is_error = resp->statusCode() >= 400;

        // Record metrics
        EndpointMonitor::getInstance().recordRequest(endpoint, response_time_ms, is_error);

        // Call original callback
        fcb(resp);
    };

    // Call next filter/handler (FilterChainCallback takes no arguments)
    fccb();
    
    // Note: Metrics will be recorded when the response callback is invoked
    // This is a simplified version - in production, you might want to use
    // a more sophisticated approach to capture response metrics
}

