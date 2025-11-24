# Đề xuất cải thiện hiệu suất cho Real-time AI Processing

## 🎯 Mục tiêu
Đạt được hiệu suất cao cho xử lý AI real-time với:
- Latency < 100ms cho single frame
- Throughput > 100 requests/second
- Support concurrent processing
- Resource-efficient

## 🔧 Các cải thiện cụ thể

### 1. Tích hợp CVEDIX SDK vào REST API

#### Vấn đề hiện tại:
- `main.cpp` có example sử dụng CVEDIX SDK nhưng không được dùng trong REST API
- `AIProcessor` chỉ là placeholder

#### Giải pháp:

**File mới: `src/core/cvedix_processor.cpp`**
```cpp
#include "core/cvedix_processor.h"
#include <cvedix/nodes/src/cvedix_rtsp_src_node.h>
#include <cvedix/nodes/infers/cvedix_yolo_detector_node.h>
// ... other CVEDIX includes

class CVEDIXProcessor : public AIProcessor {
private:
    std::shared_ptr<cvedix_nodes::cvedix_rtsp_src_node> rtsp_src_;
    std::shared_ptr<cvedix_nodes::cvedix_yolo_detector_node> detector_;
    // ... other nodes
    
protected:
    bool initializeSDK(const std::string& config) override {
        // Parse config JSON
        // Initialize CVEDIX nodes
        // Connect pipeline
        return true;
    }
    
    void processFrame() override {
        // Process frame through CVEDIX pipeline
        // Get results and call callback
    }
};
```

### 2. Async AI Processing Endpoint

#### Tạo endpoint mới: `/v1/core/ai/process`

**File: `include/api/ai_handler.h`**
```cpp
#pragma once
#include <drogon/HttpController.h>
#include "core/ai_processor.h"

class AIHandler : public drogon::HttpController<AIHandler> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(AIHandler::processImage, "/v1/core/ai/process", Post);
        ADD_METHOD_TO(AIHandler::getStatus, "/v1/core/ai/status", Get);
        ADD_METHOD_TO(AIHandler::getMetrics, "/v1/core/ai/metrics", Get);
    METHOD_LIST_END
    
    void processImage(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback);
    
    void getStatus(const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback);
    
    void getMetrics(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback);
    
private:
    static std::unique_ptr<AIProcessor> ai_processor_;
    static std::mutex processor_mutex_;
};
```

**File: `src/api/ai_handler.cpp`**
```cpp
#include "api/ai_handler.h"
#include "core/cvedix_processor.h"
#include <drogon/HttpResponse.h>
#include <json/json.h>
#include <future>

std::unique_ptr<AIProcessor> AIHandler::ai_processor_;
std::mutex AIHandler::processor_mutex_;

void AIHandler::processImage(const HttpRequestPtr &req,
                             std::function<void(const HttpResponsePtr &)> &&callback) {
    try {
        // Parse request body
        auto json = req->getJsonObject();
        if (!json) {
            auto resp = HttpResponse::newHttpJsonResponse(
                Json::Value{{"error", "Invalid JSON"}});
            resp->setStatusCode(k400BadRequest);
            callback(resp);
            return;
        }
        
        // Get image data (base64 or URL)
        std::string image_data = (*json)["image"].asString();
        std::string config = (*json)["config"].asString("");
        
        // Process asynchronously
        auto future = std::async(std::launch::async, [image_data, config]() {
            std::lock_guard<std::mutex> lock(processor_mutex_);
            
            if (!ai_processor_ || !ai_processor_->isRunning()) {
                ai_processor_ = std::make_unique<CVEDIXProcessor>();
                ai_processor_->start(config);
            }
            
            // Process image
            std::string result;
            ai_processor_->processFrame(); // Implement this
            
            return result;
        });
        
        // Return immediately with job ID
        Json::Value response;
        response["job_id"] = generateJobId();
        response["status"] = "processing";
        
        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k202Accepted);
        callback(resp);
        
        // Store future for later retrieval
        
    } catch (const std::exception& e) {
        Json::Value error;
        error["error"] = e.what();
        auto resp = HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(k500InternalServerError);
        callback(resp);
    }
}
```

### 3. Thread Pool cho AI Processing

**File: `include/core/ai_thread_pool.h`**
```cpp
#pragma once
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <vector>
#include <atomic>

class AIThreadPool {
public:
    AIThreadPool(size_t num_threads = std::thread::hardware_concurrency());
    ~AIThreadPool();
    
    template<typename F>
    void enqueue(F&& f) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            tasks_.emplace(std::forward<F>(f));
        }
        condition_.notify_one();
    }
    
    void stop();
    size_t size() const { return workers_.size(); }
    size_t queueSize() const;
    
private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_{false};
    
    void worker();
};
```

**File: `src/core/ai_thread_pool.cpp`**
```cpp
#include "core/ai_thread_pool.h"

AIThreadPool::AIThreadPool(size_t num_threads) {
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this] { worker(); });
    }
}

AIThreadPool::~AIThreadPool() {
    stop();
}

void AIThreadPool::worker() {
    while (!stop_ || !tasks_.empty()) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            condition_.wait(lock, [this] {
                return stop_ || !tasks_.empty();
            });
            
            if (stop_ && tasks_.empty()) {
                return;
            }
            
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        
        task();
    }
}

void AIThreadPool::stop() {
    stop_ = true;
    condition_.notify_all();
    
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

size_t AIThreadPool::queueSize() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return tasks_.size();
}
```

### 4. Request Queue với Semaphore

**File: `include/core/ai_request_queue.h`**
```cpp
#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <semaphore>
#include <functional>
#include <chrono>

struct AIRequest {
    int priority;
    std::string request_id;
    std::function<void(const std::string&)> callback;
    std::chrono::steady_clock::time_point timestamp;
    
    bool operator<(const AIRequest& other) const {
        // Higher priority first
        if (priority != other.priority) {
            return priority < other.priority;
        }
        // Earlier timestamp first
        return timestamp > other.timestamp;
    }
};

class AIRequestQueue {
public:
    AIRequestQueue(size_t max_concurrent = 4);
    
    bool enqueue(AIRequest request, std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));
    AIRequest dequeue();
    size_t size() const;
    void release();
    
private:
    std::priority_queue<AIRequest> queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::counting_semaphore<> semaphore_;
    size_t max_concurrent_;
};
```

### 5. Rate Limiting

**File: `include/core/rate_limiter.h`**
```cpp
#pragma once
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <string>

class RateLimiter {
public:
    RateLimiter(size_t max_requests, std::chrono::seconds window);
    
    bool allow(const std::string& key);
    void reset(const std::string& key);
    
private:
    struct TokenBucket {
        size_t tokens;
        std::chrono::steady_clock::time_point last_refill;
    };
    
    std::unordered_map<std::string, TokenBucket> buckets_;
    mutable std::mutex mutex_;
    size_t max_requests_;
    std::chrono::seconds window_;
    
    void refill(TokenBucket& bucket);
};
```

**File: `src/core/rate_limiter.cpp`**
```cpp
#include "core/rate_limiter.h"

RateLimiter::RateLimiter(size_t max_requests, std::chrono::seconds window)
    : max_requests_(max_requests), window_(window) {
}

bool RateLimiter::allow(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto& bucket = buckets_[key];
    refill(bucket);
    
    if (bucket.tokens > 0) {
        bucket.tokens--;
        return true;
    }
    
    return false;
}

void RateLimiter::refill(TokenBucket& bucket) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - bucket.last_refill).count();
    
    size_t tokens_to_add = (elapsed * max_requests_) / window_.count() / 1000;
    if (tokens_to_add > 0) {
        bucket.tokens = std::min(max_requests_, bucket.tokens + tokens_to_add);
        bucket.last_refill = now;
    }
}
```

### 6. Caching Layer

**File: `include/core/ai_cache.h`**
```cpp
#pragma once
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <string>
#include <optional>

struct CacheEntry {
    std::string data;
    std::chrono::steady_clock::time_point expiry;
    
    bool isExpired() const {
        return std::chrono::steady_clock::now() > expiry;
    }
};

class AICache {
public:
    AICache(size_t max_size = 1000, 
            std::chrono::seconds ttl = std::chrono::seconds(300));
    
    void put(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key);
    void invalidate(const std::string& key);
    void clear();
    size_t size() const;
    
private:
    std::unordered_map<std::string, CacheEntry> cache_;
    mutable std::mutex mutex_;
    size_t max_size_;
    std::chrono::seconds ttl_;
    
    void evictLRU();
    std::string generateKey(const std::string& image_data, const std::string& config);
};
```

### 7. WebSocket Support cho Real-time Streaming

**File: `include/api/ai_websocket.h`**
```cpp
#pragma once
#include <drogon/WebSocketController.h>

class AIWebSocketController : public drogon::WebSocketController<AIWebSocketController> {
public:
    void handleNewMessage(const WebSocketConnectionPtr& wsConnPtr,
                         std::string&& message,
                         const WebSocketMessageType& type) override;
    
    void handleNewConnection(const HttpRequestPtr& req,
                            const WebSocketConnectionPtr& wsConnPtr) override;
    
    void handleConnectionClosed(const WebSocketConnectionPtr& wsConnPtr) override;
    
    WS_PATH_LIST_BEGIN
        WS_PATH_ADD("/v1/core/ai/stream", drogon::Get);
    WS_PATH_LIST_END
};
```

### 8. Metrics & Monitoring

**File: `include/core/performance_monitor.h`**
```cpp
#pragma once
#include <atomic>
#include <chrono>
#include <string>
#include <unordered_map>

struct PerformanceMetrics {
    std::atomic<uint64_t> total_requests{0};
    std::atomic<uint64_t> successful_requests{0};
    std::atomic<uint64_t> failed_requests{0};
    std::atomic<double> avg_latency_ms{0.0};
    std::atomic<double> max_latency_ms{0.0};
    std::atomic<double> min_latency_ms{std::numeric_limits<double>::max()};
    std::atomic<uint64_t> queue_size{0};
    std::atomic<double> throughput_rps{0.0};
};

class PerformanceMonitor {
public:
    static PerformanceMonitor& getInstance();
    
    void recordRequest(const std::string& endpoint, 
                     std::chrono::milliseconds latency,
                     bool success);
    
    PerformanceMetrics getMetrics() const;
    Json::Value getMetricsJSON() const;
    
private:
    PerformanceMonitor() = default;
    PerformanceMetrics metrics_;
    std::chrono::steady_clock::time_point start_time_;
};
```

## 📊 Cấu hình đề xuất

### main.cpp updates
```cpp
// Thêm vào main()
#include "core/ai_thread_pool.h"
#include "core/rate_limiter.h"
#include "core/ai_cache.h"

// Global instances
static std::unique_ptr<AIThreadPool> g_ai_thread_pool;
static std::unique_ptr<RateLimiter> g_rate_limiter;
static std::unique_ptr<AICache> g_ai_cache;

int main() {
    // ... existing code ...
    
    // Initialize AI components
    size_t ai_threads = std::max(2u, std::thread::hardware_concurrency() / 2);
    g_ai_thread_pool = std::make_unique<AIThreadPool>(ai_threads);
    
    // Rate limit: 100 requests per second per client
    g_rate_limiter = std::make_unique<RateLimiter>(100, std::chrono::seconds(1));
    
    // Cache: 1000 entries, 5 minutes TTL
    g_ai_cache = std::make_unique<AICache>(1000, std::chrono::seconds(300));
    
    // Register AI handler
    static AIHandler aiHandler;
    
    // ... rest of code ...
}
```

## 🎯 Kết quả mong đợi

Sau khi implement các cải thiện trên:

1. **Latency**: < 100ms cho single frame processing
2. **Throughput**: > 100 requests/second
3. **Concurrent Processing**: Support 4-8 concurrent AI jobs
4. **Resource Usage**: Tối ưu CPU/GPU utilization
5. **Reliability**: Rate limiting, caching, error recovery

## 📝 Checklist Implementation

- [ ] Tích hợp CVEDIX SDK vào AIProcessor
- [ ] Tạo AI processing endpoint
- [ ] Implement thread pool
- [ ] Implement request queue với semaphore
- [ ] Thêm rate limiting
- [ ] Thêm caching layer
- [ ] WebSocket support
- [ ] Performance monitoring
- [ ] Unit tests
- [ ] Integration tests
- [ ] Load testing
- [ ] Documentation

---

*Tài liệu này cung cấp các code examples cụ thể để cải thiện hiệu suất real-time AI processing*

