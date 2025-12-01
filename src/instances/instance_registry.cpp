#include "instances/instance_registry.h"
#include "core/uuid_generator.h"
#include <cvedix/cvedix_version.h>
#include <cvedix/nodes/src/cvedix_rtsp_src_node.h>
#include <algorithm>
#include <typeinfo>
#include <thread>
#include <chrono>

InstanceRegistry::InstanceRegistry(
    SolutionRegistry& solutionRegistry,
    PipelineBuilder& pipelineBuilder,
    InstanceStorage& instanceStorage)
    : solution_registry_(solutionRegistry),
      pipeline_builder_(pipelineBuilder),
      instance_storage_(instanceStorage) {
}

std::string InstanceRegistry::createInstance(const CreateInstanceRequest& req) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Generate instance ID
    std::string instanceId = UUIDGenerator::generateUUID();
    
    // Get solution config if specified
    SolutionConfig* solution = nullptr;
    SolutionConfig solutionConfig;
    if (!req.solution.empty()) {
        auto optSolution = solution_registry_.getSolution(req.solution);
        if (!optSolution.has_value()) {
            return ""; // Solution not found
        }
        solutionConfig = optSolution.value();
        solution = &solutionConfig;
    }
    
    // Build pipeline if solution is provided
    std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>> pipeline;
    if (solution) {
        try {
            pipeline = pipeline_builder_.buildPipeline(*solution, req, instanceId);
        } catch (const std::bad_alloc& e) {
            std::cerr << "[InstanceRegistry] Memory allocation error building pipeline for instance " 
                      << instanceId << ": " << e.what() << std::endl;
            return "";
        } catch (const std::invalid_argument& e) {
            std::cerr << "[InstanceRegistry] Invalid argument building pipeline for instance " 
                      << instanceId << ": " << e.what() << std::endl;
            return "";
        } catch (const std::runtime_error& e) {
            std::cerr << "[InstanceRegistry] Runtime error building pipeline for instance " 
                      << instanceId << ": " << e.what() << std::endl;
            return "";
        } catch (const std::exception& e) {
            // Pipeline build failed - log error but don't crash
            std::cerr << "[InstanceRegistry] Exception building pipeline for instance " << instanceId 
                      << ": " << e.what() << " (type: " << typeid(e).name() << ")" << std::endl;
            return ""; // Return empty string to indicate failure
        } catch (...) {
            // Unknown error - try to get more info
            std::cerr << "[InstanceRegistry] Unknown error building pipeline for instance " 
                      << instanceId << " (non-standard exception)" << std::endl;
            // Try to get exception info if possible
            try {
                std::rethrow_exception(std::current_exception());
            } catch (const std::exception& e) {
                std::cerr << "[InstanceRegistry] Re-thrown exception: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "[InstanceRegistry] Could not extract exception information" << std::endl;
            }
            return "";
        }
    }
    
    // Create instance info
    InstanceInfo info = createInstanceInfo(instanceId, req, solution);
    
    // Store instance
    instances_[instanceId] = info;
    if (!pipeline.empty()) {
        pipelines_[instanceId] = pipeline;
    }
    
    // Save to storage if persistent
    if (req.persistent) {
        instance_storage_.saveInstance(instanceId, info);
    }
    
    // Auto start if requested
    if (req.autoStart && !pipeline.empty()) {
        std::cerr << "[InstanceRegistry] Auto-starting pipeline for instance " << instanceId << std::endl;
        try {
            if (startPipeline(pipeline)) {
                info.running = true;
                instances_[instanceId] = info;
                std::cerr << "[InstanceRegistry] Pipeline started successfully for instance " << instanceId << std::endl;
                std::cerr << "[InstanceRegistry] NOTE: If RTSP connection fails, the node will retry automatically" << std::endl;
            } else {
                std::cerr << "[InstanceRegistry] Failed to start pipeline for instance " 
                          << instanceId << " (pipeline created but not started)" << std::endl;
                std::cerr << "[InstanceRegistry] You can manually start it later using startInstance API" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "[InstanceRegistry] Exception starting pipeline for instance " 
                      << instanceId << ": " << e.what() << std::endl;
            std::cerr << "[InstanceRegistry] Instance created but pipeline not started. You can start it manually later." << std::endl;
            // Don't crash - instance is created but not running
        } catch (...) {
            std::cerr << "[InstanceRegistry] Unknown error starting pipeline for instance " 
                      << instanceId << std::endl;
            std::cerr << "[InstanceRegistry] Instance created but pipeline not started. You can start it manually later." << std::endl;
        }
    } else if (!pipeline.empty()) {
        std::cerr << "[InstanceRegistry] Pipeline created but not started (autoStart=false)" << std::endl;
        std::cerr << "[InstanceRegistry] Use startInstance API to start the pipeline when ready" << std::endl;
    }
    
    return instanceId;
}

bool InstanceRegistry::deleteInstance(const std::string& instanceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = instances_.find(instanceId);
    if (it == instances_.end()) {
        return false;
    }
    
    // Stop pipeline if running
    auto pipelineIt = pipelines_.find(instanceId);
    if (pipelineIt != pipelines_.end()) {
        stopPipeline(pipelineIt->second);
        pipelines_.erase(pipelineIt);
    }
    
    // Delete from storage
    if (it->second.persistent) {
        instance_storage_.deleteInstance(instanceId);
    }
    
    instances_.erase(it);
    return true;
}

std::optional<InstanceInfo> InstanceRegistry::getInstance(const std::string& instanceId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = instances_.find(instanceId);
    if (it != instances_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool InstanceRegistry::startInstance(const std::string& instanceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto instanceIt = instances_.find(instanceId);
    if (instanceIt == instances_.end()) {
        return false;
    }
    
    // Check if pipeline exists, if not, try to rebuild it
    auto pipelineIt = pipelines_.find(instanceId);
    if (pipelineIt == pipelines_.end()) {
        std::cerr << "[InstanceRegistry] Pipeline not found for instance " << instanceId 
                  << ", attempting to rebuild from instance info..." << std::endl;
        if (!rebuildPipelineFromInstanceInfo(instanceId)) {
            std::cerr << "[InstanceRegistry] Failed to rebuild pipeline for instance " << instanceId << std::endl;
            return false;
        }
        pipelineIt = pipelines_.find(instanceId);
        if (pipelineIt == pipelines_.end()) {
            return false;
        }
    }
    
    if (startPipeline(pipelineIt->second)) {
        instanceIt->second.running = true;
        return true;
    }
    
    return false;
}

bool InstanceRegistry::stopInstance(const std::string& instanceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto pipelineIt = pipelines_.find(instanceId);
    if (pipelineIt == pipelines_.end()) {
        return false;
    }
    
    auto instanceIt = instances_.find(instanceId);
    if (instanceIt == instances_.end()) {
        return false;
    }
    
    stopPipeline(pipelineIt->second);
    instanceIt->second.running = false;
    
    return true;
}

std::vector<std::string> InstanceRegistry::listInstances() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    result.reserve(instances_.size());
    for (const auto& pair : instances_) {
        result.push_back(pair.first);
    }
    return result;
}

bool InstanceRegistry::hasInstance(const std::string& instanceId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return instances_.find(instanceId) != instances_.end();
}

void InstanceRegistry::loadPersistentInstances() {
    std::vector<std::string> instanceIds = instance_storage_.loadAllInstances();
    
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& instanceId : instanceIds) {
        auto optInfo = instance_storage_.loadInstance(instanceId);
        if (optInfo.has_value()) {
            instances_[instanceId] = optInfo.value();
            // Note: Pipelines are not restored from storage
            // They need to be rebuilt if needed
        }
    }
}

InstanceInfo InstanceRegistry::createInstanceInfo(
    const std::string& instanceId,
    const CreateInstanceRequest& req,
    const SolutionConfig* solution) const {
    
    InstanceInfo info;
    info.instanceId = instanceId;
    info.displayName = req.name;
    info.group = req.group;
    info.persistent = req.persistent;
    info.frameRateLimit = req.frameRateLimit;
    info.metadataMode = req.metadataMode;
    info.statisticsMode = req.statisticsMode;
    info.diagnosticsMode = req.diagnosticsMode;
    info.debugMode = req.debugMode;
    info.detectorMode = req.detectorMode;
    info.detectionSensitivity = req.detectionSensitivity;
    info.movementSensitivity = req.movementSensitivity;
    info.sensorModality = req.sensorModality;
    info.autoStart = req.autoStart;
    info.autoRestart = req.autoRestart;
    info.inputOrientation = req.inputOrientation;
    info.inputPixelLimit = req.inputPixelLimit;
    info.loaded = true;
    info.running = false;
    info.fps = 0.0;
    
    // Get version from CVEDIX SDK
    #ifdef CVEDIX_VERSION_STRING
    info.version = CVEDIX_VERSION_STRING;
    #else
    info.version = "2025.0.1.2"; // Default version
    #endif
    
    if (solution) {
        info.solutionId = solution->solutionId;
        info.solutionName = solution->solutionName;
    }
    
    return info;
}

bool InstanceRegistry::startPipeline(const std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>>& nodes) {
    if (nodes.empty()) {
        std::cerr << "[InstanceRegistry] Cannot start pipeline: no nodes" << std::endl;
        return false;
    }
    
    try {
        // Start from the first node (source node)
        // The pipeline will start automatically when source node starts
        // Cast to rtsp_src_node since that's typically the first node in our pipelines
        auto rtspNode = std::dynamic_pointer_cast<cvedix_nodes::cvedix_rtsp_src_node>(nodes[0]);
        if (rtspNode) {
            std::cerr << "[InstanceRegistry] ========================================" << std::endl;
            std::cerr << "[InstanceRegistry] Starting RTSP pipeline..." << std::endl;
            std::cerr << "[InstanceRegistry] NOTE: RTSP node will automatically retry connection if stream is not immediately available" << std::endl;
            std::cerr << "[InstanceRegistry] NOTE: Connection warnings are normal if RTSP stream is not running yet" << std::endl;
            std::cerr << "[InstanceRegistry] NOTE: CVEDIX SDK uses retry mechanism - connection may take 10-30 seconds" << std::endl;
            std::cerr << "[InstanceRegistry] NOTE: If connection continues to fail, check:" << std::endl;
            std::cerr << "[InstanceRegistry]   1. RTSP server is running and accessible" << std::endl;
            std::cerr << "[InstanceRegistry]   2. Network connectivity (ping/port test)" << std::endl;
            std::cerr << "[InstanceRegistry]   3. RTSP URL format is correct" << std::endl;
            std::cerr << "[InstanceRegistry]   4. Firewall allows RTSP connections" << std::endl;
            std::cerr << "[InstanceRegistry] ========================================" << std::endl;
            
            // Add small delay to ensure pipeline is ready
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            rtspNode->start();
            std::cerr << "[InstanceRegistry] RTSP pipeline started (connection may take a few seconds)" << std::endl;
            std::cerr << "[InstanceRegistry] The SDK will automatically retry connection - monitor logs for connection status" << std::endl;
            return true;
        }
        // If not rtsp node, try to call start() directly (might work for other source types)
        // This is a fallback - in practice, first node should be rtsp_src
        std::cerr << "[InstanceRegistry] Warning: First node is not RTSP source node" << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "[InstanceRegistry] Exception starting pipeline: " << e.what() << std::endl;
        std::cerr << "[InstanceRegistry] This may indicate a configuration issue with the RTSP source" << std::endl;
        return false;
    }
}

void InstanceRegistry::stopPipeline(const std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>>& nodes) {
    if (nodes.empty()) {
        return;
    }
    
    try {
        // First, stop the RTSP source node if it exists (typically the first node)
        // This is important to stop the connection retry loop
        if (!nodes.empty()) {
            auto rtspNode = std::dynamic_pointer_cast<cvedix_nodes::cvedix_rtsp_src_node>(nodes[0]);
            if (rtspNode) {
                std::cerr << "[InstanceRegistry] Stopping RTSP source node..." << std::endl;
                try {
                    rtspNode->stop();
                    std::cerr << "[InstanceRegistry] RTSP source node stopped" << std::endl;
                    // Give it a moment to fully stop
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                } catch (const std::exception& e) {
                    std::cerr << "[InstanceRegistry] Exception stopping RTSP node: " << e.what() << std::endl;
                } catch (...) {
                    std::cerr << "[InstanceRegistry] Unknown error stopping RTSP node" << std::endl;
                }
            }
        }
        
        // Then detach all nodes
        for (auto& node : nodes) {
            if (node) {
                try {
                    node->detach_recursively();
                } catch (const std::exception& e) {
                    // Ignore errors during detach
                }
            }
        }
        std::cerr << "[InstanceRegistry] Pipeline stopped and detached" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[InstanceRegistry] Exception in stopPipeline: " << e.what() << std::endl;
    }
}

bool InstanceRegistry::rebuildPipelineFromInstanceInfo(const std::string& instanceId) {
    auto instanceIt = instances_.find(instanceId);
    if (instanceIt == instances_.end()) {
        return false;
    }
    
    const InstanceInfo& info = instanceIt->second;
    
    // Check if instance has a solution ID
    if (info.solutionId.empty()) {
        std::cerr << "[InstanceRegistry] Cannot rebuild pipeline: instance " << instanceId 
                  << " has no solution ID" << std::endl;
        return false;
    }
    
    // Get solution config
    auto optSolution = solution_registry_.getSolution(info.solutionId);
    if (!optSolution.has_value()) {
        std::cerr << "[InstanceRegistry] Cannot rebuild pipeline: solution '" << info.solutionId 
                  << "' not found" << std::endl;
        return false;
    }
    
    SolutionConfig solution = optSolution.value();
    
    // Create CreateInstanceRequest from InstanceInfo
    CreateInstanceRequest req;
    req.name = info.displayName;
    req.group = info.group;
    req.solution = info.solutionId;
    req.persistent = info.persistent;
    req.frameRateLimit = info.frameRateLimit;
    req.metadataMode = info.metadataMode;
    req.statisticsMode = info.statisticsMode;
    req.diagnosticsMode = info.diagnosticsMode;
    req.debugMode = info.debugMode;
    req.detectorMode = info.detectorMode;
    req.detectionSensitivity = info.detectionSensitivity;
    req.movementSensitivity = info.movementSensitivity;
    req.sensorModality = info.sensorModality;
    req.autoStart = info.autoStart;
    req.autoRestart = info.autoRestart;
    req.inputOrientation = info.inputOrientation;
    req.inputPixelLimit = info.inputPixelLimit;
    
    // Use originator.address as RTSP URL if available
    if (!info.originator.address.empty()) {
        req.additionalParams["RTSP_URL"] = info.originator.address;
    }
    
    // Build pipeline
    std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>> pipeline;
    try {
        pipeline = pipeline_builder_.buildPipeline(solution, req, instanceId);
        if (!pipeline.empty()) {
            pipelines_[instanceId] = pipeline;
            std::cerr << "[InstanceRegistry] Successfully rebuilt pipeline for instance " << instanceId << std::endl;
            return true;
        } else {
            std::cerr << "[InstanceRegistry] Pipeline build returned empty pipeline for instance " << instanceId << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "[InstanceRegistry] Exception rebuilding pipeline for instance " << instanceId 
                  << ": " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "[InstanceRegistry] Unknown error rebuilding pipeline for instance " << instanceId << std::endl;
        return false;
    }
}

