#include "core/priority_queue.h"

PriorityQueue::PriorityQueue(size_t max_size)
    : max_size_(max_size)
{
}

bool PriorityQueue::enqueue(Request request, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // Wait if queue is full
    if (queue_.size() >= max_size_) {
        if (timeout.count() > 0) {
            if (!condition_.wait_for(lock, timeout, [this] {
                return queue_.size() < max_size_;
            })) {
                return false; // Timeout
            }
        } else {
            return false; // Queue full
        }
    }
    
    request.timestamp = std::chrono::steady_clock::now();
    queue_.push(request);
    condition_.notify_one();
    
    return true;
}

std::optional<PriorityQueue::Request> PriorityQueue::dequeue(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    if (queue_.empty()) {
        if (timeout.count() > 0) {
            if (!condition_.wait_for(lock, timeout, [this] {
                return !queue_.empty();
            })) {
                return std::nullopt; // Timeout
            }
        } else {
            return std::nullopt; // Empty
        }
    }
    
    Request request = queue_.top();
    queue_.pop();
    condition_.notify_one();
    
    return request;
}

size_t PriorityQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

PriorityQueue::Stats PriorityQueue::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Stats stats;
    stats.total = queue_.size();
    stats.max_size = max_size_;
    stats.high_priority = 0;
    stats.medium_priority = 0;
    stats.low_priority = 0;
    
    // Count by priority (would need to iterate, but priority_queue doesn't support this)
    // For efficiency, we'll just return total
    // In production, maintain separate counters
    
    return stats;
}

void PriorityQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!queue_.empty()) {
        queue_.pop();
    }
    condition_.notify_all();
}

