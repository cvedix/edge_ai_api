#pragma once

#include <unordered_map>
#include <chrono>
#include <mutex>
#include <string>
#include <atomic>

/**
 * @brief Rate limiter using token bucket algorithm
 * 
 * Implements token bucket algorithm for rate limiting requests.
 * Supports per-client rate limits and adaptive throttling.
 */
class RateLimiter {
public:
    /**
     * @brief Constructor
     * @param max_requests Maximum number of requests
     * @param window Time window in seconds
     */
    RateLimiter(size_t max_requests, std::chrono::seconds window);

    /**
     * @brief Check if request is allowed
     * @param key Client identifier (IP, user ID, etc.)
     * @return true if allowed, false if rate limited
     */
    bool allow(const std::string& key);

    /**
     * @brief Reset rate limit for a key
     * @param key Client identifier
     */
    void reset(const std::string& key);

    /**
     * @brief Get remaining tokens for a key
     * @param key Client identifier
     * @return Number of remaining tokens
     * @note This method may refill tokens, so it's not const
     */
    size_t getRemainingTokens(const std::string& key);

    /**
     * @brief Set adaptive throttling based on system load
     * @param load_factor System load factor (0.0 to 1.0)
     */
    void setAdaptiveThrottling(double load_factor);

    /**
     * @brief Get rate limit statistics
     */
    struct Stats {
        size_t total_keys;
        size_t active_keys;
        double adaptive_factor;
    };
    
    Stats getStats() const;

private:
    struct TokenBucket {
        size_t tokens;
        std::chrono::steady_clock::time_point last_refill;
        std::atomic<size_t> requests_count{0};
    };

    void refill(TokenBucket& bucket);
    size_t calculateAdaptiveLimit(size_t base_limit) const;

    std::unordered_map<std::string, TokenBucket> buckets_;
    mutable std::mutex mutex_;
    size_t max_requests_;
    std::chrono::seconds window_;
    std::atomic<double> adaptive_factor_{1.0};
    
    // Cleanup expired buckets periodically
    void cleanupExpiredBuckets();
    void evictOldestBuckets(size_t count);  // Evict oldest buckets when size limit exceeded
    std::chrono::steady_clock::time_point last_cleanup_;
    static constexpr std::chrono::seconds CLEANUP_INTERVAL{300}; // 5 minutes
    static constexpr size_t MAX_BUCKETS = 10000;  // Maximum number of buckets to prevent unbounded growth
};

