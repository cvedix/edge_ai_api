#include "core/backpressure_controller.h"
#include <algorithm>
#include <cmath>

namespace BackpressureController {

void BackpressureController::configure(const std::string &instanceId,
                                       DropPolicy policy, double max_fps,
                                       size_t max_queue_size) {
  std::lock_guard<std::mutex> lock(mutex_);

  InstanceConfig &config = configs_[instanceId];
  config.policy = policy;
  config.max_fps = std::clamp(max_fps, MIN_FPS, MAX_FPS);
  config.max_queue_size = max_queue_size;
  config.min_frame_interval =
      std::chrono::milliseconds(static_cast<int>(1000.0 / config.max_fps));
  config.last_frame_time = std::chrono::steady_clock::now();

  // Initialize stats if not exists
  if (stats_.find(instanceId) == stats_.end()) {
    BackpressureStats &stats = stats_[instanceId];
    stats.target_fps.store(config.max_fps);
    stats.current_fps.store(0.0);
  }
}

bool BackpressureController::shouldDropFrame(const std::string &instanceId) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto configIt = configs_.find(instanceId);
  if (configIt == configs_.end()) {
    return false; // Not configured, don't drop
  }

  InstanceConfig &config = configIt->second;
  auto now = std::chrono::steady_clock::now();

  // Check FPS limiting
  auto time_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - config.last_frame_time);

  if (time_since_last < config.min_frame_interval) {
    // Frame too soon - drop based on policy
    if (config.policy == DropPolicy::DROP_NEWEST) {
      return true; // Drop this new frame
    }
    // For DROP_OLDEST, we'd need queue access, but we don't have it here
    // So we'll use DROP_NEWEST behavior
    return true;
  }

  // Update last frame time
  config.last_frame_time = now;
  return false;
}

void BackpressureController::recordFrameProcessed(
    const std::string &instanceId) {
  auto &stats = stats_[instanceId];
  stats.frames_processed.fetch_add(1, std::memory_order_relaxed);

  auto now = std::chrono::steady_clock::now();
  stats.last_processed_time = now;

  // Update current FPS (simple moving average)
  // Calculate FPS based on time since last processed frame
  static thread_local std::unordered_map<std::string,
                                         std::chrono::steady_clock::time_point>
      last_fps_update;
  static thread_local std::unordered_map<std::string, uint64_t>
      frame_count_since_update;

  auto &last_update = last_fps_update[instanceId];
  auto &frame_count = frame_count_since_update[instanceId];

  frame_count++;
  auto elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update)
          .count();

  if (elapsed >= 1000) { // Update FPS every second
    double fps = (frame_count * 1000.0) / elapsed;
    stats.current_fps.store(std::round(fps), std::memory_order_relaxed);
    frame_count = 0;
    last_update = now;
  }

  // Update adaptive FPS periodically
  updateAdaptiveFPS(instanceId);
}

void BackpressureController::recordFrameDropped(const std::string &instanceId) {
  auto &stats = stats_[instanceId];
  stats.frames_dropped.fetch_add(1, std::memory_order_relaxed);
  stats.last_drop_time = std::chrono::steady_clock::now();
}

void BackpressureController::recordQueueFull(const std::string &instanceId) {
  auto &stats = stats_[instanceId];
  stats.queue_full_count.fetch_add(1, std::memory_order_relaxed);
  stats.backpressure_detected.store(true, std::memory_order_relaxed);

  // Trigger adaptive FPS reduction
  updateAdaptiveFPS(instanceId);
}

double
BackpressureController::getCurrentFPS(const std::string &instanceId) const {
  auto it = stats_.find(instanceId);
  if (it != stats_.end()) {
    return it->second.current_fps.load(std::memory_order_relaxed);
  }
  return 0.0;
}

double
BackpressureController::getTargetFPS(const std::string &instanceId) const {
  auto it = stats_.find(instanceId);
  if (it != stats_.end()) {
    return it->second.target_fps.load(std::memory_order_relaxed);
  }
  return 30.0; // Default
}

bool BackpressureController::isBackpressureDetected(
    const std::string &instanceId) const {
  auto it = stats_.find(instanceId);
  if (it != stats_.end()) {
    return it->second.backpressure_detected.load(std::memory_order_relaxed);
  }
  return false;
}

BackpressureController::BackpressureStatsSnapshot
BackpressureController::getStats(const std::string &instanceId) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = stats_.find(instanceId);
  if (it != stats_.end()) {
    const BackpressureStats &stats = it->second;
    BackpressureStatsSnapshot snapshot;
    snapshot.frames_dropped =
        stats.frames_dropped.load(std::memory_order_relaxed);
    snapshot.frames_processed =
        stats.frames_processed.load(std::memory_order_relaxed);
    snapshot.queue_full_count =
        stats.queue_full_count.load(std::memory_order_relaxed);
    snapshot.current_fps = stats.current_fps.load(std::memory_order_relaxed);
    snapshot.target_fps = stats.target_fps.load(std::memory_order_relaxed);
    snapshot.backpressure_detected =
        stats.backpressure_detected.load(std::memory_order_relaxed);
    snapshot.last_drop_time = stats.last_drop_time;
    snapshot.last_processed_time = stats.last_processed_time;
    return snapshot;
  }
  return BackpressureStatsSnapshot{};
}

void BackpressureController::resetStats(const std::string &instanceId) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = stats_.find(instanceId);
  if (it != stats_.end()) {
    BackpressureStats &stats = it->second;
    stats.frames_dropped.store(0);
    stats.frames_processed.store(0);
    stats.queue_full_count.store(0);
    stats.current_fps.store(0.0);
    stats.backpressure_detected.store(false);
  }
}

void BackpressureController::updateAdaptiveFPS(const std::string &instanceId) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto configIt = configs_.find(instanceId);
  if (configIt == configs_.end()) {
    return; // Not configured
  }

  auto statsIt = stats_.find(instanceId);
  if (statsIt == stats_.end()) {
    return;
  }

  InstanceConfig &config = configIt->second;
  BackpressureStats &stats = statsIt->second;

  // Only update adaptive FPS if policy is ADAPTIVE_FPS
  if (config.policy != DropPolicy::ADAPTIVE_FPS) {
    return;
  }

  static thread_local std::unordered_map<std::string,
                                         std::chrono::steady_clock::time_point>
      last_adaptive_update;
  auto now = std::chrono::steady_clock::now();
  auto &last_update = last_adaptive_update[instanceId];

  auto elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update);
  if (elapsed < ADAPTIVE_UPDATE_INTERVAL) {
    return; // Too soon to update
  }

  last_update = now;

  double current_target = stats.target_fps.load(std::memory_order_relaxed);
  bool backpressure =
      stats.backpressure_detected.load(std::memory_order_relaxed);
  uint64_t queue_full = stats.queue_full_count.load(std::memory_order_relaxed);

  // If backpressure detected or queue full events, reduce FPS
  if (backpressure || queue_full > 0) {
    double new_target = current_target * FPS_REDUCTION_FACTOR;
    new_target = std::max(new_target, MIN_FPS);
    stats.target_fps.store(new_target, std::memory_order_relaxed);

    // Update config
    config.max_fps = new_target;
    config.min_frame_interval =
        std::chrono::milliseconds(static_cast<int>(1000.0 / new_target));

    // Reset backpressure flag after reducing FPS
    stats.backpressure_detected.store(false, std::memory_order_relaxed);
  } else {
    // No backpressure - gradually increase FPS
    double new_target = current_target * FPS_INCREASE_FACTOR;
    new_target = std::min(new_target, config.max_fps);
    new_target = std::min(new_target, MAX_FPS);

    if (new_target > current_target) {
      stats.target_fps.store(new_target, std::memory_order_relaxed);
      config.max_fps = new_target;
      config.min_frame_interval =
          std::chrono::milliseconds(static_cast<int>(1000.0 / new_target));
    }
  }
}

} // namespace BackpressureController
