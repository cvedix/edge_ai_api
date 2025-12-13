#include "core/rate_limiter.h"
#include <algorithm>
#include <cmath>

RateLimiter::RateLimiter(size_t max_requests, std::chrono::seconds window)
    : max_requests_(max_requests)
    , window_(window)
    , last_cleanup_(std::chrono::steady_clock::now())
{
}

bool RateLimiter::allow(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Cleanup expired buckets periodically
    auto now = std::chrono::steady_clock::now();
    if (now - last_cleanup_ > CLEANUP_INTERVAL) {
        cleanupExpiredBuckets();
        last_cleanup_ = now;
    }
    
    auto& bucket = buckets_[key];
    refill(bucket);
    
    size_t effective_limit = calculateAdaptiveLimit(max_requests_);
    
    if (bucket.tokens < effective_limit) {
        bucket.tokens++;
        bucket.requests_count++;
        return true;
    }
    
    return false;
}

void RateLimiter::refill(TokenBucket& bucket) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - bucket.last_refill).count();
    
    if (elapsed > 0) {
        size_t tokens_to_add = (elapsed * max_requests_) / (window_.count() * 1000);
        if (tokens_to_add > 0) {
            size_t effective_limit = calculateAdaptiveLimit(max_requests_);
            bucket.tokens = std::min(effective_limit, bucket.tokens + tokens_to_add);
            bucket.last_refill = now;
        }
    } else if (bucket.tokens == 0) {
        // Initialize if first time
        bucket.tokens = calculateAdaptiveLimit(max_requests_);
        bucket.last_refill = now;
    }
}

size_t RateLimiter::calculateAdaptiveLimit(size_t base_limit) const {
    double factor = adaptive_factor_.load();
    return static_cast<size_t>(std::floor(base_limit * factor));
}

void RateLimiter::reset(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    buckets_.erase(key);
}

size_t RateLimiter::getRemainingTokens(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = buckets_.find(key);
    if (it == buckets_.end()) {
        return calculateAdaptiveLimit(max_requests_);
    }
    
    auto& bucket = it->second;
    refill(bucket);
    
    return bucket.tokens;
}

void RateLimiter::setAdaptiveThrottling(double load_factor) {
    // load_factor: 0.0 (no load) to 1.0 (full load)
    // As load increases, reduce rate limit
    load_factor = std::max(0.0, std::min(1.0, load_factor));
    
    // Reduce limit by up to 50% under high load
    double factor = 1.0 - (load_factor * 0.5);
    adaptive_factor_.store(factor);
}

void RateLimiter::cleanupExpiredBuckets() {
    auto now = std::chrono::steady_clock::now();
    auto expire_time = now - std::chrono::seconds(window_.count() * 2);
    
    for (auto it = buckets_.begin(); it != buckets_.end();) {
        if (it->second.last_refill < expire_time) {
            it = buckets_.erase(it);
        } else {
            ++it;
        }
    }
}

RateLimiter::Stats RateLimiter::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Stats stats;
    stats.total_keys = buckets_.size();
    
    auto now = std::chrono::steady_clock::now();
    auto active_threshold = now - std::chrono::seconds(window_.count());
    
    for (const auto& [key, bucket] : buckets_) {
        if (bucket.last_refill > active_threshold) {
            stats.active_keys++;
        }
    }
    
    stats.adaptive_factor = adaptive_factor_.load();
    
    return stats;
}

