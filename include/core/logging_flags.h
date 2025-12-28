#pragma once

#include <atomic>

/**
 * @brief Global logging flags for controlling different types of logging
 *
 * These flags are set via command-line arguments and can be checked
 * throughout the codebase to enable/disable specific logging features.
 */

// Forward declarations - actual definitions are in src/main.cpp
extern std::atomic<bool> g_log_api;
extern std::atomic<bool> g_log_instance;
extern std::atomic<bool> g_log_sdk_output;

/**
 * @brief Check if API logging is enabled
 */
inline bool isApiLoggingEnabled() { return g_log_api.load(); }

/**
 * @brief Check if instance execution logging is enabled
 */
inline bool isInstanceLoggingEnabled() { return g_log_instance.load(); }

/**
 * @brief Check if SDK output logging is enabled
 *
 * This flag enables logging when SDK code returns results/outputs
 * from instance processing (e.g., detection results, metadata, etc.)
 */
inline bool isSdkOutputLoggingEnabled() { return g_log_sdk_output.load(); }
