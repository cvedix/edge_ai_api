#pragma once

#include <chrono>
#include "core/env_config.h"

/**
 * @brief Timeout constants for various operations
 *
 * These timeouts are configurable via environment variables.
 * All values are in milliseconds.
 */
namespace TimeoutConstants {

// Registry mutex lock timeout (for read operations)
// Must be >= API wrapper timeout to allow registry to timeout first
inline int getRegistryMutexTimeoutMs() {
  return EnvConfig::getInt("REGISTRY_MUTEX_TIMEOUT_MS", 2000, 100, 30000);
}

// API wrapper timeout for getInstance() calls
// Should be >= registry timeout + buffer (e.g., registry + 500ms)
inline int getApiWrapperTimeoutMs() {
  int registryTimeout = getRegistryMutexTimeoutMs();
  int apiTimeout = EnvConfig::getInt("API_WRAPPER_TIMEOUT_MS", registryTimeout + 500, 500, 60000);
  // Ensure API timeout is always >= registry timeout
  return std::max(apiTimeout, registryTimeout + 100);
}

// IPC timeout for start/stop/update operations
inline int getIpcStartStopTimeoutMs() {
  return EnvConfig::getInt("IPC_START_STOP_TIMEOUT_MS", 10000, 1000, 60000);
}

// IPC timeout for get statistics/frame operations (API calls)
inline int getIpcApiTimeoutMs() {
  return EnvConfig::getInt("IPC_API_TIMEOUT_MS", 5000, 1000, 30000);
}

// IPC timeout for get status operations (quick checks)
inline int getIpcStatusTimeoutMs() {
  return EnvConfig::getInt("IPC_STATUS_TIMEOUT_MS", 3000, 500, 15000);
}

// Frame cache mutex timeout
inline int getFrameCacheMutexTimeoutMs() {
  return EnvConfig::getInt("FRAME_CACHE_MUTEX_TIMEOUT_MS", 1000, 100, 10000);
}

// Worker state mutex timeout (for GET_STATISTICS/GET_STATUS operations)
// Should be very short since state reads should be quick
inline int getWorkerStateMutexTimeoutMs() {
  return EnvConfig::getInt("WORKER_STATE_MUTEX_TIMEOUT_MS", 100, 50, 1000);
}

// Shutdown timeout - total time before force exit
inline int getShutdownTimeoutMs() {
  return EnvConfig::getInt("SHUTDOWN_TIMEOUT_MS", 500, 100, 5000);
}

// RTSP stop timeout during normal operation
inline int getRtspStopTimeoutMs() {
  return EnvConfig::getInt("RTSP_STOP_TIMEOUT_MS", 200, 50, 2000);
}

// RTSP stop timeout during deletion/shutdown (shorter for faster exit)
inline int getRtspStopTimeoutDeletionMs() {
  return EnvConfig::getInt("RTSP_STOP_TIMEOUT_DELETION_MS", 100, 50, 1000);
}

// Destination node finalize timeout during normal operation
inline int getDestinationFinalizeTimeoutMs() {
  return EnvConfig::getInt("DESTINATION_FINALIZE_TIMEOUT_MS", 500, 100, 3000);
}

// Destination node finalize timeout during deletion/shutdown
inline int getDestinationFinalizeTimeoutDeletionMs() {
  return EnvConfig::getInt("DESTINATION_FINALIZE_TIMEOUT_DELETION_MS", 100, 50, 1000);
}

// RTMP destination node prepare timeout during normal operation
inline int getRtmpPrepareTimeoutMs() {
  return EnvConfig::getInt("RTMP_PREPARE_TIMEOUT_MS", 200, 50, 2000);
}

// RTMP destination node prepare timeout during deletion/shutdown
inline int getRtmpPrepareTimeoutDeletionMs() {
  return EnvConfig::getInt("RTMP_PREPARE_TIMEOUT_DELETION_MS", 50, 20, 500);
}

// Helper functions for std::chrono::milliseconds
inline std::chrono::milliseconds getRegistryMutexTimeout() {
  return std::chrono::milliseconds(getRegistryMutexTimeoutMs());
}

inline std::chrono::milliseconds getApiWrapperTimeout() {
  return std::chrono::milliseconds(getApiWrapperTimeoutMs());
}

inline std::chrono::milliseconds getIpcStartStopTimeout() {
  return std::chrono::milliseconds(getIpcStartStopTimeoutMs());
}

inline std::chrono::milliseconds getIpcApiTimeout() {
  return std::chrono::milliseconds(getIpcApiTimeoutMs());
}

inline std::chrono::milliseconds getIpcStatusTimeout() {
  return std::chrono::milliseconds(getIpcStatusTimeoutMs());
}

inline std::chrono::milliseconds getFrameCacheMutexTimeout() {
  return std::chrono::milliseconds(getFrameCacheMutexTimeoutMs());
}

inline std::chrono::milliseconds getWorkerStateMutexTimeout() {
  return std::chrono::milliseconds(getWorkerStateMutexTimeoutMs());
}

inline std::chrono::milliseconds getShutdownTimeout() {
  return std::chrono::milliseconds(getShutdownTimeoutMs());
}

inline std::chrono::milliseconds getRtspStopTimeout() {
  return std::chrono::milliseconds(getRtspStopTimeoutMs());
}

inline std::chrono::milliseconds getRtspStopTimeoutDeletion() {
  return std::chrono::milliseconds(getRtspStopTimeoutDeletionMs());
}

inline std::chrono::milliseconds getDestinationFinalizeTimeout() {
  return std::chrono::milliseconds(getDestinationFinalizeTimeoutMs());
}

inline std::chrono::milliseconds getDestinationFinalizeTimeoutDeletion() {
  return std::chrono::milliseconds(getDestinationFinalizeTimeoutDeletionMs());
}

inline std::chrono::milliseconds getRtmpPrepareTimeout() {
  return std::chrono::milliseconds(getRtmpPrepareTimeoutMs());
}

inline std::chrono::milliseconds getRtmpPrepareTimeoutDeletion() {
  return std::chrono::milliseconds(getRtmpPrepareTimeoutDeletionMs());
}

} // namespace TimeoutConstants

