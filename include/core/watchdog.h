#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

/**
 * @brief Watchdog class for monitoring application health
 *
 * Runs on a separate thread to monitor the application and detect
 * crashes/hangs. If the application becomes unresponsive, the watchdog can
 * trigger recovery actions.
 */
class Watchdog {
public:
  /**
   * @brief Callback function type for recovery actions
   */
  using RecoveryCallback = std::function<void()>;

  /**
   * @brief Constructor
   * @param check_interval_ms Interval between health checks in milliseconds
   * @param timeout_ms Maximum time without heartbeat before considering dead
   */
  Watchdog(uint32_t check_interval_ms = 5000, uint32_t timeout_ms = 30000);

  /**
   * @brief Destructor - stops the watchdog
   */
  ~Watchdog();

  /**
   * @brief Start the watchdog thread
   * @param recovery_callback Function to call when application is detected as
   * dead
   */
  void start(RecoveryCallback recovery_callback = nullptr);

  /**
   * @brief Stop the watchdog thread
   */
  void stop();

  /**
   * @brief Send a heartbeat to indicate the application is alive
   * This should be called periodically by the main application
   */
  void heartbeat();

  /**
   * @brief Check if the watchdog is running
   */
  bool isRunning() const { return running_.load(); }

  /**
   * @brief Get the last heartbeat time
   */
  std::chrono::steady_clock::time_point getLastHeartbeat() const;

  /**
   * @brief Get watchdog statistics
   */
  struct Stats {
    uint64_t total_heartbeats;
    uint64_t missed_heartbeats;
    uint64_t recovery_actions;
    std::chrono::steady_clock::time_point last_heartbeat;
    bool is_healthy;
  };

  Stats getStats() const;

protected:
  /**
   * @brief Check if application is still alive
   * Made protected so derived classes can override
   */
  virtual bool checkHealth();

private:
  /**
   * @brief Main watchdog loop running on separate thread
   */
  void watchdogLoop();

  // Configuration
  uint32_t check_interval_ms_;
  uint32_t timeout_ms_;

  // Thread management
  std::atomic<bool> running_;
  std::unique_ptr<std::thread> watchdog_thread_;

  // Heartbeat tracking
  mutable std::mutex heartbeat_mutex_;
  std::chrono::steady_clock::time_point last_heartbeat_;
  std::atomic<bool> heartbeat_received_;

  // Statistics
  mutable std::mutex stats_mutex_;
  Stats stats_;

  // Recovery callback
  RecoveryCallback recovery_callback_;

  // Thread synchronization
  std::condition_variable cv_;
  std::mutex cv_mutex_;
};
