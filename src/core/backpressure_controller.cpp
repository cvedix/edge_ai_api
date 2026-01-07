#include "core/backpressure_controller.h"
#include <algorithm>
#include <cmath>

namespace BackpressureController {

void BackpressureController::configure(const std::string &instanceId,
                                       DropPolicy policy, double max_fps,
                                       size_t max_queue_size) {
  std::unique_lock<std::shared_mutex> config_lock(config_mutex_);
  std::lock_guard<std::mutex> stats_lock(mutex_);

  InstanceConfig &config = configs_[instanceId];
  config.policy = policy;
  double clamped_fps = std::clamp(max_fps, MIN_FPS, MAX_FPS);
  config.max_fps.store(clamped_fps, std::memory_order_relaxed);
  config.max_queue_size = max_queue_size;
  int64_t interval_ms = static_cast<int64_t>(1000.0 / clamped_fps);
  config.min_frame_interval_ms.store(interval_ms, std::memory_order_relaxed);
  auto now = std::chrono::steady_clock::now();
  config.setLastFrameTime(now);

  // Initialize stats if not exists
  if (stats_.find(instanceId) == stats_.end()) {
    BackpressureStats &stats = stats_[instanceId];
    stats.target_fps.store(clamped_fps);
    stats.current_fps.store(0.0);
  }
}

bool BackpressureController::shouldDropFrame(const std::string &instanceId) {
  // OPTIMIZATION: Lock-free fast path using shared_lock for concurrent reads
  // This eliminates mutex contention in the hot path (called every frame)

  InstanceConfig *config = nullptr;
  {
    // Use shared_lock for read-only access (allows concurrent readers)
    std::shared_lock<std::shared_mutex> read_lock(config_mutex_);
    auto configIt = configs_.find(instanceId);
    if (configIt == configs_.end()) {
      return false; // Not configured, don't drop
    }
    config = &configIt->second;

    // PHASE 1: Check queue size first (queue-based dropping)
    // This allows dropping frames when queue is full even if FPS limit not
    // reached
    auto statsIt = stats_.find(instanceId);
    if (statsIt != stats_.end()) {
      size_t current_queue_size =
          statsIt->second.current_queue_size.load(std::memory_order_relaxed);
      size_t max_queue_size = config->max_queue_size;

      // Drop frame if queue is >= 80% full (prevent queue overflow)
      // This allows keeping high FPS but dropping frames when queue gets full
      const double queue_drop_threshold = 0.8; // 80% of max queue size
      size_t drop_threshold =
          static_cast<size_t>(max_queue_size * queue_drop_threshold);

      if (current_queue_size >= drop_threshold) {
        // Queue is getting full - drop this frame to prevent overflow
        return true;
      }
    }

    // PHASE 2: Check FPS limiting (time-based dropping)
    // Read atomic values while holding shared lock (safe)
    // The lock ensures config pointer is valid
    auto now = std::chrono::steady_clock::now();
    auto last_frame_time = config->getLastFrameTime();
    int64_t min_interval_ms =
        config->min_frame_interval_ms.load(std::memory_order_relaxed);

    // Check FPS limiting using atomic values
    auto time_since_last =
        std::chrono::duration_cast<std::chrono::milliseconds>(now -
                                                              last_frame_time);

    if (time_since_last.count() < min_interval_ms) {
      // Frame too soon - drop based on policy
      // For DROP_NEWEST (default), drop this new frame
      return true;
    }

    // Update last frame time atomically (lock-free, safe even after lock
    // release)
    config->setLastFrameTime(now);
  }
  // Lock released - atomic operations are safe without lock

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

  // Update adaptive FPS periodically (but not on every frame to avoid lock
  // contention) Only update every N frames or use try_lock to avoid blocking
  static thread_local std::unordered_map<std::string, uint64_t>
      adaptive_update_counter;
  auto &counter = adaptive_update_counter[instanceId];
  counter++;

  // Only call updateAdaptiveFPS every 60 frames (~1 second at 60 FPS, ~0.5s at
  // 120 FPS) This reduces lock contention significantly while maintaining
  // responsiveness Increased from 30 to 60 to handle higher FPS better
  if (counter >= 60) {
    counter = 0;
    updateAdaptiveFPS(instanceId);
  }
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

void BackpressureController::updateQueueSize(const std::string &instanceId,
                                             size_t queue_size) {
  auto &stats = stats_[instanceId];
  stats.current_queue_size.store(queue_size, std::memory_order_relaxed);
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
    snapshot.current_queue_size =
        stats.current_queue_size.load(std::memory_order_relaxed);
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
  // Use try_lock to avoid blocking if another thread is updating
  // This prevents lock contention from blocking frame processing
  std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
  if (!lock.owns_lock()) {
    // Another thread is updating, skip this update
    return;
  }

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

    // Update config atomically (lock-free for hot path reads)
    config.max_fps.store(new_target, std::memory_order_relaxed);
    int64_t interval_ms = static_cast<int64_t>(1000.0 / new_target);
    config.min_frame_interval_ms.store(interval_ms, std::memory_order_relaxed);

    // Reset backpressure flag after reducing FPS
    stats.backpressure_detected.store(false, std::memory_order_relaxed);
  } else {
    // No backpressure - gradually increase FPS
    double new_target = current_target * FPS_INCREASE_FACTOR;
    double max_fps_value = config.max_fps.load(std::memory_order_relaxed);
    new_target = std::min(new_target, max_fps_value);
    new_target = std::min(new_target, MAX_FPS);

    if (new_target > current_target) {
      stats.target_fps.store(new_target, std::memory_order_relaxed);
      config.max_fps.store(new_target, std::memory_order_relaxed);
      int64_t interval_ms = static_cast<int64_t>(1000.0 / new_target);
      config.min_frame_interval_ms.store(interval_ms,
                                         std::memory_order_relaxed);
    }
  }
}

void BackpressureController::updateQueueSizeConfig(
    const std::string &instanceId, size_t new_queue_size) {
  std::unique_lock<std::shared_mutex> config_lock(config_mutex_);
  auto configIt = configs_.find(instanceId);
  if (configIt != configs_.end()) {
    configIt->second.max_queue_size = new_queue_size;
  }
}

size_t
BackpressureController::getMaxQueueSize(const std::string &instanceId) const {
  std::shared_lock<std::shared_mutex> read_lock(config_mutex_);
  auto configIt = configs_.find(instanceId);
  if (configIt != configs_.end()) {
    return configIt->second.max_queue_size;
  }
  return 20; // Default
}

} // namespace BackpressureController
