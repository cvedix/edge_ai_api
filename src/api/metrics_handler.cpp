#include "api/metrics_handler.h"
#include "core/metrics_interceptor.h"
#include "core/performance_monitor.h"
#include <drogon/HttpResponse.h>
#include <json/json.h>
#include <string>

void MetricsHandler::getMetrics(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  // Set handler start time for accurate metrics
  MetricsInterceptor::setHandlerStartTime(req);

  // Check if client wants JSON format
  // Support both Accept header and query parameter
  bool wantJson = false;
  std::string acceptHeader = req->getHeader("Accept");
  std::string formatParam = req->getParameter("format");

  if (acceptHeader.find("application/json") != std::string::npos ||
      formatParam == "json") {
    wantJson = true;
  }

  HttpResponsePtr resp;

  if (wantJson) {
    // Return JSON format (easier to read)
    auto metricsJson = PerformanceMonitor::getInstance().getMetricsJSON();
    resp = HttpResponse::newHttpJsonResponse(metricsJson);
    resp->setStatusCode(k200OK);
  } else {
    // Return Prometheus format (for monitoring tools)
    auto metrics = PerformanceMonitor::getInstance().getPrometheusMetrics();
    resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    resp->setContentTypeCode(CT_TEXT_PLAIN);
    resp->setBody(metrics);
  }

  // Record metrics and call callback
  MetricsInterceptor::callWithMetrics(req, resp, std::move(callback));
}
