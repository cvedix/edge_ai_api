#include "instances/instance_registry.h"
#include "instances/instance_statistics.h"
#include "models/update_instance_request.h"
#include "core/uuid_generator.h"
#include "core/logging_flags.h"
#include "core/logger.h"
#include <cvedix/cvedix_version.h>
#include <cvedix/nodes/src/cvedix_rtsp_src_node.h>
#include <cvedix/nodes/src/cvedix_file_src_node.h>
#include <cvedix/nodes/src/cvedix_rtmp_src_node.h>
#include <cvedix/nodes/infers/cvedix_yunet_face_detector_node.h>
#include <cvedix/nodes/infers/cvedix_sface_feature_encoder_node.h>
#include <cvedix/nodes/des/cvedix_rtmp_des_node.h>
#include <cvedix/nodes/des/cvedix_app_des_node.h>
#include <cvedix/objects/cvedix_meta.h>
#include <cvedix/objects/cvedix_frame_meta.h>
#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>
#include <algorithm>
#include <typeinfo>
#include <thread>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <future>
#include <vector>
#include <cmath>

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
        std::unique_lock<std::shared_timed_mutex> lock(mutex_); // Exclusive lock for write operations
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
        // Note: pipeline is a copy, but we'll get the actual pipeline from map when initializing tracker
        std::thread([this, instanceId, pipeline]() {
            // Wait for DNN models to be ready using exponential backoff
            // This is more reliable than fixed delay as it adapts to model loading time
            waitForModelsReady(pipeline, 2000); // Max 2 seconds
            
            // Validate model files before starting pipeline
            // This prevents assertion failures when model files don't exist
            std::map<std::string, std::string> additionalParams;
            {
                std::unique_lock<std::shared_timed_mutex> lock(mutex_); // Exclusive lock for write operations
                auto instanceIt = instances_.find(instanceId);
                if (instanceIt != instances_.end()) {
                    additionalParams = instanceIt->second.additionalParams;
                }
            }
            
            bool modelValidationFailed = false;
            std::string missingModelPath;
            
            // Check for YuNet face detector node
            for (const auto& node : pipeline) {
                auto yunetNode = std::dynamic_pointer_cast<cvedix_nodes::cvedix_yunet_face_detector_node>(node);
                if (yunetNode) {
                    // Get model path from additionalParams
                    std::string modelPath;
                    auto modelPathIt = additionalParams.find("MODEL_PATH");
                    if (modelPathIt != additionalParams.end() && !modelPathIt->second.empty()) {
                        modelPath = modelPathIt->second;
                    } else {
                        // Try to get from solution config or use default
                        modelPath = "/usr/share/cvedix/cvedix_data/models/face/face_detection_yunet_2022mar.onnx";
                    }
                    
                    // Check if model file exists
                    struct stat modelStat;
                    if (stat(modelPath.c_str(), &modelStat) != 0) {
                        std::cerr << "[InstanceRegistry] ========================================" << std::endl;
                        std::cerr << "[InstanceRegistry] ✗ CRITICAL: YuNet model file not found!" << std::endl;
                        std::cerr << "[InstanceRegistry] Expected path: " << modelPath << std::endl;
                        std::cerr << "[InstanceRegistry] ========================================" << std::endl;
                        std::cerr << "[InstanceRegistry] Cannot auto-start instance - model file validation failed" << std::endl;
                        std::cerr << "[InstanceRegistry] The pipeline will crash with assertion failure if started without model file" << std::endl;
                        std::cerr << "[InstanceRegistry] Please ensure the model file exists before starting the instance" << std::endl;
                        std::cerr << "[InstanceRegistry] ========================================" << std::endl;
                        modelValidationFailed = true;
                        missingModelPath = modelPath;
                        break;
                    }
                    
                    if (!S_ISREG(modelStat.st_mode)) {
                        std::cerr << "[InstanceRegistry] ✗ CRITICAL: Model path is not a regular file: " << modelPath << std::endl;
                        std::cerr << "[InstanceRegistry] Cannot auto-start instance - model file validation failed" << std::endl;
                        modelValidationFailed = true;
                        missingModelPath = modelPath;
                        break;
                    }
                }
                
                // Check for SFace feature encoder node
                auto sfaceNode = std::dynamic_pointer_cast<cvedix_nodes::cvedix_sface_feature_encoder_node>(node);
                if (sfaceNode) {
                    // Get model path from additionalParams
                    std::string modelPath;
                    auto modelPathIt = additionalParams.find("SFACE_MODEL_PATH");
                    if (modelPathIt != additionalParams.end() && !modelPathIt->second.empty()) {
                        modelPath = modelPathIt->second;
                    } else {
                        // Use default path
                        modelPath = "/home/pnsang/project/edge_ai_sdk/cvedix_data/models/face/face_recognition_sface_2021dec.onnx";
                    }
                    
                    // Check if model file exists
                    struct stat modelStat;
                    if (stat(modelPath.c_str(), &modelStat) != 0) {
                        std::cerr << "[InstanceRegistry] ========================================" << std::endl;
                        std::cerr << "[InstanceRegistry] ✗ CRITICAL: SFace model file not found!" << std::endl;
                        std::cerr << "[InstanceRegistry] Expected path: " << modelPath << std::endl;
                        std::cerr << "[InstanceRegistry] ========================================" << std::endl;
                        std::cerr << "[InstanceRegistry] Cannot auto-start instance - model file validation failed" << std::endl;
                        std::cerr << "[InstanceRegistry] The pipeline will crash with assertion failure if started without model file" << std::endl;
                        std::cerr << "[InstanceRegistry] Please ensure the model file exists before starting the instance" << std::endl;
                        std::cerr << "[InstanceRegistry] ========================================" << std::endl;
                        modelValidationFailed = true;
                        missingModelPath = modelPath;
                        break;
                    }
                    
                    if (!S_ISREG(modelStat.st_mode)) {
                        std::cerr << "[InstanceRegistry] ✗ CRITICAL: Model path is not a regular file: " << modelPath << std::endl;
                        std::cerr << "[InstanceRegistry] Cannot auto-start instance - model file validation failed" << std::endl;
                        modelValidationFailed = true;
                        missingModelPath = modelPath;
                        break;
                    }
                }
            }
            
            // If model validation failed, don't start pipeline
            if (modelValidationFailed) {
                std::cerr << "[InstanceRegistry] ✗ Cannot auto-start instance - model file validation failed" << std::endl;
                std::cerr << "[InstanceRegistry] Missing model file: " << missingModelPath << std::endl;
                std::cerr << "[InstanceRegistry] Instance created but not started - you can start it manually after fixing the model file" << std::endl;
                return; // Exit thread without starting pipeline
            }
            
            try {
                if (startPipeline(instanceId, pipeline)) {
                    // Update running status, reset retry counter, and initialize statistics tracker (need lock briefly)
                    {
                        std::unique_lock<std::shared_timed_mutex> lock(mutex_); // Exclusive lock for write operations
                        auto instanceIt = instances_.find(instanceId);
                        if (instanceIt != instances_.end()) {
                            instanceIt->second.running = true;
                            // Reset retry counter and tracking when instance starts successfully
                            instanceIt->second.retryCount = 0;
                            instanceIt->second.retryLimitReached = false;
                            instanceIt->second.startTime = std::chrono::steady_clock::now();
                            instanceIt->second.lastActivityTime = instanceIt->second.startTime;
                            instanceIt->second.hasReceivedData = false;
                            
                            // Initialize statistics tracker
                            InstanceStatsTracker tracker;
                            tracker.start_time = std::chrono::steady_clock::now();
                            tracker.start_time_system = std::chrono::system_clock::now();  // For Unix timestamp
                            tracker.frames_processed = 0;
                            tracker.last_fps = 0.0;
                            tracker.last_fps_update = tracker.start_time;
                            tracker.frame_count_since_last_update = 0;
                            tracker.resolution = "";
                            tracker.source_resolution = "";
                            tracker.format = "BGR";  // Default format for CVEDIX
                            
                            // Try to get resolution from source node
                            auto pipelineIt = pipelines_.find(instanceId);
                            if (pipelineIt != pipelines_.end() && !pipelineIt->second.empty()) {
                                auto sourceNode = pipelineIt->second[0];
                                auto rtspNode = std::dynamic_pointer_cast<cvedix_nodes::cvedix_rtsp_src_node>(sourceNode);
                                auto fileNode = std::dynamic_pointer_cast<cvedix_nodes::cvedix_file_src_node>(sourceNode);
                                auto rtmpNode = std::dynamic_pointer_cast<cvedix_nodes::cvedix_rtmp_src_node>(sourceNode);
                                
                                try {
                                    if (rtspNode) {
                                        // Wait a bit for RTSP to connect and get resolution
                                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                                        auto width = rtspNode->get_original_width();
                                        auto height = rtspNode->get_original_height();
                                        if (width > 0 && height > 0) {
                                            tracker.source_resolution = std::to_string(width) + "x" + std::to_string(height);
                                            tracker.resolution = tracker.source_resolution;  // Use source resolution as current
                                        }
                                    } else if (fileNode) {
                                        // File source may have similar APIs, try to get resolution
                                        // Note: CVEDIX file source may not expose these APIs directly
                                        // We'll leave it empty for now and update when frame is processed
                                    } else if (rtmpNode) {
                                        // RTMP source - similar to RTSP
                                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                                        // Check if RTMP source has similar APIs
                                    }
                                } catch (const std::exception& e) {
                                    std::cerr << "[InstanceRegistry] Warning: Could not get resolution from source node: " << e.what() << std::endl;
                                } catch (...) {
                                    // Ignore errors - resolution will remain empty
                                }
                            }
                            
                            statistics_trackers_[instanceId] = tracker;
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
                        
                        // Start periodic logging in a separate thread (managed, not detached)
                        startLoggingThread(instanceId);
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
        std::unique_lock<std::shared_timed_mutex> lock(mutex_); // Exclusive lock for write operations
        
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
        
        // Remove statistics tracker
        statistics_trackers_.erase(instanceId);
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
    
    // Stop logging thread if exists
    stopLoggingThread(instanceId);
    
    // Cleanup frame cache
    {
        std::lock_guard<std::mutex> lock(frame_cache_mutex_);
        frame_caches_.erase(instanceId);
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
    std::unique_lock<std::shared_timed_mutex> lock(mutex_); // Exclusive lock for write operations
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
        std::unique_lock<std::shared_timed_mutex> lock(mutex_); // Exclusive lock for write operations
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
        std::unique_lock<std::shared_timed_mutex> lock(mutex_); // Exclusive lock for write operations
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
        std::unique_lock<std::shared_timed_mutex> lock(mutex_); // Exclusive lock for write operations
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
        std::unique_lock<std::shared_timed_mutex> lock(mutex_); // Exclusive lock for write operations
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
            std::unique_lock<std::shared_timed_mutex> lock(mutex_); // Exclusive lock for write operations
            pipelines_.erase(instanceId);
        }
        return false;
    } catch (...) {
        std::cerr << "[InstanceRegistry] ✗ Unknown exception waiting for models" << std::endl;
        // Cleanup pipeline on error
        {
            std::unique_lock<std::shared_timed_mutex> lock(mutex_); // Exclusive lock for write operations
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
        std::unique_lock<std::shared_timed_mutex> lock(mutex_); // Exclusive lock for write operations
        if (instances_.find(instanceId) == instances_.end()) {
            std::cerr << "[InstanceRegistry] ✗ Instance " << instanceId << " was deleted during stabilization delay" << std::endl;
            pipelines_.erase(instanceId);
            return false;
        }
    } // Release lock
    
    // Validate file path for file source nodes BEFORE starting pipeline
    // This prevents infinite retry loops when file doesn't exist
    auto fileNode = std::dynamic_pointer_cast<cvedix_nodes::cvedix_file_src_node>(pipelineCopy[0]);
    if (fileNode) {
        // Get file path from instance info
        std::string filePath;
        {
            std::unique_lock<std::shared_timed_mutex> lock(mutex_); // Exclusive lock for write operations
            auto instanceIt = instances_.find(instanceId);
            if (instanceIt != instances_.end()) {
                filePath = instanceIt->second.filePath;
                // Also check additionalParams for FILE_PATH
                auto filePathIt = instanceIt->second.additionalParams.find("FILE_PATH");
                if (filePathIt != instanceIt->second.additionalParams.end() && !filePathIt->second.empty()) {
                    filePath = filePathIt->second;
                }
            }
        }
        
        if (!filePath.empty()) {
            // Check if file exists and is readable
            struct stat fileStat;
            if (stat(filePath.c_str(), &fileStat) != 0) {
                std::cerr << "[InstanceRegistry] ✗ File does not exist or is not accessible: " << filePath << std::endl;
                std::cerr << "[InstanceRegistry] ✗ Cannot start instance - file validation failed" << std::endl;
                std::cerr << "[InstanceRegistry] Please check:" << std::endl;
                std::cerr << "[InstanceRegistry]   1. File path is correct: " << filePath << std::endl;
                std::cerr << "[InstanceRegistry]   2. File exists and is readable" << std::endl;
                std::cerr << "[InstanceRegistry]   3. File permissions allow read access" << std::endl;
                // Cleanup pipeline
                {
                    std::unique_lock<std::shared_timed_mutex> lock(mutex_); // Exclusive lock for write operations
                    pipelines_.erase(instanceId);
                }
                return false;
            }
            
            if (!S_ISREG(fileStat.st_mode)) {
                std::cerr << "[InstanceRegistry] ✗ Path is not a regular file: " << filePath << std::endl;
                std::cerr << "[InstanceRegistry] ✗ Cannot start instance - file validation failed" << std::endl;
                // Cleanup pipeline
                {
                    std::unique_lock<std::shared_timed_mutex> lock(mutex_); // Exclusive lock for write operations
                    pipelines_.erase(instanceId);
                }
                return false;
            }
            
            std::cerr << "[InstanceRegistry] ✓ File validation passed: " << filePath << std::endl;
        } else {
            std::cerr << "[InstanceRegistry] ⚠ Warning: File path is empty for file source node" << std::endl;
        }
    }
    
    // Validate model files for DNN nodes BEFORE starting pipeline
    // This prevents assertion failures when model files don't exist
    std::map<std::string, std::string> additionalParams;
    {
        std::unique_lock<std::shared_timed_mutex> lock(mutex_); // Exclusive lock for write operations
        auto instanceIt = instances_.find(instanceId);
        if (instanceIt != instances_.end()) {
            additionalParams = instanceIt->second.additionalParams;
        }
    }
    
    bool modelValidationFailed = false;
    std::string missingModelPath;
    
    // Check for YuNet face detector node
    for (const auto& node : pipelineCopy) {
        auto yunetNode = std::dynamic_pointer_cast<cvedix_nodes::cvedix_yunet_face_detector_node>(node);
        if (yunetNode) {
            // Get model path from additionalParams
            std::string modelPath;
            auto modelPathIt = additionalParams.find("MODEL_PATH");
            if (modelPathIt != additionalParams.end() && !modelPathIt->second.empty()) {
                modelPath = modelPathIt->second;
            } else {
                // Try to get from solution config or use default
                // For now, we'll check if the default path exists
                modelPath = "/usr/share/cvedix/cvedix_data/models/face/face_detection_yunet_2022mar.onnx";
            }
            
            // Check if model file exists
            struct stat modelStat;
            if (stat(modelPath.c_str(), &modelStat) != 0) {
                std::cerr << "[InstanceRegistry] ========================================" << std::endl;
                std::cerr << "[InstanceRegistry] ✗ CRITICAL: YuNet model file not found!" << std::endl;
                std::cerr << "[InstanceRegistry] Expected path: " << modelPath << std::endl;
                std::cerr << "[InstanceRegistry] ========================================" << std::endl;
                std::cerr << "[InstanceRegistry] Cannot start instance - model file validation failed" << std::endl;
                std::cerr << "[InstanceRegistry] The pipeline will crash with assertion failure if started without model file" << std::endl;
                std::cerr << "[InstanceRegistry] Please ensure the model file exists before starting the instance" << std::endl;
                std::cerr << "[InstanceRegistry] ========================================" << std::endl;
                modelValidationFailed = true;
                missingModelPath = modelPath;
                break;
            }
            
            if (!S_ISREG(modelStat.st_mode)) {
                std::cerr << "[InstanceRegistry] ✗ CRITICAL: Model path is not a regular file: " << modelPath << std::endl;
                std::cerr << "[InstanceRegistry] Cannot start instance - model file validation failed" << std::endl;
                modelValidationFailed = true;
                missingModelPath = modelPath;
                break;
            }
            
            std::cerr << "[InstanceRegistry] ✓ YuNet model file validation passed: " << modelPath << std::endl;
        }
        
        // Check for SFace feature encoder node
        auto sfaceNode = std::dynamic_pointer_cast<cvedix_nodes::cvedix_sface_feature_encoder_node>(node);
        if (sfaceNode) {
            // Get model path from additionalParams
            std::string modelPath;
            auto modelPathIt = additionalParams.find("SFACE_MODEL_PATH");
            if (modelPathIt != additionalParams.end() && !modelPathIt->second.empty()) {
                modelPath = modelPathIt->second;
            } else {
                // Use default path
                modelPath = "/home/pnsang/project/edge_ai_sdk/cvedix_data/models/face/face_recognition_sface_2021dec.onnx";
            }
            
            // Check if model file exists
            struct stat modelStat;
            if (stat(modelPath.c_str(), &modelStat) != 0) {
                std::cerr << "[InstanceRegistry] ========================================" << std::endl;
                std::cerr << "[InstanceRegistry] ✗ CRITICAL: SFace model file not found!" << std::endl;
                std::cerr << "[InstanceRegistry] Expected path: " << modelPath << std::endl;
                std::cerr << "[InstanceRegistry] ========================================" << std::endl;
                std::cerr << "[InstanceRegistry] Cannot start instance - model file validation failed" << std::endl;
                std::cerr << "[InstanceRegistry] The pipeline will crash with assertion failure if started without model file" << std::endl;
                std::cerr << "[InstanceRegistry] Please ensure the model file exists before starting the instance" << std::endl;
                std::cerr << "[InstanceRegistry] ========================================" << std::endl;
                modelValidationFailed = true;
                missingModelPath = modelPath;
                break;
            }
            
            if (!S_ISREG(modelStat.st_mode)) {
                std::cerr << "[InstanceRegistry] ✗ CRITICAL: Model path is not a regular file: " << modelPath << std::endl;
                std::cerr << "[InstanceRegistry] Cannot start instance - model file validation failed" << std::endl;
                modelValidationFailed = true;
                missingModelPath = modelPath;
                break;
            }
            
            std::cerr << "[InstanceRegistry] ✓ SFace model file validation passed: " << modelPath << std::endl;
        }
    }
    
    // If model validation failed, cleanup and return false
    if (modelValidationFailed) {
        std::cerr << "[InstanceRegistry] ✗ Cannot start instance - model file validation failed" << std::endl;
        std::cerr << "[InstanceRegistry] Missing model file: " << missingModelPath << std::endl;
        // Cleanup pipeline
        {
            std::unique_lock<std::shared_timed_mutex> lock(mutex_); // Exclusive lock for write operations
            pipelines_.erase(instanceId);
        }
        return false;
    }
    
    std::cerr << "[InstanceRegistry] ========================================" << std::endl;
    std::cerr << "[InstanceRegistry] Starting pipeline for instance " << instanceId << "..." << std::endl;
    std::cerr << "[InstanceRegistry] ========================================" << std::endl;
    
    // Start pipeline (isRestart=true because we rebuilt the pipeline)
    bool started = false;
    try {
        started = startPipeline(instanceId, pipelineCopy, true);
    } catch (const std::exception& e) {
        std::cerr << "[InstanceRegistry] ✗ Exception starting pipeline: " << e.what() << std::endl;
        // Cleanup pipeline on error
        {
            std::unique_lock<std::shared_timed_mutex> lock(mutex_); // Exclusive lock for write operations
            pipelines_.erase(instanceId);
        }
        return false;
    } catch (...) {
        std::cerr << "[InstanceRegistry] ✗ Unknown exception starting pipeline" << std::endl;
        // Cleanup pipeline on error
        {
            std::unique_lock<std::shared_timed_mutex> lock(mutex_); // Exclusive lock for write operations
            pipelines_.erase(instanceId);
        }
        return false;
    }
    
    // Update running status and cleanup on failure
    {
        std::unique_lock<std::shared_timed_mutex> lock(mutex_); // Exclusive lock for write operations
        auto instanceIt = instances_.find(instanceId);
        if (instanceIt != instances_.end()) {
            if (started) {
                instanceIt->second.running = true;
                
                // Initialize statistics tracker
                InstanceStatsTracker tracker;
                tracker.start_time = std::chrono::steady_clock::now();
                tracker.start_time_system = std::chrono::system_clock::now();  // For Unix timestamp
                tracker.frames_processed = 0;
                tracker.last_fps = 0.0;
                tracker.last_fps_update = tracker.start_time;
                tracker.frame_count_since_last_update = 0;
                tracker.resolution = "";
                tracker.source_resolution = "";
                tracker.format = "BGR";  // Default format for CVEDIX
                
                // Try to get resolution from source node
                if (!pipelineCopy.empty()) {
                    auto sourceNode = pipelineCopy[0];
                    auto rtspNode = std::dynamic_pointer_cast<cvedix_nodes::cvedix_rtsp_src_node>(sourceNode);
                    auto fileNode = std::dynamic_pointer_cast<cvedix_nodes::cvedix_file_src_node>(sourceNode);
                    auto rtmpNode = std::dynamic_pointer_cast<cvedix_nodes::cvedix_rtmp_src_node>(sourceNode);
                    
                    try {
                        if (rtspNode) {
                            // Wait a bit for RTSP to connect and get resolution
                            std::this_thread::sleep_for(std::chrono::milliseconds(500));
                            auto width = rtspNode->get_original_width();
                            auto height = rtspNode->get_original_height();
                            if (width > 0 && height > 0) {
                                tracker.source_resolution = std::to_string(width) + "x" + std::to_string(height);
                                tracker.resolution = tracker.source_resolution;  // Use source resolution as current
                            }
                        } else if (fileNode) {
                            // File source may have similar APIs, try to get resolution
                            // Note: CVEDIX file source may not expose these APIs directly
                            // We'll leave it empty for now and update when frame is processed
                        } else if (rtmpNode) {
                            // RTMP source - similar to RTSP
                            std::this_thread::sleep_for(std::chrono::milliseconds(500));
                            // Check if RTMP source has similar APIs
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "[InstanceRegistry] Warning: Could not get resolution from source node: " << e.what() << std::endl;
                    } catch (...) {
                        // Ignore errors - resolution will remain empty
                    }
                }
                
                statistics_trackers_[instanceId] = tracker;
                
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
            
            // Start periodic logging in a separate thread (managed, not detached)
            startLoggingThread(instanceId);
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
        std::unique_lock<std::shared_timed_mutex> lock(mutex_); // Exclusive lock for write operations
        
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
        
        // Reset statistics tracker
        statistics_trackers_.erase(instanceId);
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
    
    // Stop logging thread if exists
    stopLoggingThread(instanceId);
    
    // Cleanup frame cache
    {
        std::lock_guard<std::mutex> lock(frame_cache_mutex_);
        frame_caches_.erase(instanceId);
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
    // CRITICAL: Use try_lock_for with timeout to prevent blocking if mutex is held by other operations
    // This prevents deadlock when called from terminate handler or other critical paths
    std::shared_lock<std::shared_timed_mutex> lock(mutex_, std::defer_lock);
    
    // Try to acquire lock with timeout (1000ms)
    // If we can't get the lock quickly, return empty vector to prevent deadlock
    if (!lock.try_lock_for(std::chrono::milliseconds(1000))) {
        std::cerr << "[InstanceRegistry] WARNING: listInstances() timeout - mutex is locked, returning empty vector" << std::endl;
        if (isInstanceLoggingEnabled()) {
            PLOG_WARNING << "[InstanceRegistry] listInstances() timeout after 1000ms - mutex may be locked by another operation";
        }
        return {}; // Return empty vector to prevent blocking
    }
    
    // Successfully acquired lock, return list of instance IDs
    std::vector<std::string> result;
    result.reserve(instances_.size());
    for (const auto& pair : instances_) {
        result.push_back(pair.first);
    }
    return result;
}

int InstanceRegistry::getInstanceCount() const {
    // CRITICAL: Use try_lock_for with timeout to prevent blocking if mutex is held by other operations
    std::shared_lock<std::shared_timed_mutex> lock(mutex_, std::defer_lock);
    
    // Try to acquire lock with timeout (1000ms)
    if (!lock.try_lock_for(std::chrono::milliseconds(1000))) {
        std::cerr << "[InstanceRegistry] WARNING: getInstanceCount() timeout - mutex is locked, returning 0" << std::endl;
        if (isInstanceLoggingEnabled()) {
            PLOG_WARNING << "[InstanceRegistry] getInstanceCount() timeout after 1000ms - mutex may be locked by another operation";
        }
        return 0; // Return 0 to prevent blocking
    }
    
    // Successfully acquired lock, return count
    return static_cast<int>(instances_.size());
}

std::unordered_map<std::string, InstanceInfo> InstanceRegistry::getAllInstances() const {
    // Use shared_lock (read lock) to allow multiple concurrent readers
    // This allows multiple API requests to call getAllInstances() simultaneously
    // Writers (start/stop/update) will use exclusive lock and block readers only when writing
    std::shared_lock<std::shared_timed_mutex> lock(mutex_);
    
    // Return copy of instances - this is fast and doesn't block other readers
    return instances_;
}

bool InstanceRegistry::hasInstance(const std::string& instanceId) const {
    std::shared_lock<std::shared_timed_mutex> lock(mutex_); // Read lock - allows concurrent readers
    return instances_.find(instanceId) != instances_.end();
}

bool InstanceRegistry::updateInstance(const std::string& instanceId, const UpdateInstanceRequest& req) {
    bool isPersistent = false;
    InstanceInfo infoCopy;
    bool hasChanges = false;
    
    {
        std::unique_lock<std::shared_timed_mutex> lock(mutex_); // Exclusive lock for write operations
        
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
        std::unique_lock<std::shared_timed_mutex> lock(mutex_); // Exclusive lock for write operations
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
    
    std::unique_lock<std::shared_timed_mutex> lock(mutex_); // Exclusive lock for write operations
    for (const auto& instanceId : instanceIds) {
        auto optInfo = instance_storage_.loadInstance(instanceId);
        if (optInfo.has_value()) {
            instances_[instanceId] = optInfo.value();
            // Reset timing fields when loading from storage
            // This ensures accurate time calculations when instance starts
            instances_[instanceId].startTime = std::chrono::steady_clock::now();
            instances_[instanceId].lastActivityTime = instances_[instanceId].startTime;
            instances_[instanceId].hasReceivedData = false;
            instances_[instanceId].retryCount = 0;
            instances_[instanceId].retryLimitReached = false;
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
    
    // Initialize timing fields to current time (will be reset when instance starts)
    // This prevents incorrect time calculations if instance is checked before starting
    auto now = std::chrono::steady_clock::now();
    info.startTime = now;
    info.lastActivityTime = now;
    info.hasReceivedData = false;
    info.retryCount = 0;
    info.retryLimitReached = false;
    
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

bool InstanceRegistry::startPipeline(const std::string& instanceId, const std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>>& nodes, bool isRestart) {
    if (nodes.empty()) {
        std::cerr << "[InstanceRegistry] Cannot start pipeline: no nodes" << std::endl;
        return false;
    }
    
    // Setup frame capture hook before starting pipeline
    setupFrameCaptureHook(instanceId, nodes);
    
    // Setup queue size tracking hook before starting pipeline
    setupQueueSizeTrackingHook(instanceId, nodes);
    
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
            
            // CRITICAL: Validate file exists BEFORE starting to prevent infinite retry loops
            // Note: File path validation should have been done in startInstance(), but we check again here for safety
            // This prevents SDK from retrying indefinitely when file doesn't exist
            // We can't easily get file path from node, so we rely on validation in startInstance()
            // If we reach here and file doesn't exist, SDK will retry - but validation should have caught it
            
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
    
    // Note: We can't easily get instanceId from nodes here, so frame cache cleanup
    // will be handled in stopInstance/deleteInstance methods
    
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
                    // CRITICAL: Try stop() first, but if it blocks due to retry loop, use detach_recursively()
                    // RTSP retry loops can prevent stop() from returning, so we need a fallback
                    std::cerr << "[InstanceRegistry] Attempting to stop RTSP node (may take time if retry loop is active)..." << std::endl;
                    
                    // Try stop() with timeout protection using async
                    auto stopFuture = std::async(std::launch::async, [rtspNode]() {
                        try {
                    rtspNode->stop();
                            return true;
                        } catch (...) {
                            return false;
                        }
                    });
                    
                    // Wait max 200ms for stop() to complete
                    // RTSP retry loops can block stop(), so use short timeout and immediately detach
                    auto stopStatus = stopFuture.wait_for(std::chrono::milliseconds(200));
                    if (stopStatus == std::future_status::timeout) {
                        std::cerr << "[InstanceRegistry] ⚠ RTSP stop() timeout (200ms) - retry loop may be blocking" << std::endl;
                        std::cerr << "[InstanceRegistry] Attempting force stop using detach_recursively()..." << std::endl;
                        // Force stop using detach - this should break retry loop
                        try {
                            rtspNode->detach_recursively();
                            std::cerr << "[InstanceRegistry] ✓ RTSP node force stopped using detach_recursively()" << std::endl;
                        } catch (const std::exception& e) {
                            std::cerr << "[InstanceRegistry] ✗ Exception force stopping RTSP node: " << e.what() << std::endl;
                        } catch (...) {
                            std::cerr << "[InstanceRegistry] ✗ Unknown error force stopping RTSP node" << std::endl;
                        }
                    } else if (stopStatus == std::future_status::ready) {
                        try {
                            if (stopFuture.get()) {
                    auto stopEndTime = std::chrono::steady_clock::now();
                    auto stopDuration = std::chrono::duration_cast<std::chrono::milliseconds>(stopEndTime - stopTime).count();
                    std::cerr << "[InstanceRegistry] ✓ RTSP source node stopped in " << stopDuration << "ms" << std::endl;
                            }
                        } catch (...) {
                            std::cerr << "[InstanceRegistry] ✗ Exception getting stop result" << std::endl;
                        }
                    }
                    // Give it a moment to fully stop
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                } catch (const std::exception& e) {
                    std::cerr << "[InstanceRegistry] ✗ Exception stopping RTSP node: " << e.what() << std::endl;
                    // Try force stop as fallback
                    try {
                        rtspNode->detach_recursively();
                        std::cerr << "[InstanceRegistry] ✓ RTSP node force stopped using detach_recursively() (fallback)" << std::endl;
                    } catch (...) {
                        std::cerr << "[InstanceRegistry] ✗ Force stop also failed" << std::endl;
                    }
                } catch (...) {
                    std::cerr << "[InstanceRegistry] ✗ Unknown error stopping RTSP node" << std::endl;
                    // Try force stop as fallback
                    try {
                        rtspNode->detach_recursively();
                        std::cerr << "[InstanceRegistry] ✓ RTSP node force stopped using detach_recursively() (fallback)" << std::endl;
                    } catch (...) {
                        std::cerr << "[InstanceRegistry] ✗ Force stop also failed" << std::endl;
                    }
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
        std::unique_lock<std::shared_timed_mutex> lock(mutex_); // Exclusive lock for write operations
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
                std::unique_lock<std::shared_timed_mutex> lock(mutex_); // Exclusive lock for write operations
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
    // CRITICAL: Use shared_lock for read-only operations to allow concurrent readers
    // This prevents deadlock when multiple threads read instance data simultaneously
    std::shared_lock<std::shared_timed_mutex> lock(mutex_); // Shared lock for read operations
    
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
    std::shared_lock<std::shared_timed_mutex> lock(mutex_); // Read lock - allows concurrent readers
    
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

std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>> InstanceRegistry::getInstanceNodes(const std::string& instanceId) const {
    std::shared_lock<std::shared_timed_mutex> lock(mutex_); // Read lock - allows concurrent readers
    
    // Get pipeline for this instance
    auto pipelineIt = pipelines_.find(instanceId);
    if (pipelineIt != pipelines_.end() && !pipelineIt->second.empty()) {
        return pipelineIt->second; // Return copy of nodes
    }
    
    return {}; // Return empty vector if instance doesn't have pipeline
}

int InstanceRegistry::checkAndHandleRetryLimits() {
    // CRITICAL: Collect instances to stop while holding lock, then release lock before calling stopInstance()
    // This prevents deadlock because stopInstance() needs exclusive lock
    std::vector<std::string> instancesToStop; // Collect instances to stop while holding lock
    int stoppedCount = 0;
    auto now = std::chrono::steady_clock::now();
    
    {
        // Use exclusive lock for write operations (updating retry counts, marking instances as stopped)
        // This will block readers (getAllInstances) only when actually writing
        std::unique_lock<std::shared_timed_mutex> lock(mutex_);
        
        // Check all running instances
        for (auto& [instanceId, info] : instances_) {
            if (!info.running || info.retryLimitReached) {
                continue; // Skip non-running instances or already stopped due to retry limit
            }
            
            // Check if this is an RTSP instance (has RTSP URL)
            if (!info.rtspUrl.empty()) {
                // Get pipeline to check if RTSP node exists
                auto pipelineIt = pipelines_.find(instanceId);
                if (pipelineIt != pipelines_.end() && !pipelineIt->second.empty()) {
                    auto rtspNode = std::dynamic_pointer_cast<cvedix_nodes::cvedix_rtsp_src_node>(pipelineIt->second[0]);
                    if (rtspNode) {
                        // Calculate time since instance started
                        auto timeSinceStart = std::chrono::duration_cast<std::chrono::seconds>(
                            now - info.startTime).count();
                        
                        // Calculate time since last activity (or since start if no activity)
                        auto timeSinceActivity = std::chrono::duration_cast<std::chrono::seconds>(
                            now - info.lastActivityTime).count();
                        
                        // Only increment retry counter if:
                        // 1. Instance has been running for at least 30 seconds (give it time to connect)
                        // 2. AND instance has not received any data yet (hasReceivedData = false)
                        // 3. OR instance has been running for more than 60 seconds without activity
                        bool isLikelyRetrying = false;
                        if (timeSinceStart >= 30) {
                            if (!info.hasReceivedData) {
                                // Instance has been running for 30+ seconds without receiving any data
                                // This indicates it's likely stuck in retry loop
                                isLikelyRetrying = true;
                            } else if (timeSinceActivity > 60) {
                                // Instance received data before but now has been inactive for 60+ seconds
                                // This might indicate connection was lost and retrying
                                isLikelyRetrying = true;
                            }
                        }
                        
                        if (isLikelyRetrying) {
                            // Increment retry counter only when we detect retry is happening
                            info.retryCount++;
                            
                            std::cerr << "[InstanceRegistry] Instance " << instanceId 
                                      << " retry detected: count=" << info.retryCount 
                                      << "/" << info.maxRetryCount 
                                      << ", running=" << timeSinceStart << "s"
                                      << ", no_data=" << (!info.hasReceivedData ? "yes" : "no")
                                      << ", inactive=" << timeSinceActivity << "s" << std::endl;
                            
                            // Check if retry limit reached
                            if (info.retryCount >= info.maxRetryCount) {
                                info.retryLimitReached = true;
                                std::cerr << "[InstanceRegistry] ⚠ Instance " << instanceId 
                                          << " reached retry limit (" << info.maxRetryCount 
                                          << " retries) after " << timeSinceStart 
                                          << " seconds - stopping instance" << std::endl;
                                PLOG_WARNING << "[Instance] Instance " << instanceId 
                                             << " reached retry limit - stopping";
                                
                                // Mark as not running (will be stopped outside lock)
                                info.running = false;
                                instancesToStop.push_back(instanceId); // Collect for stopping outside lock
                                stoppedCount++;
                            }
                        } else {
                            // Check if instance is receiving data (fps > 0 indicates frames are being processed)
                            if (info.fps > 0) {
                                // Instance is receiving frames - mark as having received data
                                if (!info.hasReceivedData) {
                                    std::cerr << "[InstanceRegistry] Instance " << instanceId 
                                              << " connection successful - receiving frames (fps=" 
                                              << std::fixed << std::setprecision(2) << info.fps << ")" << std::endl;
                                    info.hasReceivedData = true;
                                }
                                // Update last activity time when receiving frames
                                info.lastActivityTime = now;
                                
                                // Reset retry counter if instance is successfully receiving data
                                if (info.retryCount > 0) {
                                    std::cerr << "[InstanceRegistry] Instance " << instanceId 
                                              << " connection successful - resetting retry counter" << std::endl;
                                    info.retryCount = 0;
                                }
                            } else {
                                // Debug: Log when RTSP is connected but no frames received
                                if (timeSinceStart > 5 && timeSinceStart < 35) {
                                    // Only log once every 5 seconds to avoid spam
                                    static std::map<std::string, std::chrono::steady_clock::time_point> lastLogTime;
                                    auto lastLog = lastLogTime.find(instanceId);
                                    bool shouldLog = false;
                                    if (lastLog == lastLogTime.end()) {
                                        shouldLog = true;
                                        lastLogTime[instanceId] = now;
                                    } else {
                                        auto timeSinceLastLog = std::chrono::duration_cast<std::chrono::seconds>(
                                            now - lastLog->second).count();
                                        if (timeSinceLastLog >= 5) {
                                            shouldLog = true;
                                            lastLogTime[instanceId] = now;
                                        }
                                    }
                                    if (shouldLog) {
                                        std::cerr << "[InstanceRegistry] Instance " << instanceId 
                                                  << " RTSP connected but no frames received yet (running=" 
                                                  << timeSinceStart << "s, fps=" << info.fps 
                                                  << "). This may be normal - RTSP streams can take 10-30 seconds to stabilize." << std::endl;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    } // CRITICAL: Release lock before calling stopInstance() to avoid deadlock
    
    // Stop instances that reached retry limit (do this outside lock to avoid deadlock)
    // stopInstance() needs exclusive lock, so we must release our lock first
    for (const auto& instanceId : instancesToStop) {
        try {
            stopInstance(instanceId);
            std::cerr << "[InstanceRegistry] ✓ Stopped instance " << instanceId 
                      << " due to retry limit" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[InstanceRegistry] ✗ Failed to stop instance " << instanceId 
                      << " due to retry limit: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "[InstanceRegistry] ✗ Failed to stop instance " << instanceId 
                      << " due to retry limit (unknown error)" << std::endl;
        }
    }
    
    return stoppedCount;
}

void InstanceRegistry::logProcessingResults(const std::string& instanceId) const {
    // CRITICAL: Use shared_lock for read-only operations to allow concurrent readers
    // This prevents deadlock when multiple threads read instance data simultaneously
    std::shared_lock<std::shared_timed_mutex> lock(mutex_); // Shared lock for read operations
    
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
    
    // Log output type - check directly from info instead of calling hasRTMPOutput to avoid deadlock
    bool hasRTMP = !info.rtmpUrl.empty() || 
                   info.additionalParams.find("RTMP_URL") != info.additionalParams.end();
    if (hasRTMP) {
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
        std::unique_lock<std::shared_timed_mutex> lock(mutex_); // Exclusive lock for write operations
        
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
        std::unique_lock<std::shared_timed_mutex> lock(mutex_); // Exclusive lock for write operations
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

void InstanceRegistry::startLoggingThread(const std::string& instanceId) {
    // Stop existing thread if any
    stopLoggingThread(instanceId);
    
    // Create stop flag
    {
        std::lock_guard<std::mutex> lock(thread_mutex_);
        logging_thread_stop_flags_.emplace(instanceId, false);
    }
    
    // Start new logging thread
    std::thread loggingThread([this, instanceId]() {
        while (true) {
            // Check stop flag first (before sleep to allow quick exit)
            {
                std::lock_guard<std::mutex> lock(thread_mutex_);
                auto flagIt = logging_thread_stop_flags_.find(instanceId);
                if (flagIt == logging_thread_stop_flags_.end() || flagIt->second.load()) {
                    // Thread should stop
                    break;
                }
            }
            
            // Wait 10 seconds between logs (but check flag periodically)
            for (int i = 0; i < 100; ++i) { // Check every 100ms for 10 seconds
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                
                // Check stop flag
                {
                    std::lock_guard<std::mutex> lock(thread_mutex_);
                    auto flagIt = logging_thread_stop_flags_.find(instanceId);
                    if (flagIt == logging_thread_stop_flags_.end() || flagIt->second.load()) {
                        return; // Exit thread
                    }
                }
                
                // Check if instance still exists and is running
                {
                    std::unique_lock<std::shared_timed_mutex> lock(mutex_); // Exclusive lock for write operations
                    auto instanceIt = instances_.find(instanceId);
                    if (instanceIt == instances_.end() || !instanceIt->second.running) {
                        // Instance deleted or stopped, exit logging thread
                        return;
                    }
                }
            }
            
            // Log processing results if instance still exists and has no RTMP output
            {
                std::unique_lock<std::shared_timed_mutex> lock(mutex_); // Exclusive lock for write operations
                auto instanceIt = instances_.find(instanceId);
                if (instanceIt == instances_.end() || !instanceIt->second.running) {
                    // Instance deleted or stopped, exit logging thread
                    break;
                }
                
                // Check if instance now has RTMP output
                if (hasRTMPOutput(instanceId)) {
                    // Instance now has RTMP output, stop logging
                    break;
                }
            }
            
            // Log processing results
            logProcessingResults(instanceId);
        }
    });
    
    // Store thread handle
    {
        std::lock_guard<std::mutex> lock(thread_mutex_);
        logging_threads_[instanceId] = std::move(loggingThread);
    }
}

void InstanceRegistry::stopLoggingThread(const std::string& instanceId) {
    std::unique_lock<std::mutex> lock(thread_mutex_);
    
    // Set stop flag
    auto flagIt = logging_thread_stop_flags_.find(instanceId);
    if (flagIt != logging_thread_stop_flags_.end()) {
        flagIt->second.store(true);
    }
    
    // Get thread handle and release lock before joining to avoid deadlock
    std::thread threadToJoin;
    auto threadIt = logging_threads_.find(instanceId);
    if (threadIt != logging_threads_.end()) {
        if (threadIt->second.joinable()) {
            threadToJoin = std::move(threadIt->second);
        }
        logging_threads_.erase(threadIt);
    }
    
    // Remove stop flag
    if (flagIt != logging_thread_stop_flags_.end()) {
        logging_thread_stop_flags_.erase(flagIt);
    }
    
    // Release lock before joining to avoid deadlock
    lock.unlock();
    
    // Join thread outside of lock
    if (threadToJoin.joinable()) {
        threadToJoin.join();
    }
}

Json::Value InstanceRegistry::getInstanceConfig(const std::string& instanceId) const {
    std::unique_lock<std::shared_timed_mutex> lock(mutex_); // Exclusive lock for write operations
    
    auto it = instances_.find(instanceId);
    if (it == instances_.end()) {
        return Json::Value(Json::objectValue); // Return empty object if not found
    }
    
    const InstanceInfo& info = it->second;
    std::string error;
    Json::Value config = instance_storage_.instanceInfoToConfigJson(info, &error);
    
    if (!error.empty()) {
        // Log error but still return config (might be partial)
        if (isApiLoggingEnabled()) {
            PLOG_WARNING << "[InstanceRegistry] Error converting instance to config: " << error;
        }
    }
    
    return config;
}

std::optional<InstanceStatistics> InstanceRegistry::getInstanceStatistics(const std::string& instanceId) {
    std::unique_lock<std::shared_timed_mutex> lock(mutex_);
    
    // Check if instance exists and is running
    auto instanceIt = instances_.find(instanceId);
    if (instanceIt == instances_.end()) {
        return std::nullopt;
    }
    
    const InstanceInfo& info = instanceIt->second;
    if (!info.running) {
        return std::nullopt;
    }
    
    // Get pipeline to access source node
    auto pipelineIt = pipelines_.find(instanceId);
    if (pipelineIt == pipelines_.end() || pipelineIt->second.empty()) {
        return std::nullopt;
    }
    
    // Get tracker (non-const reference so we can update it)
    auto trackerIt = statistics_trackers_.find(instanceId);
    if (trackerIt == statistics_trackers_.end()) {
        // Tracker not initialized yet, return default statistics
        InstanceStatistics stats;
        // Round current_framerate to nearest integer
        stats.current_framerate = std::round(info.fps);
        return stats;
    }
    
    InstanceStatsTracker& tracker = trackerIt->second;
    
    // Build statistics object
    InstanceStatistics stats;
    
    // Get source framerate and resolution from source node FIRST (before calculating stats)
    // This ensures we have the most up-to-date information
    auto sourceNode = pipelineIt->second[0];
    if (!sourceNode) {
        std::cerr << "[InstanceRegistry] [Statistics] ERROR: Source node is null!" << std::endl;
        return std::nullopt;
    }
    
    std::cerr << "[InstanceRegistry] [Statistics] Source node type: " << typeid(*sourceNode).name() << std::endl;
    auto rtspNode = std::dynamic_pointer_cast<cvedix_nodes::cvedix_rtsp_src_node>(sourceNode);
    auto fileNode = std::dynamic_pointer_cast<cvedix_nodes::cvedix_file_src_node>(sourceNode);
    auto rtmpNode = std::dynamic_pointer_cast<cvedix_nodes::cvedix_rtmp_src_node>(sourceNode);
    
    std::cerr << "[InstanceRegistry] [Statistics] Node casts - rtspNode: " << (rtspNode ? "OK" : "null") 
              << ", fileNode: " << (fileNode ? "OK" : "null") 
              << ", rtmpNode: " << (rtmpNode ? "OK" : "null") << std::endl;
    
    double sourceFps = 0.0;
    std::string sourceRes = "";
    
    try {
        if (rtspNode) {
            // Get source framerate and resolution from RTSP node
            int fps_int = rtspNode->get_original_fps();
            // Note: get_original_fps() returns -1 if not available, > 0 if available
            if (fps_int > 0) {
                sourceFps = static_cast<double>(fps_int);
                std::cerr << "[InstanceRegistry] [Statistics] RTSP source FPS: " << sourceFps << std::endl;
            } else {
                std::cerr << "[InstanceRegistry] [Statistics] RTSP source FPS not available yet (fps_int=" << fps_int << ")" << std::endl;
            }
            
            auto width = rtspNode->get_original_width();
            auto height = rtspNode->get_original_height();
            std::cerr << "[InstanceRegistry] [Statistics] RTSP source resolution: width=" << width << ", height=" << height << std::endl;
            if (width > 0 && height > 0) {
                sourceRes = std::to_string(width) + "x" + std::to_string(height);
                std::cerr << "[InstanceRegistry] [Statistics] RTSP source resolution: " << sourceRes << std::endl;
            } else {
                std::cerr << "[InstanceRegistry] [Statistics] RTSP source resolution not available yet" << std::endl;
            }
        } else if (fileNode) {
            std::cerr << "[InstanceRegistry] [Statistics] File source node detected" << std::endl;
            // File source inherits from cvedix_src_node, so it has get_original_fps/width/height methods
            int fps_int = fileNode->get_original_fps();
            if (fps_int > 0) {
                sourceFps = static_cast<double>(fps_int);
                std::cerr << "[InstanceRegistry] [Statistics] File source FPS: " << sourceFps << std::endl;
            } else {
                std::cerr << "[InstanceRegistry] [Statistics] File source FPS not available yet (fps_int=" << fps_int << ")" << std::endl;
            }
            
            auto width = fileNode->get_original_width();
            auto height = fileNode->get_original_height();
            std::cerr << "[InstanceRegistry] [Statistics] File source resolution: width=" << width << ", height=" << height << std::endl;
            if (width > 0 && height > 0) {
                sourceRes = std::to_string(width) + "x" + std::to_string(height);
                std::cerr << "[InstanceRegistry] [Statistics] File source resolution: " << sourceRes << std::endl;
            } else {
                std::cerr << "[InstanceRegistry] [Statistics] File source resolution not available yet" << std::endl;
            }
        } else if (rtmpNode) {
            std::cerr << "[InstanceRegistry] [Statistics] RTMP source node detected" << std::endl;
            // RTMP source - similar to RTSP if APIs are available
        } else {
            std::cerr << "[InstanceRegistry] [Statistics] Unknown source node type (not RTSP, File, or RTMP)" << std::endl;
            std::cerr << "[InstanceRegistry] [Statistics] Source node type: " << typeid(*sourceNode).name() << std::endl;
        }
    } catch (const std::exception& e) {
        // If APIs are not available or throw exceptions, use defaults
        std::cerr << "[InstanceRegistry] [Statistics] Exception getting source info: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "[InstanceRegistry] [Statistics] Unknown exception getting source info" << std::endl;
    }
    
    // Calculate elapsed time first
    auto now = std::chrono::steady_clock::now();
    auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(now - tracker.start_time).count();
    auto elapsed_seconds_double = std::chrono::duration<double>(now - tracker.start_time).count();
    
    // Calculate actual processing FPS based on frames actually processed
    // This is more accurate than using sourceFps directly
    double actualProcessingFps = 0.0;
    if (elapsed_seconds_double > 0.0 && tracker.frames_processed > 0) {
        // Actual FPS = frames processed / elapsed time
        actualProcessingFps = static_cast<double>(tracker.frames_processed) / elapsed_seconds_double;
        std::cerr << "[InstanceRegistry] [Statistics] DEBUG - Calculated actualProcessingFps from frames_processed: " 
                  << actualProcessingFps << " (frames_processed=" << tracker.frames_processed 
                  << ", elapsed=" << elapsed_seconds_double << "s)" << std::endl;
    }
    
    // Calculate current FPS: prefer actual processing FPS, then source FPS, then info.fps, then tracker.last_fps
    // IMPORTANT: Use actual processing FPS if available, otherwise fallback to source FPS
    double currentFps = 0.0;
    std::cerr << "[InstanceRegistry] [Statistics] Calculating FPS: sourceFps=" << sourceFps 
              << ", actualProcessingFps=" << actualProcessingFps
              << ", info.fps=" << info.fps << ", tracker.last_fps=" << tracker.last_fps << std::endl;
    
    if (actualProcessingFps > 0.0) {
        // Use actual processing FPS (most accurate for dropped frames calculation)
        currentFps = actualProcessingFps;
        tracker.last_fps = actualProcessingFps;
        tracker.last_fps_update = now;
        std::cerr << "[InstanceRegistry] [Statistics] Using actual processing FPS: " << currentFps << std::endl;
    } else if (sourceFps > 0.0) {
        // Fallback to source FPS
        currentFps = sourceFps;
        tracker.last_fps = sourceFps;
        tracker.last_fps_update = now;
        std::cerr << "[InstanceRegistry] [Statistics] Using source FPS (fallback): " << currentFps << std::endl;
    } else if (info.fps > 0.0) {
        currentFps = info.fps;
        tracker.last_fps = info.fps;
        tracker.last_fps_update = now;
        std::cerr << "[InstanceRegistry] [Statistics] Using info.fps: " << currentFps << std::endl;
    } else {
        currentFps = tracker.last_fps;
        std::cerr << "[InstanceRegistry] [Statistics] Using tracker.last_fps: " << currentFps << std::endl;
    }
    
    // Calculate frames_processed: use actual frames processed if available, otherwise estimate
    uint64_t actual_frames_processed = tracker.frames_processed;
    uint64_t calculated_frames_processed = 0;
    
    if (actual_frames_processed > 0) {
        // Use actual frames processed (most accurate)
        stats.frames_processed = actual_frames_processed;
        calculated_frames_processed = actual_frames_processed;
        std::cerr << "[InstanceRegistry] [Statistics] Using actual frames_processed: " << actual_frames_processed << std::endl;
    } else if (currentFps > 0.0 && elapsed_seconds > 0) {
        // Estimate frames processed = FPS * elapsed time
        calculated_frames_processed = static_cast<uint64_t>(currentFps * elapsed_seconds);
        stats.frames_processed = calculated_frames_processed;
        std::cerr << "[InstanceRegistry] [Statistics] Estimated frames_processed: " << calculated_frames_processed << std::endl;
    } else {
        stats.frames_processed = 0;
        calculated_frames_processed = 0;
    }
    
    std::cerr << "[InstanceRegistry] [Statistics] DEBUG - elapsed_seconds: " << elapsed_seconds 
              << ", sourceFps: " << sourceFps << ", actualProcessingFps: " << actualProcessingFps
              << ", currentFps: " << currentFps 
              << ", actual_frames_processed: " << actual_frames_processed
              << ", calculated_frames_processed: " << calculated_frames_processed << std::endl;
    
    // Calculate dropped frames: difference between expected frames (from source FPS) and actual processed frames
    // Expected frames = source FPS * elapsed time
    // Actual processed frames = actual frames processed (or calculated from actual FPS)
    // Dropped frames = expected - actual (if expected > actual)
    if (sourceFps > 0.0 && elapsed_seconds > 0) {
        uint64_t expected_frames = static_cast<uint64_t>(sourceFps * elapsed_seconds);
        tracker.expected_frames_from_source = expected_frames;
        
        // Use actual frames processed for comparison (more accurate)
        uint64_t actual_processed = (actual_frames_processed > 0) ? actual_frames_processed : calculated_frames_processed;
        
        // If we processed fewer frames than expected, the difference is dropped frames
        if (expected_frames > actual_processed) {
            uint64_t estimated_dropped = expected_frames - actual_processed;
            
            // Update dropped frames (no threshold check - any difference is significant)
            // Accumulate dropped frames over time
            if (estimated_dropped > tracker.dropped_frames) {
                tracker.dropped_frames = estimated_dropped;
            }
        }
    }
    
    stats.dropped_frames_count = tracker.dropped_frames;
    // Round current_framerate to nearest integer
    stats.current_framerate = std::round(currentFps);
    
    // Use resolution from source node if available, otherwise use tracker value
    if (!sourceRes.empty()) {
        stats.resolution = sourceRes;
        stats.source_resolution = sourceRes;
        // Update tracker with latest resolution
        tracker.resolution = sourceRes;
        tracker.source_resolution = sourceRes;
    } else {
        stats.resolution = tracker.resolution;
        stats.source_resolution = tracker.source_resolution;
    }
    
    stats.format = tracker.format;
    
    // Calculate start_time (Unix timestamp)
    auto start_time_since_epoch = tracker.start_time_system.time_since_epoch();
    auto start_time_seconds = std::chrono::duration_cast<std::chrono::seconds>(start_time_since_epoch).count();
    stats.start_time = start_time_seconds;
    
    // Set source framerate
    if (sourceFps > 0.0) {
        stats.source_framerate = sourceFps;
    } else {
        // Fallback to current framerate if source FPS not available
        stats.source_framerate = stats.current_framerate;
    }
    
    // Calculate latency (average time per frame in milliseconds)
    if (stats.frames_processed > 0 && currentFps > 0.0) {
        // Latency = 1000ms / FPS (average time per frame)
        // Round latency to nearest integer
        stats.latency = std::round(1000.0 / currentFps);
    } else {
        stats.latency = 0.0;
    }
    
    // Set default format if empty
    if (stats.format.empty()) {
        stats.format = "BGR";  // Default format
        tracker.format = "BGR";
    }
    
    // Queue size - updated via meta_arriving_hooker callback from CVEDIX SDK nodes
    // The hook tracks queue_size when meta arrives at each node's in_queue
    stats.input_queue_size = static_cast<int64_t>(tracker.current_queue_size);
    if (stats.input_queue_size == 0 && tracker.max_queue_size_seen > 0) {
        // If we've seen queue size before but current is 0, use max as indicator
        stats.input_queue_size = static_cast<int64_t>(tracker.max_queue_size_seen);
    }
    
    return stats;
}

// Base64 encoding helper function
static std::string base64_encode(const unsigned char* data, size_t length) {
    static const char base64_chars[] = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::string encoded;
    encoded.reserve(((length + 2) / 3) * 4);
    
    size_t i = 0;
    while (i < length) {
        unsigned char byte1 = data[i++];
        unsigned char byte2 = (i < length) ? data[i++] : 0;
        unsigned char byte3 = (i < length) ? data[i++] : 0;
        
        unsigned int combined = (byte1 << 16) | (byte2 << 8) | byte3;
        
        encoded += base64_chars[(combined >> 18) & 0x3F];
        encoded += base64_chars[(combined >> 12) & 0x3F];
        encoded += (i - 2 < length) ? base64_chars[(combined >> 6) & 0x3F] : '=';
        encoded += (i - 1 < length) ? base64_chars[combined & 0x3F] : '=';
    }
    
    return encoded;
}

std::string InstanceRegistry::encodeFrameToBase64(const cv::Mat& frame, int jpegQuality) const {
    if (frame.empty()) {
        return "";
    }
    
    try {
        std::vector<uchar> buffer;
        std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, jpegQuality};
        
        if (!cv::imencode(".jpg", frame, buffer, params)) {
            std::cerr << "[InstanceRegistry] Failed to encode frame to JPEG" << std::endl;
            return "";
        }
        
        if (buffer.empty()) {
            return "";
        }
        
        return base64_encode(buffer.data(), buffer.size());
    } catch (const std::exception& e) {
        std::cerr << "[InstanceRegistry] Exception encoding frame to base64: " << e.what() << std::endl;
        return "";
    } catch (...) {
        std::cerr << "[InstanceRegistry] Unknown exception encoding frame to base64" << std::endl;
        return "";
    }
}

void InstanceRegistry::updateFrameCache(const std::string& instanceId, const cv::Mat& frame) {
    if (frame.empty()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(frame_cache_mutex_);
    
    FrameCache& cache = frame_caches_[instanceId];
    frame.copyTo(cache.frame);  // Deep copy
    cache.timestamp = std::chrono::steady_clock::now();
    cache.has_frame = true;
}

void InstanceRegistry::setupFrameCaptureHook(const std::string& instanceId, 
                                             const std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>>& nodes) {
    if (nodes.empty()) {
        return;
    }
    
    // Find the last node (OSD or destination node)
    // Try to find app_des_node first (best for capturing frames)
    // If not found, try RTMP destination node
    // If not found, try to find OSD node
    
    std::shared_ptr<cvedix_nodes::cvedix_app_des_node> appDesNode;
    std::shared_ptr<cvedix_nodes::cvedix_rtmp_des_node> rtmpDesNode;
    
    // Search backwards from the end to find destination or OSD node
    for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
        auto node = *it;
        
        // Try app_des_node first
        if (!appDesNode) {
            appDesNode = std::dynamic_pointer_cast<cvedix_nodes::cvedix_app_des_node>(node);
            if (appDesNode) {
                break;
            }
        }
        
        // Try RTMP destination node
        if (!rtmpDesNode) {
            rtmpDesNode = std::dynamic_pointer_cast<cvedix_nodes::cvedix_rtmp_des_node>(node);
        }
    }
    
    // Setup hook on app_des_node if found
    if (appDesNode) {
        appDesNode->set_app_des_result_hooker([this, instanceId](std::string node_name, 
                                                                 std::shared_ptr<cvedix_objects::cvedix_meta> meta) {
            try {
                if (!meta) {
                    return;
                }
                
                if (meta->meta_type == cvedix_objects::cvedix_meta_type::FRAME) {
                    auto frame_meta = std::dynamic_pointer_cast<cvedix_objects::cvedix_frame_meta>(meta);
                    if (!frame_meta) {
                        return;
                    }
                    
                    // Update frame counter for statistics
                    {
                        std::unique_lock<std::shared_timed_mutex> lock(mutex_);
                        auto trackerIt = statistics_trackers_.find(instanceId);
                        if (trackerIt != statistics_trackers_.end()) {
                            trackerIt->second.frames_processed++;
                            trackerIt->second.frame_count_since_last_update++;
                        }
                    }
                    
                    // Prefer OSD frame (processed with overlays), fallback to original frame
                    cv::Mat frameToCache;
                    if (!frame_meta->osd_frame.empty()) {
                        frameToCache = frame_meta->osd_frame;
                    } else if (!frame_meta->frame.empty()) {
                        frameToCache = frame_meta->frame;
                    }
                    
                    if (!frameToCache.empty()) {
                        updateFrameCache(instanceId, frameToCache);
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "[InstanceRegistry] [ERROR] Exception in frame capture hook: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "[InstanceRegistry] [ERROR] Unknown exception in frame capture hook" << std::endl;
            }
        });
        
        std::cerr << "[InstanceRegistry] ✓ Frame capture hook setup completed for instance: " << instanceId << std::endl;
        return;
    }
    
    // If no app_des_node found, log warning
    std::cerr << "[InstanceRegistry] ⚠ Warning: No app_des_node found in pipeline for instance: " << instanceId << std::endl;
    std::cerr << "[InstanceRegistry] Frame capture will not be available. Consider adding app_des_node to pipeline." << std::endl;
}

void InstanceRegistry::setupQueueSizeTrackingHook(const std::string& instanceId, 
                                                  const std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>>& nodes) {
    if (nodes.empty()) {
        return;
    }
    
    // Setup meta_arriving_hooker on all nodes to track input queue size
    // meta_arriving_hooker is called when meta is pushed to in_queue, and provides queue_size parameter
    // We track the maximum queue size across all nodes (especially inference nodes which are bottlenecks)
    for (const auto& node : nodes) {
        if (!node) {
            continue;
        }
        
        // All CVEDIX nodes inherit from cvedix_meta_hookable, so we can setup hook directly
        // The hook signature is: void (std::string node_name, int queue_size, std::shared_ptr<cvedix_objects::cvedix_meta> meta)
        try {
            node->set_meta_arriving_hooker([this, instanceId](std::string node_name, int queue_size, 
                                                               std::shared_ptr<cvedix_objects::cvedix_meta> meta) {
                try {
                    // Update queue size tracking in statistics tracker
                    std::unique_lock<std::shared_timed_mutex> lock(mutex_);
                    auto trackerIt = statistics_trackers_.find(instanceId);
                    if (trackerIt != statistics_trackers_.end()) {
                        InstanceStatsTracker& tracker = trackerIt->second;
                        
                        // Track the maximum queue size across all nodes
                        // This gives us the bottleneck queue size (usually inference nodes)
                        if (queue_size > static_cast<int>(tracker.current_queue_size)) {
                            tracker.current_queue_size = static_cast<size_t>(queue_size);
                        }
                        
                        // Update max queue size seen (historical maximum)
                        if (queue_size > static_cast<int>(tracker.max_queue_size_seen)) {
                            tracker.max_queue_size_seen = static_cast<size_t>(queue_size);
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[InstanceRegistry] [ERROR] Exception in queue size tracking hook: " << e.what() << std::endl;
                } catch (...) {
                    std::cerr << "[InstanceRegistry] [ERROR] Unknown exception in queue size tracking hook" << std::endl;
                }
            });
        } catch (const std::exception& e) {
            std::cerr << "[InstanceRegistry] [WARNING] Failed to setup queue size tracking hook on node: " << e.what() << std::endl;
        } catch (...) {
            // Some nodes might not support hooks, ignore silently
        }
    }
    
    std::cerr << "[InstanceRegistry] ✓ Queue size tracking hook setup completed for instance: " << instanceId << std::endl;
}

std::string InstanceRegistry::getLastFrame(const std::string& instanceId) const {
    std::lock_guard<std::mutex> lock(frame_cache_mutex_);
    
    auto it = frame_caches_.find(instanceId);
    if (it == frame_caches_.end() || !it->second.has_frame || it->second.frame.empty()) {
        return "";  // No frame cached
    }
    
    // Encode frame to base64
    return encodeFrameToBase64(it->second.frame, 85);  // Default quality 85%
}

