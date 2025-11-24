#include "api/metrics_handler.h"
#include "core/performance_monitor.h"
#include <drogon/HttpResponse.h>

void MetricsHandler::getMetrics(const HttpRequestPtr &req,
                               std::function<void(const HttpResponsePtr &)> &&callback) {
    auto metrics = PerformanceMonitor::getInstance().getPrometheusMetrics();
    
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    resp->setContentTypeCode(CT_TEXT_PLAIN);
    resp->setBody(metrics);
    
    callback(resp);
}

