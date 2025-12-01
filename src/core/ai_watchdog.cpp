#include "core/ai_watchdog.h"
#include <iostream>

AIWatchdog::AIWatchdog(uint32_t check_interval_ms, uint32_t timeout_ms, AIProcessor* ai_processor)
    : Watchdog(check_interval_ms, timeout_ms)
    , ai_processor_(ai_processor)
{
}

AIWatchdog::AIHealthStatus AIWatchdog::getAIHealthStatus() const
{
    AIHealthStatus status;
    
    if (ai_processor_) {
        status.is_running = ai_processor_->isRunning();
        status.is_healthy = ai_processor_->isHealthy();
        
        auto metrics = ai_processor_->getMetrics();
        status.fps = metrics.fps;
        status.avg_latency_ms = metrics.avg_latency_ms;
        status.error_count = metrics.error_count;
    } else {
        status.is_running = false;
        status.is_healthy = false;
        status.fps = 0.0;
        status.avg_latency_ms = 0.0;
        status.error_count = 0;
    }
    
    return status;
}

bool AIWatchdog::checkHealth()
{
    // First check base watchdog health (heartbeat)
    if (!Watchdog::checkHealth()) {
        return false;
    }

    // Then check AI-specific health
    if (ai_processor_) {
        // Check if AI processor is running
        if (!ai_processor_->isRunning()) {
            return false;
        }

        // Check if AI processing is healthy (latency, FPS)
        if (!ai_processor_->isHealthy()) {
            std::cerr << "[AIWatchdog] AI processing unhealthy" << std::endl;
            return false;
        }
    }

    return true;
}

