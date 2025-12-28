#pragma once

#include "core/metrics_interceptor.h"
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <functional>

/**
 * @brief Helper macro to automatically record metrics and call callback
 *
 * Usage:
 *   auto resp = HttpResponse::newHttpJsonResponse(data);
 *   CALLBACK_WITH_METRICS(req, resp, callback);
 *
 * This automatically:
 * 1. Sets handler start time if not already set
 * 2. Records metrics
 * 3. Calls the callback
 */
#define CALLBACK_WITH_METRICS(req, resp, callback)                             \
  do {                                                                         \
    MetricsInterceptor::setHandlerStartTime(req);                              \
    MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));       \
  } while (0)
