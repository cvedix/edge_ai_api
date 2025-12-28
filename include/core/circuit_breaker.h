#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>

/**
 * @brief Circuit breaker pattern implementation
 *
 * Implements circuit breaker pattern for external service calls
 * to prevent cascading failures and enable fast failure.
 */
class CircuitBreaker {
public:
  enum class State {
    Closed,  // Normal operation
    Open,    // Failing, reject requests immediately
    HalfOpen // Testing if service recovered
  };

  /**
   * @brief Constructor
   * @param failure_threshold Number of failures before opening circuit
   * @param timeout_ms Time to wait before trying half-open state
   * @param success_threshold Number of successes to close circuit from
   * half-open
   */
  CircuitBreaker(
      size_t failure_threshold = 5,
      std::chrono::milliseconds timeout_ms = std::chrono::milliseconds(60000),
      size_t success_threshold = 2);

  /**
   * @brief Execute function with circuit breaker protection
   * @param func Function to execute
   * @param fallback Optional fallback function if circuit is open
   * @return Result of function or fallback
   */
  template <typename Func, typename Fallback = std::nullptr_t>
  auto execute(Func func, Fallback fallback = nullptr) -> decltype(func()) {
    if (getState() == State::Open) {
      if (shouldAttemptReset()) {
        setState(State::HalfOpen);
      } else {
        if constexpr (!std::is_same_v<Fallback, std::nullptr_t>) {
          return fallback();
        } else {
          throw std::runtime_error("Circuit breaker is OPEN");
        }
      }
    }

    try {
      auto result = func();
      onSuccess();
      return result;
    } catch (...) {
      onFailure();
      if constexpr (!std::is_same_v<Fallback, std::nullptr_t>) {
        return fallback();
      } else {
        throw;
      }
    }
  }

  /**
   * @brief Get current state
   */
  State getState() const;

  /**
   * @brief Get statistics
   */
  struct Stats {
    State state;
    size_t total_calls;
    size_t success_calls;
    size_t failure_calls;
    size_t rejected_calls;
    double success_rate;
    std::chrono::steady_clock::time_point last_failure;
    std::chrono::steady_clock::time_point last_success;
  };

  Stats getStats() const;

  /**
   * @brief Reset circuit breaker
   */
  void reset();

private:
  void onSuccess();
  void onFailure();
  bool shouldAttemptReset() const;
  void setState(State new_state);

  std::atomic<State> state_{State::Closed};
  std::atomic<size_t> failure_count_{0};
  std::atomic<size_t> success_count_{0};
  std::atomic<size_t> total_calls_{0};
  std::atomic<size_t> rejected_calls_{0};

  size_t failure_threshold_;
  size_t success_threshold_;
  std::chrono::milliseconds timeout_ms_;
  std::chrono::steady_clock::time_point last_failure_time_;
  std::chrono::steady_clock::time_point last_success_time_;
  mutable std::mutex mutex_;
};
