#include "core/adaptive_queue_size_manager.h"
#include <algorithm>
#include <cmath>

namespace AdaptiveQueueSize {

void AdaptiveQueueSizeManager::initialize(size_t min_queue_size,
                                          size_t max_queue_size,
                                          size_t default_queue_size) {
  min_queue_size_ = min_queue_size;
  max_queue_size_ = max_queue_size;
  default_queue_size_ = default_queue_size;
}

size_t AdaptiveQueueSizeManager::getRecommendedQueueSize(
    const std::string &instanceId) {
  if (!enabled_.load()) {
    return default_queue_size_;
  }

  // Check if we have metrics for this instance
  {
    std::lock_guard<std::mutex> lock(instances_mutex_);
    auto it = current_queue_sizes_.find(instanceId);
    if (it != current_queue_sizes_.end()) {
      // Return current size if exists
      return it->second;
    }
  }

  // If no metrics available yet, return default to ensure backward
  // compatibility This ensures new instances start with default queue size
  // until metrics are available
  {
    std::lock_guard<std::mutex> lock(system_metrics_mutex_);
    // Check if system metrics have been initialized (not all zeros)
    if (system_metrics_.memory_usage_percent == 0.0 &&
        system_metrics_.available_memory_mb == 0 &&
        system_metrics_.active_instances == 0) {
      // No system metrics yet - return default to maintain backward
      // compatibility
      return default_queue_size_;
    }
  }

  // Calculate new queue size
  size_t recommended = calculateQueueSize(instanceId);

  // Store and return
  {
    std::lock_guard<std::mutex> lock(instances_mutex_);
    current_queue_sizes_[instanceId] = recommended;
  }

  return recommended;
}

void AdaptiveQueueSizeManager::updateSystemMetrics(
    const SystemMetrics &metrics) {
  std::lock_guard<std::mutex> lock(system_metrics_mutex_);
  system_metrics_ = metrics;
}

void AdaptiveQueueSizeManager::updateInstanceMetrics(
    const std::string &instanceId, const InstanceMetrics &metrics) {
  std::lock_guard<std::mutex> lock(instances_mutex_);
  instance_metrics_[instanceId] = metrics;
  instance_metrics_[instanceId].last_update = std::chrono::steady_clock::now();

  // Recalculate queue size when metrics update
  size_t new_size = calculateQueueSize(instanceId);
  current_queue_sizes_[instanceId] = new_size;
}

size_t AdaptiveQueueSizeManager::getCurrentQueueSize(
    const std::string &instanceId) const {
  std::lock_guard<std::mutex> lock(instances_mutex_);
  auto it = current_queue_sizes_.find(instanceId);
  if (it != current_queue_sizes_.end()) {
    return it->second;
  }
  return default_queue_size_;
}

void AdaptiveQueueSizeManager::resetInstance(const std::string &instanceId) {
  std::lock_guard<std::mutex> lock(instances_mutex_);
  instance_metrics_.erase(instanceId);
  current_queue_sizes_.erase(instanceId);
}

size_t
AdaptiveQueueSizeManager::calculateQueueSize(const std::string &instanceId) {
  // Start with current queue size or default
  size_t current_size = default_queue_size_;
  {
    std::lock_guard<std::mutex> lock(instances_mutex_);
    auto it = current_queue_sizes_.find(instanceId);
    if (it != current_queue_sizes_.end()) {
      current_size = it->second;
    }
  }

  // Get system metrics
  SystemMetrics sys_metrics;
  {
    std::lock_guard<std::mutex> lock(system_metrics_mutex_);
    sys_metrics = system_metrics_;
  }

  // Get instance metrics
  InstanceMetrics inst_metrics;
  {
    std::lock_guard<std::mutex> lock(instances_mutex_);
    auto it = instance_metrics_.find(instanceId);
    if (it != instance_metrics_.end()) {
      inst_metrics = it->second;
    }
  }

  // Calculate adjustment based on multiple factors
  double adjustment_factor = 1.0;

  // Factor 1: Memory pressure
  if (sys_metrics.memory_usage_percent > MEMORY_HIGH_THRESHOLD) {
    // High memory pressure - reduce queue size significantly
    adjustment_factor *= 0.7; // Reduce by 30%
  } else if (sys_metrics.memory_usage_percent > MEMORY_MEDIUM_THRESHOLD) {
    // Medium memory pressure - reduce queue size moderately
    adjustment_factor *= 0.85; // Reduce by 15%
  }

  // Factor 2: Latency
  if (inst_metrics.current_latency_ms > LATENCY_HIGH_THRESHOLD) {
    // High latency - reduce queue size
    adjustment_factor *= 0.8; // Reduce by 20%
  } else if (inst_metrics.current_latency_ms > LATENCY_MEDIUM_THRESHOLD) {
    // Medium latency - reduce queue size slightly
    adjustment_factor *= 0.9; // Reduce by 10%
  }

  // Factor 3: Queue full frequency
  if (inst_metrics.queue_full_frequency > QUEUE_FULL_FREQUENCY_THRESHOLD) {
    // Frequent queue full events - increase queue size
    adjustment_factor *= 1.15; // Increase by 15%
  }

  // Factor 4: Processing speed vs source FPS
  if (inst_metrics.source_fps > 0 && inst_metrics.processing_fps > 0) {
    double processing_ratio =
        inst_metrics.processing_fps / inst_metrics.source_fps;
    if (processing_ratio < PROCESSING_SLOW_THRESHOLD) {
      // Processing slower than source - reduce queue size to prevent backlog
      adjustment_factor *= 0.85; // Reduce by 15%
    } else if (processing_ratio > 1.1) {
      // Processing faster than source - can increase queue size
      adjustment_factor *= 1.1; // Increase by 10%
    }
  }

  // Factor 5: Number of active instances (memory sharing)
  if (sys_metrics.active_instances > 10) {
    // Many instances - reduce queue size per instance
    double per_instance_factor =
        1.0 / (1.0 + (sys_metrics.active_instances - 10) * 0.05);
    adjustment_factor *= per_instance_factor;
  }

  // Calculate new size
  size_t new_size =
      static_cast<size_t>(std::round(current_size * adjustment_factor));

  // Clamp to valid range
  new_size = std::clamp(new_size, min_queue_size_, max_queue_size_);

  // Ensure minimum adjustment step to avoid thrashing
  size_t diff = (new_size > current_size) ? (new_size - current_size)
                                          : (current_size - new_size);
  if (diff < MIN_ADJUSTMENT && diff > 0) {
    // Too small change - keep current size or make minimum adjustment
    if (adjustment_factor < 1.0) {
      new_size = current_size - MIN_ADJUSTMENT;
    } else if (adjustment_factor > 1.0) {
      new_size = current_size + MIN_ADJUSTMENT;
    } else {
      new_size = current_size;
    }
    new_size = std::clamp(new_size, min_queue_size_, max_queue_size_);
  }

  return new_size;
}

} // namespace AdaptiveQueueSize
