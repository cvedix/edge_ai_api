#include "instances/instance_registry.h"
#include "models/update_instance_request.h"
#include "core/uuid_generator.h"
#include "core/logging_flags.h"
#include "core/logger.h"
#include <cvedix/cvedix_version.h>
#include <cvedix/nodes/src/cvedix_rtsp_src_node.h>
#include <cvedix/nodes/src/cvedix_file_src_node.h>
#include <cvedix/nodes/infers/cvedix_yunet_face_detector_node.h>
#include <cvedix/nodes/infers/cvedix_sface_feature_encoder_node.h>
#include <cvedix/nodes/des/cvedix_rtmp_des_node.h>
#include <algorithm>
#include <typeinfo>
#include <thread>
#include <chrono>
#include <iomanip>
#include <sstream>

InstanceRegistry::InstanceRegistry(
    SolutionRegistry& solutionRegistry,
    PipelineBuilder& pipelineBuilder,
    InstanceStorage& instanceStorage)
    : solution_registry_(solutionRegistry),
      pipeline_builder_(pipelineBuilder),
      instance_storage_(instanceStorage) {
}

std::string InstanceRegistry::createInstance(const CreateInstanceRequest& req) {
    // CRITICAL: Release lock before building pipeline and auto-starting
    // This allows multiple instances to be created concurrently without blocking each other
    
    // Generate instance ID (no lock needed)
    std::string instanceId = UUIDGenerator::generateUUID();
    
    // Get solution config if specified (no lock needed)
    SolutionConfig* solution = nullptr;
    SolutionConfig solutionConfig;
    if (!req.solution.empty()) {
        auto optSolution = solution_registry_.getSolution(req.solution);
        if (!optSolution.has_value()) {
            std::cerr << "[InstanceRegistry] Solution not found: " << req.solution << std::endl;
            std::cerr << "[InstanceRegistry] Available solutions: ";
            auto availableSolutions = solution_registry_.listSolutions();
            for (size_t i = 0; i < availableSolutions.size(); ++i) {
                std::cerr << availableSolutions[i];
                if (i < availableSolutions.size() - 1) {
                    std::cerr << ", ";
                }
            }
            std::cerr << std::endl;
            return ""; // Solution not found
        }
        solutionConfig = optSolution.value();
        solution = &solutionConfig;
    }
    
    // Build pipeline if solution is provided (do this OUTSIDE lock - can take time)
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
    
    // Create instance info (no lock needed)
    InstanceInfo info = createInstanceInfo(instanceId, req, solution);
    
    // Store instance (need lock briefly)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        instances_[instanceId] = info;
        if (!pipeline.empty()) {
            pipelines_[instanceId] = pipeline;
        }
    } // Release lock - save to storage doesn't need it
    
    // Save to storage for all instances (for debugging and inspection)
    // Only persistent instances will be loaded on server restart
    bool saved = instance_storage_.saveInstance(instanceId, info);
    if (saved) {
        if (req.persistent) {
            std::cerr << "[InstanceRegistry] Instance configuration saved (persistent - will be loaded on restart)" << std::endl;
        } else {
            std::cerr << "[InstanceRegistry] Instance configuration saved (non-persistent - for inspection only)" << std::endl;
        }
    } else {
        std::cerr << "[InstanceRegistry] Warning: Failed to save instance configuration to file" << std::endl;
    }
    
    // Auto start if requested (do this OUTSIDE lock - can take time)
    // IMPORTANT: Run auto-start in a separate thread to avoid blocking API response
    if (req.autoStart && !pipeline.empty()) {
        std::cerr << "[InstanceRegistry] ========================================" << std::endl;
        std::cerr << "[InstanceRegistry] Auto-starting pipeline for instance " << instanceId << " (async)" << std::endl;
        std::cerr << "[InstanceRegistry] ========================================" << std::endl;
        
        // Start auto-start process in a separate thread (non-blocking)
        // This allows the API to return immediately after instance creation
        std::thread([this, instanceId, pipeline]() {
            // Wait for DNN models to be ready using exponential backoff
            // This is more reliable than fixed delay as it adapts to model loading time
            waitForModelsReady(pipeline, 2000); // Max 2 seconds
            
            try {
                if (startPipeline(pipeline)) {
                    // Update running status (need lock briefly)
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        auto instanceIt = instances_.find(instanceId);
                        if (instanceIt != instances_.end()) {
                            instanceIt->second.running = true;
                        }
                    } // Release lock
                    std::cerr << "[InstanceRegistry] ========================================" << std::endl;
                    std::cerr << "[InstanceRegistry] ✓ Pipeline started successfully for instance " << instanceId << std::endl;
                    std::cerr << "[InstanceRegistry] NOTE: If RTSP connection fails, the node will retry automatically" << std::endl;
                    std::cerr << "[InstanceRegistry] NOTE: Monitor logs above for RTSP connection status" << std::endl;
                    std::cerr << "[InstanceRegistry] ========================================" << std::endl;
                    
                    // Log processing results for instances without RTMP output
                    // Wait a bit for pipeline to initialize
                    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                    if (!hasRTMPOutput(instanceId)) {
                        std::cerr << "[InstanceRegistry] Instance does not have RTMP output - enabling processing result logging" << std::endl;
                        logProcessingResults(instanceId);
                        
                        // Start periodic logging in a separate thread (non-blocking)
                        std::thread([this, instanceId]() {
                            while (true) {
                                // Wait 10 seconds between logs
                                std::this_thread::sleep_for(std::chrono::seconds(10));
                                
                                // Check if instance still exists and is running
                                {
                                    std::lock_guard<std::mutex> lock(mutex_);
                                    auto instanceIt = instances_.find(instanceId);
                                    if (instanceIt == instances_.end() || !instanceIt->second.running) {
                                        // Instance deleted or stopped, exit logging thread
                                        break;
                                    }
                                }
                                
                                // Log processing results
                                if (!hasRTMPOutput(instanceId)) {
                                    logProcessingResults(instanceId);
                                } else {
                                    // Instance now has RTMP output, stop logging
                                    break;
                                }
                            }
                        }).detach(); // Detach thread so it runs independently
                    }
                } else {
                    std::cerr << "[InstanceRegistry] ✗ Failed to start pipeline for instance " 
                              << instanceId << " (pipeline created but not started)" << std::endl;
                    std::cerr << "[InstanceRegistry] You can manually start it later using startInstance API" << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "[InstanceRegistry] ✗ Exception starting pipeline for instance " 
                          << instanceId << ": " << e.what() << std::endl;
                std::cerr << "[InstanceRegistry] Instance created but pipeline not started. You can start it manually later." << std::endl;
                // Don't crash - instance is created but not running
            } catch (...) {
                std::cerr << "[InstanceRegistry] ✗ Unknown error starting pipeline for instance " 
                          << instanceId << std::endl;
                std::cerr << "[InstanceRegistry] Instance created but pipeline not started. You can start it manually later." << std::endl;
            }
        }).detach(); // Detach thread so it runs independently and doesn't block
    } else if (!pipeline.empty()) {
        std::cerr << "[InstanceRegistry] Pipeline created but not started (autoStart=false)" << std::endl;
        std::cerr << "[InstanceRegistry] Use startInstance API to start the pipeline when ready" << std::endl;
    }
    
    return instanceId;
}

bool InstanceRegistry::deleteInstance(const std::string& instanceId) {
    // CRITICAL: Get pipeline copy and release lock before calling stopPipeline
    // stopPipeline can take a long time and doesn't need the lock
    // This prevents blocking other operations when deleting instances
    
    std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>> pipelineToStop;
    bool isPersistent = false;
    bool instanceExists = false;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = instances_.find(instanceId);
        if (it == instances_.end()) {
            return false;
        }
        
        instanceExists = true;
        isPersistent = it->second.persistent;
        
        // Get pipeline copy before releasing lock
        auto pipelineIt = pipelines_.find(instanceId);
        if (pipelineIt != pipelines_.end() && !pipelineIt->second.empty()) {
            pipelineToStop = pipelineIt->second;
        }
        
        // Remove from maps immediately to prevent other threads from accessing
        pipelines_.erase(instanceId);
        instances_.erase(it);
    } // Release lock - stopPipeline and storage operations don't need it
    
    std::cerr << "[InstanceRegistry] ========================================" << std::endl;
    std::cerr << "[InstanceRegistry] Deleting instance " << instanceId << "..." << std::endl;
    std::cerr << "[InstanceRegistry] ========================================" << std::endl;
    
    // Stop pipeline if running (cleanup before deletion) - do this outside lock
    if (!pipelineToStop.empty()) {
        std::cerr << "[InstanceRegistry] Stopping pipeline before deletion..." << std::endl;
        try {
            stopPipeline(pipelineToStop, true);  // true = deletion, cleanup everything
            pipelineToStop.clear(); // Ensure nodes are destroyed
            std::cerr << "[InstanceRegistry] Pipeline stopped and removed" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[InstanceRegistry] Exception stopping pipeline during deletion: " << e.what() << std::endl;
            // Continue with deletion anyway
        } catch (...) {
            std::cerr << "[InstanceRegistry] Unknown exception stopping pipeline during deletion" << std::endl;
            // Continue with deletion anyway
        }
    }
    
    // Delete from storage (doesn't need lock)
    if (isPersistent) {
        std::cerr << "[InstanceRegistry] Removing persistent instance from storage..." << std::endl;
        instance_storage_.deleteInstance(instanceId);
    }
    
    std::cerr << "[InstanceRegistry] ✓ Instance " << instanceId << " deleted successfully" << std::endl;
    std::cerr << "[InstanceRegistry] ========================================" << std::endl;
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

bool InstanceRegistry::startInstance(const std::string& instanceId, bool skipAutoStop) {
    // NEW BEHAVIOR: Start instance means create a new instance with same ID and config
    // This ensures fresh pipeline is built every time
    
    InstanceInfo existingInfo;
    bool instanceExists = false;
    std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>> pipelineToStop;
    bool wasRunning = false;
    
    // Get existing instance info
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto instanceIt = instances_.find(instanceId);
        if (instanceIt == instances_.end()) {
            std::cerr << "[InstanceRegistry] Instance " << instanceId << " not found" << std::endl;
            return false;
        }
        
        instanceExists = true;
        existingInfo = instanceIt->second;
        
        // Stop instance if it's running (unless skipAutoStop is true)
        wasRunning = instanceIt->second.running;
        
        if (wasRunning && !skipAutoStop) {
            std::cerr << "[InstanceRegistry] Instance " << instanceId << " is currently running, stopping it first..." << std::endl;
            instanceIt->second.running = false;
            
            // Get pipeline copy before releasing lock
            auto pipelineIt = pipelines_.find(instanceId);
            if (pipelineIt != pipelines_.end() && !pipelineIt->second.empty()) {
                pipelineToStop = pipelineIt->second;
            }
        } else if (wasRunning && skipAutoStop) {
            // If skipAutoStop is true, instance should already be stopped
            // Fail if instance is still running to prevent resource conflicts
            if (instanceIt->second.running) {
                std::cerr << "[InstanceRegistry] ✗ Error: Instance " << instanceId << " is still running despite skipAutoStop=true" << std::endl;
                std::cerr << "[InstanceRegistry] Instance must be stopped before calling startInstance with skipAutoStop=true" << std::endl;
                return false; // Fail early to prevent resource conflicts
            }
        }
        
        // Remove old pipeline from map to ensure fresh build
        // This is safe because:
        // - If wasRunning && !skipAutoStop: pipeline copy is saved in pipelineToStop before erase
        // - If wasRunning && skipAutoStop: instance should already be stopped, so no active pipeline
        // - If !wasRunning: no active pipeline to worry about
        pipelines_.erase(instanceId);
    } // Release lock
    
    // Stop pipeline if it was running and not skipping auto-stop (do this outside lock)
    // Use full cleanup to ensure OpenCV DNN state is cleared
    if (wasRunning && !skipAutoStop && !pipelineToStop.empty()) {
        stopPipeline(pipelineToStop, true); // true = full cleanup to clear DNN state
        pipelineToStop.clear(); // Ensure nodes are destroyed immediately
    }
    
    std::cerr << "[InstanceRegistry] ========================================" << std::endl;
    std::cerr << "[InstanceRegistry] Starting instance " << instanceId << " (creating new pipeline)..." << std::endl;
    std::cerr << "[InstanceRegistry] ========================================" << std::endl;
    
    if (isInstanceLoggingEnabled()) {
        PLOG_INFO << "[Instance] Starting instance: " << instanceId 
                  << " (" << existingInfo.displayName << ", solution: " << existingInfo.solutionId << ")";
    }
    
    // Rebuild pipeline from instance info (this creates a fresh pipeline)
    // Check instance still exists before rebuilding (may have been deleted)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (instances_.find(instanceId) == instances_.end()) {
            std::cerr << "[InstanceRegistry] ✗ Instance " << instanceId << " was deleted during start operation" << std::endl;
            return false;
        }
    } // Release lock
    
    if (!rebuildPipelineFromInstanceInfo(instanceId)) {
        std::cerr << "[InstanceRegistry] ✗ Failed to rebuild pipeline for instance " << instanceId << std::endl;
        return false;
    }
    
    // Get pipeline copy after rebuild
    // Check instance still exists (may have been deleted during rebuild)
    std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>> pipelineCopy;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (instances_.find(instanceId) == instances_.end()) {
            std::cerr << "[InstanceRegistry] ✗ Instance " << instanceId << " was deleted during rebuild" << std::endl;
            // Cleanup pipeline that was just created
            pipelines_.erase(instanceId);
            return false;
        }
        auto pipelineIt = pipelines_.find(instanceId);
        if (pipelineIt == pipelines_.end() || pipelineIt->second.empty()) {
            std::cerr << "[InstanceRegistry] ✗ Pipeline rebuild failed or returned empty pipeline" << std::endl;
            return false;
        }
        pipelineCopy = pipelineIt->second;
    } // Release lock
    
    std::cerr << "[InstanceRegistry] ✓ Pipeline rebuilt successfully (fresh pipeline)" << std::endl;
    
    // Wait for models to be ready (use adaptive timeout)
    std::cerr << "[InstanceRegistry] Waiting for models to be ready (adaptive, up to 2 seconds)..." << std::endl;
    std::cerr << "[InstanceRegistry] This ensures OpenCV DNN clears any cached state and models are fully initialized" << std::endl;
    
    // Check instance still exists before waiting
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (instances_.find(instanceId) == instances_.end()) {
            std::cerr << "[InstanceRegistry] ✗ Instance " << instanceId << " was deleted before model initialization" << std::endl;
            pipelines_.erase(instanceId);
            return false;
        }
    } // Release lock
    
    try {
        waitForModelsReady(pipelineCopy, 2000); // 2 seconds max
    } catch (const std::exception& e) {
        std::cerr << "[InstanceRegistry] ✗ Exception waiting for models: " << e.what() << std::endl;
        // Cleanup pipeline on error
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pipelines_.erase(instanceId);
        }
        return false;
    } catch (...) {
        std::cerr << "[InstanceRegistry] ✗ Unknown exception waiting for models" << std::endl;
        // Cleanup pipeline on error
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pipelines_.erase(instanceId);
        }
        return false;
    }
    
    // Additional delay after rebuild to ensure OpenCV DNN has fully cleared old state
    std::cerr << "[InstanceRegistry] Additional stabilization delay after rebuild (2 seconds)..." << std::endl;
    std::cerr << "[InstanceRegistry] This ensures OpenCV DNN has fully cleared any cached state from previous run" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    
    // Check instance still exists after delay
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (instances_.find(instanceId) == instances_.end()) {
            std::cerr << "[InstanceRegistry] ✗ Instance " << instanceId << " was deleted during stabilization delay" << std::endl;
            pipelines_.erase(instanceId);
            return false;
        }
    } // Release lock
    
    std::cerr << "[InstanceRegistry] ========================================" << std::endl;
    std::cerr << "[InstanceRegistry] Starting pipeline for instance " << instanceId << "..." << std::endl;
    std::cerr << "[InstanceRegistry] ========================================" << std::endl;
    
    // Start pipeline (isRestart=true because we rebuilt the pipeline)
    bool started = false;
    try {
        started = startPipeline(pipelineCopy, true);
    } catch (const std::exception& e) {
        std::cerr << "[InstanceRegistry] ✗ Exception starting pipeline: " << e.what() << std::endl;
        // Cleanup pipeline on error
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pipelines_.erase(instanceId);
        }
        return false;
    } catch (...) {
        std::cerr << "[InstanceRegistry] ✗ Unknown exception starting pipeline" << std::endl;
        // Cleanup pipeline on error
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pipelines_.erase(instanceId);
        }
        return false;
    }
    
    // Update running status and cleanup on failure
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto instanceIt = instances_.find(instanceId);
        if (instanceIt != instances_.end()) {
            if (started) {
                instanceIt->second.running = true;
                std::cerr << "[InstanceRegistry] ✓ Instance " << instanceId << " started successfully" << std::endl;
                if (isInstanceLoggingEnabled()) {
                    const auto& info = instanceIt->second;
                    PLOG_INFO << "[Instance] Instance started successfully: " << instanceId 
                              << " (" << info.displayName << ", solution: " << info.solutionId 
                              << ", running: true)";
                }
            } else {
                std::cerr << "[InstanceRegistry] ✗ Failed to start instance " << instanceId << std::endl;
                if (isInstanceLoggingEnabled()) {
                    PLOG_ERROR << "[Instance] Failed to start instance: " << instanceId 
                               << " (" << existingInfo.displayName << ")";
                }
                // Cleanup pipeline if start failed to prevent resource leak
                pipelines_.erase(instanceId);
                std::cerr << "[InstanceRegistry] Cleaned up pipeline after start failure" << std::endl;
            }
        } else {
            // Instance was deleted during start - cleanup pipeline
            pipelines_.erase(instanceId);
            std::cerr << "[InstanceRegistry] Instance " << instanceId << " was deleted during start - cleaned up pipeline" << std::endl;
            if (isInstanceLoggingEnabled()) {
                PLOG_WARNING << "[Instance] Instance was deleted during start: " << instanceId;
            }
        }
    }
    
    // Log processing results for instances without RTMP output (after a short delay to allow pipeline to start)
    if (started) {
        // Wait a bit for pipeline to initialize and start processing
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        
        // Log initial processing status
        if (!hasRTMPOutput(instanceId)) {
            std::cerr << "[InstanceRegistry] Instance does not have RTMP output - enabling processing result logging" << std::endl;
            logProcessingResults(instanceId);
            
            // Start periodic logging in a separate thread (non-blocking)
            std::thread([this, instanceId]() {
                while (true) {
                    // Wait 10 seconds between logs
                    std::this_thread::sleep_for(std::chrono::seconds(10));
                    
                    // Check if instance still exists and is running
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        auto instanceIt = instances_.find(instanceId);
                        if (instanceIt == instances_.end() || !instanceIt->second.running) {
                            // Instance deleted or stopped, exit logging thread
                            break;
                        }
                    }
                    
                    // Log processing results
                    if (!hasRTMPOutput(instanceId)) {
                        logProcessingResults(instanceId);
                    } else {
                        // Instance now has RTMP output, stop logging
                        break;
                    }
                }
            }).detach(); // Detach thread so it runs independently
        }
    }
    
    return started;
}

bool InstanceRegistry::stopInstance(const std::string& instanceId) {
    // CRITICAL: Get pipeline copy and release lock before calling stopPipeline
    // stopPipeline can take a long time and doesn't need the lock
    // This prevents deadlock if another thread (like terminate handler) needs the lock
    std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>> pipelineCopy;
    bool instanceExists = false;
    bool wasRunning = false;
    std::string displayName;
    std::string solutionId;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto pipelineIt = pipelines_.find(instanceId);
        if (pipelineIt == pipelines_.end()) {
            return false;
        }
        
        auto instanceIt = instances_.find(instanceId);
        if (instanceIt == instances_.end()) {
            return false;
        }
        
        instanceExists = true;
        wasRunning = instanceIt->second.running;
        displayName = instanceIt->second.displayName;
        solutionId = instanceIt->second.solutionId;
        
        // Copy pipeline before releasing lock
        pipelineCopy = pipelineIt->second;
        
        // Mark as not running immediately (before stopPipeline)
        instanceIt->second.running = false;
        
        // Remove from pipelines map immediately to prevent other threads from accessing it
        pipelines_.erase(pipelineIt);
    } // Release lock here - stopPipeline doesn't need it
    
    std::cerr << "[InstanceRegistry] ========================================" << std::endl;
    std::cerr << "[InstanceRegistry] Stopping instance " << instanceId << "..." << std::endl;
    std::cerr << "[InstanceRegistry] NOTE: All nodes will be fully destroyed to clear OpenCV DNN state" << std::endl;
    std::cerr << "[InstanceRegistry] ========================================" << std::endl;
    
    if (isInstanceLoggingEnabled()) {
        PLOG_INFO << "[Instance] Stopping instance: " << instanceId 
                  << " (" << displayName << ", solution: " << solutionId 
                  << ", was running: " << (wasRunning ? "true" : "false") << ")";
    }
    
    // Now call stopPipeline without holding the lock
    // This prevents deadlock if stopPipeline takes a long time
    // Use isDeletion=true to fully cleanup nodes and clear OpenCV DNN state
    // CRITICAL: stopPipeline() is now guaranteed to never throw (it catches all exceptions internally)
    try {
        stopPipeline(pipelineCopy, true);  // true = full cleanup like deletion to clear DNN state
    } catch (const std::exception& e) {
        // This should never happen since stopPipeline catches all exceptions, but just in case...
        std::cerr << "[InstanceRegistry] CRITICAL: Unexpected exception in stopPipeline: " << e.what() << std::endl;
        std::cerr << "[InstanceRegistry] This indicates a bug - stopPipeline should not throw" << std::endl;
        // Continue anyway - pipeline is already removed from map
    } catch (...) {
        // This should never happen since stopPipeline catches all exceptions, but just in case...
        std::cerr << "[InstanceRegistry] CRITICAL: Unexpected unknown exception in stopPipeline" << std::endl;
        std::cerr << "[InstanceRegistry] This indicates a bug - stopPipeline should not throw" << std::endl;
        // Continue anyway - pipeline is already removed from map
    }
    
    // Clear pipeline copy to ensure all nodes are destroyed immediately
    // This helps ensure OpenCV DNN releases all internal state
    // Wrap in try-catch to be extra safe (though clear() shouldn't throw)
    try {
        pipelineCopy.clear();
    } catch (...) {
        std::cerr << "[InstanceRegistry] Warning: Exception clearing pipeline copy (unexpected)" << std::endl;
    }
    
    std::cerr << "[InstanceRegistry] ✓ Instance " << instanceId << " stopped successfully" << std::endl;
    std::cerr << "[InstanceRegistry] NOTE: All nodes have been destroyed. Pipeline will be rebuilt from scratch when you start this instance again" << std::endl;
    std::cerr << "[InstanceRegistry] NOTE: This ensures OpenCV DNN starts with a clean state" << std::endl;
    std::cerr << "[InstanceRegistry] ========================================" << std::endl;
    
    if (isInstanceLoggingEnabled()) {
        PLOG_INFO << "[Instance] Instance stopped successfully: " << instanceId 
                  << " (" << displayName << ", solution: " << solutionId << ")";
    }
    
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

std::unordered_map<std::string, InstanceInfo> InstanceRegistry::getAllInstances() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return instances_; // Return copy of all instances in one lock acquisition
}

bool InstanceRegistry::hasInstance(const std::string& instanceId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return instances_.find(instanceId) != instances_.end();
}

bool InstanceRegistry::updateInstance(const std::string& instanceId, const UpdateInstanceRequest& req) {
    bool isPersistent = false;
    InstanceInfo infoCopy;
    bool hasChanges = false;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto instanceIt = instances_.find(instanceId);
        if (instanceIt == instances_.end()) {
            std::cerr << "[InstanceRegistry] Instance " << instanceId << " not found" << std::endl;
            return false;
        }
        
        InstanceInfo& info = instanceIt->second;
        
        // Check if instance is read-only
        if (info.readOnly) {
            std::cerr << "[InstanceRegistry] Cannot update read-only instance " << instanceId << std::endl;
            return false;
        }
        
        std::cerr << "[InstanceRegistry] ========================================" << std::endl;
        std::cerr << "[InstanceRegistry] Updating instance " << instanceId << "..." << std::endl;
        std::cerr << "[InstanceRegistry] ========================================" << std::endl;
        
        // Update fields if provided
        if (!req.name.empty()) {
            std::cerr << "[InstanceRegistry] Updating displayName: " << info.displayName << " -> " << req.name << std::endl;
            info.displayName = req.name;
            hasChanges = true;
        }
        
        if (!req.group.empty()) {
            std::cerr << "[InstanceRegistry] Updating group: " << info.group << " -> " << req.group << std::endl;
            info.group = req.group;
            hasChanges = true;
        }
        
        if (req.persistent.has_value()) {
            std::cerr << "[InstanceRegistry] Updating persistent: " << info.persistent << " -> " << req.persistent.value() << std::endl;
            info.persistent = req.persistent.value();
            hasChanges = true;
        }
        
        if (req.frameRateLimit != -1) {
            std::cerr << "[InstanceRegistry] Updating frameRateLimit: " << info.frameRateLimit << " -> " << req.frameRateLimit << std::endl;
            info.frameRateLimit = req.frameRateLimit;
            hasChanges = true;
        }
        
        if (req.metadataMode.has_value()) {
            std::cerr << "[InstanceRegistry] Updating metadataMode: " << info.metadataMode << " -> " << req.metadataMode.value() << std::endl;
            info.metadataMode = req.metadataMode.value();
            hasChanges = true;
        }
        
        if (req.statisticsMode.has_value()) {
            std::cerr << "[InstanceRegistry] Updating statisticsMode: " << info.statisticsMode << " -> " << req.statisticsMode.value() << std::endl;
            info.statisticsMode = req.statisticsMode.value();
            hasChanges = true;
        }
        
        if (req.diagnosticsMode.has_value()) {
            std::cerr << "[InstanceRegistry] Updating diagnosticsMode: " << info.diagnosticsMode << " -> " << req.diagnosticsMode.value() << std::endl;
            info.diagnosticsMode = req.diagnosticsMode.value();
            hasChanges = true;
        }
        
        if (req.debugMode.has_value()) {
            std::cerr << "[InstanceRegistry] Updating debugMode: " << info.debugMode << " -> " << req.debugMode.value() << std::endl;
            info.debugMode = req.debugMode.value();
            hasChanges = true;
        }
        
        if (!req.detectorMode.empty()) {
            std::cerr << "[InstanceRegistry] Updating detectorMode: " << info.detectorMode << " -> " << req.detectorMode << std::endl;
            info.detectorMode = req.detectorMode;
            hasChanges = true;
        }
        
        if (!req.detectionSensitivity.empty()) {
            std::cerr << "[InstanceRegistry] Updating detectionSensitivity: " << info.detectionSensitivity << " -> " << req.detectionSensitivity << std::endl;
            info.detectionSensitivity = req.detectionSensitivity;
            hasChanges = true;
        }
        
        if (!req.movementSensitivity.empty()) {
            std::cerr << "[InstanceRegistry] Updating movementSensitivity: " << info.movementSensitivity << " -> " << req.movementSensitivity << std::endl;
            info.movementSensitivity = req.movementSensitivity;
            hasChanges = true;
        }
        
        if (!req.sensorModality.empty()) {
            std::cerr << "[InstanceRegistry] Updating sensorModality: " << info.sensorModality << " -> " << req.sensorModality << std::endl;
            info.sensorModality = req.sensorModality;
            hasChanges = true;
        }
        
        if (req.autoStart.has_value()) {
            std::cerr << "[InstanceRegistry] Updating autoStart: " << info.autoStart << " -> " << req.autoStart.value() << std::endl;
            info.autoStart = req.autoStart.value();
            hasChanges = true;
        }
        
        if (req.autoRestart.has_value()) {
            std::cerr << "[InstanceRegistry] Updating autoRestart: " << info.autoRestart << " -> " << req.autoRestart.value() << std::endl;
            info.autoRestart = req.autoRestart.value();
            hasChanges = true;
        }
        
        if (req.inputOrientation != -1) {
            std::cerr << "[InstanceRegistry] Updating inputOrientation: " << info.inputOrientation << " -> " << req.inputOrientation << std::endl;
            info.inputOrientation = req.inputOrientation;
            hasChanges = true;
        }
        
        if (req.inputPixelLimit != -1) {
            std::cerr << "[InstanceRegistry] Updating inputPixelLimit: " << info.inputPixelLimit << " -> " << req.inputPixelLimit << std::endl;
            info.inputPixelLimit = req.inputPixelLimit;
            hasChanges = true;
        }
        
        // Update additionalParams (merge with existing)
        if (!req.additionalParams.empty()) {
            std::cerr << "[InstanceRegistry] Updating additionalParams..." << std::endl;
            for (const auto& pair : req.additionalParams) {
                std::cerr << "[InstanceRegistry]   " << pair.first << ": " << info.additionalParams[pair.first] << " -> " << pair.second << std::endl;
                info.additionalParams[pair.first] = pair.second;
            }
            hasChanges = true;
            
            // Update RTSP URL if changed
            auto rtspIt = req.additionalParams.find("RTSP_URL");
            if (rtspIt != req.additionalParams.end() && !rtspIt->second.empty()) {
                info.rtspUrl = rtspIt->second;
            }
            
            // Update RTMP URL if changed
            auto rtmpIt = req.additionalParams.find("RTMP_URL");
            if (rtmpIt != req.additionalParams.end() && !rtmpIt->second.empty()) {
                info.rtmpUrl = rtmpIt->second;
            }
            
            // Update FILE_PATH if changed
            auto filePathIt = req.additionalParams.find("FILE_PATH");
            if (filePathIt != req.additionalParams.end() && !filePathIt->second.empty()) {
                info.filePath = filePathIt->second;
            }
            
            // Update Detector model file
            auto detectorModelIt = req.additionalParams.find("DETECTOR_MODEL_FILE");
            if (detectorModelIt != req.additionalParams.end() && !detectorModelIt->second.empty()) {
                info.detectorModelFile = detectorModelIt->second;
            }
            
            // Update DetectorThermal model file
            auto thermalModelIt = req.additionalParams.find("DETECTOR_THERMAL_MODEL_FILE");
            if (thermalModelIt != req.additionalParams.end() && !thermalModelIt->second.empty()) {
                info.detectorThermalModelFile = thermalModelIt->second;
            }
            
            // Update confidence thresholds
            auto animalThreshIt = req.additionalParams.find("ANIMAL_CONFIDENCE_THRESHOLD");
            if (animalThreshIt != req.additionalParams.end() && !animalThreshIt->second.empty()) {
                try {
                    info.animalConfidenceThreshold = std::stod(animalThreshIt->second);
                } catch (...) {
                    std::cerr << "[InstanceRegistry] Invalid animal_confidence_threshold value: " << animalThreshIt->second << std::endl;
                }
            }
            
            auto personThreshIt = req.additionalParams.find("PERSON_CONFIDENCE_THRESHOLD");
            if (personThreshIt != req.additionalParams.end() && !personThreshIt->second.empty()) {
                try {
                    info.personConfidenceThreshold = std::stod(personThreshIt->second);
                } catch (...) {
                    std::cerr << "[InstanceRegistry] Invalid person_confidence_threshold value: " << personThreshIt->second << std::endl;
                }
            }
            
            auto vehicleThreshIt = req.additionalParams.find("VEHICLE_CONFIDENCE_THRESHOLD");
            if (vehicleThreshIt != req.additionalParams.end() && !vehicleThreshIt->second.empty()) {
                try {
                    info.vehicleConfidenceThreshold = std::stod(vehicleThreshIt->second);
                } catch (...) {
                    std::cerr << "[InstanceRegistry] Invalid vehicle_confidence_threshold value: " << vehicleThreshIt->second << std::endl;
                }
            }
            
            auto faceThreshIt = req.additionalParams.find("FACE_CONFIDENCE_THRESHOLD");
            if (faceThreshIt != req.additionalParams.end() && !faceThreshIt->second.empty()) {
                try {
                    info.faceConfidenceThreshold = std::stod(faceThreshIt->second);
                } catch (...) {
                    std::cerr << "[InstanceRegistry] Invalid face_confidence_threshold value: " << faceThreshIt->second << std::endl;
                }
            }
            
            auto licenseThreshIt = req.additionalParams.find("LICENSE_PLATE_CONFIDENCE_THRESHOLD");
            if (licenseThreshIt != req.additionalParams.end() && !licenseThreshIt->second.empty()) {
                try {
                    info.licensePlateConfidenceThreshold = std::stod(licenseThreshIt->second);
                } catch (...) {
                    std::cerr << "[InstanceRegistry] Invalid license_plate_confidence_threshold value: " << licenseThreshIt->second << std::endl;
                }
            }
            
            auto confThreshIt = req.additionalParams.find("CONF_THRESHOLD");
            if (confThreshIt != req.additionalParams.end() && !confThreshIt->second.empty()) {
                try {
                    info.confThreshold = std::stod(confThreshIt->second);
                } catch (...) {
                    std::cerr << "[InstanceRegistry] Invalid conf_threshold value: " << confThreshIt->second << std::endl;
                }
            }
            
            // Update PerformanceMode
            auto perfModeIt = req.additionalParams.find("PERFORMANCE_MODE");
            if (perfModeIt != req.additionalParams.end() && !perfModeIt->second.empty()) {
                info.performanceMode = perfModeIt->second;
            }
        }
        
        if (!hasChanges) {
            std::cerr << "[InstanceRegistry] No changes to update" << std::endl;
            std::cerr << "[InstanceRegistry] ========================================" << std::endl;
            return true; // No changes, but still success
        }
        
        // Copy info for saving (release lock before saving)
        isPersistent = info.persistent;
        infoCopy = info;
    } // Release lock here
    
    // Save to storage if persistent (do this outside lock)
    if (isPersistent) {
        bool saved = instance_storage_.saveInstance(instanceId, infoCopy);
        if (saved) {
            std::cerr << "[InstanceRegistry] Instance configuration saved to file" << std::endl;
        } else {
            std::cerr << "[InstanceRegistry] Warning: Failed to save instance configuration to file" << std::endl;
        }
    }
    
    std::cerr << "[InstanceRegistry] ✓ Instance " << instanceId << " updated successfully" << std::endl;
    
    // Check if instance is running and restart it to apply changes
    bool wasRunning = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto instanceIt = instances_.find(instanceId);
        if (instanceIt != instances_.end()) {
            wasRunning = instanceIt->second.running;
        }
    }
    
    if (wasRunning) {
        std::cerr << "[InstanceRegistry] Instance is running, restarting to apply changes..." << std::endl;
        
        // Stop instance first
        if (stopInstance(instanceId)) {
            // Wait a moment for cleanup
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // Start instance again (this will rebuild pipeline with new config)
            if (startInstance(instanceId, true)) {
                std::cerr << "[InstanceRegistry] ✓ Instance restarted successfully with new configuration" << std::endl;
            } else {
                std::cerr << "[InstanceRegistry] ⚠ Instance stopped but failed to restart" << std::endl;
                std::cerr << "[InstanceRegistry] NOTE: Configuration has been updated. You can manually start the instance later." << std::endl;
            }
        } else {
            std::cerr << "[InstanceRegistry] ⚠ Failed to stop instance for restart" << std::endl;
            std::cerr << "[InstanceRegistry] NOTE: Configuration has been updated. Restart the instance manually to apply changes." << std::endl;
        }
    } else {
        std::cerr << "[InstanceRegistry] Instance is not running. Changes will take effect when instance is started." << std::endl;
    }
    
    std::cerr << "[InstanceRegistry] ========================================" << std::endl;
    
    return true;
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
    
    // Detector configuration (detailed)
    info.detectorModelFile = req.detectorModelFile;
    info.animalConfidenceThreshold = req.animalConfidenceThreshold;
    info.personConfidenceThreshold = req.personConfidenceThreshold;
    info.vehicleConfidenceThreshold = req.vehicleConfidenceThreshold;
    info.faceConfidenceThreshold = req.faceConfidenceThreshold;
    info.licensePlateConfidenceThreshold = req.licensePlateConfidenceThreshold;
    info.confThreshold = req.confThreshold;
    
    // DetectorThermal configuration
    info.detectorThermalModelFile = req.detectorThermalModelFile;
    
    // Performance mode
    info.performanceMode = req.performanceMode;
    
    // SolutionManager settings
    info.recommendedFrameRate = req.recommendedFrameRate;
    
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
    
    // Copy all additional parameters from request (MODEL_PATH, SFACE_MODEL_PATH, RESIZE_RATIO, etc.)
    info.additionalParams = req.additionalParams;
    
    // Extract RTMP URL from request
    auto rtmpIt = req.additionalParams.find("RTMP_URL");
    if (rtmpIt != req.additionalParams.end() && !rtmpIt->second.empty()) {
        info.rtmpUrl = rtmpIt->second;
        
        // Generate RTSP URL from RTMP URL
        // Pattern: rtmp://host:1935/live/stream_key -> rtsp://host:8554/live/stream_key_0
        // RTMP node automatically adds "_0" suffix to stream key
        std::string rtmpUrl = rtmpIt->second;
        
        // Replace RTMP protocol and port with RTSP
        size_t protocolPos = rtmpUrl.find("rtmp://");
        if (protocolPos != std::string::npos) {
            std::string rtspUrl = rtmpUrl;
            
            // Replace protocol
            rtspUrl.replace(protocolPos, 7, "rtsp://");
            
            // Replace port 1935 with 8554 (common RTSP port for conversion)
            size_t portPos = rtspUrl.find(":1935");
            if (portPos != std::string::npos) {
                rtspUrl.replace(portPos, 5, ":8554");
            }
            
            // Add "_0" suffix to stream key if not already present
            // RTMP node automatically adds this suffix
            size_t lastSlash = rtspUrl.find_last_of('/');
            if (lastSlash != std::string::npos) {
                std::string streamKey = rtspUrl.substr(lastSlash + 1);
                if (streamKey.find("_0") == std::string::npos && !streamKey.empty()) {
                    rtspUrl += "_0";
                }
            }
            
            info.rtspUrl = rtspUrl;
        }
    }
    
    // Extract FILE_PATH from request (for file source solutions)
    auto filePathIt = req.additionalParams.find("FILE_PATH");
    if (filePathIt != req.additionalParams.end() && !filePathIt->second.empty()) {
        info.filePath = filePathIt->second;
    }
    
    return info;
}

// Helper function to wait for DNN models to be ready using exponential backoff
// This is more reliable than fixed delay as it adapts to model loading time
// If maxWaitMs < 0, wait indefinitely (no timeout)
void InstanceRegistry::waitForModelsReady(const std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>>& nodes, 
                                         int maxWaitMs) {
    // Check if pipeline contains DNN models (face detector, feature encoder, etc.)
    bool hasDNNModels = false;
    for (const auto& node : nodes) {
        // Check for YuNet face detector
        if (std::dynamic_pointer_cast<cvedix_nodes::cvedix_yunet_face_detector_node>(node)) {
            hasDNNModels = true;
            break;
        }
        // Check for SFace feature encoder
        if (std::dynamic_pointer_cast<cvedix_nodes::cvedix_sface_feature_encoder_node>(node)) {
            hasDNNModels = true;
            break;
        }
    }
    
    if (!hasDNNModels) {
        // No DNN models, minimal delay
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return;
    }
    
    // Check if unlimited wait (maxWaitMs < 0)
    bool unlimitedWait = (maxWaitMs < 0);
    
    if (unlimitedWait) {
        std::cerr << "[InstanceRegistry] Waiting for DNN models to initialize (UNLIMITED - will wait until ready)..." << std::endl;
        std::cerr << "[InstanceRegistry] NOTE: This will wait indefinitely until models are ready" << std::endl;
    } else {
        std::cerr << "[InstanceRegistry] Waiting for DNN models to initialize (adaptive, max " << maxWaitMs << "ms)..." << std::endl;
    }
    
    // Use exponential backoff with adaptive waiting
    // Start with small delays and increase gradually
    int currentDelay = 200; // Start with 200ms
    int totalWaited = 0;
    int attempt = 0;
    const int maxDelayPerAttempt = 2000; // Cap at 2 seconds per attempt for unlimited wait
    
    // For unlimited wait, use a very large number for maxAttempts
    // For limited wait, calculate based on maxWaitMs
    int maxAttempts = unlimitedWait ? 1000000 : ((maxWaitMs / 1600) + 10);
    
    // With exponential backoff: 200, 400, 800, 1600, 2000, 2000, 2000...
    while (unlimitedWait || (totalWaited < maxWaitMs && attempt < maxAttempts)) {
        int delayThisRound;
        if (unlimitedWait) {
            // For unlimited wait, use exponential backoff up to maxDelayPerAttempt
            delayThisRound = std::min(currentDelay, maxDelayPerAttempt);
        } else {
            // For limited wait, calculate remaining time
            delayThisRound = std::min(currentDelay, maxWaitMs - totalWaited);
        }
        
        if (unlimitedWait) {
            std::cerr << "[InstanceRegistry]   Attempt " << (attempt + 1) << ": waiting " << delayThisRound << "ms (total: " << totalWaited << "ms, unlimited wait)..." << std::endl;
        } else {
            std::cerr << "[InstanceRegistry]   Attempt " << (attempt + 1) << ": waiting " << delayThisRound << "ms (total: " << totalWaited << "ms / " << maxWaitMs << "ms)..." << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(delayThisRound));
        totalWaited += delayThisRound;
        
        // Exponential backoff: double the delay each time
        // For unlimited wait, cap at maxDelayPerAttempt
        // For limited wait, cap at 1600ms per attempt
        int maxDelay = unlimitedWait ? maxDelayPerAttempt : 1600;
        currentDelay = std::min(currentDelay * 2, maxDelay);
        attempt++;
        
        // For shorter waits (create scenario), can exit early
        if (!unlimitedWait && maxWaitMs <= 2000 && totalWaited >= 1000) {
            // For create scenario (2s max), exit after 1s if models usually ready
            std::cerr << "[InstanceRegistry]   Models should be ready now (waited " << totalWaited << "ms)" << std::endl;
            break;
        }
        
        // For unlimited wait, log progress every 10 seconds
        if (unlimitedWait && totalWaited > 0 && totalWaited % 10000 == 0) {
            std::cerr << "[InstanceRegistry]   Still waiting... (total: " << (totalWaited / 1000) << " seconds)" << std::endl;
        }
    }
    
    if (unlimitedWait) {
        std::cerr << "[InstanceRegistry] ✓ Models initialization wait completed (total: " << totalWaited << "ms, unlimited wait)" << std::endl;
    } else {
        std::cerr << "[InstanceRegistry] ✓ Models initialization wait completed (total: " << totalWaited << "ms / " << maxWaitMs << "ms)" << std::endl;
    }
}

bool InstanceRegistry::startPipeline(const std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>>& nodes, bool isRestart) {
    if (nodes.empty()) {
        std::cerr << "[InstanceRegistry] Cannot start pipeline: no nodes" << std::endl;
        return false;
    }
    
    try {
        // Start from the first node (source node)
        // The pipeline will start automatically when source node starts
        // Check for RTSP source node first
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
            
            std::cerr << "[InstanceRegistry] Calling rtspNode->start()..." << std::endl;
            auto startTime = std::chrono::steady_clock::now();
            rtspNode->start();
            auto endTime = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
            
            std::cerr << "[InstanceRegistry] ✓ RTSP node start() completed in " << duration << "ms" << std::endl;
            std::cerr << "[InstanceRegistry] RTSP pipeline started (connection may take a few seconds)" << std::endl;
            std::cerr << "[InstanceRegistry] The SDK will automatically retry connection - monitor logs for connection status" << std::endl;
            std::cerr << "[InstanceRegistry] NOTE: Check CVEDIX SDK logs above for RTSP connection status" << std::endl;
            std::cerr << "[InstanceRegistry] NOTE: Look for messages like 'rtspsrc' or connection errors" << std::endl;
            std::cerr << "[InstanceRegistry] ========================================" << std::endl;
            std::cerr << "[InstanceRegistry] HOW TO VERIFY PIPELINE IS WORKING:" << std::endl;
            std::cerr << "[InstanceRegistry]   1. Check output files (from build directory):" << std::endl;
            std::cerr << "[InstanceRegistry]      ls -lht ./output/<instanceId>/" << std::endl;
            std::cerr << "[InstanceRegistry]      Or from project root: ./build/output/<instanceId>/" << std::endl;
            std::cerr << "[InstanceRegistry]   2. Check CVEDIX SDK logs for 'rtspsrc' connection messages:" << std::endl;
            std::cerr << "[InstanceRegistry]      - Direct run: ./bin/edge_ai_api 2>&1 | grep -i rtspsrc" << std::endl;
            std::cerr << "[InstanceRegistry]      - Service: sudo journalctl -u edge-ai-api | grep -i rtspsrc" << std::endl;
            std::cerr << "[InstanceRegistry]      - Enable GStreamer debug: export GST_DEBUG=rtspsrc:4" << std::endl;
            std::cerr << "[InstanceRegistry]      - See docs/HOW_TO_CHECK_LOGS.md for details" << std::endl;
            std::cerr << "[InstanceRegistry]   3. Check instance status: GET /v1/core/instances/<instanceId>" << std::endl;
            std::cerr << "[InstanceRegistry]   4. Monitor file creation:" << std::endl;
            std::cerr << "[InstanceRegistry]      watch -n 1 'ls -lht ./output/<instanceId>/ | head -5'" << std::endl;
            std::cerr << "[InstanceRegistry]   5. If files are being created, pipeline is working!" << std::endl;
            std::cerr << "[InstanceRegistry]   NOTE: Files are created in working directory (usually build/)" << std::endl;
            std::cerr << "[InstanceRegistry] ========================================" << std::endl;
            return true;
        }
        
        // Check for file source node
        auto fileNode = std::dynamic_pointer_cast<cvedix_nodes::cvedix_file_src_node>(nodes[0]);
        if (fileNode) {
            std::cerr << "[InstanceRegistry] ========================================" << std::endl;
            std::cerr << "[InstanceRegistry] Starting file source pipeline..." << std::endl;
            std::cerr << "[InstanceRegistry] ========================================" << std::endl;
            
            // CRITICAL: Delay BEFORE start() to ensure model is fully ready
            // Once fileNode->start() is called, frames are immediately sent to the pipeline
            // If model is not ready, shape mismatch errors will occur
            // For restart scenarios, use longer delay to ensure OpenCV DNN has fully cleared old state
            if (isRestart) {
                std::cerr << "[InstanceRegistry] CRITICAL: Final synchronization delay before starting file source (restart: 5 seconds)..." << std::endl;
                std::cerr << "[InstanceRegistry] This delay is CRITICAL - once start() is called, frames are sent immediately" << std::endl;
                std::cerr << "[InstanceRegistry] Model must be fully ready before start() to prevent shape mismatch errors" << std::endl;
                std::cerr << "[InstanceRegistry] Using longer delay for restart to ensure OpenCV DNN state is fully cleared" << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            } else {
                std::cerr << "[InstanceRegistry] Final synchronization delay before starting file source (2 seconds)..." << std::endl;
                std::cerr << "[InstanceRegistry] Ensuring model is ready before start() to prevent shape mismatch errors" << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            }
            
            std::cerr << "[InstanceRegistry] Calling fileNode->start()..." << std::endl;
            auto startTime = std::chrono::steady_clock::now();
            
            try {
                fileNode->start();
            } catch (const std::exception& e) {
                std::cerr << "[InstanceRegistry] ✗ Exception during fileNode->start(): " << e.what() << std::endl;
                std::cerr << "[InstanceRegistry] This may indicate a problem with the video file or model initialization" << std::endl;
                return false;
            } catch (...) {
                std::cerr << "[InstanceRegistry] ✗ Unknown exception during fileNode->start()" << std::endl;
                std::cerr << "[InstanceRegistry] This may indicate a critical error - check logs above for details" << std::endl;
                return false;
            }
            
            auto endTime = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
            
            std::cerr << "[InstanceRegistry] ✓ File source node start() completed in " << duration << "ms" << std::endl;
            
            // Additional delay after start() to allow first frame to be processed
            // Note: This delay is less critical than the delay BEFORE start()
            // because frames are already being sent, but it helps ensure smooth processing
            if (isRestart) {
                std::cerr << "[InstanceRegistry] Additional stabilization delay after start() (restart: 1 second)..." << std::endl;
                std::cerr << "[InstanceRegistry] This allows first frame to be processed smoothly" << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            } else {
                std::cerr << "[InstanceRegistry] Additional stabilization delay after start() (500ms)..." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            
            // NOTE: Shape mismatch errors may still occur if:
            // 1. Video has inconsistent frame sizes (most common cause)
            //    - Check with: ffprobe -v error -select_streams v:0 -show_entries frame=width,height -of csv=s=x:p=0 video.mp4 | sort -u
            //    - If multiple sizes appear, video needs re-encoding with fixed resolution
            // 2. Model (especially YuNet 2022mar) doesn't handle dynamic input well
            //    - Solution: Use YuNet 2023mar model
            // 3. Resize ratio doesn't produce consistent dimensions
            //    - Solution: Re-encode video with fixed resolution, then use resize_ratio=1.0
            // If this happens, SIGABRT handler will catch it and stop the instance
            std::cerr << "[InstanceRegistry] File source pipeline started successfully" << std::endl;
            std::cerr << "[InstanceRegistry] ========================================" << std::endl;
            std::cerr << "[InstanceRegistry] IMPORTANT: If you see shape mismatch errors, the most likely cause is:" << std::endl;
            std::cerr << "[InstanceRegistry]   Video has inconsistent frame sizes (different resolutions per frame)" << std::endl;
            std::cerr << "[InstanceRegistry] Solutions (in order of recommendation):" << std::endl;
            std::cerr << "[InstanceRegistry]   1. Re-encode video with fixed resolution:" << std::endl;
            std::cerr << "[InstanceRegistry]      ffmpeg -i input.mp4 -vf \"scale=640:360:force_original_aspect_ratio=decrease,pad=640:360:(ow-iw)/2:(oh-ih)/2\" \\" << std::endl;
            std::cerr << "[InstanceRegistry]             -c:v libx264 -preset fast -crf 23 -c:a copy output.mp4" << std::endl;
            std::cerr << "[InstanceRegistry]      Then use RESIZE_RATIO: \"1.0\" in additionalParams" << std::endl;
            std::cerr << "[InstanceRegistry]   2. Use YuNet 2023mar model (better dynamic input support)" << std::endl;
            std::cerr << "[InstanceRegistry]   3. Check video resolution consistency:" << std::endl;
            std::cerr << "[InstanceRegistry]      ffprobe -v error -select_streams v:0 -show_entries frame=width,height \\" << std::endl;
            std::cerr << "[InstanceRegistry]              -of csv=s=x:p=0 video.mp4 | sort -u" << std::endl;
            std::cerr << "[InstanceRegistry] ========================================" << std::endl;
            std::cerr << "[InstanceRegistry] ========================================" << std::endl;
            return true;
        }
        
        // If not a recognized source node, cannot start pipeline
        // Only RTSP and File source nodes are currently supported
        std::cerr << "[InstanceRegistry] ✗ Error: First node is not a recognized source node (RTSP or File)" << std::endl;
        std::cerr << "[InstanceRegistry] Currently supported source node types:" << std::endl;
        std::cerr << "[InstanceRegistry]   - cvedix_rtsp_src_node (for RTSP streams)" << std::endl;
        std::cerr << "[InstanceRegistry]   - cvedix_file_src_node (for video files)" << std::endl;
        std::cerr << "[InstanceRegistry] Please ensure your solution config uses one of these as the first node" << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "[InstanceRegistry] Exception starting pipeline: " << e.what() << std::endl;
        std::cerr << "[InstanceRegistry] This may indicate a configuration issue with the RTSP source" << std::endl;
        return false;
    }
}

void InstanceRegistry::stopPipeline(const std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>>& nodes, bool isDeletion) {
    if (nodes.empty()) {
        return;
    }
    
    try {
        // Check if pipeline contains DNN models (face detector, feature encoder, etc.)
        // These need extra time to finish processing and clear internal state
        bool hasDNNModels = false;
        for (const auto& node : nodes) {
            if (std::dynamic_pointer_cast<cvedix_nodes::cvedix_yunet_face_detector_node>(node) ||
                std::dynamic_pointer_cast<cvedix_nodes::cvedix_sface_feature_encoder_node>(node)) {
                hasDNNModels = true;
                break;
            }
        }
        
        // First, give destination nodes (like RTMP) time to flush and finalize
        // This helps reduce GStreamer warnings during cleanup
        if (isDeletion) {
            std::cerr << "[InstanceRegistry] Waiting for destination nodes to finalize..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        
        // First, stop the source node if it exists (typically the first node)
        // This is important to stop the connection retry loop or file reading
        if (!nodes.empty() && nodes[0]) {
            // Try RTSP source node first
            auto rtspNode = std::dynamic_pointer_cast<cvedix_nodes::cvedix_rtsp_src_node>(nodes[0]);
            if (rtspNode) {
                if (isDeletion) {
                    std::cerr << "[InstanceRegistry] Stopping RTSP source node (deletion)..." << std::endl;
                } else {
                    std::cerr << "[InstanceRegistry] Stopping RTSP source node..." << std::endl;
                }
                try {
                    auto stopTime = std::chrono::steady_clock::now();
                    rtspNode->stop();
                    auto stopEndTime = std::chrono::steady_clock::now();
                    auto stopDuration = std::chrono::duration_cast<std::chrono::milliseconds>(stopEndTime - stopTime).count();
                    std::cerr << "[InstanceRegistry] ✓ RTSP source node stopped in " << stopDuration << "ms" << std::endl;
                    // Give it a moment to fully stop
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                } catch (const std::exception& e) {
                    std::cerr << "[InstanceRegistry] ✗ Exception stopping RTSP node: " << e.what() << std::endl;
                } catch (...) {
                    std::cerr << "[InstanceRegistry] ✗ Unknown error stopping RTSP node" << std::endl;
                }
            } else {
                // Try file source node
                auto fileNode = std::dynamic_pointer_cast<cvedix_nodes::cvedix_file_src_node>(nodes[0]);
                if (fileNode) {
                    if (isDeletion) {
                        std::cerr << "[InstanceRegistry] Stopping file source node (deletion)..." << std::endl;
                    } else {
                        std::cerr << "[InstanceRegistry] Stopping file source node..." << std::endl;
                    }
                    try {
                        // For file source, we need to detach to stop reading
                        // But we'll keep the nodes in memory so they can be restarted (unless deletion)
                        fileNode->detach_recursively();
                        std::cerr << "[InstanceRegistry] ✓ File source node stopped" << std::endl;
                    } catch (const std::exception& e) {
                        std::cerr << "[InstanceRegistry] ✗ Exception stopping file node: " << e.what() << std::endl;
                    } catch (...) {
                        std::cerr << "[InstanceRegistry] ✗ Unknown error stopping file node" << std::endl;
                    }
                } else {
                    // Generic stop for other source types
                    try {
                        if (nodes[0]) {
                            nodes[0]->detach_recursively();
                        }
                    } catch (...) {
                        // Ignore errors
                    }
                }
            }
        }
        
        // CRITICAL: After stopping source node, wait for DNN processing nodes to finish
        // This ensures all frames in the processing queue are handled and DNN models
        // have cleared their internal state before we detach or restart
        // This prevents shape mismatch errors when restarting
        if (hasDNNModels) {
            if (isDeletion) {
                std::cerr << "[InstanceRegistry] Waiting for DNN models to finish processing (deletion, 1 second)..." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            } else {
                // For stop (not deletion), use longer delay to ensure DNN state is fully cleared
                // This is critical to prevent shape mismatch errors when restarting
                std::cerr << "[InstanceRegistry] Waiting for DNN models to finish processing and clear state (stop, 2 seconds)..." << std::endl;
                std::cerr << "[InstanceRegistry] This ensures OpenCV DNN releases all internal state before restart" << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            }
        }
        
        // Give GStreamer time to properly cleanup after detach
        // This helps reduce warnings about VideoWriter finalization
        if (isDeletion) {
            std::cerr << "[InstanceRegistry] Waiting for GStreamer cleanup..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            std::cerr << "[InstanceRegistry] Pipeline stopped and fully destroyed (all nodes cleared)" << std::endl;
            std::cerr << "[InstanceRegistry] NOTE: All nodes have been destroyed to ensure clean state (especially OpenCV DNN)" << std::endl;
            std::cerr << "[InstanceRegistry] NOTE: GStreamer warnings about VideoWriter finalization are normal during cleanup" << std::endl;
        } else {
            // Note: We detach nodes but keep them in the pipeline vector
            // This allows the pipeline to be rebuilt when restarting
            // The nodes will be recreated when startInstance is called if needed
            std::cerr << "[InstanceRegistry] Pipeline stopped (nodes detached but kept for potential restart)" << std::endl;
            std::cerr << "[InstanceRegistry] NOTE: Pipeline will be automatically rebuilt when restarting" << std::endl;
            if (hasDNNModels) {
                std::cerr << "[InstanceRegistry] NOTE: DNN models have been given time to clear internal state" << std::endl;
                std::cerr << "[InstanceRegistry] NOTE: This helps prevent shape mismatch errors when restarting" << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[InstanceRegistry] Exception in stopPipeline: " << e.what() << std::endl;
        std::cerr << "[InstanceRegistry] NOTE: GStreamer warnings during cleanup are usually harmless" << std::endl;
        // Swallow exception - don't let it propagate to prevent terminate handler deadlock
    } catch (...) {
        std::cerr << "[InstanceRegistry] Unknown exception in stopPipeline - caught and ignored" << std::endl;
        // Swallow exception - don't let it propagate to prevent terminate handler deadlock
    }
    // Ensure function never throws - this prevents deadlock in terminate handler
}

bool InstanceRegistry::rebuildPipelineFromInstanceInfo(const std::string& instanceId) {
    std::cerr << "[InstanceRegistry] ========================================" << std::endl;
    std::cerr << "[InstanceRegistry] Rebuilding pipeline for instance " << instanceId << "..." << std::endl;
    std::cerr << "[InstanceRegistry] NOTE: This is normal when restarting an instance." << std::endl;
    std::cerr << "[InstanceRegistry] After stop(), pipeline is removed from map and nodes are detached." << std::endl;
    std::cerr << "[InstanceRegistry] Rebuilding ensures fresh pipeline with clean DNN model state." << std::endl;
    std::cerr << "[InstanceRegistry] ========================================" << std::endl;
    
    // Get instance info (need lock)
    InstanceInfo info;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto instanceIt = instances_.find(instanceId);
        if (instanceIt == instances_.end()) {
            return false;
        }
        info = instanceIt->second; // Copy instance info
    } // Release lock - rest of function doesn't need it
    
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
    
    // Restore all additional parameters from InstanceInfo
    // This includes MODEL_PATH, SFACE_MODEL_PATH, RESIZE_RATIO, etc.
    req.additionalParams = info.additionalParams;
    
    // Also restore individual fields for backward compatibility
    // Use originator.address as RTSP URL if available
    if (!info.originator.address.empty() && req.additionalParams.find("RTSP_URL") == req.additionalParams.end()) {
        req.additionalParams["RTSP_URL"] = info.originator.address;
    }
    
    // Restore RTMP URL if available (override if not in additionalParams)
    if (!info.rtmpUrl.empty() && req.additionalParams.find("RTMP_URL") == req.additionalParams.end()) {
        req.additionalParams["RTMP_URL"] = info.rtmpUrl;
    }
    
    // Restore FILE_PATH if available (override if not in additionalParams)
    if (!info.filePath.empty() && req.additionalParams.find("FILE_PATH") == req.additionalParams.end()) {
        req.additionalParams["FILE_PATH"] = info.filePath;
    }
    
    // Build pipeline (this can take time, so don't hold lock)
    std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>> pipeline;
    try {
        pipeline = pipeline_builder_.buildPipeline(solution, req, instanceId);
        if (!pipeline.empty()) {
            // Store pipeline (need lock briefly)
            {
                std::lock_guard<std::mutex> lock(mutex_);
                pipelines_[instanceId] = pipeline;
            } // Release lock
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

bool InstanceRegistry::hasRTMPOutput(const std::string& instanceId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if instance exists
    auto instanceIt = instances_.find(instanceId);
    if (instanceIt == instances_.end()) {
        return false;
    }
    
    // Check if RTMP_URL is in additionalParams
    const auto& additionalParams = instanceIt->second.additionalParams;
    if (additionalParams.find("RTMP_URL") != additionalParams.end() && 
        !additionalParams.at("RTMP_URL").empty()) {
        return true;
    }
    
    // Check if rtmpUrl field is set
    if (!instanceIt->second.rtmpUrl.empty()) {
        return true;
    }
    
    // Check if pipeline has RTMP destination node
    auto pipelineIt = pipelines_.find(instanceId);
    if (pipelineIt != pipelines_.end()) {
        for (const auto& node : pipelineIt->second) {
            if (std::dynamic_pointer_cast<cvedix_nodes::cvedix_rtmp_des_node>(node)) {
                return true;
            }
        }
    }
    
    return false;
}

std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>> InstanceRegistry::getSourceNodesFromRunningInstances() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>> sourceNodes;
    
    // Iterate through all instances
    for (const auto& [instanceId, info] : instances_) {
        // Only get source nodes from running instances
        if (!info.running) {
            continue;
        }
        
        // Get pipeline for this instance
        auto pipelineIt = pipelines_.find(instanceId);
        if (pipelineIt != pipelines_.end() && !pipelineIt->second.empty()) {
            // Source node is always the first node in the pipeline
            const auto& sourceNode = pipelineIt->second[0];
            
            // Verify it's a source node (RTSP or file source)
            auto rtspNode = std::dynamic_pointer_cast<cvedix_nodes::cvedix_rtsp_src_node>(sourceNode);
            auto fileNode = std::dynamic_pointer_cast<cvedix_nodes::cvedix_file_src_node>(sourceNode);
            
            if (rtspNode || fileNode) {
                sourceNodes.push_back(sourceNode);
            }
        }
    }
    
    return sourceNodes;
}

void InstanceRegistry::logProcessingResults(const std::string& instanceId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if instance exists
    auto instanceIt = instances_.find(instanceId);
    if (instanceIt == instances_.end()) {
        return;
    }
    
    const InstanceInfo& info = instanceIt->second;
    
    // Only log for running instances
    if (!info.running) {
        return;
    }
    
    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    std::string timestamp = ss.str();
    
    // Log to PLOG if SDK output logging is enabled
    if (isSdkOutputLoggingEnabled()) {
        PLOG_INFO << "[SDKOutput] [" << timestamp << "] Instance: " << info.displayName 
                  << " (" << instanceId << ") - FPS: " << std::fixed << std::setprecision(2) << info.fps
                  << ", Solution: " << info.solutionId;
    }
    
    // Log processing results
    std::cerr << "[InstanceProcessingLog] ========================================" << std::endl;
    std::cerr << "[InstanceProcessingLog] [" << timestamp << "] Instance: " << info.displayName 
              << " (" << instanceId << ")" << std::endl;
    std::cerr << "[InstanceProcessingLog] Solution: " << info.solutionName 
              << " (" << info.solutionId << ")" << std::endl;
    std::cerr << "[InstanceProcessingLog] Status: RUNNING" << std::endl;
    std::cerr << "[InstanceProcessingLog] FPS: " << std::fixed << std::setprecision(2) << info.fps << std::endl;
    
    // Log input source
    if (!info.filePath.empty()) {
        std::cerr << "[InstanceProcessingLog] Input Source: FILE - " << info.filePath << std::endl;
    } else if (info.additionalParams.find("RTSP_URL") != info.additionalParams.end()) {
        std::cerr << "[InstanceProcessingLog] Input Source: RTSP - " 
                  << info.additionalParams.at("RTSP_URL") << std::endl;
    } else if (info.additionalParams.find("FILE_PATH") != info.additionalParams.end()) {
        std::cerr << "[InstanceProcessingLog] Input Source: FILE - " 
                  << info.additionalParams.at("FILE_PATH") << std::endl;
    }
    
    // Log output type
    if (hasRTMPOutput(instanceId)) {
        std::cerr << "[InstanceProcessingLog] Output: RTMP Stream" << std::endl;
        if (!info.rtmpUrl.empty()) {
            std::cerr << "[InstanceProcessingLog] RTMP URL: " << info.rtmpUrl << std::endl;
        } else if (info.additionalParams.find("RTMP_URL") != info.additionalParams.end()) {
            std::cerr << "[InstanceProcessingLog] RTMP URL: " 
                      << info.additionalParams.at("RTMP_URL") << std::endl;
        }
    } else {
        std::cerr << "[InstanceProcessingLog] Output: File-based (no RTMP stream)" << std::endl;
        
        // Check for file destination output directory
        auto pipelineIt = pipelines_.find(instanceId);
        if (pipelineIt != pipelines_.end()) {
            // Try to determine output directory from file_des node
            // Output is typically in ./output/{instanceId} or ./build/output/{instanceId}
            std::cerr << "[InstanceProcessingLog] Output Directory: ./output/" << instanceId 
                      << " (or ./build/output/" << instanceId << ")" << std::endl;
        }
    }
    
    // Log detection settings
    std::cerr << "[InstanceProcessingLog] Detection Sensitivity: " << info.detectionSensitivity << std::endl;
    if (!info.detectorMode.empty()) {
        std::cerr << "[InstanceProcessingLog] Detector Mode: " << info.detectorMode << std::endl;
    }
    
    // Log processing modes
    if (info.statisticsMode) {
        std::cerr << "[InstanceProcessingLog] Statistics Mode: ENABLED" << std::endl;
    }
    if (info.metadataMode) {
        std::cerr << "[InstanceProcessingLog] Metadata Mode: ENABLED" << std::endl;
    }
    if (info.debugMode) {
        std::cerr << "[InstanceProcessingLog] Debug Mode: ENABLED" << std::endl;
    }
    
    // Log frame rate limit if set
    if (info.frameRateLimit > 0) {
        std::cerr << "[InstanceProcessingLog] Frame Rate Limit: " << info.frameRateLimit << " fps" << std::endl;
    }
    
    std::cerr << "[InstanceProcessingLog] ========================================" << std::endl;
}

bool InstanceRegistry::updateInstanceFromConfig(const std::string& instanceId, const Json::Value& configJson) {
    std::cerr << "[InstanceRegistry] ========================================" << std::endl;
    std::cerr << "[InstanceRegistry] Updating instance from config: " << instanceId << std::endl;
    std::cerr << "[InstanceRegistry] ========================================" << std::endl;
    
    bool wasRunning = false;
    bool isPersistent = false;
    InstanceInfo currentInfo;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto instanceIt = instances_.find(instanceId);
        if (instanceIt == instances_.end()) {
            std::cerr << "[InstanceRegistry] Instance " << instanceId << " not found" << std::endl;
            return false;
        }
        
        InstanceInfo& info = instanceIt->second;
        
        // Check if instance is read-only
        if (info.readOnly) {
            std::cerr << "[InstanceRegistry] Cannot update read-only instance " << instanceId << std::endl;
            return false;
        }
        
        wasRunning = info.running;
        isPersistent = info.persistent;
        currentInfo = info; // Copy current info
    } // Release lock
    
    // Convert current InstanceInfo to config JSON
    std::string conversionError;
    Json::Value existingConfig = instance_storage_.instanceInfoToConfigJson(currentInfo, &conversionError);
    if (existingConfig.isNull() || existingConfig.empty()) {
        std::cerr << "[InstanceRegistry] Failed to convert current InstanceInfo to config: " << conversionError << std::endl;
        return false;
    }
    
    // List of keys to preserve (TensorRT model IDs, Zone IDs, etc.)
    std::vector<std::string> preserveKeys;
    
    // Collect UUID-like keys (TensorRT model IDs) from existing config
    for (const auto& key : existingConfig.getMemberNames()) {
        if (key.length() >= 36 && key.find('-') != std::string::npos) {
            preserveKeys.push_back(key);
        }
    }
    
    // Add special keys to preserve
    std::vector<std::string> specialKeys = {
        "AnimalTracker", "LicensePlateTracker", "ObjectAttributeExtraction", 
        "ObjectMovementClassifier", "PersonTracker", "VehicleTracker", "Global"
    };
    preserveKeys.insert(preserveKeys.end(), specialKeys.begin(), specialKeys.end());
    
    // Merge configs (preserve Zone, Tripwire, DetectorRegions if not in update)
    if (!instance_storage_.mergeConfigs(existingConfig, configJson, preserveKeys)) {
        std::cerr << "[InstanceRegistry] Merge failed for instance " << instanceId << std::endl;
        return false;
    }
    
    // Ensure InstanceId matches
    existingConfig["InstanceId"] = instanceId;
    
    // Convert merged config back to InstanceInfo
    auto optInfo = instance_storage_.configJsonToInstanceInfo(existingConfig, &conversionError);
    if (!optInfo.has_value()) {
        std::cerr << "[InstanceRegistry] Failed to convert config to InstanceInfo: " << conversionError << std::endl;
        return false;
    }
    
    InstanceInfo updatedInfo = optInfo.value();
    
    // Preserve runtime state
    updatedInfo.loaded = currentInfo.loaded;
    updatedInfo.running = currentInfo.running;
    updatedInfo.fps = currentInfo.fps;
    
    // Update instance in registry
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto instanceIt = instances_.find(instanceId);
        if (instanceIt == instances_.end()) {
            std::cerr << "[InstanceRegistry] Instance " << instanceId << " not found during update" << std::endl;
            return false;
        }
        
        InstanceInfo& info = instanceIt->second;
        
        // Update all fields from merged config
        info = updatedInfo;
        
        std::cerr << "[InstanceRegistry] ✓ Instance info updated in registry" << std::endl;
    } // Release lock
    
    // Save to storage
    if (isPersistent) {
        bool saved = instance_storage_.saveInstance(instanceId, updatedInfo);
        if (saved) {
            std::cerr << "[InstanceRegistry] Instance configuration saved to file" << std::endl;
        } else {
            std::cerr << "[InstanceRegistry] Warning: Failed to save instance configuration to file" << std::endl;
        }
    }
    
    std::cerr << "[InstanceRegistry] ✓ Instance " << instanceId << " updated successfully from config" << std::endl;
    
    // Restart instance if it was running to apply changes
    if (wasRunning) {
        std::cerr << "[InstanceRegistry] Instance was running, restarting to apply changes..." << std::endl;
        
        // Stop instance first
        if (stopInstance(instanceId)) {
            // Wait a moment for cleanup
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // Start instance again (this will rebuild pipeline with new config)
            if (startInstance(instanceId, true)) {
                std::cerr << "[InstanceRegistry] ✓ Instance restarted successfully with new configuration" << std::endl;
            } else {
                std::cerr << "[InstanceRegistry] ⚠ Instance stopped but failed to restart" << std::endl;
            }
        } else {
            std::cerr << "[InstanceRegistry] ⚠ Failed to stop instance for restart" << std::endl;
        }
    }
    
    std::cerr << "[InstanceRegistry] ========================================" << std::endl;
    return true;
}

