#pragma once

#include <string>

/**
 * @brief Platform Detection Utilities
 *
 * Detects the current platform/hardware for GStreamer pipeline selection
 */
namespace PlatformDetector {
/**
 * @brief Detect platform name
 * @return Platform name: "jetson", "nvidia", "msdk", "vaapi", or "auto"
 */
std::string detectPlatform();

/**
 * @brief Check if running on Jetson
 */
bool isJetson();

/**
 * @brief Check if running on NVIDIA GPU
 */
bool isNVIDIA();

/**
 * @brief Check if Intel MSDK is available
 */
bool isMSDK();

/**
 * @brief Check if VAAPI is available
 */
bool isVAAPI();
} // namespace PlatformDetector
