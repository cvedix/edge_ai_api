#include "core/circuit_breaker.h"
#include <stdexcept>

CircuitBreaker::CircuitBreaker(size_t failure_threshold,
                               std::chrono::milliseconds timeout_ms,
                               size_t success_threshold)
    : failure_threshold_(failure_threshold)
    , success_threshold_(success_threshold)
    , timeout_ms_(timeout_ms)
    , last_failure_time_(std::chrono::steady_clock::now())
    , last_success_time_(std::chrono::steady_clock::now())
{
}

CircuitBreaker::State CircuitBreaker::getState() const {
    return state_.load();
}

void CircuitBreaker::onSuccess() {
    total_calls_++;
    success_count_++;
    last_success_time_ = std::chrono::steady_clock::now();
    
    State current = state_.load();
    if (current == State::HalfOpen) {
        if (success_count_.load() >= success_threshold_) {
            setState(State::Closed);
            failure_count_ = 0;
        }
    } else if (current == State::Closed) {
        // Reset failure count on success
        failure_count_ = 0;
    }
}

void CircuitBreaker::onFailure() {
    total_calls_++;
    failure_count_++;
    last_failure_time_ = std::chrono::steady_clock::now();
    
    State current = state_.load();
    if (current == State::HalfOpen) {
        // Any failure in half-open goes back to open
        setState(State::Open);
    } else if (current == State::Closed) {
        if (failure_count_.load() >= failure_threshold_) {
            setState(State::Open);
        }
    }
}

bool CircuitBreaker::shouldAttemptReset() const {
    if (state_.load() != State::Open) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_failure_time_);
    
    return elapsed >= timeout_ms_;
}

void CircuitBreaker::setState(State new_state) {
    state_.store(new_state);
    
    if (new_state == State::HalfOpen) {
        // Reset counters when entering half-open
        success_count_ = 0;
    }
}

CircuitBreaker::Stats CircuitBreaker::getStats() const {
    Stats stats;
    stats.state = state_.load();
    stats.total_calls = total_calls_.load();
    stats.success_calls = success_count_.load();
    stats.failure_calls = failure_count_.load();
    stats.rejected_calls = rejected_calls_.load();
    stats.last_failure = last_failure_time_;
    stats.last_success = last_success_time_;
    
    size_t total = stats.total_calls;
    stats.success_rate = total > 0 ? 
        static_cast<double>(stats.success_calls) / total : 0.0;
    
    return stats;
}

void CircuitBreaker::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    setState(State::Closed);
    failure_count_ = 0;
    success_count_ = 0;
    total_calls_ = 0;
    rejected_calls_ = 0;
}

