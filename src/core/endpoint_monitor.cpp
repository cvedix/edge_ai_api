#include "core/endpoint_monitor.h"
#include <algorithm>

EndpointMonitor &EndpointMonitor::getInstance() {
  static EndpointMonitor instance;
  return instance;
}

void EndpointMonitor::recordRequest(const std::string &endpoint,
                                    uint64_t response_time_ms, bool is_error) {
  std::lock_guard<std::mutex> lock(stats_mutex_);

  // Get or create stats for endpoint
  auto it = endpoint_stats_.find(endpoint);
  if (it == endpoint_stats_.end()) {
    it = endpoint_stats_.emplace(endpoint, std::make_shared<EndpointStats>())
             .first;
  }

  auto &stats = it->second;

  // Update statistics
  stats->request_count++;
  if (is_error) {
    stats->error_count++;
  }

  stats->total_response_time_ms += response_time_ms;

  uint64_t current_max = stats->max_response_time_ms.load();
  while (response_time_ms > current_max &&
         !stats->max_response_time_ms.compare_exchange_weak(current_max,
                                                            response_time_ms)) {
    current_max = stats->max_response_time_ms.load();
  }

  uint64_t current_min = stats->min_response_time_ms.load();
  while (response_time_ms < current_max &&
         !stats->min_response_time_ms.compare_exchange_weak(current_min,
                                                            response_time_ms)) {
    current_min = stats->min_response_time_ms.load();
  }

  stats->last_request_time = std::chrono::steady_clock::now();

  // Update health status
  uint64_t avg_response_time = stats->total_response_time_ms.load() /
                               std::max(1UL, stats->request_count.load());
  double error_rate = static_cast<double>(stats->error_count.load()) /
                      std::max(1UL, stats->request_count.load());

  bool healthy = (avg_response_time < 1000) && (error_rate < 0.1);
  stats->is_healthy.store(healthy);
}

std::shared_ptr<EndpointMonitor::EndpointStats>
EndpointMonitor::getStats(const std::string &endpoint) const {
  std::lock_guard<std::mutex> lock(stats_mutex_);
  auto it = endpoint_stats_.find(endpoint);
  if (it != endpoint_stats_.end()) {
    return it->second;
  }
  return nullptr;
}

std::map<std::string, std::shared_ptr<EndpointMonitor::EndpointStats>>
EndpointMonitor::getAllStats() const {
  std::lock_guard<std::mutex> lock(stats_mutex_);
  return endpoint_stats_;
}

bool EndpointMonitor::isEndpointHealthy(const std::string &endpoint,
                                        uint64_t max_avg_response_time_ms,
                                        double max_error_rate) const {
  auto stats = getStats(endpoint);
  if (!stats || stats->request_count.load() == 0) {
    return true; // No data yet, assume healthy
  }

  // ✅ Safe division: request_count is already checked to be > 0 above
  uint64_t request_count = stats->request_count.load();
  if (request_count == 0) {
    return true; // Double-check to prevent division by zero
  }

  uint64_t avg_response_time =
      stats->total_response_time_ms.load() / request_count;
  double error_rate = static_cast<double>(stats->error_count.load()) /
                      static_cast<double>(request_count);

  return (avg_response_time <= max_avg_response_time_ms) &&
         (error_rate <= max_error_rate);
}

void EndpointMonitor::resetStats(const std::string &endpoint) {
  std::lock_guard<std::mutex> lock(stats_mutex_);
  auto it = endpoint_stats_.find(endpoint);
  if (it != endpoint_stats_.end()) {
    auto stats = it->second;
    stats->request_count.store(0);
    stats->error_count.store(0);
    stats->total_response_time_ms.store(0);
    stats->max_response_time_ms.store(0);
    stats->min_response_time_ms.store(UINT64_MAX);
    stats->is_healthy.store(true);
  }
}

void EndpointMonitor::resetAllStats() {
  std::lock_guard<std::mutex> lock(stats_mutex_);
  for (auto &pair : endpoint_stats_) {
    auto stats = pair.second;
    stats->request_count.store(0);
    stats->error_count.store(0);
    stats->total_response_time_ms.store(0);
    stats->max_response_time_ms.store(0);
    stats->min_response_time_ms.store(UINT64_MAX);
    stats->is_healthy.store(true);
  }
}
