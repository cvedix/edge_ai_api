#pragma once

#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

using namespace drogon;

/**
 * @brief Metrics endpoint handler
 * 
 * Endpoint: GET /v1/core/metrics
 * Returns: Prometheus format metrics
 */
class MetricsHandler : public drogon::HttpController<MetricsHandler> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(MetricsHandler::getMetrics, "/v1/core/metrics", Get);
    METHOD_LIST_END

    /**
     * @brief Handle GET /v1/core/metrics
     * Returns Prometheus format metrics
     */
    void getMetrics(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback);
};

