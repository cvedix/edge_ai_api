#include "instances/subprocess_instance_manager.h"
#include "core/timeout_constants.h"
#include "core/uuid_generator.h"
#include "models/solution_config.h"
#include <chrono>
#include <future>
#include <iostream>
#include <thread>

SubprocessInstanceManager::SubprocessInstanceManager(
    SolutionRegistry &solutionRegistry, InstanceStorage &instanceStorage,
    const std::string &workerExecutable)
    : solution_registry_(solutionRegistry), instance_storage_(instanceStorage) {

  supervisor_ = std::make_unique<worker::WorkerSupervisor>(workerExecutable);

  // Set up callbacks
  supervisor_->setStateChangeCallback([this](const std::string &id,
                                             worker::WorkerState oldState,
                                             worker::WorkerState newState) {
    onWorkerStateChange(id, oldState, newState);
  });

  supervisor_->setErrorCallback(
      [this](const std::string &id, const std::string &error) {
        onWorkerError(id, error);
      });

  // Start supervisor monitoring
  supervisor_->start();

  std::cout << "[SubprocessInstanceManager] Initialized with worker: "
            << workerExecutable << std::endl;
}

SubprocessInstanceManager::~SubprocessInstanceManager() { stopAllWorkers(); }

std::string
SubprocessInstanceManager::createInstance(const CreateInstanceRequest &req) {
  // Generate instance ID
  std::string instanceId = UUIDGenerator::generateUUID();

  // Validate solution if provided
  if (!req.solution.empty()) {
    auto optSolution = solution_registry_.getSolution(req.solution);
    if (!optSolution.has_value()) {
      throw std::invalid_argument("Solution not found: " + req.solution);
    }
  }

  // Build config for worker
  Json::Value config = buildWorkerConfig(req);

  // Spawn worker process
  if (!supervisor_->spawnWorker(instanceId, config)) {
    throw std::runtime_error("Failed to spawn worker for instance: " +
                             instanceId);
  }

  // Create local instance info
  InstanceInfo info;
  info.instanceId = instanceId;
  info.displayName = req.name.empty() ? instanceId : req.name;
  info.solutionId = req.solution;
  info.running = false;
  info.loaded = true;
  info.autoStart = req.autoStart;
  info.persistent = req.persistent;
  info.startTime = std::chrono::steady_clock::now();

  // Copy additional params (including source URLs)
  info.additionalParams = req.additionalParams;
  if (req.additionalParams.count("RTSP_URL")) {
    info.rtspUrl = req.additionalParams.at("RTSP_URL");
  }
  if (req.additionalParams.count("RTMP_URL")) {
    info.rtmpUrl = req.additionalParams.at("RTMP_URL");
  }
  if (req.additionalParams.count("FILE_PATH")) {
    info.filePath = req.additionalParams.at("FILE_PATH");
  }

  {
    std::lock_guard<std::mutex> lock(instances_mutex_);
    instances_[instanceId] = info;
  }

  // Persist if requested
  if (req.persistent) {
    instance_storage_.saveInstance(instanceId, info);
  }

  // Auto-start if requested
  if (req.autoStart) {
    startInstance(instanceId);
  }

  std::cout << "[SubprocessInstanceManager] Created instance: " << instanceId
            << std::endl;
  return instanceId;
}

bool SubprocessInstanceManager::deleteInstance(const std::string &instanceId) {
  // Terminate worker
  if (!supervisor_->terminateWorker(instanceId, false)) {
    // Worker might not exist, but we should still clean up local state
    std::cerr << "[SubprocessInstanceManager] Worker not found for: "
              << instanceId << std::endl;
  }

  // Remove from local cache
  {
    std::lock_guard<std::mutex> lock(instances_mutex_);
    instances_.erase(instanceId);
  }

  // Remove from storage
  instance_storage_.deleteInstance(instanceId);

  std::cout << "[SubprocessInstanceManager] Deleted instance: " << instanceId
            << std::endl;
  return true;
}

bool SubprocessInstanceManager::startInstance(const std::string &instanceId,
                                              bool /*skipAutoStop*/) {
  // Check worker state - accept both READY and BUSY states
  // BUSY is OK because worker can handle multiple commands
  auto workerState = supervisor_->getWorkerState(instanceId);
  if (workerState != worker::WorkerState::READY && 
      workerState != worker::WorkerState::BUSY) {
    std::cerr << "[SubprocessInstanceManager] Worker not ready: " << instanceId
              << " (state: " << static_cast<int>(workerState) << ")" << std::endl;
    
    // If worker doesn't exist, try to check if instance exists in storage
    // and spawn worker if needed
    if (workerState == worker::WorkerState::STOPPED) {
      auto optStoredInfo = instance_storage_.loadInstance(instanceId);
      if (optStoredInfo.has_value()) {
        std::cout << "[SubprocessInstanceManager] Instance found in storage but worker not running. "
                  << "Spawning worker for instance: " << instanceId << std::endl;
        
        const auto &info = optStoredInfo.value();
        Json::Value config;
        config["SolutionId"] = info.solutionId;
        config["DisplayName"] = info.displayName;
        config["RtspUrl"] = info.rtspUrl;
        config["RtmpUrl"] = info.rtmpUrl;
        config["FilePath"] = info.filePath;
        
        // Add additional params if any
        if (!info.additionalParams.empty()) {
          Json::Value params;
          for (const auto &[key, value] : info.additionalParams) {
            params[key] = value;
          }
          config["AdditionalParams"] = params;
        }
        
        if (supervisor_->spawnWorker(instanceId, config)) {
          // Add to local cache
          {
            std::lock_guard<std::mutex> lock(instances_mutex_);
            instances_[instanceId] = info;
          }
          std::cout << "[SubprocessInstanceManager] Worker spawned for instance: " 
                    << instanceId << std::endl;
          // Continue to start instance below
        } else {
          std::cerr << "[SubprocessInstanceManager] Failed to spawn worker for instance: "
                    << instanceId << std::endl;
          return false;
        }
      } else {
        return false;
      }
    } else {
      return false;
    }
  }

  // Send START command to worker
  // Use configurable timeout for start operation (default: 10 seconds)
  worker::IPCMessage msg;
  msg.type = worker::MessageType::START_INSTANCE;
  msg.payload["instance_id"] = instanceId;

  auto response = supervisor_->sendToWorker(
      instanceId, msg, TimeoutConstants::getIpcStartStopTimeoutMs());

  if (response.type == worker::MessageType::START_INSTANCE_RESPONSE &&
      response.payload.get("success", false).asBool()) {

    // Verify instance is actually running by querying status
    // Wait for pipeline to start (up to 15 seconds with retries)
    // Note: Pipeline starts async, so we need more time for RTSP connection
    const int maxRetries = 30; // 30 retries * 500ms = 15 seconds
    const int retryDelayMs = 500;
    bool verified = false;

    for (int retry = 0; retry < maxRetries; retry++) {
      // Query instance status
      worker::IPCMessage statusMsg;
      statusMsg.type = worker::MessageType::GET_INSTANCE_STATUS;
      statusMsg.payload["instance_id"] = instanceId;

      // Use configurable timeout for status check (default: 3 seconds)
      auto statusResponse = supervisor_->sendToWorker(
          instanceId, statusMsg, TimeoutConstants::getIpcStatusTimeoutMs());

      if (statusResponse.type ==
          worker::MessageType::GET_INSTANCE_STATUS_RESPONSE) {
        if (statusResponse.payload.isMember("data")) {
          const auto &data = statusResponse.payload["data"];
          bool running = data.get("running", false).asBool();
          std::string state = data.get("state", "").asString();

          // Accept both "running" and "starting" states (pipeline is in process)
          if (running || state == "starting") {
            // If still starting, continue waiting
            if (state == "starting") {
              if (retry < maxRetries - 1) {
                // Don't break yet, wait for it to become running
                std::this_thread::sleep_for(std::chrono::milliseconds(retryDelayMs));
                continue;
              } else {
                // Timeout but state is "starting", consider it OK (will become running soon)
                verified = true;
                std::cout << "[SubprocessInstanceManager] Instance " << instanceId
                          << " is starting (will become running soon)" << std::endl;
                break;
              }
            } else if (running) {
              verified = true;
              std::cout << "[SubprocessInstanceManager] ✓ Verified instance "
                        << instanceId << " is running (retry " << (retry + 1)
                        << "/" << maxRetries << ")" << std::endl;
              break;
            }
          } else {
            // Check if there's an error
            if (data.isMember("last_error") &&
                !data["last_error"].asString().empty()) {
              std::cerr << "[SubprocessInstanceManager] Instance " << instanceId
                        << " start failed with error: "
                        << data["last_error"].asString() << std::endl;
              // Update local state to reflect failure
              {
                std::lock_guard<std::mutex> lock(instances_mutex_);
                if (instances_.count(instanceId)) {
                  instances_[instanceId].running = false;
                }
              }
              return false;
            }
          }
        }
      }

      // Wait before retry
      if (retry < maxRetries - 1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(retryDelayMs));
      }
    }

    if (!verified) {
      std::cerr << "[SubprocessInstanceManager] ✗ Failed to verify instance "
                << instanceId << " is running after start command" << std::endl;
      // Update local state to reflect failure
      {
        std::lock_guard<std::mutex> lock(instances_mutex_);
        if (instances_.count(instanceId)) {
          instances_[instanceId].running = false;
        }
      }
      return false;
    }

    // Update local state - ensure instance exists in cache
    {
      std::lock_guard<std::mutex> lock(instances_mutex_);
      if (instances_.count(instanceId)) {
        instances_[instanceId].running = true;
      } else {
        // Instance not in local cache but worker exists and is running
        // This can happen if instance was loaded from storage but cache was cleared
        // or instance was created elsewhere. Create basic instance info from worker.
        std::cerr << "[SubprocessInstanceManager] Warning: Instance " << instanceId
                  << " not in local cache but worker is running. Creating cache entry..."
                  << std::endl;
        
        // Try to get worker info to create instance info
        auto optWorkerInfo = supervisor_->getWorkerInfo(instanceId);
        if (optWorkerInfo.has_value()) {
          InstanceInfo info;
          info.instanceId = instanceId;
          info.displayName = instanceId; // Default name
          info.running = true;
          info.loaded = true;
          info.autoStart = false;
          info.persistent = false;
          info.startTime = std::chrono::steady_clock::now();
          
          // Try to load from storage if available
          auto optStoredInfo = instance_storage_.loadInstance(instanceId);
          if (optStoredInfo.has_value()) {
            const auto &storedInfo = optStoredInfo.value();
            info.displayName = storedInfo.displayName;
            info.solutionId = storedInfo.solutionId;
            info.autoStart = storedInfo.autoStart;
            info.persistent = storedInfo.persistent;
            info.additionalParams = storedInfo.additionalParams;
            info.rtspUrl = storedInfo.rtspUrl;
            info.rtmpUrl = storedInfo.rtmpUrl;
            info.filePath = storedInfo.filePath;
            std::cout << "[SubprocessInstanceManager] Loaded instance info from storage for "
                      << instanceId << std::endl;
          }
          
          instances_[instanceId] = info;
          std::cout << "[SubprocessInstanceManager] Created cache entry for instance "
                    << instanceId << std::endl;
        } else {
          std::cerr << "[SubprocessInstanceManager] Error: Cannot create cache entry for "
                    << instanceId << " - worker info not available" << std::endl;
        }
      }
    }

    std::cout << "[SubprocessInstanceManager] ✓ Started and verified instance: "
              << instanceId << std::endl;
    return true;
  }

  std::cerr << "[SubprocessInstanceManager] Failed to start: " << instanceId
            << " - "
            << response.payload.get("error", "Unknown error").asString()
            << std::endl;
  return false;
}

bool SubprocessInstanceManager::stopInstance(const std::string &instanceId) {
  // Check worker state - accept both READY and BUSY states
  // BUSY is OK because worker can handle multiple commands
  auto workerState = supervisor_->getWorkerState(instanceId);
  if (workerState != worker::WorkerState::READY && 
      workerState != worker::WorkerState::BUSY) {
    std::cerr << "[SubprocessInstanceManager] Worker not ready: " << instanceId
              << " (state: " << static_cast<int>(workerState) << ")" << std::endl;
    return false;
  }

  // Send STOP command to worker
  // Use configurable timeout for stop operation (default: 10 seconds)
  worker::IPCMessage msg;
  msg.type = worker::MessageType::STOP_INSTANCE;
  msg.payload["instance_id"] = instanceId;

  auto response = supervisor_->sendToWorker(
      instanceId, msg, TimeoutConstants::getIpcStartStopTimeoutMs());

  if (response.type == worker::MessageType::STOP_INSTANCE_RESPONSE &&
      response.payload.get("success", false).asBool()) {

    // Update local state
    {
      std::lock_guard<std::mutex> lock(instances_mutex_);
      if (instances_.count(instanceId)) {
        instances_[instanceId].running = false;
      }
    }

    std::cout << "[SubprocessInstanceManager] Stopped instance: " << instanceId
              << std::endl;
    return true;
  }

  std::cerr << "[SubprocessInstanceManager] Failed to stop: " << instanceId
            << std::endl;
  return false;
}

bool SubprocessInstanceManager::updateInstance(const std::string &instanceId,
                                               const Json::Value &configJson) {
  // Check worker state - accept both READY and BUSY states
  // BUSY is OK because worker can handle multiple commands
  auto workerState = supervisor_->getWorkerState(instanceId);
  if (workerState != worker::WorkerState::READY && 
      workerState != worker::WorkerState::BUSY) {
    std::cerr << "[SubprocessInstanceManager] Worker not ready: " << instanceId
              << " (state: " << static_cast<int>(workerState) << ")" << std::endl;
    return false;
  }

  // Send UPDATE command to worker
  // Use configurable timeout for update operation (default: 10 seconds)
  worker::IPCMessage msg;
  msg.type = worker::MessageType::UPDATE_INSTANCE;
  msg.payload["instance_id"] = instanceId;
  msg.payload["config"] = configJson;

  auto response = supervisor_->sendToWorker(
      instanceId, msg, TimeoutConstants::getIpcStartStopTimeoutMs());

  if (response.type != worker::MessageType::UPDATE_INSTANCE_RESPONSE) {
    std::cerr
        << "[SubprocessInstanceManager] Invalid response type for update: "
        << static_cast<int>(response.type) << std::endl;
    return false;
  }

  bool success = response.payload.get("success", false).asBool();

  if (success) {
    // Update local cache with new config
    std::lock_guard<std::mutex> lock(instances_mutex_);
    auto it = instances_.find(instanceId);
    if (it != instances_.end()) {
      // Update instance info from config if possible
      // Note: Full config merge is handled by worker, we just update local
      // cache
      if (configJson.isMember("DisplayName")) {
        it->second.displayName = configJson["DisplayName"].asString();
      }
      if (configJson.isMember("AdditionalParams")) {
        const auto &params = configJson["AdditionalParams"];
        if (params.isObject()) {
          for (const auto &key : params.getMemberNames()) {
            it->second.additionalParams[key] = params[key].asString();
          }
          // Update URLs from AdditionalParams
          if (it->second.additionalParams.count("RTSP_URL")) {
            it->second.rtspUrl = it->second.additionalParams.at("RTSP_URL");
          }
          if (it->second.additionalParams.count("RTMP_URL")) {
            it->second.rtmpUrl = it->second.additionalParams.at("RTMP_URL");
          }
          if (it->second.additionalParams.count("FILE_PATH")) {
            it->second.filePath = it->second.additionalParams.at("FILE_PATH");
          }
        }
      }
    }
    std::cout << "[SubprocessInstanceManager] Updated instance: " << instanceId
              << std::endl;
  } else {
    std::string error =
        response.payload.get("error", "Unknown error").asString();
    std::cerr << "[SubprocessInstanceManager] Failed to update instance "
              << instanceId << ": " << error << std::endl;
  }

  return success;
}

std::optional<InstanceInfo>
SubprocessInstanceManager::getInstance(const std::string &instanceId) const {
  std::unique_lock<std::mutex> lock(instances_mutex_);
  auto it = instances_.find(instanceId);
  if (it != instances_.end()) {
    InstanceInfo info = it->second;
    bool isRunning = info.running;

    // If instance is running, try to get FPS from statistics
    if (isRunning) {
      // Release lock before calling getInstanceStatistics to avoid deadlock
      lock.unlock();
      // Note: getInstanceStatistics is not const, so we use const_cast
      // This is safe because we're only reading statistics, not modifying
      // instance
      auto optStats =
          const_cast<SubprocessInstanceManager *>(this)->getInstanceStatistics(
              instanceId);
      lock.lock();

      // Re-check instance still exists and is still running after lock
      it = instances_.find(instanceId);
      if (it != instances_.end() && it->second.running &&
          optStats.has_value()) {
        // Update fps from statistics
        info.fps = optStats.value().current_framerate;
      }
    }

    return info;
  }
  return std::nullopt;
}

std::vector<std::string> SubprocessInstanceManager::listInstances() const {
  std::lock_guard<std::mutex> lock(instances_mutex_);
  std::vector<std::string> ids;
  ids.reserve(instances_.size());
  for (const auto &[id, _] : instances_) {
    ids.push_back(id);
  }
  return ids;
}

std::vector<InstanceInfo> SubprocessInstanceManager::getAllInstances() const {
  // CRITICAL: Sync workers with cache before returning instances
  // This ensures all running workers are included even if cache was cleared
  // or instance was started without being added to cache
  
  // Get all worker IDs from supervisor (these are actual running instances)
  auto workerIds = supervisor_->getWorkerIds();
  
  // Ensure all workers are in cache
  for (const auto &instanceId : workerIds) {
    {
      std::lock_guard<std::mutex> lock(instances_mutex_);
      // If instance not in cache, restore it
      if (instances_.count(instanceId) == 0) {
        // Try to load from storage first
        auto optStoredInfo = instance_storage_.loadInstance(instanceId);
        if (optStoredInfo.has_value()) {
          instances_[instanceId] = optStoredInfo.value();
          // Update running status based on worker state
          auto workerState = supervisor_->getWorkerState(instanceId);
          instances_[instanceId].running = (workerState == worker::WorkerState::READY || 
                                           workerState == worker::WorkerState::BUSY);
          std::cout << "[SubprocessInstanceManager] Restored instance " << instanceId
                    << " from storage to cache in getAllInstances()" << std::endl;
        } else {
          // Create minimal instance info from worker
          InstanceInfo info;
          info.instanceId = instanceId;
          info.displayName = instanceId;
          auto workerState = supervisor_->getWorkerState(instanceId);
          info.running = (workerState == worker::WorkerState::READY || 
                         workerState == worker::WorkerState::BUSY);
          info.loaded = true;
          info.autoStart = false;
          info.persistent = false;
          info.startTime = std::chrono::steady_clock::now();
          instances_[instanceId] = info;
          std::cout << "[SubprocessInstanceManager] Created minimal instance entry for "
                    << instanceId << " in getAllInstances()" << std::endl;
        }
      }
    }
  }
  
  // Now get all instances from cache (after syncing with workers)
  // CRITICAL: Do NOT call getInstance() for each instance as it calls getInstanceStatistics()
  // which can block for up to 5 seconds per instance. Instead, return cached info directly
  // and only update FPS if it's safe to do so (non-blocking or with timeout)
  std::vector<InstanceInfo> result;
  std::vector<std::string> instanceIds;
  {
    std::lock_guard<std::mutex> lock(instances_mutex_);
    instanceIds.reserve(instances_.size());
    for (const auto &[instanceId, _] : instances_) {
      instanceIds.push_back(instanceId);
    }
    // Copy all instances while holding lock (fast operation)
    result.reserve(instanceIds.size());
    for (const auto &instanceId : instanceIds) {
      result.push_back(instances_.at(instanceId));
    }
  }

  // Try to update FPS for running instances using async with timeout
  // This prevents getAllInstances() from blocking if statistics call hangs
  for (auto &info : result) {
    if (info.running) {
      // Try to get FPS asynchronously with short timeout (500ms)
      // If it fails or times out, just use cached FPS value
      try {
        auto future = std::async(std::launch::async, [this, &info]() -> double {
          try {
            auto optStats = const_cast<SubprocessInstanceManager *>(this)->getInstanceStatistics(info.instanceId);
            if (optStats.has_value()) {
              return optStats.value().current_framerate;
            }
          } catch (...) {
            // Ignore errors, return cached FPS
          }
          return info.fps; // Return cached FPS on error
        });

        auto status = future.wait_for(std::chrono::milliseconds(500));
        if (status == std::future_status::ready) {
          info.fps = future.get();
        }
        // If timeout, just use cached FPS - don't block
      } catch (...) {
        // Ignore exceptions, use cached FPS
      }
    }
  }

  return result;
}

bool SubprocessInstanceManager::hasInstance(
    const std::string &instanceId) const {
  std::lock_guard<std::mutex> lock(instances_mutex_);
  return instances_.count(instanceId) > 0;
}

int SubprocessInstanceManager::getInstanceCount() const {
  std::lock_guard<std::mutex> lock(instances_mutex_);
  return static_cast<int>(instances_.size());
}

std::optional<InstanceStatistics>
SubprocessInstanceManager::getInstanceStatistics(
    const std::string &instanceId) {
  // Flush output immediately to ensure logs appear
  std::cout.flush();
  std::cerr.flush();
  
  std::cout << "[SubprocessInstanceManager] ===== getInstanceStatistics START =====" << std::endl;
  std::cout << "[SubprocessInstanceManager] Thread ID: " << std::this_thread::get_id() << std::endl;
  std::cout << "[SubprocessInstanceManager] Instance ID: " << instanceId << std::endl;
  std::cout.flush();
  
  // Check worker state first - if worker exists and is running, we can get statistics
  // even if instance is not in local cache (e.g., cache was cleared but worker is still running)
  std::cout << "[SubprocessInstanceManager] About to call supervisor_->getWorkerState()..." << std::endl;
  std::cout.flush();
  auto getState_start = std::chrono::steady_clock::now();
  auto workerState = supervisor_->getWorkerState(instanceId);
  auto getState_end = std::chrono::steady_clock::now();
  auto getState_duration = std::chrono::duration_cast<std::chrono::milliseconds>(getState_end - getState_start).count();
  
  std::cout << "[SubprocessInstanceManager] getWorkerState() completed in " << getState_duration << "ms" << std::endl;
  std::cout << "[SubprocessInstanceManager] Worker state: " << static_cast<int>(workerState) << std::endl;
  
  // If worker doesn't exist at all, check if instance exists in cache/storage
  // If worker exists and is running, we can proceed even without instance in cache
  if (workerState == worker::WorkerState::STOPPED) {
    // Worker doesn't exist - check if instance exists in cache or storage
    bool instanceExists = false;
    {
      std::lock_guard<std::mutex> lock(instances_mutex_);
      instanceExists = instances_.find(instanceId) != instances_.end();
    }
    
    if (!instanceExists) {
      auto optStoredInfo = instance_storage_.loadInstance(instanceId);
      instanceExists = optStoredInfo.has_value();
    }
    
    if (!instanceExists) {
      std::cerr << "[SubprocessInstanceManager] Instance not found: " << instanceId
                << " (not in cache or storage, and worker doesn't exist)" << std::endl;
      return std::nullopt;
    }
  }
  
  // Check if instance exists in cache or storage (for restoring running flag later)
  bool instanceExists = false;
  bool instanceRunning = false;
  {
    std::lock_guard<std::mutex> lock(instances_mutex_);
    auto it = instances_.find(instanceId);
    if (it != instances_.end()) {
      instanceExists = true;
      instanceRunning = it->second.running;
      std::cout << "[SubprocessInstanceManager] Instance found in cache, running: " 
                << instanceRunning << std::endl;
    }
  }
  
  if (!instanceExists) {
    // Try to load from storage
    auto optStoredInfo = instance_storage_.loadInstance(instanceId);
    if (optStoredInfo.has_value()) {
      instanceExists = true;
      instanceRunning = optStoredInfo.value().running;
      std::cout << "[SubprocessInstanceManager] Instance found in storage, running: " 
                << instanceRunning << std::endl;
    }
  }
  
  // If worker is STOPPED or CRASHED, but instance exists and was running
  // Try to spawn worker if needed (similar to startInstance logic)
  if (workerState == worker::WorkerState::STOPPED || 
      workerState == worker::WorkerState::CRASHED) {
    std::cerr << "[SubprocessInstanceManager] Worker for instance " << instanceId
              << " is " << (workerState == worker::WorkerState::CRASHED ? "crashed" : "stopped")
              << " but instance exists" << std::endl;
    
    // If instance was running, try to restore worker
    if (instanceRunning) {
      std::cout << "[SubprocessInstanceManager] Instance was running, attempting to restore worker..." << std::endl;
      // Try to spawn worker from storage
      auto optStoredInfo = instance_storage_.loadInstance(instanceId);
      if (optStoredInfo.has_value()) {
        const auto &info = optStoredInfo.value();
        Json::Value config;
        config["SolutionId"] = info.solutionId;
        config["DisplayName"] = info.displayName;
        config["RtspUrl"] = info.rtspUrl;
        config["RtmpUrl"] = info.rtmpUrl;
        config["FilePath"] = info.filePath;
        
        if (!info.additionalParams.empty()) {
          Json::Value params;
          for (const auto &[key, value] : info.additionalParams) {
            params[key] = value;
          }
          config["AdditionalParams"] = params;
        }
        
        if (supervisor_->spawnWorker(instanceId, config)) {
          // Add to cache
          {
            std::lock_guard<std::mutex> lock(instances_mutex_);
            instances_[instanceId] = info;
          }
          std::cout << "[SubprocessInstanceManager] Worker spawned, waiting for ready..." << std::endl;
          // Wait for worker to become ready (with timeout)
          const int maxWaitRetries = 20; // 20 retries * 250ms = 5 seconds
          const int waitDelayMs = 250;
          bool workerReady = false;
          for (int retry = 0; retry < maxWaitRetries; retry++) {
            workerState = supervisor_->getWorkerState(instanceId);
            if (workerState == worker::WorkerState::READY || 
                workerState == worker::WorkerState::BUSY) {
              workerReady = true;
              std::cout << "[SubprocessInstanceManager] Worker became ready after " 
                        << (retry * waitDelayMs) << "ms" << std::endl;
              break;
            }
            if (retry < maxWaitRetries - 1) {
              std::this_thread::sleep_for(std::chrono::milliseconds(waitDelayMs));
            }
          }
          if (!workerReady) {
            std::cerr << "[SubprocessInstanceManager] Worker did not become ready after spawn" << std::endl;
          }
        }
      }
    }
    
    // If still STOPPED or CRASHED after trying to restore, return nullopt
    if (workerState == worker::WorkerState::STOPPED || 
        workerState == worker::WorkerState::CRASHED) {
      std::cerr << "[SubprocessInstanceManager] Cannot get statistics: Worker for instance "
                << instanceId << " is " << (workerState == worker::WorkerState::CRASHED ? "crashed" : "stopped")
                << std::endl;
      return std::nullopt;
    }
  }
  
  // CRITICAL: If worker is in STARTING state, wait for it to become READY
  // sendToWorker() only accepts READY or BUSY states
  if (workerState == worker::WorkerState::STARTING) {
    std::cout << "[SubprocessInstanceManager] Worker is STARTING, waiting for READY..." << std::endl;
    const int maxWaitRetries = 20; // 20 retries * 250ms = 5 seconds
    const int waitDelayMs = 250;
    bool workerReady = false;
    for (int retry = 0; retry < maxWaitRetries; retry++) {
      workerState = supervisor_->getWorkerState(instanceId);
      if (workerState == worker::WorkerState::READY || 
          workerState == worker::WorkerState::BUSY) {
        workerReady = true;
        std::cout << "[SubprocessInstanceManager] Worker became ready after " 
                  << (retry * waitDelayMs) << "ms" << std::endl;
        break;
      }
      if (workerState == worker::WorkerState::STOPPED || 
          workerState == worker::WorkerState::CRASHED) {
        std::cerr << "[SubprocessInstanceManager] Worker stopped/crashed while waiting for READY" << std::endl;
        return std::nullopt;
      }
      if (retry < maxWaitRetries - 1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(waitDelayMs));
      }
    }
    if (!workerReady) {
      std::cerr << "[SubprocessInstanceManager] Worker did not become ready after waiting (timeout)" << std::endl;
      return std::nullopt;
    }
  }
  
  // If worker is STOPPING, we cannot get statistics
  if (workerState == worker::WorkerState::STOPPING) {
    std::cerr << "[SubprocessInstanceManager] Cannot get statistics: Worker for instance "
              << instanceId << " is stopping" << std::endl;
    return std::nullopt;
  }

  // Worker exists and is in a valid state (READY or BUSY)
  // Try to get statistics from worker
  // Note: Even if instance is not in local cache, we can still get statistics from worker
  
  // If instance not in cache but worker exists, try to restore cache entry
  {
    std::lock_guard<std::mutex> lock(instances_mutex_);
    if (instances_.count(instanceId) == 0) {
      // Instance not in cache but worker exists - restore from storage
      std::cerr << "[SubprocessInstanceManager] Warning: Instance " << instanceId
                << " not in local cache but worker exists. Attempting to restore from storage..."
                << std::endl;
      
      auto optStoredInfo = instance_storage_.loadInstance(instanceId);
      if (optStoredInfo.has_value()) {
        instances_[instanceId] = optStoredInfo.value();
        std::cout << "[SubprocessInstanceManager] Restored instance " << instanceId
                  << " from storage to cache" << std::endl;
      } else {
        // Create minimal instance info from worker
        // First, try to get actual status from worker to determine if instance is running
        bool isRunning = false;
        try {
          worker::IPCMessage statusMsg;
          statusMsg.type = worker::MessageType::GET_INSTANCE_STATUS;
          statusMsg.payload["instance_id"] = instanceId;
          auto statusResponse = supervisor_->sendToWorker(
              instanceId, statusMsg, TimeoutConstants::getIpcStatusTimeoutMs());
          if (statusResponse.type == worker::MessageType::GET_INSTANCE_STATUS_RESPONSE) {
            if (statusResponse.payload.isMember("data")) {
              const auto &data = statusResponse.payload["data"];
              isRunning = data.get("running", false).asBool();
            }
          }
        } catch (...) {
          // If we can't get status, assume running if worker is READY/BUSY
          isRunning = (workerState == worker::WorkerState::READY || 
                      workerState == worker::WorkerState::BUSY);
        }
        
        InstanceInfo info;
        info.instanceId = instanceId;
        info.displayName = instanceId;
        info.running = isRunning;
        info.loaded = true;
        info.autoStart = false;
        info.persistent = false;
        info.startTime = std::chrono::steady_clock::now();
        instances_[instanceId] = info;
        std::cout << "[SubprocessInstanceManager] Created minimal instance entry for "
                  << instanceId << " (running: " << (isRunning ? "true" : "false") << ")" << std::endl;
      }
    }
  }

  // Don't check isWorkerReady here - sendToWorker handles worker state check
  // and accepts both READY and BUSY states, so statistics can be retrieved
  // even while pipeline is starting or other operations are in progress

  // Send GET_STATISTICS command to worker
  // Use configurable timeout for API calls (default: 5 seconds)
  worker::IPCMessage msg;
  msg.type = worker::MessageType::GET_STATISTICS;
  msg.payload["instance_id"] = instanceId;

  auto timeoutMs = TimeoutConstants::getIpcApiTimeoutMs();
  std::cout << "[SubprocessInstanceManager] ===== SENDING GET_STATISTICS TO WORKER =====" << std::endl;
  std::cout << "[SubprocessInstanceManager] Instance ID: " << instanceId << std::endl;
  std::cout << "[SubprocessInstanceManager] Timeout: " << timeoutMs << "ms" << std::endl;
  std::cout << "[SubprocessInstanceManager] Calling supervisor_->sendToWorker()..." << std::endl;
  
  auto send_start = std::chrono::steady_clock::now();
  auto response = supervisor_->sendToWorker(instanceId, msg, timeoutMs);
  auto send_end = std::chrono::steady_clock::now();
  auto send_duration = std::chrono::duration_cast<std::chrono::milliseconds>(send_end - send_start).count();
  
  std::cout << "[SubprocessInstanceManager] ===== RECEIVED RESPONSE FROM WORKER =====" << std::endl;
  std::cout << "[SubprocessInstanceManager] Duration: " << send_duration << "ms" << std::endl;
  std::cout << "[SubprocessInstanceManager] Response type: " << static_cast<int>(response.type) 
            << " (expected: " << static_cast<int>(worker::MessageType::GET_STATISTICS_RESPONSE) << ")" << std::endl;

  // Debug logging for better troubleshooting
  if (response.type != worker::MessageType::GET_STATISTICS_RESPONSE) {
    std::cerr << "[SubprocessInstanceManager] Failed to get statistics for instance "
              << instanceId << ": Invalid response type "
              << static_cast<int>(response.type) << " (expected: "
              << static_cast<int>(worker::MessageType::GET_STATISTICS_RESPONSE) << ")"
              << std::endl;
    
    // Check for specific error responses
    if (response.type == worker::MessageType::ERROR_RESPONSE) {
      if (response.payload.isMember("error")) {
        std::cerr << "[SubprocessInstanceManager] Worker error: "
                  << response.payload["error"].asString() << std::endl;
      }
      if (response.payload.isMember("message")) {
        std::cerr << "[SubprocessInstanceManager] Worker message: "
                  << response.payload["message"].asString() << std::endl;
      }
    } else if (response.type == worker::MessageType::PONG) {
      std::cerr << "[SubprocessInstanceManager] Received PONG instead of statistics - worker may be busy or not ready" << std::endl;
    }
    return std::nullopt;
  }

  // Check success field - also accept if success field is missing but status is OK
  // Worker response format: { "success": true/false, "status": 0, "data": {...} }
  bool hasSuccess = response.payload.isMember("success");
  bool success = response.payload.get("success", true).asBool(); // Default to true if missing
  bool hasStatus = response.payload.isMember("status");
  int status = response.payload.get("status", 0).asInt(); // Default to 0 (OK) if missing
  
  std::cout << "[SubprocessInstanceManager] Response check - success: " << success 
            << " (has field: " << hasSuccess << "), status: " << status 
            << " (has field: " << hasStatus << ")" << std::endl;
  
  // Check if response indicates error
  // Accept if: success=true OR (status=0 AND success field missing)
  if (hasSuccess && !success) {
    // Explicit failure
    std::string errorMsg = response.payload.get("error", "Unknown error").asString();
    std::string message = response.payload.get("message", "").asString();
    std::cerr << "[SubprocessInstanceManager] Statistics request failed for instance "
              << instanceId << ": " << errorMsg;
    if (!message.empty()) {
      std::cerr << " (" << message << ")";
    }
    std::cerr << " (status: " << status << ", success: " << success << ")" << std::endl;
    return std::nullopt;
  }
  
  if (hasStatus && status != 0) {
    // Status indicates error
    std::string errorMsg = response.payload.get("error", "Unknown error").asString();
    std::string message = response.payload.get("message", "").asString();
    std::cerr << "[SubprocessInstanceManager] Statistics request failed for instance "
              << instanceId << ": " << errorMsg;
    if (!message.empty()) {
      std::cerr << " (" << message << ")";
    }
    std::cerr << " (status: " << status << ", success: " << success << ")" << std::endl;
    return std::nullopt;
  }

  // Response is OK, parse statistics
  // Check for "data" field first (preferred format)
  if (response.payload.isMember("data")) {
    std::cout << "[SubprocessInstanceManager] Parsing statistics from 'data' field" << std::endl;
    InstanceStatistics stats;
    const auto &data = response.payload["data"];
    stats.frames_processed = data.get("frames_processed", 0).asUInt64();
    stats.start_time = data.get("start_time", 0).asInt64();
    stats.current_framerate = data.get("current_framerate", 0.0).asDouble();
    stats.source_framerate = data.get("source_framerate", 0.0).asDouble();
    stats.latency = data.get("latency", 0.0).asDouble();
    stats.input_queue_size = data.get("input_queue_size", 0).asUInt64();
    stats.dropped_frames_count = data.get("dropped_frames_count", 0).asUInt64();
    stats.resolution = data.get("resolution", "").asString();
    stats.source_resolution = data.get("source_resolution", "").asString();
    stats.format = data.get("format", "").asString();
    std::cout << "[SubprocessInstanceManager] Successfully parsed statistics for instance "
              << instanceId << std::endl;
    return stats;
  }
  
  // Fallback: check if statistics fields are at root level
  if (response.payload.isMember("frames_processed") || 
      response.payload.isMember("current_framerate")) {
    std::cout << "[SubprocessInstanceManager] Parsing statistics from root level fields" << std::endl;
    InstanceStatistics stats;
    stats.frames_processed = response.payload.get("frames_processed", 0).asUInt64();
    stats.start_time = response.payload.get("start_time", 0).asInt64();
    stats.current_framerate = response.payload.get("current_framerate", 0.0).asDouble();
    stats.source_framerate = response.payload.get("source_framerate", 0.0).asDouble();
    stats.latency = response.payload.get("latency", 0.0).asDouble();
    stats.input_queue_size = response.payload.get("input_queue_size", 0).asUInt64();
    stats.dropped_frames_count = response.payload.get("dropped_frames_count", 0).asUInt64();
    stats.resolution = response.payload.get("resolution", "").asString();
    stats.source_resolution = response.payload.get("source_resolution", "").asString();
    stats.format = response.payload.get("format", "").asString();
    std::cout << "[SubprocessInstanceManager] Successfully parsed statistics from root level for instance "
              << instanceId << std::endl;
    return stats;
  }

  // No data field and no statistics fields - return empty statistics
  std::cerr << "[SubprocessInstanceManager] Warning: Statistics response OK but no data/statistics fields for instance "
            << instanceId << std::endl;
  std::cerr << "[SubprocessInstanceManager] Response payload keys: ";
  for (const auto &key : response.payload.getMemberNames()) {
    std::cerr << key << " ";
  }
  std::cerr << std::endl;
  InstanceStatistics stats;
  return stats;
}

std::string
SubprocessInstanceManager::getLastFrame(const std::string &instanceId) const {
  // Don't check isWorkerReady here - sendToWorker handles worker state check
  // and accepts both READY and BUSY states, so frame can be retrieved
  // even while pipeline is starting or other operations are in progress

  std::cout << "[SubprocessInstanceManager] getLastFrame() called for instance: " 
            << instanceId << std::endl;

  // Send GET_LAST_FRAME command to worker
  // Use configurable timeout for API calls (default: 5 seconds)
  worker::IPCMessage msg;
  msg.type = worker::MessageType::GET_LAST_FRAME;
  msg.payload["instance_id"] = instanceId;

  std::cout << "[SubprocessInstanceManager] Sending GET_LAST_FRAME IPC message to worker for instance: " 
            << instanceId << std::endl;

  auto response = const_cast<worker::WorkerSupervisor *>(supervisor_.get())
                      ->sendToWorker(instanceId, msg,
                                     TimeoutConstants::getIpcApiTimeoutMs());

  std::cout << "[SubprocessInstanceManager] Received response from worker for instance: " 
            << instanceId << ", response type: " << static_cast<int>(response.type) << std::endl;

  if (response.type == worker::MessageType::GET_LAST_FRAME_RESPONSE &&
      response.payload.get("success", false).asBool()) {
    std::string frameBase64 = response.payload["data"].get("frame", "").asString();
    bool hasFrame = response.payload["data"].get("has_frame", false).asBool();
    
    std::cout << "[SubprocessInstanceManager] Worker response: success=true, has_frame=" 
              << hasFrame << ", frame_size=" << frameBase64.length() << " chars" << std::endl;
    
    return frameBase64;
  }

  std::cout << "[SubprocessInstanceManager] Worker response: success=false or invalid response type" << std::endl;
  return "";
}

Json::Value SubprocessInstanceManager::getInstanceConfig(
    const std::string &instanceId) const {
  std::lock_guard<std::mutex> lock(instances_mutex_);
  auto it = instances_.find(instanceId);
  if (it != instances_.end()) {
    // Build config from InstanceInfo
    Json::Value config;
    config["InstanceId"] = it->second.instanceId;
    config["DisplayName"] = it->second.displayName;
    config["SolutionId"] = it->second.solutionId;
    config["Running"] = it->second.running;
    config["Loaded"] = it->second.loaded;
    config["RtspUrl"] = it->second.rtspUrl;
    config["RtmpUrl"] = it->second.rtmpUrl;
    config["FilePath"] = it->second.filePath;
    return config;
  }
  return Json::Value();
}

bool SubprocessInstanceManager::updateInstanceFromConfig(
    const std::string &instanceId, const Json::Value &configJson) {
  return updateInstance(instanceId, configJson);
}

bool SubprocessInstanceManager::hasRTMPOutput(
    const std::string &instanceId) const {
  std::lock_guard<std::mutex> lock(instances_mutex_);
  auto it = instances_.find(instanceId);
  if (it != instances_.end()) {
    return !it->second.rtmpUrl.empty();
  }
  return false;
}

void SubprocessInstanceManager::loadPersistentInstances() {
  // Get list of instance IDs from storage directory
  auto instanceIds = instance_storage_.loadAllInstances();
  if (instanceIds.empty()) {
    std::cout << "[SubprocessInstanceManager] No persistent instances found"
              << std::endl;
    return;
  }

  int loadedCount = 0;
  for (const auto &instanceId : instanceIds) {
    auto optInfo = instance_storage_.loadInstance(instanceId);
    if (!optInfo.has_value()) {
      continue;
    }

    const auto &info = optInfo.value();

    // Build config for worker
    Json::Value config;
    config["SolutionId"] = info.solutionId;
    config["DisplayName"] = info.displayName;
    config["RtspUrl"] = info.rtspUrl;
    config["RtmpUrl"] = info.rtmpUrl;
    config["FilePath"] = info.filePath;

    // Spawn worker
    if (supervisor_->spawnWorker(instanceId, config)) {
      std::lock_guard<std::mutex> lock(instances_mutex_);
      instances_[instanceId] = info;
      loadedCount++;

      // Auto-start if was running before
      if (info.autoStart || info.running) {
        startInstance(instanceId);
      }
    } else {
      std::cerr << "[SubprocessInstanceManager] Failed to restore instance: "
                << instanceId << std::endl;
    }
  }

  std::cout << "[SubprocessInstanceManager] Loaded " << loadedCount
            << " persistent instances" << std::endl;
}

int SubprocessInstanceManager::checkAndHandleRetryLimits() {
  // In subprocess mode, retry limits are handled by WorkerSupervisor
  // Workers that crash are automatically restarted up to a limit
  // This method checks for instances that have exceeded retry limits

  std::lock_guard<std::mutex> lock(instances_mutex_);
  int stoppedCount = 0;

  // Check each instance for retry limit
  for (auto &[instanceId, info] : instances_) {
    // Check if instance has exceeded retry limit
    // In subprocess mode, retry limit is typically handled by WorkerSupervisor
    // But we can check local retry count if needed
    if (info.retryCount > 0 && info.retryLimitReached) {
      // Stop the instance
      stopInstance(instanceId);
      stoppedCount++;
    }
  }

  return stoppedCount;
}

void SubprocessInstanceManager::stopAllWorkers() {
  auto workerIds = supervisor_->getWorkerIds();
  for (const auto &id : workerIds) {
    supervisor_->terminateWorker(id, false);
  }
  supervisor_->stop();
}

Json::Value SubprocessInstanceManager::buildWorkerConfig(
    const CreateInstanceRequest &req) const {
  Json::Value config;

  // Get solution config if specified
  if (!req.solution.empty()) {
    auto optSolution = solution_registry_.getSolution(req.solution);
    if (optSolution.has_value()) {
      // Serialize solution config manually
      const auto &sol = optSolution.value();
      Json::Value solJson;
      solJson["SolutionId"] = sol.solutionId;
      solJson["SolutionName"] = sol.solutionName;
      solJson["SolutionType"] = sol.solutionType;
      solJson["IsDefault"] = sol.isDefault;
      config["Solution"] = solJson;
    }
  }

  // Add instance-specific config
  config["Name"] = req.name;
  config["Group"] = req.group;
  config["AutoStart"] = req.autoStart;
  config["Persistent"] = req.persistent;

  // Add additional parameters (includes source URLs)
  for (const auto &[key, value] : req.additionalParams) {
    config["AdditionalParams"][key] = value;
  }

  return config;
}

void SubprocessInstanceManager::updateInstanceCache(
    const std::string &instanceId, const worker::IPCMessage &response) {
  if (!response.payload.isMember("data")) {
    return;
  }

  std::lock_guard<std::mutex> lock(instances_mutex_);
  auto it = instances_.find(instanceId);
  if (it == instances_.end()) {
    return;
  }

  const auto &data = response.payload["data"];
  if (data.isMember("running")) {
    it->second.running = data["running"].asBool();
  }
}

void SubprocessInstanceManager::onWorkerStateChange(
    const std::string &instanceId, worker::WorkerState oldState,
    worker::WorkerState newState) {
  std::cout << "[SubprocessInstanceManager] Worker " << instanceId
            << " state: " << static_cast<int>(oldState) << " -> "
            << static_cast<int>(newState) << std::endl;

  // Update local state based on worker state
  std::lock_guard<std::mutex> lock(instances_mutex_);
  auto it = instances_.find(instanceId);
  if (it == instances_.end()) {
    return;
  }

  switch (newState) {
  case worker::WorkerState::READY:
    // READY means worker is ready to accept commands, NOT that instance stopped
    // Don't modify running flag here - it's managed by startInstance/stopInstance
    it->second.loaded = true;
    break;
  case worker::WorkerState::BUSY:
    // BUSY means worker is processing a command
    // Don't modify running flag here either
    break;
  case worker::WorkerState::CRASHED:
    it->second.running = false;
    it->second.loaded = false;
    it->second.retryLimitReached = true;
    break;
  case worker::WorkerState::STOPPED:
    it->second.running = false;
    it->second.loaded = false;
    break;
  case worker::WorkerState::STOPPING:
    // Don't modify running flag - instance might still be running during stop
    break;
  default:
    break;
  }
}

void SubprocessInstanceManager::onWorkerError(const std::string &instanceId,
                                              const std::string &error) {
  std::cerr << "[SubprocessInstanceManager] Worker " << instanceId
            << " error: " << error << std::endl;

  std::lock_guard<std::mutex> lock(instances_mutex_);
  auto it = instances_.find(instanceId);
  if (it != instances_.end()) {
    it->second.running = false;
    it->second.retryCount++;
  }
}
