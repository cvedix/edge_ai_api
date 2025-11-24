#include "api/watchdog_handler.h"
#include "core/watchdog.h"
#include "core/health_monitor.h"
#include <drogon/HttpResponse.h>
#include <json/json.h>

// Static members initialization
Watchdog* WatchdogHandler::g_watchdog = nullptr;
HealthMonitor* WatchdogHandler::g_health_monitor = nullptr;

void WatchdogHandler::getWatchdogStatus(const HttpRequestPtr &req,
                                       std::function<void(const HttpResponsePtr &)> &&callback)
{
    try {
        Json::Value response;

        // Watchdog statistics
        if (g_watchdog) {
            auto stats = g_watchdog->getStats();
            Json::Value watchdog_info;
            watchdog_info["running"] = g_watchdog->isRunning();
            watchdog_info["total_heartbeats"] = static_cast<Json::Int64>(stats.total_heartbeats);
            watchdog_info["missed_heartbeats"] = static_cast<Json::Int64>(stats.missed_heartbeats);
            watchdog_info["recovery_actions"] = static_cast<Json::Int64>(stats.recovery_actions);
            watchdog_info["is_healthy"] = stats.is_healthy;
            
            // Last heartbeat time
            auto last_hb = stats.last_heartbeat;
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_hb).count();
            watchdog_info["seconds_since_last_heartbeat"] = static_cast<Json::Int64>(elapsed);
            
            response["watchdog"] = watchdog_info;
        } else {
            response["watchdog"] = Json::Value(Json::objectValue);
            response["watchdog"]["error"] = "Watchdog not initialized";
        }

        // Health monitor statistics
        if (g_health_monitor) {
            auto metrics = g_health_monitor->getMetrics();
            Json::Value monitor_info;
            monitor_info["running"] = g_health_monitor->isRunning();
            monitor_info["cpu_usage_percent"] = metrics.cpu_usage_percent;
            monitor_info["memory_usage_mb"] = static_cast<Json::Int64>(metrics.memory_usage_mb);
            monitor_info["request_count"] = static_cast<Json::Int64>(metrics.request_count);
            monitor_info["error_count"] = static_cast<Json::Int64>(metrics.error_count);
            
            response["health_monitor"] = monitor_info;
        } else {
            response["health_monitor"] = Json::Value(Json::objectValue);
            response["health_monitor"]["error"] = "Health monitor not initialized";
        }

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

