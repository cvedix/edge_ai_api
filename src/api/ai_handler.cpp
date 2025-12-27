#include "api/ai_handler.h"
#include "core/metrics_interceptor.h"
#include "core/performance_monitor.h"
#include <chrono>
#include <drogon/HttpResponse.h>
#include <iomanip>
#include <json/json.h>
#include <random>
#include <sstream>
#include <thread>

// Static members
std::shared_ptr<PriorityQueue> AIHandler::request_queue_;
std::shared_ptr<AICache> AIHandler::cache_;
std::shared_ptr<RateLimiter> AIHandler::rate_limiter_;
std::shared_ptr<ResourceManager> AIHandler::resource_manager_;
std::unique_ptr<std::counting_semaphore<>>
    AIHandler::concurrent_semaphore_; // Fixed: Use smart pointer to prevent
                                      // memory leak
std::atomic<uint64_t> AIHandler::job_counter_{0};
size_t AIHandler::max_concurrent_ = 4;

void AIHandler::initialize(std::shared_ptr<PriorityQueue> queue,
                           std::shared_ptr<AICache> cache,
                           std::shared_ptr<RateLimiter> rate_limiter,
                           std::shared_ptr<ResourceManager> resource_manager,
                           size_t max_concurrent) {

  request_queue_ = queue;
  cache_ = cache;
  rate_limiter_ = rate_limiter;
  resource_manager_ = resource_manager;
  max_concurrent_ = max_concurrent;

  // Fixed: Use smart pointer - automatically handles cleanup, no memory leak
  concurrent_semaphore_ =
      std::make_unique<std::counting_semaphore<>>(max_concurrent);
}

std::string AIHandler::getClientKey(const HttpRequestPtr &req) const {
  // Try to get client IP
  std::string client_ip = req->getPeerAddr().toIp();
  if (client_ip.empty()) {
    client_ip = "unknown";
  }

  // Could also use API key or user ID if available
  return client_ip;
}

std::string AIHandler::generateJobId() const {
  std::ostringstream oss;
  oss << "job_" << std::hex << std::setfill('0') << std::setw(16)
      << job_counter_.fetch_add(1);
  return oss.str();
}

void AIHandler::processImage(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  // Set handler start time for accurate metrics
  MetricsInterceptor::setHandlerStartTime(req);

  auto start_time = std::chrono::steady_clock::now();

  try {
    // Rate limiting
    std::string client_key = getClientKey(req);
    if (!rate_limiter_->allow(client_key)) {
      Json::Value error;
      error["error"] = "Rate limit exceeded";
      error["message"] = "Too many requests. Please try again later.";

      auto resp = HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(k429TooManyRequests);

      // Record metrics and call callback
      MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
      return;
    }

    // Parse request
    auto json = req->getJsonObject();
    if (!json) {
      Json::Value error;
      error["error"] = "Invalid JSON";

      auto resp = HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(k400BadRequest);

      // Record metrics and call callback
      MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
      return;
    }

    std::string image_data = (*json).get("image", "").asString();
    std::string config = (*json).get("config", "").asString();
    std::string priority_str = (*json).get("priority", "medium").asString();

    if (image_data.empty()) {
      Json::Value error;
      error["error"] = "Missing image data";

      auto resp = HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(k400BadRequest);

      // Record metrics and call callback
      MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
      return;
    }

    // Check cache
    std::string cache_key = AICache::generateKey(image_data, config);
    if (cache_) {
      auto cached_result = cache_->get(cache_key);
      if (cached_result.has_value()) {
        Json::Value response;
        response["status"] = "success";
        response["result"] = cached_result.value();
        response["cached"] = true;

        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k200OK);

        // Record metrics and call callback
        MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
        return;
      }
    }

    // Determine priority
    PriorityQueue::Priority priority = PriorityQueue::Priority::Medium;
    if (priority_str == "high" || priority_str == "critical") {
      priority = PriorityQueue::Priority::High;
    } else if (priority_str == "low") {
      priority = PriorityQueue::Priority::Low;
    }

    // Generate job ID
    std::string job_id = generateJobId();

    // Enqueue request
    PriorityQueue::Request request;
    request.priority = priority;
    request.request_id = job_id;
    // Capture req to use in metrics recording
    auto req_ptr = req;
    request.task = [image_data, config, cache_key, callback, start_time,
                    req_ptr]() {
      // Process AI request
      // TODO: Integrate with actual AI processor

      // Simulate processing
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      // Generate result (placeholder)
      Json::Value result;
      result["status"] = "success";
      result["detections"] = Json::arrayValue;
      result["confidence"] = 0.95;

      // Cache result
      if (cache_) {
        Json::StreamWriterBuilder builder;
        std::string result_str = Json::writeString(builder, result);
        cache_->put(cache_key, result_str);
      }

      // Send response
      auto resp = HttpResponse::newHttpJsonResponse(result);
      resp->setStatusCode(k200OK);

      // Record metrics and call callback
      MetricsInterceptor::callWithMetrics(req_ptr, resp, std::move(callback));
    };
    request.timestamp = std::chrono::steady_clock::now();
    request.timeout = std::chrono::milliseconds(30000);

    if (!request_queue_ || !request_queue_->enqueue(request)) {
      Json::Value error;
      error["error"] = "Queue full";
      error["message"] = "Request queue is full. Please try again later.";

      auto resp = HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(k503ServiceUnavailable);

      // Record metrics and call callback
      MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
      return;
    }

    // Return job ID immediately
    Json::Value response;
    response["status"] = "queued";
    response["job_id"] = job_id;
    response["message"] = "Request queued for processing";

    auto resp = HttpResponse::newHttpJsonResponse(response);
    resp->setStatusCode(k202Accepted);

    // Record metrics and call callback
    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));

  } catch (const std::exception &e) {
    Json::Value error;
    error["error"] = "Internal server error";
    error["message"] = e.what();

    auto resp = HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(k500InternalServerError);

    // Record metrics and call callback
    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
  }
}

void AIHandler::processBatch(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  // Similar to processImage but for multiple images
  // TODO: Implement batch processing
  Json::Value response;
  response["status"] = "not_implemented";
  response["message"] = "Batch processing not yet implemented";

  auto resp = HttpResponse::newHttpJsonResponse(response);
  resp->setStatusCode(k501NotImplemented);

  // Record metrics and call callback
  MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
}

void AIHandler::getStatus(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  // Set handler start time for accurate metrics
  MetricsInterceptor::setHandlerStartTime(req);

  Json::Value response;

  if (request_queue_) {
    auto stats = request_queue_->getStats();
    response["queue_size"] = static_cast<Json::UInt64>(stats.total);
    response["queue_max"] = static_cast<Json::UInt64>(stats.max_size);
  }

  if (resource_manager_) {
    auto gpu_stats = resource_manager_->getStats();
    response["gpu_total"] = static_cast<Json::UInt64>(gpu_stats.total_gpus);
    response["gpu_available"] =
        static_cast<Json::UInt64>(gpu_stats.available_gpus);
  }

  auto resp = HttpResponse::newHttpJsonResponse(response);
  resp->setStatusCode(k200OK);

  // Record metrics and call callback
  MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
}

void AIHandler::getMetrics(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  // Set handler start time for accurate metrics
  MetricsInterceptor::setHandlerStartTime(req);

  auto metrics = PerformanceMonitor::getInstance().getMetricsJSON();

  // Add cache stats
  if (cache_) {
    auto cache_stats = cache_->getStats();
    Json::Value cache_data;
    cache_data["size"] = static_cast<Json::UInt64>(cache_stats.entries);
    cache_data["max_size"] = static_cast<Json::UInt64>(cache_stats.max_size);
    cache_data["hit_rate"] = cache_stats.hit_rate;
    metrics["cache"] = cache_data;
  }

  // Add rate limiter stats
  if (rate_limiter_) {
    auto rate_stats = rate_limiter_->getStats();
    Json::Value rate_data;
    rate_data["total_keys"] = static_cast<Json::UInt64>(rate_stats.total_keys);
    rate_data["active_keys"] =
        static_cast<Json::UInt64>(rate_stats.active_keys);
    metrics["rate_limiter"] = rate_data;
  }

  auto resp = HttpResponse::newHttpJsonResponse(metrics);
  resp->setStatusCode(k200OK);

  // Record metrics and call callback
  MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
}
