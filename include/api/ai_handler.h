#pragma once

#include "core/ai_cache.h"
#include "core/priority_queue.h"
#include "core/rate_limiter.h"
#include "core/resource_manager.h"
#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <json/json.h>
#include <memory>
#include <semaphore>
#include <string>

using namespace drogon;

/**
 * @brief AI processing handler
 *
 * Endpoints:
 * - POST /v1/core/ai/process - Process single image/frame
 * - POST /v1/core/ai/batch - Process batch of images/frames
 * - GET /v1/core/ai/status - Get processing status
 * - GET /v1/core/ai/metrics - Get processing metrics
 */
class AIHandler : public drogon::HttpController<AIHandler> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(AIHandler::processImage, "/v1/core/ai/process", Post);
  ADD_METHOD_TO(AIHandler::processBatch, "/v1/core/ai/batch", Post);
  ADD_METHOD_TO(AIHandler::getStatus, "/v1/core/ai/status", Get);
  ADD_METHOD_TO(AIHandler::getMetrics, "/v1/core/ai/metrics", Get);
  METHOD_LIST_END

  /**
   * @brief Process single image/frame
   */
  void processImage(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Process batch of images/frames
   */
  void processBatch(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Get processing status
   */
  void getStatus(const HttpRequestPtr &req,
                 std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Get processing metrics
   */
  void getMetrics(const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback);

  /**
   * @brief Initialize handler with dependencies
   */
  static void initialize(std::shared_ptr<PriorityQueue> queue,
                         std::shared_ptr<AICache> cache,
                         std::shared_ptr<RateLimiter> rate_limiter,
                         std::shared_ptr<ResourceManager> resource_manager,
                         size_t max_concurrent);

private:
  std::string getClientKey(const HttpRequestPtr &req) const;
  std::string generateJobId() const;
  void processRequestAsync(const std::string &image_data,
                           const std::string &config,
                           PriorityQueue::Priority priority,
                           std::function<void(const std::string &)> callback);

  static std::shared_ptr<PriorityQueue> request_queue_;
  static std::shared_ptr<AICache> cache_;
  static std::shared_ptr<RateLimiter> rate_limiter_;
  static std::shared_ptr<ResourceManager> resource_manager_;
  static std::unique_ptr<std::counting_semaphore<>>
      concurrent_semaphore_; // Fixed: Use smart pointer to prevent memory leak
  static std::atomic<uint64_t> job_counter_;
  static size_t max_concurrent_;
};
