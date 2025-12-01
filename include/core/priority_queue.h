#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <string>
#include <functional>
#include <atomic>

/**
 * @brief Priority queue for AI processing requests
 * 
 * Implements priority queue with QoS levels (high/medium/low)
 * and preemption support for high-priority requests.
 */
class PriorityQueue {
public:
    enum class Priority {
        Low = 0,
        Medium = 1,
        High = 2,
        Critical = 3
    };

    struct Request {
        Priority priority;
        std::string request_id;
        std::function<void()> task;
        std::chrono::steady_clock::time_point timestamp;
        std::chrono::milliseconds timeout;
        
        bool operator<(const Request& other) const {
            // Higher priority first
            if (priority != other.priority) {
                return priority < other.priority;
            }
            // Earlier timestamp first for same priority
            return timestamp > other.timestamp;
        }
    };

    /**
     * @brief Constructor
     * @param max_size Maximum queue size
     */
    explicit PriorityQueue(size_t max_size = 1000);

    /**
     * @brief Enqueue a request
     * @param request Request to enqueue
     * @param timeout Maximum time to wait if queue is full
     * @return true if enqueued, false if timeout or queue full
     */
    bool enqueue(Request request, std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

    /**
     * @brief Dequeue a request (blocks until available)
     * @param timeout Maximum time to wait
     * @return Request or empty optional if timeout
     */
    std::optional<Request> dequeue(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

    /**
     * @brief Get queue size
     */
    size_t size() const;

    /**
     * @brief Get queue statistics
     */
    struct Stats {
        size_t total;
        size_t high_priority;
        size_t medium_priority;
        size_t low_priority;
        size_t max_size;
    };
    
    Stats getStats() const;

    /**
     * @brief Clear all requests
     */
    void clear();

private:
    std::priority_queue<Request> queue_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    size_t max_size_;
};

