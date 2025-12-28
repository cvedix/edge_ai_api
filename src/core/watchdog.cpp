#include "core/watchdog.h"
#include <chrono>
#include <future>
#include <iostream>

Watchdog::Watchdog(uint32_t check_interval_ms, uint32_t timeout_ms)
    : check_interval_ms_(check_interval_ms), timeout_ms_(timeout_ms),
      running_(false), last_heartbeat_(std::chrono::steady_clock::now()),
      heartbeat_received_(false) {
  stats_.total_heartbeats = 0;
  stats_.missed_heartbeats = 0;
  stats_.recovery_actions = 0;
  stats_.last_heartbeat = last_heartbeat_;
  stats_.is_healthy = true;
}

Watchdog::~Watchdog() { stop(); }

void Watchdog::start(RecoveryCallback recovery_callback) {
  if (running_.load()) {
    std::cerr << "[Watchdog] Already running" << std::endl;
    return;
  }

  recovery_callback_ = recovery_callback;
  running_.store(true);
  heartbeat_received_.store(true);
  last_heartbeat_ = std::chrono::steady_clock::now();

  watchdog_thread_ =
      std::make_unique<std::thread>(&Watchdog::watchdogLoop, this);

  std::cout << "[Watchdog] Started (check_interval=" << check_interval_ms_
            << "ms, timeout=" << timeout_ms_ << "ms)" << std::endl;
}

void Watchdog::stop() {
  if (!running_.load()) {
    return;
  }

  running_.store(false);
  cv_.notify_all();

  if (watchdog_thread_ && watchdog_thread_->joinable()) {
    // Use timeout to avoid blocking indefinitely during shutdown
    auto future =
        std::async(std::launch::async, [this]() { watchdog_thread_->join(); });

    if (future.wait_for(std::chrono::seconds(1)) ==
        std::future_status::timeout) {
      std::cerr << "[Watchdog] Warning: Thread join timeout, detaching..."
                << std::endl;
      watchdog_thread_->detach();
    }
  }

  std::cout << "[Watchdog] Stopped" << std::endl;
}

void Watchdog::heartbeat() {
  std::lock_guard<std::mutex> lock(heartbeat_mutex_);
  last_heartbeat_ = std::chrono::steady_clock::now();
  heartbeat_received_.store(true);

  {
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    stats_.total_heartbeats++;
    stats_.last_heartbeat = last_heartbeat_;
  }
}

void Watchdog::watchdogLoop() {
  std::cout << "[Watchdog] Monitoring thread started" << std::endl;

  while (running_.load()) {
    std::unique_lock<std::mutex> lock(cv_mutex_);

    // Wait for check interval or until notified to stop
    if (cv_.wait_for(lock, std::chrono::milliseconds(check_interval_ms_),
                     [this] { return !running_.load(); })) {
      // Notified to stop
      break;
    }

    // Check health
    if (!checkHealth()) {
      std::cerr << "[Watchdog] WARNING: Application appears to be unresponsive!"
                << std::endl;

      {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.is_healthy = false;
        stats_.missed_heartbeats++;
      }

      // Trigger recovery action if callback is set
      if (recovery_callback_) {
        std::cerr << "[Watchdog] Triggering recovery action..." << std::endl;
        try {
          recovery_callback_();
          {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.recovery_actions++;
          }
        } catch (const std::exception &e) {
          std::cerr << "[Watchdog] Recovery action failed: " << e.what()
                    << std::endl;
        }
      } else {
        std::cerr
            << "[Watchdog] No recovery callback set. Application may be dead."
            << std::endl;
      }
    } else {
      {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.is_healthy = true;
      }
    }
  }

  std::cout << "[Watchdog] Monitoring thread stopped" << std::endl;
}

bool Watchdog::checkHealth() {
  std::lock_guard<std::mutex> lock(heartbeat_mutex_);

  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                     now - last_heartbeat_)
                     .count();

  // Check if heartbeat was received and is within timeout
  bool received = heartbeat_received_.load();
  bool within_timeout = elapsed < static_cast<int64_t>(timeout_ms_);

  if (received && within_timeout) {
    // Reset heartbeat flag for next check
    heartbeat_received_.store(false);
    return true;
  }

  return false;
}

std::chrono::steady_clock::time_point Watchdog::getLastHeartbeat() const {
  std::lock_guard<std::mutex> lock(heartbeat_mutex_);
  return last_heartbeat_;
}

Watchdog::Stats Watchdog::getStats() const {
  std::lock_guard<std::mutex> lock(stats_mutex_);
  return stats_;
}
