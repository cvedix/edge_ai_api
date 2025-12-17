#pragma once

#include "core/watchdog.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>

/**
 * @brief Health Monitor that runs on separate thread
 *
 * Monitors application health metrics and sends heartbeats to watchdog.
 * This runs independently to ensure monitoring continues even if main thread
 * has issues.
 */
class HealthMonitor {
public:
  /**
   * @brief Constructor
   * @param monitor_interval_ms Interval between health checks
   */
  HealthMonitor(uint32_t monitor_interval_ms = 1000);

  /**
   * @brief Destructor
   */
  ~HealthMonitor();

  /**
   * @brief Start the health monitoring thread
   * @param watchdog Reference to watchdog to send heartbeats to
   */
  void start(Watchdog &watchdog);

  /**
   * @brief Stop the health monitoring thread
   */
  void stop();

  /**
   * @brief Check if monitor is running
   */
  bool isRunning() const { return running_.load(); }

  /**
   * @brief Get current health metrics
   */
  struct HealthMetrics {
    double cpu_usage_percent;
    size_t memory_usage_mb;
    uint64_t request_count;
    uint64_t error_count;
    std::chrono::steady_clock::time_point last_check;
  };

  HealthMetrics getMetrics() const;

private:
  /**
   * @brief Monitoring loop running on separate thread
   */
  void monitorLoop();

  /**
   * @brief Collect health metrics
   */
  HealthMetrics collectMetrics();

  // Configuration
  uint32_t monitor_interval_ms_;

  // Thread management
  std::atomic<bool> running_;
  std::unique_ptr<std::thread> monitor_thread_;

  // Metrics
  mutable std::mutex metrics_mutex_;
  HealthMetrics metrics_;

  // Watchdog reference
  Watchdog *watchdog_;

  // Statistics
  std::atomic<uint64_t> request_count_;
  std::atomic<uint64_t> error_count_;
};
