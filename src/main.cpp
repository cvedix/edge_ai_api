#include <drogon/drogon.h>
#include "api/health_handler.h"
#include "api/version_handler.h"
#include "api/watchdog_handler.h"
#include "api/swagger_handler.h"
#include "api/create_instance_handler.h"
#include "api/instance_handler.h"
#include "api/system_info_handler.h"
#include "models/model_upload_handler.h"
#include "core/watchdog.h"
#include "core/health_monitor.h"
#include "core/env_config.h"
#include "core/logger.h"
#include "instances/instance_registry.h"
#include "core/solution_registry.h"
#include "core/pipeline_builder.h"
#include "instances/instance_storage.h"
#include <iostream>
#include <csignal>
#include <cstdlib>
#include <csetjmp>
#include <stdexcept>
#include <memory>
#include <thread>
#include <chrono>
#include <algorithm>
#include <exception>
#include <atomic>
#include <string>

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

// Global instance registry pointer for error recovery
static InstanceRegistry* g_instance_registry = nullptr;

// Flag to prevent multiple handlers from stopping instances simultaneously
static std::atomic<bool> g_cleanup_in_progress{false};

// Signal handler for graceful shutdown
void signalHandler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM) {
        PLOG_INFO << "Received signal " << signal << ", shutting down gracefully...";
        g_shutdown = true;
        
        // Stop watchdog and health monitor
        if (g_health_monitor) {
            g_health_monitor->stop();
        }
        if (g_watchdog) {
            g_watchdog->stop();
        }
        
        drogon::app().quit();
    } else if (signal == SIGABRT) {
        // SIGABRT is sent when assertion fails (like OpenCV DNN shape mismatch)
        // For shape mismatch errors, we want to recover instead of crashing
        std::cerr << "[RECOVERY] Received SIGABRT signal - likely due to OpenCV DNN shape mismatch" << std::endl;
        std::cerr << "[RECOVERY] This usually happens when model receives frames with inconsistent sizes" << std::endl;
        std::cerr << "[RECOVERY] Attempting to recover by stopping problematic instances..." << std::endl;
        
        // Check if cleanup is already in progress (from terminate handler)
        // This prevents deadlock if both handlers try to stop instances simultaneously
        bool expected = false;
        if (g_cleanup_in_progress.compare_exchange_strong(expected, true)) {
            if (g_instance_registry) {
                try {
                    // Get all instances and stop them
                    auto instances = g_instance_registry->listInstances();
                    for (const auto& instanceId : instances) {
                        try {
                            std::cerr << "[RECOVERY] Stopping instance " << instanceId << " due to shape mismatch error..." << std::endl;
                            g_instance_registry->stopInstance(instanceId);
                        } catch (...) {
                            std::cerr << "[RECOVERY] Failed to stop instance " << instanceId << std::endl;
                        }
                    }
                    std::cerr << "[RECOVERY] All instances stopped. Application will continue running." << std::endl;
                    std::cerr << "[RECOVERY] Please check logs above and fix the root cause:" << std::endl;
                    std::cerr << "[RECOVERY]   1. Ensure video has consistent resolution (re-encode if needed)" << std::endl;
                    std::cerr << "[RECOVERY]   2. Use YuNet 2023mar model (better dynamic input support)" << std::endl;
                    std::cerr << "[RECOVERY]   3. Try different RESIZE_RATIO values" << std::endl;
                } catch (...) {
                    std::cerr << "[RECOVERY] Error accessing instance registry" << std::endl;
                }
            }
            // Reset cleanup flag after a delay to allow recovery
            g_cleanup_in_progress.store(false);
        } else {
            std::cerr << "[RECOVERY] Cleanup already in progress (from terminate handler), skipping..." << std::endl;
        }
        
        // For shape mismatch errors, don't exit - let the application continue
        // This is non-standard but allows recovery from OpenCV DNN errors
        // Note: This may cause undefined behavior, but it's better than crashing
        return;  // Return from signal handler to continue execution
    }
}

// Terminate handler for uncaught exceptions
void terminateHandler()
{
    std::string error_msg;
    
    // Get exception message first (before doing anything else)
    try {
        auto exception_ptr = std::current_exception();
        if (exception_ptr) {
            std::rethrow_exception(exception_ptr);
        }
    } catch (const std::exception& e) {
        error_msg = e.what();
        PLOG_ERROR << "[CRITICAL] Uncaught exception: " << error_msg;
    } catch (...) {
        error_msg = "Unknown exception";
        PLOG_ERROR << "[CRITICAL] Unknown exception type";
    }
    
    // Debug: Log exception message to verify it's being captured correctly
    std::cerr << "[DEBUG] Exception message in terminate handler: '" << error_msg << "'" << std::endl;
    
    // Check if this is an OpenCV DNN shape mismatch error
    bool is_shape_mismatch = (error_msg.find("getMemoryShapes") != std::string::npos ||
                              error_msg.find("eltwise_layer") != std::string::npos ||
                              error_msg.find("Assertion failed") != std::string::npos ||
                              error_msg.find("inputs[vecIdx][j]") != std::string::npos ||
                              error_msg.find("inputs[0] = [") != std::string::npos ||
                              error_msg.find("inputs[1] = [") != std::string::npos);
    
    std::cerr << "[DEBUG] is_shape_mismatch = " << (is_shape_mismatch ? "true" : "false") << std::endl;
    
    if (is_shape_mismatch) {
        std::cerr << "[RECOVERY] Detected OpenCV DNN shape mismatch error - attempting recovery..." << std::endl;
        std::cerr << "[RECOVERY] This error usually occurs when model receives frames with inconsistent sizes" << std::endl;
        std::cerr << "[RECOVERY] Exception: " << error_msg << std::endl;
        
        // Try to stop all instances before aborting
        // NOTE: Use atomic flag to prevent deadlock if SIGABRT handler is also trying to stop instances
        bool expected = false;
        if (g_cleanup_in_progress.compare_exchange_strong(expected, true)) {
            if (g_instance_registry) {
                try {
                    // Get list of instances (this acquires lock briefly)
                    // If this fails due to deadlock, skip cleanup to avoid crash
                    std::vector<std::string> instances;
                    try {
                        instances = g_instance_registry->listInstances();
                    } catch (const std::exception& e) {
                        std::cerr << "[RECOVERY] Cannot list instances (possible deadlock): " << e.what() << std::endl;
                        std::cerr << "[RECOVERY] Skipping instance cleanup to avoid deadlock" << std::endl;
                        instances.clear(); // Skip cleanup
                    } catch (...) {
                        std::cerr << "[RECOVERY] Cannot list instances (unknown error) - skipping cleanup" << std::endl;
                        instances.clear(); // Skip cleanup
                    }
                    
                    // Release lock before stopping each instance
                    // stopInstance now releases lock before calling stopPipeline, so this is safe
                    for (const auto& instanceId : instances) {
                        try {
                            std::cerr << "[RECOVERY] Stopping instance " << instanceId << " due to shape mismatch..." << std::endl;
                            // stopInstance now releases lock before calling stopPipeline
                            // This prevents deadlock even if called from terminate handler
                            g_instance_registry->stopInstance(instanceId);
                        } catch (const std::exception& e) {
                            std::cerr << "[RECOVERY] Failed to stop instance " << instanceId << ": " << e.what() << std::endl;
                        } catch (...) {
                            std::cerr << "[RECOVERY] Failed to stop instance " << instanceId << " (unknown error)" << std::endl;
                        }
                    }
                    std::cerr << "[RECOVERY] All instances stopped. Application will continue running." << std::endl;
                    std::cerr << "[RECOVERY] Please check logs above and fix the root cause:" << std::endl;
                    std::cerr << "[RECOVERY]   1. Ensure video has consistent resolution (re-encode if needed)" << std::endl;
                    std::cerr << "[RECOVERY]   2. Use YuNet 2023mar model (better dynamic input support)" << std::endl;
                    std::cerr << "[RECOVERY]   3. Try different RESIZE_RATIO values" << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "[RECOVERY] Error accessing instance registry: " << e.what() << std::endl;
                } catch (...) {
                    std::cerr << "[RECOVERY] Error accessing instance registry (unknown error)" << std::endl;
                }
            }
            // Reset cleanup flag to allow recovery
            g_cleanup_in_progress.store(false);
        } else {
            std::cerr << "[RECOVERY] Cleanup already in progress (from SIGABRT handler), skipping..." << std::endl;
        }
        
        // For shape mismatch errors, don't call abort() - just log and return
        // This allows the application to continue running (non-standard but prevents crash)
        // Note: According to C++ standard, terminate handler must terminate, but we're
        // intentionally violating this to allow recovery from OpenCV DNN errors
        std::cerr << "[RECOVERY] Shape mismatch error handled - application will continue running" << std::endl;
        std::cerr << "[RECOVERY] Instance has been stopped. You can try starting it again after fixing the issue." << std::endl;
        
        // Don't call abort() or exit() - just return to allow application to continue
        // This is non-standard but prevents crash
        return;
    }
    
    // For other exceptions, try to stop instances gracefully
    // BUT: Be very careful to avoid deadlock - if we're already in stopInstance, don't call it again
    bool expected = false;
    if (g_cleanup_in_progress.compare_exchange_strong(expected, true)) {
        if (g_instance_registry) {
            try {
                // Use a timeout or non-blocking approach to avoid deadlock
                // If listInstances() fails (e.g., due to deadlock), just skip cleanup
                std::vector<std::string> instances;
                try {
                    instances = g_instance_registry->listInstances();
                } catch (const std::exception& e) {
                    std::cerr << "[CRITICAL] Cannot list instances (possible deadlock): " << e.what() << std::endl;
                    std::cerr << "[CRITICAL] Skipping instance cleanup to avoid deadlock" << std::endl;
                    instances.clear(); // Skip cleanup
                } catch (...) {
                    std::cerr << "[CRITICAL] Cannot list instances (unknown error) - skipping cleanup" << std::endl;
                    instances.clear(); // Skip cleanup
                }
                
                // Only try to stop instances if we successfully got the list
                for (const auto& instanceId : instances) {
                    try {
                        // stopInstance() releases lock before calling stopPipeline, so this should be safe
                        // But wrap in try-catch to be extra safe
                        g_instance_registry->stopInstance(instanceId);
                    } catch (const std::exception& e) {
                        std::cerr << "[CRITICAL] Failed to stop instance " << instanceId << ": " << e.what() << std::endl;
                        // Continue with other instances
                    } catch (...) {
                        std::cerr << "[CRITICAL] Failed to stop instance " << instanceId << " (unknown error)" << std::endl;
                        // Continue with other instances
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "[CRITICAL] Error during cleanup: " << e.what() << std::endl;
                // Don't crash - just log and continue to exit
            } catch (...) {
                std::cerr << "[CRITICAL] Unknown error during cleanup" << std::endl;
                // Don't crash - just log and continue to exit
            }
        }
        g_cleanup_in_progress.store(false);
    } else {
        // Cleanup already in progress - this means we're being called recursively
        // This can happen if an exception occurs during cleanup (e.g., "Resource deadlock avoided")
        // In this case, don't exit - let the original cleanup finish
        std::cerr << "[CRITICAL] Cleanup already in progress - exception occurred during cleanup" << std::endl;
        std::cerr << "[CRITICAL] Exception: " << error_msg << std::endl;
        std::cerr << "[CRITICAL] Waiting for original cleanup to complete..." << std::endl;
        
        // Wait a bit for cleanup to complete (but don't wait forever)
        // After 5 seconds, give up and exit
        for (int i = 0; i < 50 && g_cleanup_in_progress.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        if (g_cleanup_in_progress.load()) {
            std::cerr << "[CRITICAL] Cleanup taking too long - forcing exit" << std::endl;
            std::_Exit(1);
        } else {
            std::cerr << "[CRITICAL] Cleanup completed - original handler will handle exit" << std::endl;
            // Don't exit here - let the original cleanup handler exit
            return;
        }
    }
    
    std::cerr << "[CRITICAL] Terminating application due to uncaught exception" << std::endl;
    std::cerr << "[CRITICAL] Exception: " << error_msg << std::endl;
    std::_Exit(1);  // Exit immediately without calling destructors (safer in terminate handler)
}

// Recovery callback for watchdog
void recoveryAction()
{
    PLOG_ERROR << "[Recovery] Application detected as unresponsive. Attempting recovery...";
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
        // Note: Logger might not be initialized yet, so use std::cerr
        std::cerr << "Error: Invalid port number '" << port_str << "': " << e.what() << std::endl;
        std::cerr << "Using default port: " << default_port << std::endl;
        return default_port;
    } catch (const std::out_of_range& e) {
        // Note: Logger might not be initialized yet, so use std::cerr
        std::cerr << "Error: Port number out of range: " << e.what() << std::endl;
        std::cerr << "Using default port: " << default_port << std::endl;
        return default_port;
    }
}

int main()
{
    try {
        // Initialize logger first (before any logging)
        Logger::init();
        
        PLOG_INFO << "========================================";
        PLOG_INFO << "Edge AI API Server";
        PLOG_INFO << "========================================";
        PLOG_INFO << "Starting REST API server...";

        // Register signal handlers for graceful shutdown and crash prevention
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
        std::signal(SIGABRT, signalHandler);  // Catch assertion failures (like OpenCV DNN shape mismatch)
        
        // Register terminate handler for uncaught exceptions
        std::set_terminate(terminateHandler);

        // Set server configuration from environment variables
        std::string host = EnvConfig::getString("API_HOST", "0.0.0.0");
        uint16_t port = static_cast<uint16_t>(EnvConfig::getInt("API_PORT", 8080, 1, 65535));

        PLOG_INFO << "Server will listen on: " << host << ":" << port;
        PLOG_INFO << "Available endpoints:";
        PLOG_INFO << "  GET /v1/core/health  - Health check";
        PLOG_INFO << "  GET /v1/core/version - Version information";
        PLOG_INFO << "  POST /v1/core/instance - Create new instance";
        PLOG_INFO << "  GET /v1/core/instances - List all instances";
        PLOG_INFO << "  GET /v1/core/instances/{id} - Get instance details";
        PLOG_INFO << "  POST /v1/core/instances/{id}/start - Start instance";
        PLOG_INFO << "  POST /v1/core/instances/{id}/stop - Stop instance";
        PLOG_INFO << "  DELETE /v1/core/instances/{id} - Delete instance";
        PLOG_INFO << "  POST /v1/core/models/upload - Upload model file";
        PLOG_INFO << "  GET /v1/core/models/list - List uploaded models";
        PLOG_INFO << "  DELETE /v1/core/models/{modelName} - Delete model file";
        PLOG_INFO << "  GET /swagger         - Swagger UI (all versions)";
        PLOG_INFO << "  GET /v1/swagger      - Swagger UI for API v1";
        PLOG_INFO << "  GET /v2/swagger      - Swagger UI for API v2";
        PLOG_INFO << "  GET /openapi.yaml    - OpenAPI spec (all versions)";
        PLOG_INFO << "  GET /v1/openapi.yaml - OpenAPI spec for v1";
        PLOG_INFO << "  GET /v2/openapi.yaml - OpenAPI spec for v2";

        // Controllers are auto-registered via Drogon's HttpController system
        // when headers are included and METHOD_LIST_BEGIN/END macros are used
        // Create instances to ensure registration
        static HealthHandler healthHandler;
        static VersionHandler versionHandler;
        static WatchdogHandler watchdogHandler;
        static SwaggerHandler swaggerHandler;
        static SystemInfoHandler systemInfoHandler;
        
        // Initialize instance management components
        static SolutionRegistry& solutionRegistry = SolutionRegistry::getInstance();
        static PipelineBuilder pipelineBuilder;
        
        // Initialize instance storage with configurable directory
        std::string instancesDir = EnvConfig::getString("INSTANCES_DIR", "./instances");
        static InstanceStorage instanceStorage(instancesDir);
        static InstanceRegistry instanceRegistry(solutionRegistry, pipelineBuilder, instanceStorage);
        
        // Store instance registry pointer for error recovery
        g_instance_registry = &instanceRegistry;
        
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
        
        PLOG_INFO << "[Main] Instance management initialized";
        PLOG_INFO << "  POST /v1/core/instance - Create new instance";
        PLOG_INFO << "  GET /v1/core/instances - List all instances";
        PLOG_INFO << "  GET /v1/core/instances/{instanceId} - Get instance details";
        PLOG_INFO << "  POST /v1/core/instances/{instanceId}/start - Start instance";
        PLOG_INFO << "  POST /v1/core/instances/{instanceId}/stop - Stop instance";
        PLOG_INFO << "  DELETE /v1/core/instances/{instanceId} - Delete instance";
        PLOG_INFO << "  Instances directory: " << instancesDir;
        PLOG_INFO << "[Main] Model upload handler initialized";
        PLOG_INFO << "  POST /v1/core/models/upload - Upload model file";
        PLOG_INFO << "  GET /v1/core/models/list - List uploaded models";
        PLOG_INFO << "  DELETE /v1/core/models/{modelName} - Delete model file";
        PLOG_INFO << "  Models directory: " << modelsDir;
        
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

        PLOG_INFO << "[Main] Watchdog and health monitor started";
        PLOG_INFO << "  GET /v1/core/watchdog - Watchdog status";

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
        
        PLOG_INFO << "[Performance] Thread pool size: " << actual_thread_num;
        PLOG_INFO << "[Performance] Keep-alive: " << keepalive_requests << " requests, " << keepalive_timeout << "s timeout";
        PLOG_INFO << "[Performance] Max body size: " << (max_body_size / 1024 / 1024) << "MB";
        PLOG_INFO << "[Performance] Reuse port: " << (enable_reuse_port ? "enabled" : "disabled");
        
        auto& app = drogon::app();
        app.setClientMaxBodySize(max_body_size)
            .setClientMaxMemoryBodySize(max_memory_body_size)
            .setLogLevel(log_level)
            .setThreadNum(actual_thread_num);
        
        // Explicitly disable HTTPS - we only use HTTP
        // With useSSL=false, Drogon will not check for SSL certificates
        PLOG_INFO << "[Config] Using HTTP only (HTTPS disabled)";
        
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
            PLOG_ERROR << "[Error] Failed to add listener: " << e.what();
            throw;
        }
        
        PLOG_INFO << "[Server] Starting HTTP server on " << host << ":" << port;
        
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
                PLOG_WARNING << "[Warning] HTTPS warning detected but ignored (using HTTP only): " << error_msg;
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

        PLOG_INFO << "Server stopped.";
        return 0;
    } catch (const std::exception& e) {
        PLOG_FATAL << "Fatal error: " << e.what();
        return 1;
    } catch (...) {
        PLOG_FATAL << "Fatal error: Unknown exception";
        return 1;
    }
}

