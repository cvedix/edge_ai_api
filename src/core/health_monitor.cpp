#include "core/health_monitor.h"
#include <fstream>
#include <future>
#include <iostream>
#include <sstream>
#include <sys/resource.h>
#include <unistd.h>

HealthMonitor::HealthMonitor(uint32_t monitor_interval_ms)
    : monitor_interval_ms_(monitor_interval_ms), running_(false),
      watchdog_(nullptr), request_count_(0), error_count_(0) {
  metrics_.cpu_usage_percent = 0.0;
  metrics_.memory_usage_mb = 0;
  metrics_.request_count = 0;
  metrics_.error_count = 0;
  metrics_.last_check = std::chrono::steady_clock::now();
}

HealthMonitor::~HealthMonitor() { stop(); }

void HealthMonitor::start(Watchdog &watchdog) {
  if (running_.load()) {
    std::cerr << "[HealthMonitor] Already running" << std::endl;
    return;
  }

  watchdog_ = &watchdog;
  running_.store(true);
  monitor_thread_ =
      std::make_unique<std::thread>(&HealthMonitor::monitorLoop, this);

  std::cout << "[HealthMonitor] Started (interval=" << monitor_interval_ms_
            << "ms)" << std::endl;
}

void HealthMonitor::stop() {
  if (!running_.load()) {
    return;
  }

  running_.store(false);

  if (monitor_thread_ && monitor_thread_->joinable()) {
    // Use timeout to avoid blocking indefinitely during shutdown
    auto future =
        std::async(std::launch::async, [this]() { monitor_thread_->join(); });

    if (future.wait_for(std::chrono::seconds(1)) ==
        std::future_status::timeout) {
      std::cerr << "[HealthMonitor] Warning: Thread join timeout, detaching..."
                << std::endl;
      monitor_thread_->detach();
    }
  }

  std::cout << "[HealthMonitor] Stopped" << std::endl;
}

void HealthMonitor::monitorLoop() {
  std::cout << "[HealthMonitor] Monitoring thread started" << std::endl;

  while (running_.load()) {
    // Collect metrics
    HealthMetrics metrics = collectMetrics();

    // Update stored metrics
    {
      std::lock_guard<std::mutex> lock(metrics_mutex_);
      metrics_ = metrics;
    }

    // Send heartbeat to watchdog
    if (watchdog_) {
      watchdog_->heartbeat();
    }

    // Sleep until next check
    std::this_thread::sleep_for(
        std::chrono::milliseconds(monitor_interval_ms_));
  }

  std::cout << "[HealthMonitor] Monitoring thread stopped" << std::endl;
}

HealthMonitor::HealthMetrics HealthMonitor::collectMetrics() {
  HealthMetrics metrics;

  // Get memory usage from /proc/self/status
  try {
    std::ifstream status_file("/proc/self/status");
    std::string line;
    while (std::getline(status_file, line)) {
      if (line.find("VmRSS:") == 0) {
        std::istringstream iss(line);
        std::string key;
        size_t value;
        std::string unit;
        iss >> key >> value >> unit;
        metrics.memory_usage_mb = value / 1024; // Convert from KB to MB
        break;
      }
    }
  } catch (...) {
    // If we can't read /proc, use getrusage as fallback
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
      metrics.memory_usage_mb = usage.ru_maxrss / 1024; // Convert from KB to MB
    }
  }

  // Get CPU usage (simplified - in production, calculate over time)
  // For now, we'll just set a placeholder
  metrics.cpu_usage_percent =
      0.0; // TODO: Implement proper CPU usage calculation

  // Get request/error counts
  metrics.request_count = request_count_.load();
  metrics.error_count = error_count_.load();
  metrics.last_check = std::chrono::steady_clock::now();

  return metrics;
}

HealthMonitor::HealthMetrics HealthMonitor::getMetrics() const {
  std::lock_guard<std::mutex> lock(metrics_mutex_);
  return metrics_;
}
