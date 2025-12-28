#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

/**
 * @brief Connection pool for external services
 *
 * Manages a pool of reusable connections to avoid creating new connections
 * for each request, improving performance and resource utilization.
 */
template <typename ConnectionType> class ConnectionPool {
public:
  struct Connection {
    std::shared_ptr<ConnectionType> conn;
    std::chrono::steady_clock::time_point last_used;
    bool in_use;

    bool isExpired(std::chrono::seconds max_idle) const {
      if (in_use)
        return false;
      auto now = std::chrono::steady_clock::now();
      return (now - last_used) > max_idle;
    }
  };

  ConnectionPool(size_t min_size, size_t max_size,
                 std::chrono::seconds max_idle_time = std::chrono::seconds(300))
      : min_size_(min_size), max_size_(max_size), max_idle_time_(max_idle_time),
        active_connections_(0) {}

  virtual ~ConnectionPool() = default;

  /**
   * @brief Get a connection from the pool
   * @param timeout Maximum time to wait for a connection
   * @return Shared pointer to connection, or nullptr if timeout
   */
  std::shared_ptr<ConnectionType>
  acquire(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) {
    std::unique_lock<std::mutex> lock(mutex_);

    // Try to get existing connection
    while (!available_connections_.empty()) {
      auto conn = available_connections_.front();
      available_connections_.pop();

      if (conn->isExpired(max_idle_time_)) {
        // Connection expired, remove it
        active_connections_--;
        continue;
      }

      conn->in_use = true;
      conn->last_used = std::chrono::steady_clock::now();
      return conn->conn;
    }

    // Create new connection if under max size
    if (active_connections_ < max_size_) {
      auto new_conn = createConnection();
      if (new_conn) {
        active_connections_++;
        auto conn_wrapper = std::make_shared<Connection>();
        conn_wrapper->conn = new_conn;
        conn_wrapper->in_use = true;
        conn_wrapper->last_used = std::chrono::steady_clock::now();
        return new_conn;
      }
    }

    // Wait for available connection
    if (condition_.wait_for(lock, timeout, [this] {
          return !available_connections_.empty() ||
                 active_connections_ < max_size_;
        })) {
      if (!available_connections_.empty()) {
        auto conn = available_connections_.front();
        available_connections_.pop();
        conn->in_use = true;
        conn->last_used = std::chrono::steady_clock::now();
        return conn->conn;
      } else if (active_connections_ < max_size_) {
        auto new_conn = createConnection();
        if (new_conn) {
          active_connections_++;
          return new_conn;
        }
      }
    }

    return nullptr; // Timeout
  }

  /**
   * @brief Return a connection to the pool
   */
  void release(std::shared_ptr<ConnectionType> conn) {
    if (!conn)
      return;

    std::lock_guard<std::mutex> lock(mutex_);

    auto conn_wrapper = std::make_shared<Connection>();
    conn_wrapper->conn = conn;
    conn_wrapper->in_use = false;
    conn_wrapper->last_used = std::chrono::steady_clock::now();

    available_connections_.push(conn_wrapper);
    condition_.notify_one();
  }

  /**
   * @brief Get pool statistics
   */
  struct Stats {
    size_t active;
    size_t available;
    size_t total;
  };

  Stats getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return {active_connections_, available_connections_.size(),
            active_connections_};
  }

protected:
  /**
   * @brief Create a new connection (to be implemented by derived class)
   */
  virtual std::shared_ptr<ConnectionType> createConnection() = 0;

private:
  size_t min_size_;
  size_t max_size_;
  std::chrono::seconds max_idle_time_;

  std::queue<std::shared_ptr<Connection>> available_connections_;
  mutable std::mutex mutex_;
  std::condition_variable condition_;
  std::atomic<size_t> active_connections_;
};
