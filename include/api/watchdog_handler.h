#pragma once

#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <json/json.h>

// Forward declarations
class Watchdog;
class HealthMonitor;

using namespace drogon;

/**
 * @brief Watchdog status endpoint handler
 * 
 * Endpoint: GET /v1/core/watchdog
 * Returns: JSON with watchdog and health monitor statistics
 */
class WatchdogHandler : public drogon::HttpController<WatchdogHandler>
{
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(WatchdogHandler::getWatchdogStatus, "/v1/core/watchdog", Get);
    METHOD_LIST_END

    /**
     * @brief Handle GET /v1/core/watchdog
     * 
     * @param req HTTP request
     * @param callback Response callback
     */
    void getWatchdogStatus(const HttpRequestPtr &req,
                          std::function<void(const HttpResponsePtr &)> &&callback);

    /**
     * @brief Set watchdog instance (called from main)
     */
    static void setWatchdog(Watchdog* watchdog) { g_watchdog = watchdog; }

    /**
     * @brief Set health monitor instance (called from main)
     */
    static void setHealthMonitor(HealthMonitor* monitor) { g_health_monitor = monitor; }

private:
    static Watchdog* g_watchdog;
    static HealthMonitor* g_health_monitor;
};

