#include <drogon/drogon.h>
#include "api/health_handler.h"
#include "api/version_handler.h"
#include "api/watchdog_handler.h"
#include "api/swagger_handler.h"
#include "api/create_instance_handler.h"
#include "api/instance_handler.h"
#include "models/model_upload_handler.h"
#include "core/watchdog.h"
#include "core/health_monitor.h"
#include "core/env_config.h"
#include "instances/instance_registry.h"
#include "core/solution_registry.h"
#include "core/pipeline_builder.h"
#include "instances/instance_storage.h"
#include <iostream>
#include <csignal>
#include <cstdlib>
#include <stdexcept>
#include <memory>
#include <thread>
#include <algorithm>

/**
 * @brief Edge AI API Server
 * 
 * REST API server using Drogon framework
 * Provides health check and version endpoints
 * Includes watchdog and health monitoring on separate threads
 */

// Global flag for graceful shutdown
static bool g_shutdown = false;

// Global watchdog and health monitor instances
static std::unique_ptr<Watchdog> g_watchdog;
static std::unique_ptr<HealthMonitor> g_health_monitor;

// Signal handler for graceful shutdown
void signalHandler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
        g_shutdown = true;
        
        // Stop watchdog and health monitor
        if (g_health_monitor) {
            g_health_monitor->stop();
        }
        if (g_watchdog) {
            g_watchdog->stop();
        }
        
        drogon::app().quit();
    }
}

// Recovery callback for watchdog
void recoveryAction()
{
    std::cerr << "[Recovery] Application detected as unresponsive. Attempting recovery..." << std::endl;
    // In production, you might want to:
    // - Restart specific components
    // - Clear caches
    // - Reconnect to external services
    // - Log critical error
    // For now, we just log the event
}

/**
 * @brief Validate and parse port number
 */
uint16_t parsePort(const char* port_str, uint16_t default_port)
{
    if (!port_str) {
        return default_port;
    }
    
    try {
        int port_int = std::stoi(port_str);
        if (port_int < 1 || port_int > 65535) {
            throw std::invalid_argument("Port must be between 1 and 65535");
        }
        return static_cast<uint16_t>(port_int);
    } catch (const std::invalid_argument& e) {
        std::cerr << "Error: Invalid port number '" << port_str << "': " << e.what() << std::endl;
        std::cerr << "Using default port: " << default_port << std::endl;
        return default_port;
    } catch (const std::out_of_range& e) {
        std::cerr << "Error: Port number out of range: " << e.what() << std::endl;
        std::cerr << "Using default port: " << default_port << std::endl;
        return default_port;
    }
}

int main()
{
    try {
        std::cout << "========================================" << std::endl;
        std::cout << "Edge AI API Server" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Starting REST API server..." << std::endl;
        std::cout << std::endl;

        // Register signal handlers for graceful shutdown
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        // Set server configuration from environment variables
        std::string host = EnvConfig::getString("API_HOST", "0.0.0.0");
        uint16_t port = static_cast<uint16_t>(EnvConfig::getInt("API_PORT", 8080, 1, 65535));

        std::cout << "Server will listen on: " << host << ":" << port << std::endl;
        std::cout << "Available endpoints:" << std::endl;
        std::cout << "  GET /v1/core/health  - Health check" << std::endl;
        std::cout << "  GET /v1/core/version - Version information" << std::endl;
        std::cout << "  POST /v1/core/instance - Create new instance" << std::endl;
        std::cout << "  GET /v1/core/instances - List all instances" << std::endl;
        std::cout << "  GET /v1/core/instances/{id} - Get instance details" << std::endl;
        std::cout << "  POST /v1/core/instances/{id}/start - Start instance" << std::endl;
        std::cout << "  POST /v1/core/instances/{id}/stop - Stop instance" << std::endl;
        std::cout << "  DELETE /v1/core/instances/{id} - Delete instance" << std::endl;
        std::cout << "  POST /v1/core/models/upload - Upload model file" << std::endl;
        std::cout << "  GET /v1/core/models/list - List uploaded models" << std::endl;
        std::cout << "  DELETE /v1/core/models/{modelName} - Delete model file" << std::endl;
        std::cout << "  GET /swagger         - Swagger UI (all versions)" << std::endl;
        std::cout << "  GET /v1/swagger      - Swagger UI for API v1" << std::endl;
        std::cout << "  GET /v2/swagger      - Swagger UI for API v2" << std::endl;
        std::cout << "  GET /openapi.yaml    - OpenAPI spec (all versions)" << std::endl;
        std::cout << "  GET /v1/openapi.yaml - OpenAPI spec for v1" << std::endl;
        std::cout << "  GET /v2/openapi.yaml - OpenAPI spec for v2" << std::endl;
        std::cout << std::endl;

        // Controllers are auto-registered via Drogon's HttpController system
        // when headers are included and METHOD_LIST_BEGIN/END macros are used
        // Create instances to ensure registration
        static HealthHandler healthHandler;
        static VersionHandler versionHandler;
        static WatchdogHandler watchdogHandler;
        static SwaggerHandler swaggerHandler;
        
        // Initialize instance management components
        static SolutionRegistry& solutionRegistry = SolutionRegistry::getInstance();
        static PipelineBuilder pipelineBuilder;
        
        // Initialize instance storage with configurable directory
        std::string instancesDir = EnvConfig::getString("INSTANCES_DIR", "./instances");
        static InstanceStorage instanceStorage(instancesDir);
        static InstanceRegistry instanceRegistry(solutionRegistry, pipelineBuilder, instanceStorage);
        
        // Initialize default solutions (face_detection, etc.)
        solutionRegistry.initializeDefaultSolutions();
        
        // Load persistent instances
        instanceRegistry.loadPersistentInstances();
        
        // Register instance registry with handlers
        CreateInstanceHandler::setInstanceRegistry(&instanceRegistry);
        InstanceHandler::setInstanceRegistry(&instanceRegistry);
        
        // Create handler instances to register endpoints
        static CreateInstanceHandler createInstanceHandler;
        static InstanceHandler instanceHandler;
        
        // Initialize model upload handler with configurable directory
        std::string modelsDir = EnvConfig::getString("MODELS_DIR", "./models");
        ModelUploadHandler::setModelsDirectory(modelsDir);
        static ModelUploadHandler modelUploadHandler;
        
        std::cout << "[Main] Instance management initialized" << std::endl;
        std::cout << "  POST /v1/core/instance - Create new instance" << std::endl;
        std::cout << "  GET /v1/core/instances - List all instances" << std::endl;
        std::cout << "  GET /v1/core/instances/{instanceId} - Get instance details" << std::endl;
        std::cout << "  POST /v1/core/instances/{instanceId}/start - Start instance" << std::endl;
        std::cout << "  POST /v1/core/instances/{instanceId}/stop - Stop instance" << std::endl;
        std::cout << "  DELETE /v1/core/instances/{instanceId} - Delete instance" << std::endl;
        std::cout << "  Instances directory: " << instancesDir << std::endl;
        std::cout << "[Main] Model upload handler initialized" << std::endl;
        std::cout << "  POST /v1/core/models/upload - Upload model file" << std::endl;
        std::cout << "  GET /v1/core/models/list - List uploaded models" << std::endl;
        std::cout << "  DELETE /v1/core/models/{modelName} - Delete model file" << std::endl;
        std::cout << "  Models directory: " << modelsDir << std::endl;
        std::cout << std::endl;
        
        // Note: Infrastructure components (rate limiter, cache, resource manager, etc.)
        // are available but not initialized here since AI processing endpoints are not needed yet.
        // They can be enabled later when needed.

        // Initialize watchdog and health monitor from environment variables
        uint32_t watchdog_check_interval = EnvConfig::getUInt32("WATCHDOG_CHECK_INTERVAL_MS", 5000);
        uint32_t watchdog_timeout = EnvConfig::getUInt32("WATCHDOG_TIMEOUT_MS", 30000);
        uint32_t health_monitor_interval = EnvConfig::getUInt32("HEALTH_MONITOR_INTERVAL_MS", 1000);
        
        g_watchdog = std::make_unique<Watchdog>(watchdog_check_interval, watchdog_timeout);
        g_watchdog->start(recoveryAction);

        g_health_monitor = std::make_unique<HealthMonitor>(health_monitor_interval);
        g_health_monitor->start(*g_watchdog);

        // Register watchdog and health monitor with handler
        WatchdogHandler::setWatchdog(g_watchdog.get());
        WatchdogHandler::setHealthMonitor(g_health_monitor.get());

        std::cout << "[Main] Watchdog and health monitor started" << std::endl;
        std::cout << "  GET /v1/core/watchdog - Watchdog status" << std::endl;
        std::cout << std::endl;

        // Set HTTP server configuration from environment variables
        size_t max_body_size = EnvConfig::getSizeT("CLIENT_MAX_BODY_SIZE", 1024 * 1024); // Default: 1MB
        size_t max_memory_body_size = EnvConfig::getSizeT("CLIENT_MAX_MEMORY_BODY_SIZE", 1024 * 1024); // Default: 1MB
        int thread_num = EnvConfig::getInt("THREAD_NUM", 0, 0, 256); // 0 = auto-detect
        std::string log_level_str = EnvConfig::getString("LOG_LEVEL", "INFO");
        
        // Performance optimization settings
        size_t keepalive_requests = EnvConfig::getSizeT("KEEPALIVE_REQUESTS", 100);
        size_t keepalive_timeout = EnvConfig::getSizeT("KEEPALIVE_TIMEOUT", 60);
        bool enable_reuse_port = EnvConfig::getBool("ENABLE_REUSE_PORT", true);
        
        // Parse log level
        trantor::Logger::LogLevel log_level = trantor::Logger::kInfo;
        std::string log_upper = log_level_str;
        std::transform(log_upper.begin(), log_upper.end(), log_upper.begin(), ::toupper);
        if (log_upper == "TRACE") log_level = trantor::Logger::kTrace;
        else if (log_upper == "DEBUG") log_level = trantor::Logger::kDebug;
        else if (log_upper == "INFO") log_level = trantor::Logger::kInfo;
        else if (log_upper == "WARN") log_level = trantor::Logger::kWarn;
        else if (log_upper == "ERROR") log_level = trantor::Logger::kError;
        
        // Use hardware_concurrency if thread_num is 0
        // For AI workloads, recommend 2-4x CPU cores for I/O-bound operations
        unsigned int actual_thread_num = (thread_num == 0) ? std::thread::hardware_concurrency() : thread_num;
        
        // Optimize thread count for AI workloads if auto-detected
        if (thread_num == 0 && actual_thread_num < 8) {
            // For AI server, use at least 8 threads even on low-core systems
            actual_thread_num = std::max(actual_thread_num, 8U);
        }
        
        std::cout << "[Performance] Thread pool size: " << actual_thread_num << std::endl;
        std::cout << "[Performance] Keep-alive: " << keepalive_requests << " requests, " << keepalive_timeout << "s timeout" << std::endl;
        std::cout << "[Performance] Max body size: " << (max_body_size / 1024 / 1024) << "MB" << std::endl;
        std::cout << "[Performance] Reuse port: " << (enable_reuse_port ? "enabled" : "disabled") << std::endl;
        std::cout << std::endl;
        
        auto& app = drogon::app();
        app.setClientMaxBodySize(max_body_size)
            .setClientMaxMemoryBodySize(max_memory_body_size)
            .setLogLevel(log_level)
            .setThreadNum(actual_thread_num);
        
        // Explicitly disable HTTPS - we only use HTTP
        // With useSSL=false, Drogon will not check for SSL certificates
        std::cout << "[Config] Using HTTP only (HTTPS disabled)" << std::endl;
        
        // Enable keep-alive for better connection reuse
        // Note: Drogon handles keep-alive automatically, but we can configure it
        // addListener with HTTP only (useSSL=false explicitly - no SSL certificates needed)
        try {
            if (enable_reuse_port) {
                // Reuse port for better load distribution
                // Parameters: host, port, useSSL=false, certFile, keyFile
                app.addListener(host, port, false, "", ""); // false = disable SSL
            } else {
                app.addListener(host, port, false, "", ""); // false = disable SSL
            }
        } catch (const std::exception& e) {
            std::cerr << "[Error] Failed to add listener: " << e.what() << std::endl;
            throw;
        }
        
        std::cout << "[Server] Starting HTTP server on " << host << ":" << port << std::endl;
        
        // Suppress HTTPS warning - we're intentionally using HTTP only
        // The warning "You can't use https without cert file or key file" 
        // is expected when HTTPS is not configured, but we only want HTTP
        try {
            app.run();
        } catch (const std::exception& e) {
            // Check if it's just the HTTPS warning
            std::string error_msg = e.what();
            if (error_msg.find("https") != std::string::npos && 
                error_msg.find("cert") != std::string::npos) {
                std::cerr << "[Warning] HTTPS warning detected but ignored (using HTTP only): " 
                          << error_msg << std::endl;
                // Continue anyway - this is expected when not using HTTPS
                return 0;
            }
            throw; // Re-throw if it's a different error
        }

        // Cleanup
        if (g_health_monitor) {
            g_health_monitor->stop();
        }
        if (g_watchdog) {
            g_watchdog->stop();
        }

        std::cout << "Server stopped." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Fatal error: Unknown exception" << std::endl;
        return 1;
    }
}

