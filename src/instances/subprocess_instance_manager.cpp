#include "instances/subprocess_instance_manager.h"
#include "core/uuid_generator.h"
#include "models/solution_config.h"
#include <iostream>

SubprocessInstanceManager::SubprocessInstanceManager(
    SolutionRegistry& solutionRegistry,
    InstanceStorage& instanceStorage,
    const std::string& workerExecutable)
    : solution_registry_(solutionRegistry)
    , instance_storage_(instanceStorage) {
    
    supervisor_ = std::make_unique<worker::WorkerSupervisor>(workerExecutable);
    
    // Set up callbacks
    supervisor_->setStateChangeCallback(
        [this](const std::string& id, worker::WorkerState oldState, worker::WorkerState newState) {
            onWorkerStateChange(id, oldState, newState);
        });
    
    supervisor_->setErrorCallback(
        [this](const std::string& id, const std::string& error) {
            onWorkerError(id, error);
        });
    
    // Start supervisor monitoring
    supervisor_->start();
    
    std::cout << "[SubprocessInstanceManager] Initialized with worker: " << workerExecutable << std::endl;
}

SubprocessInstanceManager::~SubprocessInstanceManager() {
    stopAllWorkers();
}

std::string SubprocessInstanceManager::createInstance(const CreateInstanceRequest& req) {
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
        throw std::runtime_error("Failed to spawn worker for instance: " + instanceId);
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
    
    std::cout << "[SubprocessInstanceManager] Created instance: " << instanceId << std::endl;
    return instanceId;
}

bool SubprocessInstanceManager::deleteInstance(const std::string& instanceId) {
    // Terminate worker
    if (!supervisor_->terminateWorker(instanceId, false)) {
        // Worker might not exist, but we should still clean up local state
        std::cerr << "[SubprocessInstanceManager] Worker not found for: " << instanceId << std::endl;
    }
    
    // Remove from local cache
    {
        std::lock_guard<std::mutex> lock(instances_mutex_);
        instances_.erase(instanceId);
    }
    
    // Remove from storage
    instance_storage_.deleteInstance(instanceId);
    
    std::cout << "[SubprocessInstanceManager] Deleted instance: " << instanceId << std::endl;
    return true;
}

bool SubprocessInstanceManager::startInstance(const std::string& instanceId, bool /*skipAutoStop*/) {
    if (!supervisor_->isWorkerReady(instanceId)) {
        std::cerr << "[SubprocessInstanceManager] Worker not ready: " << instanceId << std::endl;
        return false;
    }
    
    // Send START command to worker
    worker::IPCMessage msg;
    msg.type = worker::MessageType::START_INSTANCE;
    msg.payload["instance_id"] = instanceId;
    
    auto response = supervisor_->sendToWorker(instanceId, msg);
    
    if (response.type == worker::MessageType::START_INSTANCE_RESPONSE &&
        response.payload.get("success", false).asBool()) {
        
        // Update local state
        {
            std::lock_guard<std::mutex> lock(instances_mutex_);
            if (instances_.count(instanceId)) {
                instances_[instanceId].running = true;
            }
        }
        
        std::cout << "[SubprocessInstanceManager] Started instance: " << instanceId << std::endl;
        return true;
    }
    
    std::cerr << "[SubprocessInstanceManager] Failed to start: " << instanceId 
              << " - " << response.payload.get("error", "Unknown error").asString() << std::endl;
    return false;
}

bool SubprocessInstanceManager::stopInstance(const std::string& instanceId) {
    if (!supervisor_->isWorkerReady(instanceId)) {
        std::cerr << "[SubprocessInstanceManager] Worker not ready: " << instanceId << std::endl;
        return false;
    }
    
    // Send STOP command to worker
    worker::IPCMessage msg;
    msg.type = worker::MessageType::STOP_INSTANCE;
    msg.payload["instance_id"] = instanceId;
    
    auto response = supervisor_->sendToWorker(instanceId, msg);
    
    if (response.type == worker::MessageType::STOP_INSTANCE_RESPONSE &&
        response.payload.get("success", false).asBool()) {
        
        // Update local state
        {
            std::lock_guard<std::mutex> lock(instances_mutex_);
            if (instances_.count(instanceId)) {
                instances_[instanceId].running = false;
            }
        }
        
        std::cout << "[SubprocessInstanceManager] Stopped instance: " << instanceId << std::endl;
        return true;
    }
    
    std::cerr << "[SubprocessInstanceManager] Failed to stop: " << instanceId << std::endl;
    return false;
}

bool SubprocessInstanceManager::updateInstance(const std::string& instanceId, const Json::Value& configJson) {
    if (!supervisor_->isWorkerReady(instanceId)) {
        std::cerr << "[SubprocessInstanceManager] Worker not ready: " << instanceId << std::endl;
        return false;
    }
    
    // Send UPDATE command to worker
    worker::IPCMessage msg;
    msg.type = worker::MessageType::UPDATE_INSTANCE;
    msg.payload["instance_id"] = instanceId;
    msg.payload["config"] = configJson;
    
    auto response = supervisor_->sendToWorker(instanceId, msg);
    
    return response.type == worker::MessageType::UPDATE_INSTANCE_RESPONSE &&
           response.payload.get("success", false).asBool();
}

std::optional<InstanceInfo> SubprocessInstanceManager::getInstance(const std::string& instanceId) const {
    std::lock_guard<std::mutex> lock(instances_mutex_);
    auto it = instances_.find(instanceId);
    if (it != instances_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<std::string> SubprocessInstanceManager::listInstances() const {
    std::lock_guard<std::mutex> lock(instances_mutex_);
    std::vector<std::string> ids;
    ids.reserve(instances_.size());
    for (const auto& [id, _] : instances_) {
        ids.push_back(id);
    }
    return ids;
}

std::vector<InstanceInfo> SubprocessInstanceManager::getAllInstances() const {
    std::lock_guard<std::mutex> lock(instances_mutex_);
    std::vector<InstanceInfo> result;
    result.reserve(instances_.size());
    for (const auto& [_, info] : instances_) {
        result.push_back(info);
    }
    return result;
}

bool SubprocessInstanceManager::hasInstance(const std::string& instanceId) const {
    std::lock_guard<std::mutex> lock(instances_mutex_);
    return instances_.count(instanceId) > 0;
}

int SubprocessInstanceManager::getInstanceCount() const {
    std::lock_guard<std::mutex> lock(instances_mutex_);
    return static_cast<int>(instances_.size());
}

std::optional<InstanceStatistics> SubprocessInstanceManager::getInstanceStatistics(const std::string& instanceId) {
    if (!supervisor_->isWorkerReady(instanceId)) {
        return std::nullopt;
    }
    
    // Send GET_STATISTICS command to worker
    worker::IPCMessage msg;
    msg.type = worker::MessageType::GET_STATISTICS;
    msg.payload["instance_id"] = instanceId;
    
    auto response = supervisor_->sendToWorker(instanceId, msg);
    
    if (response.type == worker::MessageType::GET_STATISTICS_RESPONSE &&
        response.payload.get("success", false).asBool()) {
        
        InstanceStatistics stats;
        const auto& data = response.payload["data"];
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
        return stats;
    }
    
    return std::nullopt;
}

std::string SubprocessInstanceManager::getLastFrame(const std::string& instanceId) const {
    if (!supervisor_->isWorkerReady(instanceId)) {
        return "";
    }
    
    // Send GET_LAST_FRAME command to worker
    worker::IPCMessage msg;
    msg.type = worker::MessageType::GET_LAST_FRAME;
    msg.payload["instance_id"] = instanceId;
    
    auto response = const_cast<worker::WorkerSupervisor*>(supervisor_.get())->sendToWorker(instanceId, msg);
    
    if (response.type == worker::MessageType::GET_LAST_FRAME_RESPONSE &&
        response.payload.get("success", false).asBool()) {
        return response.payload["data"].get("frame", "").asString();
    }
    
    return "";
}

Json::Value SubprocessInstanceManager::getInstanceConfig(const std::string& instanceId) const {
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

bool SubprocessInstanceManager::updateInstanceFromConfig(const std::string& instanceId, const Json::Value& configJson) {
    return updateInstance(instanceId, configJson);
}

bool SubprocessInstanceManager::hasRTMPOutput(const std::string& instanceId) const {
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
        std::cout << "[SubprocessInstanceManager] No persistent instances found" << std::endl;
        return;
    }
    
    int loadedCount = 0;
    for (const auto& instanceId : instanceIds) {
        auto optInfo = instance_storage_.loadInstance(instanceId);
        if (!optInfo.has_value()) {
            continue;
        }
        
        const auto& info = optInfo.value();
        
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
            std::cerr << "[SubprocessInstanceManager] Failed to restore instance: " << instanceId << std::endl;
        }
    }
    
    std::cout << "[SubprocessInstanceManager] Loaded " << loadedCount << " persistent instances" << std::endl;
}

int SubprocessInstanceManager::checkAndHandleRetryLimits() {
    // In subprocess mode, retry limits are handled by WorkerSupervisor
    // Workers that crash are automatically restarted up to a limit
    // This method checks for instances that have exceeded retry limits
    
    std::lock_guard<std::mutex> lock(instances_mutex_);
    int stoppedCount = 0;
    
    // Check each instance for retry limit
    for (auto& [instanceId, info] : instances_) {
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
    for (const auto& id : workerIds) {
        supervisor_->terminateWorker(id, false);
    }
    supervisor_->stop();
}

Json::Value SubprocessInstanceManager::buildWorkerConfig(const CreateInstanceRequest& req) const {
    Json::Value config;
    
    // Get solution config if specified
    if (!req.solution.empty()) {
        auto optSolution = solution_registry_.getSolution(req.solution);
        if (optSolution.has_value()) {
            // Serialize solution config manually
            const auto& sol = optSolution.value();
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
    for (const auto& [key, value] : req.additionalParams) {
        config["AdditionalParams"][key] = value;
    }
    
    return config;
}

void SubprocessInstanceManager::updateInstanceCache(const std::string& instanceId, 
                                                     const worker::IPCMessage& response) {
    if (!response.payload.isMember("data")) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(instances_mutex_);
    auto it = instances_.find(instanceId);
    if (it == instances_.end()) {
        return;
    }
    
    const auto& data = response.payload["data"];
    if (data.isMember("running")) {
        it->second.running = data["running"].asBool();
    }
}

void SubprocessInstanceManager::onWorkerStateChange(const std::string& instanceId,
                                                     worker::WorkerState oldState,
                                                     worker::WorkerState newState) {
    std::cout << "[SubprocessInstanceManager] Worker " << instanceId 
              << " state: " << static_cast<int>(oldState) 
              << " -> " << static_cast<int>(newState) << std::endl;
    
    // Update local state based on worker state
    std::lock_guard<std::mutex> lock(instances_mutex_);
    auto it = instances_.find(instanceId);
    if (it == instances_.end()) {
        return;
    }
    
    switch (newState) {
        case worker::WorkerState::READY:
            it->second.loaded = true;
            it->second.running = false;
            break;
        case worker::WorkerState::CRASHED:
            it->second.running = false;
            it->second.retryLimitReached = true;
            break;
        case worker::WorkerState::STOPPED:
            it->second.running = false;
            it->second.loaded = false;
            break;
        default:
            break;
    }
}

void SubprocessInstanceManager::onWorkerError(const std::string& instanceId, 
                                               const std::string& error) {
    std::cerr << "[SubprocessInstanceManager] Worker " << instanceId 
              << " error: " << error << std::endl;
    
    std::lock_guard<std::mutex> lock(instances_mutex_);
    auto it = instances_.find(instanceId);
    if (it != instances_.end()) {
        it->second.running = false;
        it->second.retryCount++;
    }
}
