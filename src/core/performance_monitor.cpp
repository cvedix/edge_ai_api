#include "core/performance_monitor.h"
#include <algorithm>
#include <iomanip>
#include <map>
#include <sstream>
#include <vector>

// New function to record request with method and status
void PerformanceMonitor::recordRequest(const std::string &method,
                                       const std::string &endpoint, int status,
                                       double duration_seconds) {
  // Create key: "method:endpoint:status"
  std::string key = method + ":" + endpoint + ":" + std::to_string(status);

  RequestMetrics *metrics_ptr = nullptr;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    metrics_ptr = &request_metrics_[key];
  }

  // Update counters atomically
  metrics_ptr->count.fetch_add(1, std::memory_order_relaxed);

  // For atomic<double>, use compare_exchange_weak loop to add
  double current_total =
      metrics_ptr->total_duration_seconds.load(std::memory_order_relaxed);
  double new_total = current_total + duration_seconds;
  while (!metrics_ptr->total_duration_seconds.compare_exchange_weak(
      current_total, new_total, std::memory_order_relaxed)) {
    new_total = current_total + duration_seconds;
  }

  // Update min/max
  double current_min =
      metrics_ptr->min_duration_seconds.load(std::memory_order_relaxed);
  while (duration_seconds < current_min &&
         !metrics_ptr->min_duration_seconds.compare_exchange_weak(
             current_min, duration_seconds, std::memory_order_relaxed)) {
    current_min =
        metrics_ptr->min_duration_seconds.load(std::memory_order_relaxed);
  }

  double current_max =
      metrics_ptr->max_duration_seconds.load(std::memory_order_relaxed);
  while (duration_seconds > current_max &&
         !metrics_ptr->max_duration_seconds.compare_exchange_weak(
             current_max, duration_seconds, std::memory_order_relaxed)) {
    current_max =
        metrics_ptr->max_duration_seconds.load(std::memory_order_relaxed);
  }
}

// Legacy function for backward compatibility
void PerformanceMonitor::recordRequest(const std::string &endpoint,
                                       std::chrono::milliseconds latency,
                                       bool success) {
  // PHASE 2 OPTIMIZATION: Lock only to find/create metrics entry, then release
  EndpointMetrics *metrics_ptr = nullptr;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    metrics_ptr = &endpoint_metrics_[endpoint];
  }
  // Lock released - now update atomic counters without lock

  // Update counters using atomic operations (no lock needed)
  metrics_ptr->total_requests.fetch_add(1, std::memory_order_relaxed);
  if (success) {
    metrics_ptr->successful_requests.fetch_add(1, std::memory_order_relaxed);
  } else {
    metrics_ptr->failed_requests.fetch_add(1, std::memory_order_relaxed);
  }

  // Update latency statistics
  uint64_t latency_ms = latency.count();
  metrics_ptr->total_latency_ms.fetch_add(latency_ms,
                                          std::memory_order_relaxed);

  // Calculate average (need to read current values)
  uint64_t total = metrics_ptr->total_requests.load(std::memory_order_relaxed);
  uint64_t total_latency =
      metrics_ptr->total_latency_ms.load(std::memory_order_relaxed);
  if (total > 0) {
    double new_avg = static_cast<double>(total_latency) / total;
    // Use compare-and-swap loop for atomic update
    double current_avg =
        metrics_ptr->avg_latency_ms.load(std::memory_order_relaxed);
    while (!metrics_ptr->avg_latency_ms.compare_exchange_weak(
        current_avg, new_avg, std::memory_order_relaxed)) {
      // Recalculate if value changed
      total = metrics_ptr->total_requests.load(std::memory_order_relaxed);
      total_latency =
          metrics_ptr->total_latency_ms.load(std::memory_order_relaxed);
      if (total > 0) {
        new_avg = static_cast<double>(total_latency) / total;
      } else {
        break;
      }
    }
  }

  // Update min/max using compare-and-swap
  double current_min =
      metrics_ptr->min_latency_ms.load(std::memory_order_relaxed);
  while (latency_ms < current_min &&
         !metrics_ptr->min_latency_ms.compare_exchange_weak(
             current_min, static_cast<double>(latency_ms),
             std::memory_order_relaxed)) {
    // Retry if value changed
  }

  double current_max =
      metrics_ptr->max_latency_ms.load(std::memory_order_relaxed);
  while (latency_ms > current_max &&
         !metrics_ptr->max_latency_ms.compare_exchange_weak(
             current_max, static_cast<double>(latency_ms),
             std::memory_order_relaxed)) {
    // Retry if value changed
  }
}

PerformanceMonitor::EndpointMetricsSnapshot
PerformanceMonitor::getEndpointMetrics(const std::string &endpoint) const {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = endpoint_metrics_.find(endpoint);
  if (it != endpoint_metrics_.end()) {
    const auto &metrics = it->second;
    EndpointMetricsSnapshot snapshot;
    snapshot.total_requests = metrics.total_requests.load();
    snapshot.successful_requests = metrics.successful_requests.load();
    snapshot.failed_requests = metrics.failed_requests.load();
    snapshot.avg_latency_ms = metrics.avg_latency_ms.load();
    snapshot.max_latency_ms = metrics.max_latency_ms.load();
    snapshot.min_latency_ms = metrics.min_latency_ms.load();
    return snapshot;
  }

  return EndpointMetricsSnapshot{};
}

Json::Value PerformanceMonitor::getMetricsJSON() const {
  std::lock_guard<std::mutex> lock(mutex_);

  Json::Value root;
  Json::Value endpoints(Json::objectValue);

  for (const auto &[endpoint, metrics] : endpoint_metrics_) {
    Json::Value endpoint_data;
    endpoint_data["total_requests"] =
        static_cast<Json::UInt64>(metrics.total_requests.load());
    endpoint_data["successful_requests"] =
        static_cast<Json::UInt64>(metrics.successful_requests.load());
    endpoint_data["failed_requests"] =
        static_cast<Json::UInt64>(metrics.failed_requests.load());
    endpoint_data["avg_latency_ms"] = metrics.avg_latency_ms.load();
    endpoint_data["max_latency_ms"] = metrics.max_latency_ms.load();
    endpoint_data["min_latency_ms"] = metrics.min_latency_ms.load();

    endpoints[endpoint] = endpoint_data;
  }

  root["endpoints"] = endpoints;

  auto overall = getOverallStats();
  Json::Value overall_data;
  overall_data["total_requests"] =
      static_cast<Json::UInt64>(overall.total_requests);
  overall_data["successful_requests"] =
      static_cast<Json::UInt64>(overall.successful_requests);
  overall_data["failed_requests"] =
      static_cast<Json::UInt64>(overall.failed_requests);
  overall_data["avg_latency_ms"] = overall.avg_latency_ms;
  overall_data["throughput_rps"] = overall.throughput_rps;

  root["overall"] = overall_data;

  return root;
}

std::string PerformanceMonitor::getPrometheusMetrics() const {
  std::lock_guard<std::mutex> lock(mutex_);

  std::ostringstream oss;

  if (request_metrics_.empty() && endpoint_metrics_.empty()) {
    oss << "# No metrics available yet. Metrics will appear after requests are "
           "processed.\n";
    return oss.str();
  }

  // Histogram buckets: 0.1, 0.3, 0.5, 1.0, +Inf
  const std::vector<double> buckets = {0.1, 0.3, 0.5, 1.0};

  // Group metrics by method:endpoint:status
  std::map<std::string,
           std::vector<std::pair<std::string, const RequestMetrics *>>>
      grouped;

  for (const auto &[key, metrics] : request_metrics_) {
    // Parse key: "method:endpoint:status"
    size_t pos1 = key.find(':');
    size_t pos2 = key.find(':', pos1 + 1);
    if (pos1 == std::string::npos || pos2 == std::string::npos)
      continue;

    std::string method = key.substr(0, pos1);
    std::string endpoint = key.substr(pos1 + 1, pos2 - pos1 - 1);
    std::string status = key.substr(pos2 + 1);

    std::string group_key = method + ":" + endpoint;
    grouped[group_key].push_back({status, &metrics});
  }

  // Write http_requests_total counter
  oss << "# HELP http_requests_total Total number of HTTP requests\n";
  oss << "# TYPE http_requests_total counter\n";
  for (const auto &[group_key, status_list] : grouped) {
    size_t pos = group_key.find(':');
    std::string method = group_key.substr(0, pos);
    std::string endpoint = group_key.substr(pos + 1);

    for (const auto &[status, metrics] : status_list) {
      uint64_t count = metrics->count.load();
      if (count > 0) {
        oss << "http_requests_total{method=\"" << method << "\",endpoint=\""
            << endpoint << "\",status=\"" << status << "\"} " << count << "\n";
      }
    }
  }
  oss << "\n";

  // Write http_request_duration_seconds histogram
  oss << "# HELP http_request_duration_seconds HTTP request latency\n";
  oss << "# TYPE http_request_duration_seconds histogram\n";

  for (const auto &[group_key, status_list] : grouped) {
    size_t pos = group_key.find(':');
    std::string method = group_key.substr(0, pos);
    std::string endpoint = group_key.substr(pos + 1);

    for (const auto &[status, metrics] : status_list) {
      uint64_t count = metrics->count.load();
      double total_duration = metrics->total_duration_seconds.load();

      if (count == 0)
        continue;

      // Calculate buckets - for now, use average duration to estimate
      // distribution In a production system, you'd want to track individual
      // durations
      std::vector<uint64_t> bucket_counts(buckets.size() + 1, 0);
      double avg_duration = total_duration / count;

      // Distribute requests into buckets based on average duration
      // All requests go into buckets >= average, +Inf always has all
      bool found_bucket = false;
      for (size_t i = 0; i < buckets.size(); i++) {
        if (avg_duration <= buckets[i]) {
          bucket_counts[i] = count;
          found_bucket = true;
          // Fill all smaller buckets with 0 (they're below average)
          break;
        }
      }
      // If average is above all buckets, all go to +Inf
      if (!found_bucket) {
        // Average is > 1.0, all requests in +Inf
      }
      // +Inf bucket always contains all requests
      bucket_counts[buckets.size()] = count;

      // Write buckets
      for (size_t i = 0; i < buckets.size(); i++) {
        oss << "http_request_duration_seconds_bucket{method=\"" << method
            << "\",endpoint=\"" << endpoint << "\",le=\"" << buckets[i]
            << "\"} " << bucket_counts[i] << "\n";
      }
      oss << "http_request_duration_seconds_bucket{method=\"" << method
          << "\",endpoint=\"" << endpoint << "\",le=\"+Inf\"} "
          << bucket_counts[buckets.size()] << "\n";

      // Write sum and count
      oss << "http_request_duration_seconds_sum{method=\"" << method
          << "\",endpoint=\"" << endpoint << "\"} " << std::fixed
          << std::setprecision(2) << total_duration << "\n";
      oss << "http_request_duration_seconds_count{method=\"" << method
          << "\",endpoint=\"" << endpoint << "\"} " << count << "\n";
      oss << "\n";
    }
  }

  return oss.str();
}

PerformanceMonitor::OverallStats PerformanceMonitor::getOverallStats() const {
  std::lock_guard<std::mutex> lock(mutex_);

  OverallStats stats{};
  double total_latency = 0.0;
  uint64_t total_count = 0;

  for (const auto &[endpoint, metrics] : endpoint_metrics_) {
    stats.total_requests += metrics.total_requests.load();
    stats.successful_requests += metrics.successful_requests.load();
    stats.failed_requests += metrics.failed_requests.load();

    total_latency +=
        metrics.avg_latency_ms.load() * metrics.total_requests.load();
    total_count += metrics.total_requests.load();
  }

  // ✅ Safe division: check total_count before dividing
  if (total_count > 0) {
    stats.avg_latency_ms = total_latency / static_cast<double>(total_count);
  } else {
    stats.avg_latency_ms = 0.0;
  }

  stats.throughput_rps = calculateThroughput();

  return stats;
}

double PerformanceMonitor::calculateThroughput() const {
  auto now = std::chrono::steady_clock::now();
  auto elapsed =
      std::chrono::duration_cast<std::chrono::seconds>(now - start_time_)
          .count();

  // ✅ Safe division: check elapsed before dividing
  if (elapsed <= 0) {
    return 0.0;
  }

  uint64_t total = 0;
  for (const auto &[endpoint, metrics] : endpoint_metrics_) {
    total += metrics.total_requests.load();
  }

  return static_cast<double>(total) / static_cast<double>(elapsed);
}

void PerformanceMonitor::reset() {
  std::lock_guard<std::mutex> lock(mutex_);
  endpoint_metrics_.clear();
  start_time_ = std::chrono::steady_clock::now();
}
