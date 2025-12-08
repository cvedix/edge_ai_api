#include <drogon/drogon.h>
#include "api/health_handler.h"
#include "api/version_handler.h"
#include "api/watchdog_handler.h"
#include "api/swagger_handler.h"
#include "api/create_instance_handler.h"
#include "api/instance_handler.h"
#include "api/system_info_handler.h"
#include "api/solution_handler.h"
#include "api/group_handler.h"
#include "api/log_handler.h"
#include "models/model_upload_handler.h"
#include "core/watchdog.h"
#include "core/health_monitor.h"
#include "core/env_config.h"
#include "core/logger.h"
#include "core/categorized_logger.h"
#include "core/logging_flags.h"
#include "instances/instance_registry.h"
#include "solutions/solution_registry.h"
#include "core/pipeline_builder.h"
#include "instances/instance_storage.h"
#include "solutions/solution_storage.h"
#include "groups/group_registry.h"
#include "groups/group_storage.h"
#include <cvedix/utils/analysis_board/cvedix_analysis_board.h>
#include <cvedix/nodes/src/cvedix_rtsp_src_node.h>
#include <cvedix/nodes/src/cvedix_file_src_node.h>
#include <iostream>
#include <csignal>
#include <cstdlib>
#include <csetjmp>
#include <unistd.h>
#include <sys/types.h>
#include <stdexcept>
#include <memory>
#include <thread>
#include <chrono>
#include <algorithm>
#include <exception>
#include <atomic>
#include <string>
#include <future>
#include <vector>
#include <fstream>

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

// Flag to indicate force exit requested (bypass SIGABRT recovery)
static std::atomic<bool> g_force_exit{false};

// Global debug flag
static std::atomic<bool> g_debug_mode{false};

// Global logging flags (exported via logging_flags.h)
std::atomic<bool> g_log_api{false};
std::atomic<bool> g_log_instance{false};
std::atomic<bool> g_log_sdk_output{false};

// Global analysis board thread management
static std::unique_ptr<std::thread> g_analysis_board_display_thread;
static std::atomic<bool> g_stop_analysis_board{false};
static std::atomic<bool> g_analysis_board_running{false};
static std::atomic<bool> g_analysis_board_disabled{false}; // Flag to disable after Qt abort

// Signal handler for graceful shutdown
void signalHandler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM) {
        static std::atomic<int> signal_count{0};
        static std::atomic<bool> signal_handling{false};
        
        // Prevent multiple signal handlers from running simultaneously
        // Use compare_exchange to ensure only one handler runs at a time
        bool expected = false;
        if (!signal_handling.compare_exchange_strong(expected, true)) {
            // Another handler is already running, just increment count and return
            signal_count.fetch_add(1);
            return;
        }
        
        int count = signal_count.fetch_add(1) + 1;
        
        if (count == 1) {
            // First signal - attempt graceful shutdown
            PLOG_INFO << "Received signal " << signal << ", shutting down gracefully...";
            std::cerr << "[SHUTDOWN] Received signal " << signal << ", shutting down gracefully..." << std::endl;
            std::cerr << "[SHUTDOWN] Press Ctrl+C again to force immediate exit" << std::endl;
            g_shutdown = true;
            
            // CRITICAL: Call quit() immediately in signal handler (thread-safe in Drogon)
            // This ensures the main event loop exits even if cleanup threads are slow
            try {
                auto& app = drogon::app();
                app.quit();
                // Also try to stop the event loop explicitly
                app.getLoop()->quit();
            } catch (...) {
                // Ignore errors - try to exit anyway
            }
            
            // Stop all instances first (this is critical for clean shutdown)
            // Use a separate thread with timeout to avoid blocking
            std::thread([]() {
                if (g_instance_registry) {
                    try {
                        std::cerr << "[SHUTDOWN] Stopping all instances (timeout: 3 seconds)..." << std::endl;
                        PLOG_INFO << "Stopping all instances before shutdown...";
                        
                        // Get list of instances (with timeout protection)
                        std::vector<std::string> instances;
                        try {
                            instances = g_instance_registry->listInstances();
                        } catch (...) {
                            std::cerr << "[SHUTDOWN] Warning: Cannot list instances, skipping..." << std::endl;
                            instances.clear();
                        }
                        
                        // Stop instances with timeout protection
                        // Use async with timeout to prevent blocking
                        int stopped_count = 0;
                        for (const auto& instanceId : instances) {
                            if (g_force_exit.load()) break; // Check if force exit was requested
                            
                            try {
                                auto optInfo = g_instance_registry->getInstance(instanceId);
                                if (optInfo.has_value() && optInfo.value().running) {
                                    std::cerr << "[SHUTDOWN] Stopping instance: " << instanceId << std::endl;
                                    PLOG_INFO << "Stopping instance: " << instanceId;
                                    
                                    // Use async with timeout to prevent blocking
                                    // Capture instanceId by value and use global registry
                                    auto future = std::async(std::launch::async, [instanceId]() -> bool {
                                        try {
                                            if (g_instance_registry) {
                                                g_instance_registry->stopInstance(instanceId);
                                                return true;
                                            }
                                            return false;
                                        } catch (...) {
                                            return false;
                                        }
                                    });
                                    
                                    // Wait with timeout (1 second per instance)
                                    auto status = future.wait_for(std::chrono::seconds(1));
                                    if (status == std::future_status::timeout) {
                                        std::cerr << "[SHUTDOWN] Warning: Instance " << instanceId << " stop timeout (1s), skipping..." << std::endl;
                                        // Don't wait for it - continue with other instances
                                    } else if (status == std::future_status::ready) {
                                        try {
                                            if (future.get()) {
                                                stopped_count++;
                                            }
                                        } catch (...) {
                                            // Ignore errors from get()
                                        }
                                    }
                                }
                            } catch (const std::exception& e) {
                                PLOG_WARNING << "Failed to stop instance " << instanceId << ": " << e.what();
                                std::cerr << "[SHUTDOWN] Warning: Failed to stop instance " << instanceId << ": " << e.what() << std::endl;
                            } catch (...) {
                                PLOG_WARNING << "Failed to stop instance " << instanceId << " (unknown error)";
                                std::cerr << "[SHUTDOWN] Warning: Failed to stop instance " << instanceId << " (unknown error)" << std::endl;
                            }
                        }
                        std::cerr << "[SHUTDOWN] Stopped " << stopped_count << " instance(s)" << std::endl;
                        PLOG_INFO << "All instances stopped";
                    } catch (const std::exception& e) {
                        PLOG_WARNING << "Error stopping instances: " << e.what();
                        std::cerr << "[SHUTDOWN] Warning: Error stopping instances: " << e.what() << std::endl;
                    } catch (...) {
                        PLOG_WARNING << "Error stopping instances (unknown error)";
                        std::cerr << "[SHUTDOWN] Warning: Error stopping instances (unknown error)" << std::endl;
                    }
                }
                
                // Stop watchdog and health monitor (quick, should not block)
                if (g_health_monitor) {
                    try {
                        g_health_monitor->stop();
                    } catch (...) {
                        // Ignore errors
                    }
                }
                if (g_watchdog) {
                    try {
                        g_watchdog->stop();
                    } catch (...) {
                        // Ignore errors
                    }
                }
            }).detach();
            
            // Start shutdown timer thread - force exit after 3 seconds if still running
            // Give instances time to stop gracefully, but not too long
            std::thread([]() {
                std::this_thread::sleep_for(std::chrono::seconds(3));
                if (g_shutdown && !g_force_exit.load()) {
                    PLOG_WARNING << "Shutdown timeout reached - forcing exit";
                    std::cerr << "[CRITICAL] Shutdown timeout (3s) - KILLING PROCESS NOW" << std::endl;
                    std::fflush(stdout);
                    std::fflush(stderr);
                    
                    // Set force exit flag
                    g_force_exit = true;
                    
                    // Unregister all signal handlers to prevent recovery
                    std::signal(SIGINT, SIG_DFL);
                    std::signal(SIGTERM, SIG_DFL);
                    std::signal(SIGABRT, SIG_DFL);
                    
                    // Use _exit() immediately - no need to call quit() again
                    _exit(1);
                }
            }).detach();
        } else {
            // Second signal (Ctrl+C again) - force immediate exit
            // This should happen immediately, no delays, no cleanup
            PLOG_WARNING << "Received signal " << signal << " again (" << count << " times) - forcing immediate exit";
            std::cerr << "[CRITICAL] Force exit requested (signal received " << count << " times) - KILLING PROCESS NOW" << std::endl;
            std::fflush(stdout);
            std::fflush(stderr);
            
            // Set force exit flag to bypass SIGABRT recovery
            g_force_exit = true;
            
            // Unregister all signal handlers to prevent any recovery logic
            std::signal(SIGINT, SIG_DFL);
            std::signal(SIGTERM, SIG_DFL);
            std::signal(SIGABRT, SIG_DFL);
            
            // Use _exit() immediately - this bypasses all cleanup and exits immediately
            _exit(1);
        }
    } else if (signal == SIGABRT) {
        // If force exit was requested, exit immediately without recovery
        if (g_force_exit.load()) {
            std::cerr << "[CRITICAL] Force exit confirmed - terminating immediately" << std::endl;
            std::fflush(stdout);
            std::fflush(stderr);
            _exit(1);
        }
        
        // SIGABRT can be triggered by:
        // 1. OpenCV DNN shape mismatch (recover by stopping instances)
        // 2. Qt display error in analysis board (don't stop instances, just disable board)
        
        // Check if analysis board is running - if so, this is likely a Qt display error
        if (g_analysis_board_running.load() || g_debug_mode.load()) {
            std::cerr << "[RECOVERY] Received SIGABRT signal - likely due to Qt display error in analysis board" << std::endl;
            std::cerr << "[RECOVERY] Analysis board cannot connect to display - disabling analysis board" << std::endl;
            std::cerr << "[RECOVERY] Server will continue running - instances are NOT affected" << std::endl;
            
            // Disable analysis board permanently
            g_analysis_board_disabled = true;
            g_analysis_board_running = false;
            
            // Don't stop instances - this is just a display error, not a pipeline error
            // Return to allow server to continue
            return;
        }
        
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
 * @brief Parse command line arguments
 * @param argc Argument count
 * @param argv Argument vector
 * @return true if parsing successful, false if help requested
 */
bool parseArguments(int argc, char* argv[])
{
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--debug" || arg == "-d") {
            g_debug_mode = true;
            std::cerr << "[Main] Debug mode enabled - analysis board will be displayed" << std::endl;
        } else if (arg == "--log-api" || arg == "--debug-api") {
            g_log_api = true;
            std::cerr << "[Main] API logging enabled" << std::endl;
        } else if (arg == "--log-instance" || arg == "--debug-instance") {
            g_log_instance = true;
            std::cerr << "[Main] Instance execution logging enabled" << std::endl;
        } else if (arg == "--log-sdk-output" || arg == "--debug-sdk-output") {
            g_log_sdk_output = true;
            std::cerr << "[Main] SDK output logging enabled" << std::endl;
        } else if (arg == "--help" || arg == "-h") {
            std::cerr << "Usage: " << argv[0] << " [OPTIONS]" << std::endl;
            std::cerr << "Options:" << std::endl;
            std::cerr << "  --debug, -d                    Enable debug mode (display analysis board)" << std::endl;
            std::cerr << "  --log-api, --debug-api              Enable API request/response logging" << std::endl;
            std::cerr << "  --log-instance, --debug-instance     Enable instance execution logging (start/stop/status)" << std::endl;
            std::cerr << "  --log-sdk-output, --debug-sdk-output    Enable SDK output logging (when SDK returns results)" << std::endl;
            std::cerr << "  --help, -h                     Show this help message" << std::endl;
            return false;
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            std::cerr << "Use --help for usage information" << std::endl;
            return false;
        }
    }
    return true;
}

/**
 * @brief Check if display is available and actually working
 */
static bool has_display() {
#if defined(_WIN32)
    return true;
#else
    const char *display = std::getenv("DISPLAY");
    const char *wayland = std::getenv("WAYLAND_DISPLAY");
    
    // Check if DISPLAY or WAYLAND_DISPLAY is set
    if (!display && !wayland) {
        return false;
    }
    
    // Try to verify X server is actually running (for X11)
    if (display && display[0] != '\0') {
        // Check if X11 socket exists
        std::string displayStr(display);
        if (displayStr[0] == ':') {
            std::string socketPath = "/tmp/.X11-unix/X" + displayStr.substr(1);
            // Check if socket file exists
            std::ifstream socketFile(socketPath);
            if (!socketFile.good()) {
                // Socket doesn't exist - X server is not running
                return false;
            }
        }
        
        // Try to test X server connection using xdpyinfo if available
        // This is a more reliable check
        std::string testCmd = "timeout 1 xdpyinfo -display " + std::string(display) + " >/dev/null 2>&1";
        int status = std::system(testCmd.c_str());
        if (status != 0) {
            // xdpyinfo failed or not available - assume display is not accessible
            // Note: If xdpyinfo is not installed, this will fail but we'll still try
            // The actual Qt connection test will catch the real error
            return false;
        }
    }
    
    return true;
#endif
}

/**
 * @brief Thread function to run analysis board display (blocking call)
 */
void runAnalysisBoardDisplay(const std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>>& sourceNodes)
{
    // Set thread name for debugging
    #ifdef __GLIBC__
    pthread_setname_np(pthread_self(), "analysis-board");
    #endif
    
    // Note: We can't easily catch SIGABRT in a thread-specific way
    // Qt will abort if display is not accessible, which will trigger global SIGABRT handler
    // The best we can do is catch exceptions during board creation
    
    // Mark thread as running
    g_analysis_board_running = true;
    
    try {
        PLOG_INFO << "[Debug] Creating analysis board for " << sourceNodes.size() << " running instance(s)";
        
        // Check if analysis board is disabled (due to previous Qt abort)
        if (g_analysis_board_disabled.load()) {
            PLOG_WARNING << "[Debug] Analysis board is disabled (Qt display error detected previously)";
            g_analysis_board_running = false;
            return;
        }
        
        // Double-check display before creating board
        if (!has_display()) {
            PLOG_WARNING << "[Debug] Display not available when creating board - skipping";
            g_analysis_board_running = false;
            return;
        }
        
        // Try to create board - this may throw if display is not accessible
        std::unique_ptr<cvedix_utils::cvedix_analysis_board> board;
        try {
            board = std::make_unique<cvedix_utils::cvedix_analysis_board>(sourceNodes);
            PLOG_INFO << "[Debug] Analysis board created successfully";
        } catch (const std::exception& e) {
            std::cerr << "[Debug] Failed to create analysis board: " << e.what() << std::endl;
            PLOG_ERROR << "[Debug] Failed to create analysis board: " << e.what();
            PLOG_WARNING << "[Debug] This usually means X server is not accessible. Analysis board disabled.";
            g_analysis_board_disabled = true; // Disable permanently to prevent retry
            g_analysis_board_running = false;
            return;
        } catch (...) {
            std::cerr << "[Debug] Unknown error creating analysis board" << std::endl;
            PLOG_ERROR << "[Debug] Unknown error creating analysis board";
            g_analysis_board_disabled = true; // Disable permanently to prevent retry
            g_analysis_board_running = false;
            return;
        }
        
        // CRITICAL: Do NOT call board.display() if display is not accessible
        // Qt will abort if it cannot connect to display, and we can't catch that abort reliably
        // So we must verify display is accessible BEFORE calling display()
        
        // Final check: Test X server connection before calling display()
        const char *display = std::getenv("DISPLAY");
        if (display && display[0] != '\0') {
            // Try to test X server connection using xdpyinfo
            std::string testCmd = "timeout 1 xdpyinfo -display " + std::string(display) + " >/dev/null 2>&1";
            int status = std::system(testCmd.c_str());
            if (status != 0) {
                PLOG_WARNING << "[Debug] X server connection test failed - not calling board.display()";
                PLOG_WARNING << "[Debug] This prevents Qt abort. Analysis board disabled.";
                g_analysis_board_disabled = true;
                g_analysis_board_running = false;
                return;
            }
            PLOG_INFO << "[Debug] X server connection test passed";
        }
        
        // If we reach here, display is verified to be accessible
        // Now we can safely call board.display()
        PLOG_INFO << "[Debug] Starting analysis board display (blocking call)";
        PLOG_INFO << "[Debug] Display verified - calling board.display()";
        
        // Try to display - this should work now since we verified display
        try {
            // display() refreshes every 1 second and doesn't auto-close
            // This is a blocking call that should run until shutdown
            board->display(1, false);
            
            // If we reach here, display() returned (shouldn't happen for blocking call)
            PLOG_WARNING << "[Debug] Analysis board display() returned unexpectedly";
            g_analysis_board_disabled = true;
        } catch (const std::exception& e) {
            std::cerr << "[Debug] Exception displaying analysis board: " << e.what() << std::endl;
            PLOG_ERROR << "[Debug] Exception displaying analysis board: " << e.what();
            g_analysis_board_disabled = true;
        } catch (...) {
            std::cerr << "[Debug] Unknown exception displaying analysis board" << std::endl;
            PLOG_ERROR << "[Debug] Unknown exception displaying analysis board";
            g_analysis_board_disabled = true;
        }
    } catch (const std::exception& e) {
        std::cerr << "[Debug] Error in analysis board thread: " << e.what() << std::endl;
        PLOG_ERROR << "[Debug] Error in analysis board thread: " << e.what();
        // Don't rethrow - just log and exit thread
    } catch (...) {
        std::cerr << "[Debug] Unknown error in analysis board thread" << std::endl;
        PLOG_ERROR << "[Debug] Unknown error in analysis board thread";
        // Don't rethrow - just log and exit thread
    }
    
    // Mark thread as stopped
    g_analysis_board_running = false;
    PLOG_INFO << "[Debug] Analysis board display thread stopped";
}

/**
 * @brief Debug thread function to display analysis board for running instances
 */
void debugAnalysisBoardThread()
{
    if (!g_instance_registry) {
        return;
    }
    
    // Check if display is available
    bool display_available = has_display();
    if (!display_available) {
        PLOG_WARNING << "[Debug] DISPLAY/WAYLAND not found. Analysis board requires display to work.";
        PLOG_WARNING << "[Debug] Analysis board will be disabled. Set DISPLAY or WAYLAND_DISPLAY environment variable to enable.";
        PLOG_WARNING << "[Debug] Example: export DISPLAY=:0 before running server";
        // Keep thread running but don't try to display board
        while (!g_shutdown && g_debug_mode.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        return;
    }
    
    PLOG_INFO << "[Debug] Analysis board thread started (display available)";
    
    size_t lastSourceNodeCount = 0;
    
    while (!g_shutdown && g_debug_mode.load()) {
        try {
            // Get source nodes from all running instances
            auto sourceNodes = g_instance_registry->getSourceNodesFromRunningInstances();
            
            // If we have source nodes, create and display analysis board
            if (!sourceNodes.empty()) {
                // Check if analysis board is disabled
                if (g_analysis_board_disabled.load()) {
                    // Analysis board is disabled - just sleep and check again
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    continue;
                }
                
                // Check if we need to create/update the board
                bool needUpdate = false;
                
                if (sourceNodes.size() != lastSourceNodeCount) {
                    needUpdate = true;
                    PLOG_INFO << "[Debug] Instance count changed: " << lastSourceNodeCount 
                              << " -> " << sourceNodes.size();
                }
                
                // Check if board thread is actually running (using atomic flag)
                bool threadRunning = g_analysis_board_running.load();
                
                // Only create new thread if not running and we have instances
                if (!threadRunning) {
                    // Create new board with current source nodes
                    auto sourceNodesCopy = sourceNodes; // Copy for thread
                    g_stop_analysis_board = false;
                    g_analysis_board_display_thread = std::make_unique<std::thread>(
                        runAnalysisBoardDisplay, sourceNodesCopy
                    );
                    g_analysis_board_display_thread->detach(); // Detach so it runs independently
                    
                    lastSourceNodeCount = sourceNodes.size();
                    PLOG_INFO << "[Debug] Analysis board display thread started";
                    
                    // Wait a bit for thread to initialize and set running flag
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    
                    // Check if thread is still running after initialization
                    // Wait a bit longer to see if thread exits due to Qt abort
                    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                    
                    if (!g_analysis_board_running.load()) {
                        PLOG_WARNING << "[Debug] Analysis board thread exited early (likely Qt display error)";
                        PLOG_WARNING << "[Debug] Analysis board disabled - Qt cannot connect to display";
                        PLOG_WARNING << "[Debug] To enable: ensure X server is running and DISPLAY is set correctly";
                        // Disable analysis board permanently to prevent retry
                        g_analysis_board_disabled = true;
                        g_analysis_board_running.store(true); // Set to true to prevent retry
                        lastSourceNodeCount = sourceNodes.size(); // Update count to prevent retry
                    } else {
                        PLOG_INFO << "[Debug] Analysis board thread is running successfully";
                    }
                } else {
                    // Thread is running - just update count
                    if (needUpdate) {
                        PLOG_INFO << "[Debug] Analysis board already running with " << lastSourceNodeCount 
                                  << " instance(s) - current: " << sourceNodes.size();
                    }
                    lastSourceNodeCount = sourceNodes.size();
                }
                
                // Sleep a bit before checking again
                std::this_thread::sleep_for(std::chrono::seconds(2));
            } else {
                // No running instances
                if (lastSourceNodeCount > 0) {
                    PLOG_INFO << "[Debug] No running instances - waiting for instances to start...";
                    lastSourceNodeCount = 0;
                }
                
                // Stop board display thread if running
                // Note: board.display() is blocking, so we can't easily stop it
                // It will stop when instances are stopped or when Qt error occurs
                // Just reset the flag - thread will set g_analysis_board_running = false when it exits
                if (g_analysis_board_running.load()) {
                    g_stop_analysis_board = true;
                    // Thread will exit on its own when board.display() fails or instances stop
                }
                g_analysis_board_display_thread.reset();
                
                // Sleep a bit before checking again
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        } catch (const std::exception& e) {
            std::cerr << "[Debug] Error in analysis board thread: " << e.what() << std::endl;
            PLOG_ERROR << "[Debug] Error in analysis board thread: " << e.what();
            std::this_thread::sleep_for(std::chrono::seconds(5));
        } catch (...) {
            std::cerr << "[Debug] Unknown error in analysis board thread" << std::endl;
            PLOG_ERROR << "[Debug] Unknown error in analysis board thread";
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
    
    // Cleanup: stop board display thread
    if (g_analysis_board_display_thread && g_analysis_board_display_thread->joinable()) {
        g_stop_analysis_board = true;
        g_analysis_board_display_thread.reset();
    }
    
    PLOG_INFO << "[Debug] Analysis board thread stopped";
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

int main(int argc, char* argv[])
{
    try {
        // Parse command line arguments
        if (!parseArguments(argc, argv)) {
            return 0; // Help was requested, exit normally
        }
        
        // Initialize categorized logger first (before any logging)
        // This sets up log directories, daily rotation, and cleanup
        CategorizedLogger::init();
        
        PLOG_INFO << "========================================";
        PLOG_INFO << "Edge AI API Server";
        PLOG_INFO << "========================================";
        if (g_debug_mode.load()) {
            PLOG_INFO << "Debug mode: ENABLED";
        }
        if (g_log_api.load()) {
            PLOG_INFO << "API logging: ENABLED";
        }
        if (g_log_instance.load()) {
            PLOG_INFO << "Instance execution logging: ENABLED";
        }
        if (g_log_sdk_output.load()) {
            PLOG_INFO << "SDK output logging: ENABLED";
        }
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
        PLOG_INFO << "  POST /v1/core/instance/{id}/input - Set input source";
        PLOG_INFO << "  POST /v1/core/instance/{id}/config - Set config value at path";
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
        PLOG_INFO << "  GET /v1/core/logs - List all log files by category";
        PLOG_INFO << "  GET /v1/core/logs/{category} - Get logs by category (with filtering)";
        PLOG_INFO << "  GET /v1/core/logs/{category}/{date} - Get logs by category and date (with filtering)";

        // Controllers are auto-registered via Drogon's HttpController system
        // when headers are included and METHOD_LIST_BEGIN/END macros are used
        // Create instances to ensure registration
        static HealthHandler healthHandler;
        static VersionHandler versionHandler;
        static WatchdogHandler watchdogHandler;
        static SwaggerHandler swaggerHandler;
        static SystemInfoHandler systemInfoHandler;
        static LogHandler logHandler;
        
        // Initialize instance management components
        static SolutionRegistry& solutionRegistry = SolutionRegistry::getInstance();
        static PipelineBuilder pipelineBuilder;
        
        // Initialize instance storage with configurable directory
        // Default: /var/lib/edge_ai_api/instances (auto-created if needed)
        std::string instancesDir = EnvConfig::resolveDataDir("INSTANCES_DIR", "instances");
        PLOG_INFO << "[Main] Instances directory: " << instancesDir;
        static InstanceStorage instanceStorage(instancesDir);
        static InstanceRegistry instanceRegistry(solutionRegistry, pipelineBuilder, instanceStorage);
        
        // Store instance registry pointer for error recovery
        g_instance_registry = &instanceRegistry;
        
        // Initialize default solutions (face_detection, etc.)
        solutionRegistry.initializeDefaultSolutions();
        
        // Initialize solution storage and load custom solutions
        // Default: /var/lib/edge_ai_api/solutions (auto-created if needed)
        std::string solutionsDir = EnvConfig::resolveDataDir("SOLUTIONS_DIR", "solutions");
        PLOG_INFO << "[Main] Solutions directory: " << solutionsDir;
        static SolutionStorage solutionStorage(solutionsDir);
        
        // Load persisted custom solutions
        auto customSolutions = solutionStorage.loadAllSolutions();
        for (const auto& config : customSolutions) {
            // Check if solution ID conflicts with default solution
            if (solutionRegistry.isDefaultSolution(config.solutionId)) {
                PLOG_WARNING << "[Main] Skipping custom solution '" << config.solutionId 
                           << "': ID conflicts with default system solution. "
                           << "Please rename the solution to use a different ID.";
                continue;
            }
            
            // Register solution (registerSolution will also check for default solutions)
            solutionRegistry.registerSolution(config);
            PLOG_INFO << "[Main] Loaded custom solution: " << config.solutionId << " (" << config.solutionName << ")";
        }
        
        // Load persistent instances
        instanceRegistry.loadPersistentInstances();
        
        // Auto-start instances with autoStart flag
        PLOG_INFO << "[Main] Checking for instances with autoStart flag...";
        auto instancesToCheck = instanceRegistry.getAllInstances();
        int autoStartCount = 0;
        int autoStartSuccessCount = 0;
        for (const auto& [instanceId, info] : instancesToCheck) {
            if (info.autoStart) {
                autoStartCount++;
                PLOG_INFO << "[Main] Auto-starting instance: " << instanceId << " (" << info.displayName << ")";
                if (instanceRegistry.startInstance(instanceId)) {
                    autoStartSuccessCount++;
                    PLOG_INFO << "[Main] ✓ Successfully auto-started instance: " << instanceId;
                } else {
                    PLOG_WARNING << "[Main] ✗ Failed to auto-start instance: " << instanceId;
                }
            }
        }
        if (autoStartCount > 0) {
            PLOG_INFO << "[Main] Auto-start summary: " << autoStartSuccessCount << "/" << autoStartCount << " instances started successfully";
        } else {
            PLOG_INFO << "[Main] No instances with autoStart flag found";
        }
        
        // Register instance registry and solution registry with handlers
        CreateInstanceHandler::setInstanceRegistry(&instanceRegistry);
        CreateInstanceHandler::setSolutionRegistry(&solutionRegistry);
        InstanceHandler::setInstanceRegistry(&instanceRegistry);
        
        // Register solution registry and storage with solution handler
        SolutionHandler::setSolutionRegistry(&solutionRegistry);
        SolutionHandler::setSolutionStorage(&solutionStorage);
        
        // Initialize group registry and storage
        // Default: /var/lib/edge_ai_api/groups (auto-created if needed)
        std::string groupsDir = EnvConfig::resolveDataDir("GROUPS_DIR", "groups");
        PLOG_INFO << "[Main] Groups directory: " << groupsDir;
        static GroupStorage groupStorage(groupsDir);
        static GroupRegistry& groupRegistry = GroupRegistry::getInstance();
        
        // Initialize default groups
        groupRegistry.initializeDefaultGroups();
        
        // Load persisted groups
        auto persistedGroups = groupStorage.loadAllGroups();
        for (const auto& group : persistedGroups) {
            if (!groupRegistry.groupExists(group.groupId)) {
                groupRegistry.registerGroup(group.groupId, group.groupName, group.description);
                PLOG_INFO << "[Main] Loaded group: " << group.groupId << " (" << group.groupName << ")";
            }
        }
        
        // Sync groups with instances after loading
        // This ensures groups have correct instance counts and instance IDs
        auto allInstances = instanceRegistry.getAllInstances();
        std::map<std::string, std::vector<std::string>> groupInstancesMap;
        for (const auto& [instanceId, info] : allInstances) {
            if (!info.group.empty()) {
                // Auto-create group if it doesn't exist
                if (!groupRegistry.groupExists(info.group)) {
                    groupRegistry.registerGroup(info.group, info.group, "");
                    PLOG_INFO << "[Main] Auto-created group from instance: " << info.group;
                }
                groupInstancesMap[info.group].push_back(instanceId);
            }
        }
        // Update group registry with instance IDs
        for (const auto& [groupId, instanceIds] : groupInstancesMap) {
            groupRegistry.setInstanceIds(groupId, instanceIds);
        }
        
        // Register group registry, storage, and instance registry with group handler
        GroupHandler::setGroupRegistry(&groupRegistry);
        GroupHandler::setGroupStorage(&groupStorage);
        GroupHandler::setInstanceRegistry(&instanceRegistry);
        
        // Create handler instances to register endpoints
        static CreateInstanceHandler createInstanceHandler;
        static InstanceHandler instanceHandler;
        static SolutionHandler solutionHandler;
        static GroupHandler groupHandler;
        
        // Initialize model upload handler with configurable directory
        // Default: /var/lib/edge_ai_api/models (auto-created if needed)
        std::string modelsDir = EnvConfig::resolveDataDir("MODELS_DIR", "models");
        PLOG_INFO << "[Main] Models directory: " << modelsDir;
        ModelUploadHandler::setModelsDirectory(modelsDir);
        static ModelUploadHandler modelUploadHandler;
        
        PLOG_INFO << "[Main] Instance management initialized";
        PLOG_INFO << "  POST /v1/core/instance - Create new instance";
        PLOG_INFO << "  GET /v1/core/instances - List all instances";
        PLOG_INFO << "  GET /v1/core/instances/{instanceId} - Get instance details";
        PLOG_INFO << "  POST /v1/core/instance/{instanceId}/input - Set input source";
        PLOG_INFO << "  POST /v1/core/instance/{instanceId}/config - Set config value at path";
        PLOG_INFO << "  POST /v1/core/instances/{instanceId}/start - Start instance";
        PLOG_INFO << "  POST /v1/core/instances/{instanceId}/stop - Stop instance";
        PLOG_INFO << "  DELETE /v1/core/instances/{instanceId} - Delete instance";
        
        PLOG_INFO << "[Main] Solution management initialized";
        PLOG_INFO << "  GET /v1/core/solutions - List all solutions";
        PLOG_INFO << "  GET /v1/core/solutions/{solutionId} - Get solution details";
        PLOG_INFO << "  POST /v1/core/solutions - Create new solution";
        PLOG_INFO << "  PUT /v1/core/solutions/{solutionId} - Update solution";
        PLOG_INFO << "  DELETE /v1/core/solutions/{solutionId} - Delete solution";
        PLOG_INFO << "  Instances directory: " << instancesDir;
        
        PLOG_INFO << "[Main] Group management initialized";
        PLOG_INFO << "  GET /v1/core/groups - List all groups";
        PLOG_INFO << "  GET /v1/core/groups/{groupId} - Get group details";
        PLOG_INFO << "  POST /v1/core/groups - Create new group";
        PLOG_INFO << "  PUT /v1/core/groups/{groupId} - Update group";
        PLOG_INFO << "  DELETE /v1/core/groups/{groupId} - Delete group";
        PLOG_INFO << "  GET /v1/core/groups/{groupId}/instances - Get instances in group";
        PLOG_INFO << "  Groups directory: " << groupsDir;
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

        // Start debug analysis board thread if debug mode is enabled
        std::thread debugThread;
        if (g_debug_mode.load()) {
            PLOG_INFO << "[Main] Starting debug analysis board thread...";
            debugThread = std::thread(debugAnalysisBoardThread);
            debugThread.detach(); // Detach so it runs independently
        }

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
        PLOG_INFO << "[Server] Access http://" << host << ":" << port << "/v1/swagger to view all APIs";
        
        // Suppress HTTPS warning - we're intentionally using HTTP only
        // The warning "You can't use https without cert file or key file" 
        // is expected when HTTPS is not configured, but we only want HTTP
        try {
            // Run server - this blocks until quit() is called
            app.run();
            
            // After app.run() returns, ensure we exit cleanly
            // If we're here, quit() was called, so proceed with cleanup
            if (g_shutdown || g_force_exit.load()) {
                PLOG_INFO << "[Server] Shutdown signal received, cleaning up...";
            }
            
            // If force exit was requested, skip cleanup and exit immediately
            if (g_force_exit.load()) {
                std::cerr << "[SHUTDOWN] Force exit requested, skipping cleanup..." << std::endl;
                _exit(0);
            }
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

        // Cleanup - but only if not force exit
        if (!g_force_exit.load()) {
            // Note: Debug thread will stop automatically when g_shutdown is true
            if (g_health_monitor) {
                g_health_monitor->stop();
            }
            if (g_watchdog) {
                g_watchdog->stop();
            }
            
            // Stop log cleanup thread (with timeout protection)
            CategorizedLogger::shutdown();
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

