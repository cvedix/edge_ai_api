#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

/**
 * @brief Cache for AI processing results
 *
 * Implements LRU cache with TTL for caching AI processing results.
 * Supports cache invalidation and size limits.
 */
class AICache {
public:
  struct CacheEntry {
    std::string data;
    std::chrono::steady_clock::time_point created_at;
    std::chrono::steady_clock::time_point expiry;
    size_t access_count;
    std::chrono::steady_clock::time_point last_accessed;

    bool isExpired() const { return std::chrono::steady_clock::now() > expiry; }
  };

  /**
   * @brief Constructor
   * @param max_size Maximum number of cache entries
   * @param default_ttl Default TTL in seconds
   */
  AICache(size_t max_size = 1000,
          std::chrono::seconds default_ttl = std::chrono::seconds(300));

  /**
   * @brief Put value into cache
   * @param key Cache key
   * @param value Cache value
   * @param ttl Optional TTL (uses default if not specified)
   */
  void put(const std::string &key, const std::string &value,
           std::chrono::seconds ttl = std::chrono::seconds(0));

  /**
   * @brief Get value from cache
   * @param key Cache key
   * @return Cached value or empty optional if not found/expired
   */
  std::optional<std::string> get(const std::string &key);

  /**
   * @brief Invalidate cache entry
   * @param key Cache key
   */
  void invalidate(const std::string &key);

  /**
   * @brief Invalidate all entries matching pattern
   * @param pattern Pattern to match (simple substring match)
   */
  void invalidatePattern(const std::string &pattern);

  /**
   * @brief Clear all cache entries
   */
  void clear();

  /**
   * @brief Get cache size
   */
  size_t size() const;

  /**
   * @brief Get cache statistics
   */
  struct Stats {
    size_t entries;
    size_t max_size;
    size_t hits;
    size_t misses;
    double hit_rate;
  };

  Stats getStats() const;

  /**
   * @brief Generate cache key from image data and config
   * @param image_data Image data (or hash)
   * @param config Configuration string
   * @return Cache key
   */
  static std::string generateKey(const std::string &image_data,
                                 const std::string &config);

private:
  void evictLRU();
  void cleanupExpired();

  std::unordered_map<std::string, CacheEntry> cache_;
  std::list<std::string> access_order_; // For LRU tracking
  std::unordered_map<std::string, std::list<std::string>::iterator> access_map_;
  mutable std::mutex mutex_;
  size_t max_size_;
  std::chrono::seconds default_ttl_;

  std::atomic<size_t> hits_{0};
  std::atomic<size_t> misses_{0};

  std::chrono::steady_clock::time_point last_cleanup_;
  static constexpr std::chrono::seconds CLEANUP_INTERVAL{60}; // 1 minute
};
