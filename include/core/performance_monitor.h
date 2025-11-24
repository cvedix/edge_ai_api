#pragma once

#include <atomic>
#include <chrono>
#include <string>
#include <unordered_map>
#include <mutex>
#include <json/json.h>

/**
 * @brief Performance monitoring and metrics collection
 * 
 * Collects metrics for Prometheus export and observability.
 * Tracks request latency, throughput, error rates, etc.
 */
class PerformanceMonitor {
public:
    struct EndpointMetrics {
        std::atomic<uint64_t> total_requests{0};
        std::atomic<uint64_t> successful_requests{0};
        std::atomic<uint64_t> failed_requests{0};
        std::atomic<double> avg_latency_ms{0.0};
        std::atomic<double> max_latency_ms{0.0};
        std::atomic<double> min_latency_ms{std::numeric_limits<double>::max()};
        std::atomic<uint64_t> total_latency_ms{0};
    };

    static PerformanceMonitor& getInstance() {
        static PerformanceMonitor instance;
        return instance;
    }

    /**
     * @brief Record a request
     * @param endpoint Endpoint path
     * @param latency Request latency in milliseconds
     * @param success Whether request was successful
     */
    void recordRequest(const std::string& endpoint,
                     std::chrono::milliseconds latency,
                     bool success = true);

    /**
     * @brief Get metrics for an endpoint
     */
    EndpointMetrics getEndpointMetrics(const std::string& endpoint) const;

    /**
     * @brief Get all metrics as JSON
     */
    Json::Value getMetricsJSON() const;

    /**
     * @brief Get Prometheus format metrics
     */
    std::string getPrometheusMetrics() const;

    /**
     * @brief Get overall statistics
     */
    struct OverallStats {
        uint64_t total_requests;
        uint64_t successful_requests;
        uint64_t failed_requests;
        double avg_latency_ms;
        double throughput_rps;
    };
    
    OverallStats getOverallStats() const;

    /**
     * @brief Reset all metrics
     */
    void reset();

private:
    PerformanceMonitor() = default;
    ~PerformanceMonitor() = default;
    PerformanceMonitor(const PerformanceMonitor&) = delete;
    PerformanceMonitor& operator=(const PerformanceMonitor&) = delete;

    std::unordered_map<std::string, EndpointMetrics> endpoint_metrics_;
    mutable std::mutex mutex_;
    std::chrono::steady_clock::time_point start_time_;
    
    double calculateThroughput() const;
};

