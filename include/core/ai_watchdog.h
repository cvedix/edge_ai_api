#pragma once

#include "core/watchdog.h"
#include "core/ai_processor.h"
#include <memory>

/**
 * @brief Watchdog specifically for AI Processing
 * 
 * Monitors AI processing thread health, latency, and FPS.
 * Faster check interval than global watchdog for realtime requirements.
 */
class AIWatchdog : public Watchdog {
public:
    /**
     * @brief Constructor
     * @param check_interval_ms Check interval (default: 1000ms for realtime)
     * @param timeout_ms Timeout (default: 5000ms - faster than global)
     * @param ai_processor Reference to AI processor to monitor
     */
    AIWatchdog(uint32_t check_interval_ms = 1000,
               uint32_t timeout_ms = 5000,
               AIProcessor* ai_processor = nullptr);

    /**
     * @brief Set AI processor to monitor
     */
    void setAIProcessor(AIProcessor* processor) { ai_processor_ = processor; }

    /**
     * @brief Get AI-specific health status
     */
    struct AIHealthStatus {
        bool is_running;
        bool is_healthy;
        double fps;
        double avg_latency_ms;
        uint64_t error_count;
    };

    AIHealthStatus getAIHealthStatus() const;

private:
    /**
     * @brief Override health check to include AI-specific checks
     */
    bool checkHealth() override;

    AIProcessor* ai_processor_;
};

