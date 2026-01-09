/**
 * @file adaptive_queue_size_manager.h
 * @brief Adaptive Queue Size Manager - Dynamic queue sizing based on system
 * status
 *
 * Automatically adjusts queue size per instance based on:
 * - System memory usage
 * - Processing latency
 * - Queue full events frequency
 * - Processing speed vs source FPS
 */

#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

namespace AdaptiveQueueSize {

/**
 * @brief System status metrics for queue size adjustment
 */
struct SystemMetrics {
  double memory_usage_percent{0.0}; // System memory usage percentage
  size_t available_memory_mb{0};    // Available memory in MB
  double cpu_usage_percent{0.0};    // CPU usage percentage
  size_t active_instances{0};       // Number of active instances
};

/**
 * @brief Instance-specific metrics for queue size adjustment
 */
struct InstanceMetrics {
  double current_latency_ms{0.0};   // Average processing latency
  double queue_full_frequency{0.0}; // Queue full events per second
  double processing_fps{0.0};       // Actual processing FPS
  double source_fps{0.0};           // Source FPS
  size_t current_queue_size{0};     // Current queue size
  std::chrono::steady_clock::time_point last_update;
};

/**
 * @brief Adaptive Queue Size Manager
 *
 * Dynamically adjusts queue size for each instance based on system status
 * and instance metrics to balance between:
 * - Memory usage (reduce when memory pressure)
 * - Latency (reduce when latency too high)
 * - Throughput (increase when processing can handle more)
 */
class AdaptiveQueueSizeManager {
public:
  static AdaptiveQueueSizeManager &getInstance() {
    static AdaptiveQueueSizeManager instance;
    return instance;
  }

  /**
   * @brief Initialize with default queue size range
   * @param min_queue_size Minimum queue size (default: 5)
   * @param max_queue_size Maximum queue size (default: 30)
   * @param default_queue_size Default queue size (default: 20)
   */
  void initialize(size_t min_queue_size = 5, size_t max_queue_size = 30,
                  size_t default_queue_size = 20);

  /**
   * @brief Get recommended queue size for an instance
   * @param instanceId Instance ID
   * @return Recommended queue size
   */
  size_t getRecommendedQueueSize(const std::string &instanceId);

  /**
   * @brief Update system metrics (called periodically)
   * @param metrics System metrics
   */
  void updateSystemMetrics(const SystemMetrics &metrics);

  /**
   * @brief Update instance metrics
   * @param instanceId Instance ID
   * @param metrics Instance metrics
   */
  void updateInstanceMetrics(const std::string &instanceId,
                             const InstanceMetrics &metrics);

  /**
   * @brief Get current queue size for instance
   * @param instanceId Instance ID
   * @return Current queue size
   */
  size_t getCurrentQueueSize(const std::string &instanceId) const;

  /**
   * @brief Enable/disable adaptive queue sizing
   * @param enabled Enable adaptive sizing
   */
  void setEnabled(bool enabled) { enabled_.store(enabled); }

  /**
   * @brief Check if adaptive sizing is enabled
   */
  bool isEnabled() const { return enabled_.load(); }

  /**
   * @brief Reset instance metrics
   * @param instanceId Instance ID
   */
  void resetInstance(const std::string &instanceId);

private:
  AdaptiveQueueSizeManager() = default;
  ~AdaptiveQueueSizeManager() = default;
  AdaptiveQueueSizeManager(const AdaptiveQueueSizeManager &) = delete;
  AdaptiveQueueSizeManager &
  operator=(const AdaptiveQueueSizeManager &) = delete;

  /**
   * @brief Calculate recommended queue size based on metrics
   * @param instanceId Instance ID
   * @return Recommended queue size
   */
  size_t calculateQueueSize(const std::string &instanceId);

  // Configuration
  std::atomic<bool> enabled_{true};
  size_t min_queue_size_{5};
  size_t max_queue_size_{30};
  size_t default_queue_size_{20};

  // System metrics (updated periodically)
  mutable std::mutex system_metrics_mutex_;
  SystemMetrics system_metrics_;

  // Instance metrics and queue sizes
  mutable std::mutex instances_mutex_;
  std::unordered_map<std::string, InstanceMetrics> instance_metrics_;
  std::unordered_map<std::string, size_t> current_queue_sizes_;

  // Thresholds for adjustment
  static constexpr double MEMORY_HIGH_THRESHOLD = 80.0;     // 80% memory usage
  static constexpr double MEMORY_MEDIUM_THRESHOLD = 60.0;   // 60% memory usage
  static constexpr double LATENCY_HIGH_THRESHOLD = 500.0;   // 500ms latency
  static constexpr double LATENCY_MEDIUM_THRESHOLD = 300.0; // 300ms latency
  static constexpr double QUEUE_FULL_FREQUENCY_THRESHOLD = 5.0; // 5 events/sec
  static constexpr double PROCESSING_SLOW_THRESHOLD = 0.8; // 80% of source FPS

  // Adjustment factors
  static constexpr double REDUCE_FACTOR = 0.8;   // Reduce by 20%
  static constexpr double INCREASE_FACTOR = 1.2; // Increase by 20%
  static constexpr size_t MIN_ADJUSTMENT = 1;    // Minimum adjustment step
};

} // namespace AdaptiveQueueSize
