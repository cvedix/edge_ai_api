#include "api/endpoints_handler.h"
#include "core/endpoint_monitor.h"
#include <drogon/HttpResponse.h>
#include <json/json.h>

void EndpointsHandler::getEndpointsStats(const HttpRequestPtr &req,
                                        std::function<void(const HttpResponsePtr &)> &&callback)
{
    try {
        Json::Value response;
        Json::Value endpoints(Json::arrayValue);

        auto& monitor = EndpointMonitor::getInstance();
        auto allStats = monitor.getAllStats();

        for (const auto& pair : allStats) {
            const std::string& endpoint = pair.first;
            const auto& stats = pair.second;

            Json::Value endpointInfo;
            endpointInfo["endpoint"] = endpoint;
            endpointInfo["request_count"] = static_cast<Json::Int64>(stats->request_count.load());
            endpointInfo["error_count"] = static_cast<Json::Int64>(stats->error_count.load());
            
            // Calculate average response time
            uint64_t req_count = stats->request_count.load();
            if (req_count > 0) {
                uint64_t avg_response_time = stats->total_response_time_ms.load() / req_count;
                endpointInfo["avg_response_time_ms"] = static_cast<Json::Int64>(avg_response_time);
                endpointInfo["max_response_time_ms"] = static_cast<Json::Int64>(stats->max_response_time_ms.load());
                endpointInfo["min_response_time_ms"] = static_cast<Json::Int64>(
                    stats->min_response_time_ms.load() == UINT64_MAX ? 0 : stats->min_response_time_ms.load());
                
                // Calculate error rate
                double error_rate = static_cast<double>(stats->error_count.load()) / req_count;
                endpointInfo["error_rate"] = error_rate;
            } else {
                endpointInfo["avg_response_time_ms"] = 0;
                endpointInfo["max_response_time_ms"] = 0;
                endpointInfo["min_response_time_ms"] = 0;
                endpointInfo["error_rate"] = 0.0;
            }

            endpointInfo["is_healthy"] = stats->is_healthy.load();
            endpoints.append(endpointInfo);
        }

        response["endpoints"] = endpoints;
        response["total_endpoints"] = static_cast<Json::Int64>(allStats.size());

        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k200OK);
        
        // Add CORS headers
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
        
        callback(resp);
    } catch (const std::exception& e) {
        Json::Value errorResponse;
        errorResponse["error"] = "Internal server error";
        errorResponse["message"] = e.what();
        
        auto resp = HttpResponse::newHttpJsonResponse(errorResponse);
        resp->setStatusCode(k500InternalServerError);
        callback(resp);
    }
}

