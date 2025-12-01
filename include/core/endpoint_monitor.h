#pragma once

#include <string>
#include <chrono>
#include <atomic>
#include <mutex>
#include <map>
#include <memory>

/**
 * @brief Monitor individual API endpoints
 * 
 * Tracks response time, error rate, and request count for each endpoint.
 * Can be used alongside global watchdog for detailed monitoring.
 */
class EndpointMonitor {
public:
    /**
     * @brief Statistics for an endpoint
     */
    struct EndpointStats {
        std::atomic<uint64_t> request_count{0};
        std::atomic<uint64_t> error_count{0};
        std::atomic<uint64_t> total_response_time_ms{0};
        std::atomic<uint64_t> max_response_time_ms{0};
        std::atomic<uint64_t> min_response_time_ms{UINT64_MAX};
        std::chrono::steady_clock::time_point last_request_time;
        std::atomic<bool> is_healthy{true};
    };

    /**
     * @brief Get singleton instance
     */
    static EndpointMonitor& getInstance();

    /**
     * @brief Record a request for an endpoint
     * @param endpoint Endpoint path (e.g., "/v1/core/health")
     * @param response_time_ms Response time in milliseconds
     * @param is_error Whether the request resulted in an error
     */
    void recordRequest(const std::string& endpoint, 
                      uint64_t response_time_ms, 
                      bool is_error = false);

    /**
     * @brief Get statistics for an endpoint
     * @param endpoint Endpoint path
     * @return EndpointStats or nullptr if not found
     */
    std::shared_ptr<EndpointStats> getStats(const std::string& endpoint) const;

    /**
     * @brief Get all endpoint statistics
     */
    std::map<std::string, std::shared_ptr<EndpointStats>> getAllStats() const;

    /**
     * @brief Check if an endpoint is healthy
     * @param endpoint Endpoint path
     * @param max_avg_response_time_ms Maximum average response time (ms)
     * @param max_error_rate Maximum error rate (0.0-1.0)
     * @return true if healthy
     */
    bool isEndpointHealthy(const std::string& endpoint,
                          uint64_t max_avg_response_time_ms = 1000,
                          double max_error_rate = 0.1) const;

    /**
     * @brief Reset statistics for an endpoint
     */
    void resetStats(const std::string& endpoint);

    /**
     * @brief Reset all statistics
     */
    void resetAllStats();

private:
    EndpointMonitor() = default;
    ~EndpointMonitor() = default;
    EndpointMonitor(const EndpointMonitor&) = delete;
    EndpointMonitor& operator=(const EndpointMonitor&) = delete;

    mutable std::mutex stats_mutex_;
    std::map<std::string, std::shared_ptr<EndpointStats>> endpoint_stats_;
};

