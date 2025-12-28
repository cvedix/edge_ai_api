#pragma once

#include <atomic>
#include <chrono>
#include <json/json.h>
#include <mutex>
#include <string>
#include <unordered_map>

/**
 * @brief Performance monitoring and metrics collection
 *
 * Collects metrics for Prometheus export and observability.
 * Tracks request latency, throughput, error rates, etc.
 */
class PerformanceMonitor {
public:
  // Key: "method:endpoint:status" (e.g., "GET:/v1/core/health:200")
  struct RequestMetrics {
    std::atomic<uint64_t> count{0};
    std::atomic<double> total_duration_seconds{0.0};
    std::atomic<double> max_duration_seconds{0.0};
    std::atomic<double> min_duration_seconds{
        std::numeric_limits<double>::max()};
  };

  // Legacy structure for backward compatibility
  struct EndpointMetrics {
    std::atomic<uint64_t> total_requests{0};
    std::atomic<uint64_t> successful_requests{0};
    std::atomic<uint64_t> failed_requests{0};
    std::atomic<double> avg_latency_ms{0.0};
    std::atomic<double> max_latency_ms{0.0};
    std::atomic<double> min_latency_ms{std::numeric_limits<double>::max()};
    std::atomic<uint64_t> total_latency_ms{0};
  };

  static PerformanceMonitor &getInstance() {
    static PerformanceMonitor instance;
    return instance;
  }

  /**
   * @brief Record a request
   * @param endpoint Endpoint path
   * @param latency Request latency in milliseconds
   * @param success Whether request was successful
   */
  void recordRequest(const std::string &endpoint,
                     std::chrono::milliseconds latency, bool success = true);

  /**
   * @brief Record a request with method and status code
   * @param method HTTP method (GET, POST, etc.)
   * @param endpoint Endpoint path
   * @param status HTTP status code
   * @param duration Request duration in seconds
   */
  void recordRequest(const std::string &method, const std::string &endpoint,
                     int status, double duration_seconds);

  /**
   * @brief Get metrics for an endpoint (read-only snapshot)
   */
  struct EndpointMetricsSnapshot {
    uint64_t total_requests;
    uint64_t successful_requests;
    uint64_t failed_requests;
    double avg_latency_ms;
    double max_latency_ms;
    double min_latency_ms;
  };

  EndpointMetricsSnapshot getEndpointMetrics(const std::string &endpoint) const;

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
  PerformanceMonitor(const PerformanceMonitor &) = delete;
  PerformanceMonitor &operator=(const PerformanceMonitor &) = delete;

  std::unordered_map<std::string, EndpointMetrics> endpoint_metrics_; // Legacy
  std::unordered_map<std::string, RequestMetrics>
      request_metrics_; // New: method:endpoint:status
  mutable std::mutex mutex_;
  std::chrono::steady_clock::time_point start_time_;

  double calculateThroughput() const;
};
