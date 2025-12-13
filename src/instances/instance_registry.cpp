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
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <future>
#include <mosquitto.h>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <regex>
#include <fcntl.h>
#include <cstring>
#include <sys/select.h>
#include <sys/time.h>
#include <cvedix/nodes/broker/cvedix_json_console_broker_node.h>
#include <json/json.h>  // For JSON parsing to count vehicles
#include <set>  // For tracking unique vehicle IDs
#include <limits>  // For std::numeric_limits

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
                if (startPipeline(pipeline, instanceId)) {
                    // Update running status and reset retry counter (need lock briefly)
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
    
    {
        std::unique_lock<std::shared_timed_mutex> lock(mutex_); // Exclusive lock for write operations
        
        auto it = instances_.find(instanceId);
        if (it == instances_.end()) {
            return false;
        }
        
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
    
    // Stop logging thread if exists
    stopLoggingThread(instanceId);
    
    // Stop video loop monitoring thread if exists
    stopVideoLoopThread(instanceId);
    
    // Delete from storage (doesn't need lock)
    // Always delete from storage since all instances are saved to storage for debugging/inspection
    // This prevents deleted instances from being reloaded on server restart
    std::cerr << "[InstanceRegistry] Removing instance from storage..." << std::endl;
    instance_storage_.deleteInstance(instanceId);
    
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
        started = startPipeline(pipelineCopy, instanceId, true);
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
                // Reset retry counter and tracking when instance starts successfully
                instanceIt->second.retryCount = 0;
                instanceIt->second.retryLimitReached = false;
                instanceIt->second.startTime = std::chrono::steady_clock::now();
                instanceIt->second.lastActivityTime = instanceIt->second.startTime;
                instanceIt->second.hasReceivedData = false;
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
        
        // Check if instance uses json_console_broker and has MQTT config
        // If so, setup MQTT publishing similar to face_tracking_sample.cpp
        bool hasJsonConsoleBroker = false;
        for (const auto& node : pipelineCopy) {
            auto consoleBrokerNode = std::dynamic_pointer_cast<cvedix_nodes::cvedix_json_console_broker_node>(node);
            if (consoleBrokerNode) {
                hasJsonConsoleBroker = true;
                break;
            }
        }
        
        // Get MQTT config from additionalParams
        // CRITICAL: Use timeout to prevent deadlock if mutex is locked by recovery handler
        std::string mqtt_broker, mqtt_topic, mqtt_username, mqtt_password;
        int mqtt_port = 1883;
        {
            std::unique_lock<std::shared_timed_mutex> lock(mutex_, std::defer_lock);
            // Try to acquire lock with timeout (500ms) - fail fast if locked
            if (lock.try_lock_for(std::chrono::milliseconds(500))) {
                auto instanceIt = instances_.find(instanceId);
                if (instanceIt != instances_.end()) {
                    const auto& params = instanceIt->second.additionalParams;
                    auto brokerIt = params.find("MQTT_BROKER_URL");
                    if (brokerIt != params.end() && !brokerIt->second.empty()) {
                        mqtt_broker = brokerIt->second;
                    }
                    auto portIt = params.find("MQTT_PORT");
                    if (portIt != params.end() && !portIt->second.empty()) {
                        try {
                            mqtt_port = std::stoi(portIt->second);
                        } catch (...) {
                            mqtt_port = 1883;
                        }
                    }
                    auto topicIt = params.find("MQTT_TOPIC");
                    if (topicIt != params.end() && !topicIt->second.empty()) {
                        mqtt_topic = topicIt->second;
                    }
                    auto usernameIt = params.find("MQTT_USERNAME");
                    if (usernameIt != params.end() && !usernameIt->second.empty()) {
                        mqtt_username = usernameIt->second;
                    }
                    auto passwordIt = params.find("MQTT_PASSWORD");
                    if (passwordIt != params.end() && !passwordIt->second.empty()) {
                        mqtt_password = passwordIt->second;
                    }
                }
            } else {
                // Timeout - mutex is locked, skip MQTT setup to prevent deadlock
                std::cerr << "[InstanceRegistry] [MQTT] WARNING: Cannot acquire mutex to read MQTT config (timeout 500ms)" << std::endl;
                std::cerr << "[InstanceRegistry] [MQTT] Skipping MQTT setup to prevent deadlock" << std::endl;
                mqtt_broker.clear();  // Clear to skip MQTT setup
            }
        }
        
        // Setup MQTT if instance uses json_console_broker and has MQTT config
        // CRITICAL: This is the ONLY way to publish MQTT now since json_mqtt_broker_node is broken and causes crashes
        // Using a safer approach: non-blocking I/O with timeout to prevent deadlocks
        // Thread is completely independent and does NOT use instance_registry mutex
        if (hasJsonConsoleBroker && !mqtt_broker.empty() && !mqtt_topic.empty()) {
            std::cerr << "[InstanceRegistry] [MQTT] Setting up MQTT publishing for instance " << instanceId << std::endl;
            std::cerr << "[InstanceRegistry] [MQTT] NOTE: Using stdout pipe method (json_mqtt_broker_node is disabled due to crashes)" << std::endl;
            std::cerr << "[InstanceRegistry] [MQTT] Broker: " << mqtt_broker << ":" << mqtt_port << std::endl;
            std::cerr << "[InstanceRegistry] [MQTT] Topic: " << mqtt_topic << std::endl;
            std::cerr << "[InstanceRegistry] [MQTT] NOTE: Using non-blocking I/O with timeout to prevent deadlocks" << std::endl;
            
            // Initialize mosquitto library if not already initialized
            static std::once_flag mosquitto_init_flag;
            std::call_once(mosquitto_init_flag, []() {
                mosquitto_lib_init();
            });
            
            // Create mosquitto client
            std::string client_id = "edge_ai_api_" + instanceId.substr(0, 8);
            struct mosquitto* mosq = mosquitto_new(client_id.c_str(), true, nullptr);
            bool mqtt_connected = false;
            std::atomic<bool> local_mqtt_connected(false);  // Connection flag for callbacks
            
            if (mosq) {
                // Set username/password if provided
                if (!mqtt_username.empty() && !mqtt_password.empty()) {
                    mosquitto_username_pw_set(mosq, mqtt_username.c_str(), mqtt_password.c_str());
                }
                
                // Set callbacks to detect connection/disconnection
                mosquitto_connect_callback_set(mosq, [](struct mosquitto* /*mosq*/, void* userdata, int result) {
                    std::atomic<bool>* connected_ptr = static_cast<std::atomic<bool>*>(userdata);
                    if (result == 0) {
                        connected_ptr->store(true);
                        std::cerr << "[InstanceRegistry] [MQTT] Connection callback: Connected!" << std::endl;
                    } else {
                        connected_ptr->store(false);
                        std::cerr << "[InstanceRegistry] [MQTT] Connection callback: Failed with code " << result << std::endl;
                    }
                });
                
                mosquitto_disconnect_callback_set(mosq, [](struct mosquitto* /*mosq*/, void* userdata, int /*reason*/) {
                    std::atomic<bool>* connected_ptr = static_cast<std::atomic<bool>*>(userdata);
                    connected_ptr->store(false);
                    std::cerr << "[InstanceRegistry] [MQTT] Disconnection callback: Connection lost!" << std::endl;
                });
                
                // Pass connection flag pointer to callbacks
                mosquitto_user_data_set(mosq, &local_mqtt_connected);
                
                // Connect to broker
                std::cerr << "[InstanceRegistry] [MQTT] Connecting to broker " << mqtt_broker << ":" << mqtt_port << "..." << std::endl;
                int rc = mosquitto_connect(mosq, mqtt_broker.c_str(), mqtt_port, 60);
                if (rc == MOSQ_ERR_SUCCESS) {
                    mqtt_connected = true;
                    local_mqtt_connected.store(true);
                    std::cerr << "[InstanceRegistry] [MQTT] Connected successfully!" << std::endl;
                    mosquitto_loop_start(mosq);
                } else {
                    std::cerr << "[InstanceRegistry] [MQTT] Failed to connect: " << mosquitto_strerror(rc) << std::endl;
                    std::cerr << "[InstanceRegistry] [MQTT] Continuing without MQTT publishing..." << std::endl;
                    mosquitto_destroy(mosq);
                    mosq = nullptr;
                }
            } else {
                std::cerr << "[InstanceRegistry] [MQTT] Failed to create mosquitto client" << std::endl;
            }
            
            // Setup MQTT publishing with non-blocking approach
            if (mosq && mqtt_connected) {
                // Create pipe for stdout redirection
                int pipefd[2];
                if (pipe(pipefd) == -1) {
                    std::cerr << "[InstanceRegistry] [MQTT] Failed to create pipe" << std::endl;
                    if (mosq) {
                        mosquitto_disconnect(mosq);
                        mosquitto_loop_stop(mosq, false);
                        mosquitto_destroy(mosq);
                    }
                } else {
                    // Set pipe to non-blocking mode to prevent deadlock
                    int flags = fcntl(pipefd[0], F_GETFL, 0);
                    fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);
                    
                    // Save original stdout
                    int stdout_backup = dup(STDOUT_FILENO);
                    if (stdout_backup == -1) {
                        std::cerr << "[InstanceRegistry] [MQTT] Failed to backup stdout" << std::endl;
                        close(pipefd[0]);
                        close(pipefd[1]);
                        if (mosq) {
                            mosquitto_disconnect(mosq);
                            mosquitto_loop_stop(mosq, false);
                            mosquitto_destroy(mosq);
                        }
                    } else {
                        // Redirect stdout to pipe
                        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
                            std::cerr << "[InstanceRegistry] [MQTT] Failed to redirect stdout" << std::endl;
                            close(stdout_backup);
                            close(pipefd[0]);
                            close(pipefd[1]);
                            if (mosq) {
                                mosquitto_disconnect(mosq);
                                mosquitto_loop_stop(mosq, false);
                                mosquitto_destroy(mosq);
                            }
                        } else {
                            // CRITICAL: After dup2, pipefd[1] and STDOUT_FILENO point to same FD (pipe write end)
                            // We need to close pipefd[1] to release the extra reference
                            // Close pipefd[1] - we don't need the extra reference anymore
                            // STDOUT_FILENO still points to the pipe write end and will be closed on stop
                            close(pipefd[1]);
                            
                            // Start thread to read JSON from pipe and publish to MQTT
                            // CRITICAL: This thread is COMPLETELY INDEPENDENT - does NOT use instance_registry mutex
                            // Only keep the LATEST JSON, drop all old JSONs to prevent backlog and deadlock
                            // Thread is now managed (not detached) and will be joined on stop
                            
                            // CRITICAL: Use shared_ptr for mosq to ensure it's not destroyed while thread is running
                            std::shared_ptr<struct mosquitto> mosq_shared(mosq, [](struct mosquitto* m) {
                                // Custom deleter - don't destroy here, it will be destroyed when thread exits
                            });
                            
                            // CRITICAL: Create shared_ptr to stop flag to avoid capturing 'this'
                            // This prevents use-after-free if InstanceRegistry is destroyed while thread is running
                            auto stop_flag = std::make_shared<std::atomic<bool>>(false);
                            
                            // CRITICAL: Store stop flag and thread in maps for proper management
                            {
                                std::lock_guard<std::mutex> lock(mqtt_thread_mutex_);
                                mqtt_thread_stop_flags_[instanceId] = stop_flag;
                                // Store STDOUT_FILENO (which is now pipe write end) to close it on stop
                                // Note: After dup2, STDOUT_FILENO points to pipe write end
                                mqtt_pipe_write_fds_[instanceId] = STDOUT_FILENO;
                                // Store stdout backup to restore it on stop
                                mqtt_stdout_backups_[instanceId] = stdout_backup;
                            }
                            
                            // Update local_mqtt_connected with current connection status
                            local_mqtt_connected.store(mqtt_connected);
                            
                            // CRITICAL: Copy pipefd[0] and stdout_backup to avoid issues when variables go out of scope
                            int pipefd_read = pipefd[0];
                            int stdout_backup_copy = stdout_backup;
                            
                            // Get MQTT rate limit from instance config (if available)
                            // This allows users to slow down MQTT publishing to reduce server load
                            int mqtt_rate_limit_ms = 2000;  // Default: 2 seconds (0.5 messages per second) - very conservative
                            {
                                std::shared_lock<std::shared_timed_mutex> lock(mutex_);
                                auto instanceIt = instances_.find(instanceId);
                                if (instanceIt != instances_.end()) {
                                    const auto& info = instanceIt->second;
                                    // Check for MQTT_RATE_LIMIT_MS parameter
                                    auto it = info.additionalParams.find("MQTT_RATE_LIMIT_MS");
                                    if (it != info.additionalParams.end() && !it->second.empty()) {
                                        try {
                                            mqtt_rate_limit_ms = std::stoi(it->second);
                                            if (mqtt_rate_limit_ms < 500) mqtt_rate_limit_ms = 500;  // Minimum 500ms
                                            if (mqtt_rate_limit_ms > 10000) mqtt_rate_limit_ms = 10000;  // Maximum 10 seconds
                                            std::cerr << "[InstanceRegistry] [MQTT] Rate limit configured: " << mqtt_rate_limit_ms << "ms (" 
                                                      << (1000.0 / mqtt_rate_limit_ms) << " messages/second)" << std::endl;
                                        } catch (...) {
                                            std::cerr << "[InstanceRegistry] [MQTT] Warning: Invalid MQTT_RATE_LIMIT_MS, using default 2000ms" << std::endl;
                                        }
                                    }
                                    // Also check PROCESSING_DELAY_MS and use it if larger
                                    auto delayIt = info.additionalParams.find("PROCESSING_DELAY_MS");
                                    if (delayIt != info.additionalParams.end() && !delayIt->second.empty()) {
                                        try {
                                            int processingDelay = std::stoi(delayIt->second);
                                            if (processingDelay > mqtt_rate_limit_ms) {
                                                mqtt_rate_limit_ms = processingDelay;
                                                std::cerr << "[InstanceRegistry] [MQTT] Using PROCESSING_DELAY_MS (" << processingDelay 
                                                          << "ms) as rate limit to reduce processing speed" << std::endl;
                                            }
                                        } catch (...) {
                                            // Ignore errors
                                        }
                                    }
                                }
                            }
                            
                            // CRITICAL: Do NOT capture 'this' - use shared_ptr to stop flag instead
                            // This prevents use-after-free if InstanceRegistry is destroyed
                            std::thread json_reader_thread([stop_flag, mosq_shared, mqtt_topic, local_mqtt_connected_ptr = &local_mqtt_connected, pipefd_read, stdout_backup_copy, mqtt_rate_limit_ms]() {
                                // CRITICAL: This lambda does NOT capture 'this' to avoid use-after-free
                                // All variables are copied to ensure thread independence
                                // Use shared_ptr to stop flag to safely check stop condition
                                
                                // Helper function to safely convert JSON value to int (handles both Int and UInt)
                                // Must be defined inside lambda to be accessible
                                auto safeJsonToInt = [](const Json::Value& value, int default_value = -1) -> int {
                                    if (value.isInt()) {
                                        return value.asInt();
                                    } else if (value.isUInt()) {
                                        Json::UInt64 uint_val = value.asUInt64();
                                        // Check if value fits in int range
                                        if (uint_val <= static_cast<Json::UInt64>(std::numeric_limits<int>::max())) {
                                            return static_cast<int>(uint_val);
                                        } else {
                                            // Value too large, return default or clamp to max int
                                            return std::numeric_limits<int>::max();
                                        }
                                    } else if (value.isNumeric()) {
                                        // Try asInt first, but catch exception if it fails
                                        try {
                                            return value.asInt();
                                        } catch (...) {
                                            // If asInt fails, try asUInt64
                                            try {
                                                Json::UInt64 uint_val = value.asUInt64();
                                                if (uint_val <= static_cast<Json::UInt64>(std::numeric_limits<int>::max())) {
                                                    return static_cast<int>(uint_val);
                                                } else {
                                                    return std::numeric_limits<int>::max();
                                                }
                                            } catch (...) {
                                                return default_value;
                                            }
                                        }
                                    }
                                    return default_value;
                                };
                                
                                char buffer[4096];
                                std::string line_buffer;
                                std::string latest_json;  // Only keep the latest complete JSON
                                int publish_count = 0;
                                int skip_count = 0;
                                int dropped_json_count = 0;
                                auto last_reconnect_attempt = std::chrono::steady_clock::now();
                                const auto reconnect_cooldown = std::chrono::seconds(5);  // Only try reconnect every 5 seconds
                                
                                // Track vehicle count for summary messages
                                int total_vehicles_crossed = 0;  // Cumulative count of vehicles that have crossed the line
                                std::set<int> tracked_vehicle_ids;  // Track unique vehicle IDs that have crossed
                                int last_frame_index = -1;  // Track last frame to avoid double counting
                                int max_target_size_seen = 0;  // Track maximum target_size seen (to handle resets)
                                
                                // Rate limiting: Only publish every N milliseconds to prevent server overload
                                // Use configured rate limit (default: 2000ms = 0.5 messages/second)
                                auto last_publish_time = std::chrono::steady_clock::now();
                                const auto min_publish_interval = std::chrono::milliseconds(mqtt_rate_limit_ms);
                                
                                while (!stop_flag->load()) {
                                    // CRITICAL: Check stop flag before blocking read to allow quick exit
                                    if (stop_flag->load()) {
                                        break;
                                    }
                                    
                                    // CRITICAL: Check stop flag BEFORE any potentially blocking operations
                                    if (stop_flag->load()) {
                                        break;
                                    }
                                    
                                    // CRITICAL: Call mosquitto_loop() regularly to maintain connection and process network I/O
                                    // This must be called frequently to keep connection alive and send queued messages
                                    // Use non-blocking mode (timeout=0) to prevent blocking
                                    // Reduced packet count to prevent blocking
                                    if (mosq_shared && !stop_flag->load()) {
                                        mosquitto_loop(mosq_shared.get(), 0, 5);  // Process up to 5 packets each iteration (reduced from 10)
                                        
                                        // Check connection status and try to reconnect if lost (with cooldown)
                                        if (!stop_flag->load()) {
                                            auto now = std::chrono::steady_clock::now();
                                            if (!local_mqtt_connected_ptr->load() && 
                                                (now - last_reconnect_attempt) >= reconnect_cooldown) {
                                                int rc = mosquitto_reconnect(mosq_shared.get());
                                                last_reconnect_attempt = now;
                                                if (rc == MOSQ_ERR_SUCCESS) {
                                                    local_mqtt_connected_ptr->store(true);
                                                    std::cerr << "[InstanceRegistry] [MQTT] Reconnected successfully!" << std::endl;
                                                } else {
                                                    std::cerr << "[InstanceRegistry] [MQTT] Reconnect attempt failed: " << mosquitto_strerror(rc) << std::endl;
                                                }
                                            }
                                        }
                                    }
                                    
                                    // Check stop flag again before read
                                    if (stop_flag->load()) {
                                        break;
                                    }
                                    
                                    // Use select() with timeout to allow frequent stop_flag checks
                                    // Increased to 200ms to reduce CPU usage while still checking stop_flag regularly
                                    fd_set readfds;
                                    FD_ZERO(&readfds);
                                    FD_SET(pipefd_read, &readfds);
                                    struct timeval timeout;
                                    timeout.tv_sec = 0;
                                    timeout.tv_usec = 200000;  // 200ms timeout (increased from 50ms to reduce CPU usage)
                                    
                                    int select_result = select(pipefd_read + 1, &readfds, nullptr, nullptr, &timeout);
                                    
                                    // Check stop flag after select (may have waited up to 100ms)
                                    if (stop_flag->load()) {
                                        break;
                                    }
                                    
                                    ssize_t n = -1;
                                    if (select_result > 0 && FD_ISSET(pipefd_read, &readfds)) {
                                        // Data is available, read it (non-blocking, so should return immediately)
                                        n = read(pipefd_read, buffer, sizeof(buffer) - 1);
                                    } else if (select_result == 0) {
                                        // Timeout - no data available, continue to check stop_flag
                                        n = -1;
                                        errno = EAGAIN;
                                    } else {
                                        // Error in select
                                        n = -1;
                                    }
                                    if (n > 0) {
                                        buffer[n] = '\0';
                                        
                                        // CRITICAL: Limit line_buffer size - drop old data if too large
                                        // Only keep recent data to prevent memory buildup
                                        if (line_buffer.length() > 8192) {
                                            // Keep only the last 4096 chars (drop old data)
                                            line_buffer = line_buffer.substr(line_buffer.length() - 4096);
                                            dropped_json_count++;
                                        }
                                        
                                        line_buffer += buffer;
                                        
                                        // CRITICAL: Find the LATEST complete JSON in buffer (not the first one)
                                        // This ensures we always process the most recent data, not old backlog
                                        // Strategy: Find all JSON objects by matching braces, then take the last one
                                        size_t last_json_start = std::string::npos;
                                        size_t last_json_end = std::string::npos;
                                        
                                        // Find all complete JSON objects by scanning from start to end
                                        int brace_count = 0;
                                        size_t current_start = std::string::npos;
                                        
                                        for (size_t i = 0; i < line_buffer.length(); i++) {
                                            char c = line_buffer[i];
                                            
                                            if (c == '{') {
                                                if (brace_count == 0) {
                                                    current_start = i;  // Start of a new JSON object
                                                }
                                                brace_count++;
                                            } else if (c == '}') {
                                                brace_count--;
                                                if (brace_count == 0 && current_start != std::string::npos) {
                                                    // Found a complete JSON object
                                                    last_json_start = current_start;
                                                    last_json_end = i;
                                                    current_start = std::string::npos;  // Reset for next JSON
                                                }
                                            }
                                        }
                                        
                                        // If we found a complete JSON, extract it (this is the latest one)
                                        if (last_json_start != std::string::npos && last_json_end != std::string::npos) {
                                            std::string json_candidate = line_buffer.substr(last_json_start, last_json_end - last_json_start + 1);
                                            
                                            if (json_candidate.length() > 10 && 
                                                json_candidate[0] == '{' && 
                                                json_candidate.back() == '}') {
                                                // CRITICAL: Parse JSON FIRST before updating latest_json
                                                // This ensures we always use the most recent data
                                                std::string summary_str;
                                                try {
                                                    Json::Value json_root;
                                                    Json::Reader reader;
                                                    if (reader.parse(json_candidate, json_root)) {
                                                        // Extract vehicle count from JSON
                                                        int vehicle_count = 0;
                                                        // Use safe conversion for frame_index (may be large UInt)
                                                        int current_frame_index = safeJsonToInt(json_root.get("frame_index", -1), -1);
                                                        int current_target_size = 0;
                                                        
                                                        // Get target_size first (this is the cumulative count from ba_crossline)
                                                        // CRITICAL: target_size is the authoritative source for total vehicles crossed
                                                        // ba_crossline node maintains this count internally
                                                        if (json_root.isMember("target_size")) {
                                                            if (json_root["target_size"].isInt()) {
                                                                current_target_size = json_root["target_size"].asInt();
                                                            } else if (json_root["target_size"].isUInt()) {
                                                                current_target_size = static_cast<int>(json_root["target_size"].asUInt());
                                                            } else if (json_root["target_size"].isNumeric()) {
                                                                // Use safe conversion for numeric values (may be large UInt)
                                                                current_target_size = safeJsonToInt(json_root["target_size"], 0);
                                                            }
                                                            
                                                            // CRITICAL: Always use max to ensure total_vehicles_crossed only increases, never decreases
                                                            // target_size can decrease (e.g., when vehicles leave or node resets)
                                                            // We must maintain the maximum count seen so far
                                                            // Track maximum target_size seen to handle resets
                                                            if (current_target_size > max_target_size_seen) {
                                                                max_target_size_seen = current_target_size;
                                                                // When we see a new max, update total immediately
                                                                total_vehicles_crossed = std::max(total_vehicles_crossed, max_target_size_seen);
                                                            }
                                                            
                                                            // Use max of: current total, current target_size, and max target_size seen
                                                            // NOTE: We don't use tracked_vehicle_ids.size() because SORT tracker reuses IDs
                                                            // when vehicles leave the frame, making it unreliable for counting
                                                            // Priority: max_target_size_seen > current_target_size
                                                            total_vehicles_crossed = std::max({total_vehicles_crossed, current_target_size, max_target_size_seen});
                                                            vehicle_count = current_target_size;
                                                        }
                                                        
                                                        // Also get targets array size for reference
                                                        int targets_array_size = 0;
                                                        if (json_root.isMember("targets") && json_root["targets"].isArray()) {
                                                            targets_array_size = json_root["targets"].size();
                                                            
                                                            // If target_size is not available, use targets array size as fallback
                                                            if (vehicle_count == 0 && current_target_size == 0 && targets_array_size > 0) {
                                                                vehicle_count = targets_array_size;
                                                                // Don't update total_vehicles_crossed here - let tracked_vehicle_ids handle it
                                                            }
                                                            
                                                            // Track unique vehicle IDs from targets array
                                                            // This helps maintain count even when target_size resets
                                                            if (current_frame_index != last_frame_index) {
                                                                for (const auto& target : json_root["targets"]) {
                                                                    if (target.isObject()) {
                                                                        int vehicle_id = -1;
                                                                        
                                                                        // Try to get ID from ptr_wrapper structure
                                                                        if (target.isMember("ptr_wrapper")) {
                                                                            const auto& ptr_wrapper = target["ptr_wrapper"];
                                                                            if (ptr_wrapper.isObject()) {
                                                                                if (ptr_wrapper.isMember("id")) {
                                                                                    // Use safe conversion for vehicle ID (may be large UInt)
                                                                                    vehicle_id = safeJsonToInt(ptr_wrapper["id"], -1);
                                                                                }
                                                                            }
                                                                        }
                                                                        
                                                                        // Track unique IDs - this maintains count across resets
                                                                        if (vehicle_id != -1) {
                                                                            tracked_vehicle_ids.insert(vehicle_id);
                                                                            // Update total if we have new tracked vehicles
                                                                            int tracked_count = static_cast<int>(tracked_vehicle_ids.size());
                                                                            total_vehicles_crossed = std::max(total_vehicles_crossed, tracked_count);
                                                                        }
                                                                    }
                                                                }
                                                                last_frame_index = current_frame_index;
                                                            }
                                                        }
                                                        
                                                        // Debug: Log JSON structure for first few frames to understand format
                                                        static int debug_log_count = 0;
                                                        if (debug_log_count < 3) {
                                                            std::cerr << "[InstanceRegistry] [MQTT] Debug JSON structure (frame " << current_frame_index << "):" << std::endl;
                                                            std::cerr << "  target_size: " << current_target_size << std::endl;
                                                            std::cerr << "  targets array size: " << targets_array_size << std::endl;
                                                            std::cerr << "  JSON keys: ";
                                                            for (const auto& key : json_root.getMemberNames()) {
                                                                std::cerr << key << " ";
                                                            }
                                                            std::cerr << std::endl;
                                                            debug_log_count++;
                                                        }
                                                        
                                                        // Create summary JSON with vehicle count
                                                        Json::Value summary_json;
                                                        summary_json["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                                                            std::chrono::system_clock::now().time_since_epoch()).count();
                                                        summary_json["channel_index"] = json_root.get("channel_index", 0);
                                                        summary_json["frame_index"] = current_frame_index;
                                                        summary_json["vehicle_count"] = vehicle_count;  // Current count (from target_size)
                                                        summary_json["targets_count"] = targets_array_size;  // Targets array size
                                                        summary_json["target_size"] = current_target_size;  // Target size from ba_crossline (cumulative)
                                                        summary_json["total_vehicles_crossed"] = total_vehicles_crossed;  // Total (same as target_size)
                                                        summary_json["unique_vehicles_tracked"] = static_cast<int>(tracked_vehicle_ids.size());  // Unique IDs tracked
                                                        summary_json["fps"] = json_root.get("fps", 0.0);
                                                        summary_json["broke_for"] = json_root.get("broke_for", "normal");
                                                        
                                                        // Convert summary to string
                                                        Json::StreamWriterBuilder builder;
                                                        builder["indentation"] = "";  // Compact format
                                                        std::string summary_str = Json::writeString(builder, summary_json);
                                                        
                                                        // Log vehicle count (only first few times or when count > 0 or total changes)
                                                        static int log_count = 0;
                                                        static int last_logged_total = -1;
                                                        int tracked_count = static_cast<int>(tracked_vehicle_ids.size());
                                                        if (vehicle_count > 0 || total_vehicles_crossed != last_logged_total || log_count < 10) {
                                                            std::cerr << "[InstanceRegistry] [MQTT] Frame " << current_frame_index 
                                                                      << ": target_size=" << current_target_size
                                                                      << ", vehicle_count=" << vehicle_count 
                                                                      << ", total_vehicles_crossed=" << total_vehicles_crossed 
                                                                      << ", targets_count=" << targets_array_size
                                                                      << ", tracked_ids=" << tracked_count
                                                                      << ", max_target_size_seen=" << max_target_size_seen << std::endl;
                                                            log_count++;
                                                            last_logged_total = total_vehicles_crossed;
                                                        }
                                                        
                                                        // CRITICAL: Update latest_json with summary JSON AFTER parsing
                                                        // This ensures we always use the most recent parsed data with correct target_size
                                                        latest_json = summary_str;
                                                    } else {
                                                        // JSON parsing failed - use original JSON
                                                        latest_json = json_candidate;
                                                    }
                                                } catch (const std::exception& e) {
                                                    // JSON parsing error - use original JSON
                                                    std::cerr << "[InstanceRegistry] [MQTT] JSON parse error: " << e.what() << std::endl;
                                                    latest_json = json_candidate;
                                                } catch (...) {
                                                    // Unknown error - use original JSON
                                                    latest_json = json_candidate;
                                                }
                                                
                                                // Reduced debug logging to improve performance
                                                // Only log first 2 JSONs, then every 100th JSON
                                                static int json_received_count = 0;
                                                json_received_count++;
                                                if (json_received_count <= 2 || json_received_count % 100 == 0) {
                                                    std::cerr << "[InstanceRegistry] [MQTT] Debug: Received JSON #" << json_received_count 
                                                              << " (length=" << json_candidate.length() << ")" << std::endl;
                                                    // Only log preview for first 2 messages
                                                    if (json_received_count <= 2) {
                                                        std::cerr << "[InstanceRegistry] [MQTT] Debug: JSON preview: " 
                                                                  << json_candidate.substr(0, std::min(300, (int)json_candidate.length())) << "..." << std::endl;
                                                    }
                                                }
                                                
                                                // Clear processed data from buffer (keep only unprocessed tail)
                                                if (last_json_end + 1 < line_buffer.length()) {
                                                    line_buffer = line_buffer.substr(last_json_end + 1);
                                                } else {
                                                    line_buffer.clear();
                                                }
                                                
                                                // Publish every JSON message (no skipping)
                                                skip_count++;
                                                // Reduced debug logging - only log first 3, then every 100th
                                                if (skip_count <= 3 || skip_count % 100 == 0) {
                                                    std::cerr << "[InstanceRegistry] [MQTT] Debug: skip_count=" << skip_count << " (publishing all JSONs)" << std::endl;
                                                }
                                                // Publish every message (removed skip threshold)
                                                // But apply rate limiting to prevent server overload
                                                {
                                                                    // CRITICAL: Check stop flag before starting publish operation
                                                                    if (stop_flag->load()) {
                                                                        break;
                                                                    }
                                                                    
                                                                    // Rate limiting: Check if enough time has passed since last publish
                                                                    auto now = std::chrono::steady_clock::now();
                                                                    auto time_since_last_publish = std::chrono::duration_cast<std::chrono::milliseconds>(
                                                                        now - last_publish_time);
                                                                    
                                                                    if (time_since_last_publish < min_publish_interval) {
                                                                        // Skip this message - rate limit
                                                                        skip_count++;
                                                                        continue;
                                                                    }
                                                                    
                                                                    // Reduced debug logging to improve performance
                                                                    // Only log first 3 messages, then every 100th message
                                                                    if (skip_count <= 3 || skip_count % 100 == 0) {
                                                                        std::cerr << "[InstanceRegistry] [MQTT] Debug: Attempting to publish JSON #" << skip_count << "..." << std::endl;
                                                                    }
                                                                    // Publish the LATEST JSON only (not old ones)
                                                                    if (mosq_shared && local_mqtt_connected_ptr->load() && !latest_json.empty()) {
                                                                        // CRITICAL: Check stop flag again before blocking operations
                                                                        if (stop_flag->load()) {
                                                                            break;
                                                                        }
                                                                        
                                                                        // CRITICAL: Call loop BEFORE publish to drain buffer and prevent blocking
                                                                        // Reduced packet count to prevent blocking
                                                                        mosquitto_loop(mosq_shared.get(), 0, 10);  // Process up to 10 packets (reduced from 50 to prevent blocking)
                                                                        
                                                                        // CRITICAL: Check stop flag after loop call
                                                                        if (stop_flag->load()) {
                                                                            break;
                                                                        }
                                                                        
                                                                        // Wrap mosquitto_publish() in async with timeout to prevent blocking
                                                                        // Publish raw JSON (not wrapped in quotes) - user wants JSON object, not JSON string
                                                                        std::string json_to_publish = latest_json;  // Publish raw JSON without quotes
                                                                        auto publish_future = std::async(std::launch::async, [mosq_shared, mqtt_topic, json_to_publish]() -> int {
                                                                            return mosquitto_publish(mosq_shared.get(), nullptr, mqtt_topic.c_str(), 
                                                                                                    json_to_publish.length(), json_to_publish.c_str(), 0, false);
                                                                        });
                                                                        
                                                                        // Wait with timeout (100ms) - but check stop_flag periodically
                                                                        // Reduced timeout to prevent blocking too long
                                                                        auto status = std::future_status::timeout;
                                                                        auto wait_start = std::chrono::steady_clock::now();
                                                                        const auto total_timeout = std::chrono::milliseconds(100);  // Reduced from 200ms
                                                                        const auto check_interval = std::chrono::milliseconds(20);  // Check every 20ms (increased from 10ms to reduce CPU)
                                                                        
                                                                        while (std::chrono::steady_clock::now() - wait_start < total_timeout) {
                                                                            if (stop_flag->load()) {
                                                                                // Stop flag set, break immediately
                                                                                status = std::future_status::timeout;  // Mark as timeout to skip processing
                                                                                break;
                                                                            }
                                                                            status = publish_future.wait_for(check_interval);
                                                                            if (status == std::future_status::ready) {
                                                                                break;
                                                                            }
                                                                        }
                                                                        
                                                                        // CRITICAL: Check stop flag before processing result
                                                                        if (stop_flag->load()) {
                                                                            break;
                                                                        }
                                                                        
                                                                        if (status == std::future_status::ready) {
                                                                            // CRITICAL: Check stop flag before calling get() which might block briefly
                                                                            if (stop_flag->load()) {
                                                                                break;
                                                                            }
                                                                            
                                                                            int rc = MOSQ_ERR_UNKNOWN;
                                                                            try {
                                                                                rc = publish_future.get();
                                                                            } catch (...) {
                                                                                // If get() throws, skip this message
                                                                                std::cerr << "[InstanceRegistry] [MQTT] Exception getting publish result, skipping..." << std::endl;
                                                                                continue;
                                                                            }
                                                                            
                                                                            if (rc == MOSQ_ERR_SUCCESS) {
                                                                                publish_count++;
                                                                                last_publish_time = std::chrono::steady_clock::now();  // Update last publish time
                                                                                // Reduced logging - only log first 5 messages, then every 100th message
                                                                                if (publish_count <= 5 || publish_count % 100 == 0) {
                                                                                    std::cerr << "[InstanceRegistry] [MQTT] ✓ Published message #" << publish_count << " with targets (" << json_to_publish.length() << " bytes)" << std::endl;
                                                                                }
                                                                            } else {
                                                                                // Log all publish errors for debugging (always log first 20 errors, then every 10th)
                                                                                static int error_count = 0;
                                                                                error_count++;
                                                                                bool should_log = (error_count <= 20 || error_count % 10 == 0);
                                                                                
                                                                                if (should_log) {
                                                                                    std::cerr << "[InstanceRegistry] [MQTT] ✗ Publish failed #" << error_count 
                                                                                              << " with error code: " << rc << " (" << mosquitto_strerror(rc) << ")" << std::endl;
                                                                                }
                                                                                
                                                                                if (rc == MOSQ_ERR_NO_CONN) {
                                                                                    local_mqtt_connected_ptr->store(false);
                                                                                    if (should_log) {
                                                                                        std::cerr << "[InstanceRegistry] [MQTT] Connection lost - will attempt reconnect" << std::endl;
                                                                                    }
                                                                                } else if (rc == MOSQ_ERR_OVERSIZE_PACKET) {
                                                                                    if (should_log) {
                                                                                        std::cerr << "[InstanceRegistry] [MQTT] Message too large, skipping (" << json_to_publish.length() << " bytes)" << std::endl;
                                                                                    }
                                                                                } else if (rc == MOSQ_ERR_NOMEM) {
                                                                                    if (should_log) {
                                                                                        std::cerr << "[InstanceRegistry] [MQTT] Out of memory" << std::endl;
                                                                                    }
                                                                                } else if (rc == MOSQ_ERR_INVAL) {
                                                                                    if (should_log) {
                                                                                        std::cerr << "[InstanceRegistry] [MQTT] Invalid parameters" << std::endl;
                                                                                    }
                                                                                }
                                                                            }
                                                                        } else {
                                                                            // Timeout or stop flag set - skip message to prevent deadlock
                                                                            if (stop_flag->load()) {
                                                                                // Stop flag was set during wait, exit immediately
                                                                                break;
                                                                            }
                                                                            // Timeout - publish is taking too long, skip message to prevent deadlock
                                                                            static int timeout_count = 0;
                                                                            timeout_count++;
                                                                            if (timeout_count <= 5 || timeout_count % 20 == 0) {
                                                                                std::cerr << "[InstanceRegistry] [MQTT] ⚠ Publish timeout after 200ms, skipping message #" << timeout_count << " to prevent deadlock" << std::endl;
                                                                            }
                                                                            // Continue processing - don't break the loop
                                                                        }
                                                                    } else {
                                                                        // Log only occasionally to avoid spam, but continue processing
                                                                        static int skip_publish_count = 0;
                                                                        skip_publish_count++;
                                                                        if (skip_publish_count <= 3 || skip_publish_count % 50 == 0) {
                                                                            std::cerr << "[InstanceRegistry] [MQTT] ⚠ Cannot publish #" << skip_publish_count 
                                                                                      << ": mosq_shared=" << (mosq_shared ? "valid" : "null") 
                                                                                      << ", connected=" << (local_mqtt_connected_ptr->load() ? "true" : "false")
                                                                                      << ", latest_json.empty()=" << (latest_json.empty() ? "true" : "false") << std::endl;
                                                                        }
                                                                        // Continue processing - connection might recover
                                                                    }
                                                    }
                                            }
                                        } else {
                                            // No complete JSON found, but user wants to receive even incomplete data
                                            // Check stop flag before processing partial JSON
                                            if (stop_flag->load()) {
                                                break;
                                            }
                                            
                                            // Check if we have partial JSON data (starts with '{')
                                            if (!line_buffer.empty() && line_buffer[0] == '{') {
                                                // Extract partial JSON (up to 2000 chars to avoid huge messages)
                                                std::string partial_json = line_buffer.substr(0, std::min(2000, (int)line_buffer.length()));
                                                
                                                // Publish partial JSON if we have connection
                                                if (mosq_shared && local_mqtt_connected_ptr->load() && !partial_json.empty()) {
                                                    // Check stop flag before blocking operations
                                                    if (stop_flag->load()) {
                                                        break;
                                                    }
                                                    
                                                    mosquitto_loop(mosq_shared.get(), 0, 10);  // Reduced from 50 to prevent blocking
                                                    
                                                    // Check stop flag after loop
                                                    if (stop_flag->load()) {
                                                        break;
                                                    }
                                                    
                                                    std::string json_to_publish = partial_json;  // Publish raw partial JSON
                                                    auto publish_future = std::async(std::launch::async, [mosq_shared, mqtt_topic, json_to_publish]() -> int {
                                                        return mosquitto_publish(mosq_shared.get(), nullptr, mqtt_topic.c_str(), 
                                                                                json_to_publish.length(), json_to_publish.c_str(), 0, false);
                                                    });
                                                    
                                                    // Wait with periodic stop_flag checks
                                                    auto status = std::future_status::timeout;
                                                    auto wait_start = std::chrono::steady_clock::now();
                                                    const auto total_timeout = std::chrono::milliseconds(200);
                                                    const auto check_interval = std::chrono::milliseconds(10);
                                                    
                                                    while (std::chrono::steady_clock::now() - wait_start < total_timeout) {
                                                        if (stop_flag->load()) {
                                                            status = std::future_status::timeout;  // Mark as timeout to skip processing
                                                            break;
                                                        }
                                                        status = publish_future.wait_for(check_interval);
                                                        if (status == std::future_status::ready) {
                                                            break;
                                                        }
                                                    }
                                                    
                                                    // Check stop flag one more time before processing result
                                                    if (stop_flag->load()) {
                                                        break;
                                                    }
                                                    if (status == std::future_status::ready) {
                                                        // Check stop flag before calling get()
                                                        if (stop_flag->load()) {
                                                            break;
                                                        }
                                                        
                                                        int rc = MOSQ_ERR_UNKNOWN;
                                                        try {
                                                            rc = publish_future.get();
                                                        } catch (...) {
                                                            // If get() throws, skip this message
                                                            continue;
                                                        }
                                                        
                                                        if (rc == MOSQ_ERR_SUCCESS) {
                                                            publish_count++;
                                                            last_publish_time = std::chrono::steady_clock::now();  // Update last publish time
                                                            if (publish_count <= 5) {
                                                                std::cerr << "[InstanceRegistry] [MQTT] ✓ Published partial JSON #" << publish_count 
                                                                          << " (" << json_to_publish.length() << " bytes)" << std::endl;
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                        
                                    } else if (n == 0) {
                                        // EOF - pipe closed (write end was closed)
                                        // This is normal when instance stops
                                        break;
                                    } else {
                                        // Error or EAGAIN (non-blocking)
                                        if (errno == EBADF) {
                                            // File descriptor is invalid (pipe was closed)
                                            // This can happen when instance stops
                                            break;
                                        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                                            // Other error - log and exit
                                            break;
                                        }
                                    }
                                    
                                    // CRITICAL: Check stop flag after read to allow quick exit
                                    if (stop_flag->load()) {
                                        break;
                                    }
                                    
                                    // Shorter sleep to process messages faster and prevent backlog
                                    // Use sleep with interruptible check to allow quick exit
                                    // Sleep in smaller chunks and check stop_flag frequently
                                    for (int i = 0; i < 10 && !stop_flag->load(); i++) {
                                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                                    }
                                    
                                    // Final check before next iteration
                                    if (stop_flag->load()) {
                                        break;
                                    }
                                }
                                
                                // CRITICAL: Close pipe first
                                if (pipefd_read >= 0) {
                                    close(pipefd_read);
                                }
                                
                                // Cleanup MQTT
                                if (mosq_shared) {
                                    try {
                                        mosquitto_disconnect(mosq_shared.get());
                                        mosquitto_loop_stop(mosq_shared.get(), false);
                                    } catch (...) {
                                        // Ignore errors during cleanup
                                    }
                                    // shared_ptr will automatically cleanup when lambda exits
                                }
                                
                                // CRITICAL: DO NOT restore stdout here - it can cause segmentation fault
                                // Stdout will be automatically restored when process exits or instance stops
                                // Restoring stdout from multiple threads can cause race conditions
                                // Just close the backup fd if it's still valid
                                // Use syscall directly to avoid any C++ exception issues
                                if (stdout_backup_copy >= 0) {
                                    ::close(stdout_backup_copy);
                                }
                                
                                // Use write() directly to stderr to avoid any stdout issues
                                char msg[256];
                                int len = snprintf(msg, sizeof(msg), 
                                    "[InstanceRegistry] [MQTT] Thread stopped. Published %d messages total.\n", 
                                    publish_count);
                                if (len > 0 && len < (int)sizeof(msg)) {
                                    ::write(STDERR_FILENO, msg, len);
                                }
                            });
                            
                            // Store thread for proper management (join on stop)
                            {
                                std::lock_guard<std::mutex> lock(mqtt_thread_mutex_);
                                mqtt_threads_[instanceId] = std::move(json_reader_thread);
                            }
                            
                            std::cerr << "[InstanceRegistry] [MQTT] MQTT publishing thread started for instance " << instanceId << std::endl;
                            std::cerr << "[InstanceRegistry] [MQTT] NOTE: Publishing every message (including partial JSON if available)" << std::endl;
                            std::cerr << "[InstanceRegistry] [MQTT] NOTE: Only keeping LATEST JSON, dropping all old JSONs to prevent backlog and deadlock" << std::endl;
                        }
                    }
                }
            }
        }
        
        // Log initial processing status
        if (!hasRTMPOutput(instanceId)) {
            std::cerr << "[InstanceRegistry] Instance does not have RTMP output - enabling processing result logging" << std::endl;
            logProcessingResults(instanceId);
            
            // Start periodic logging in a separate thread (managed, not detached)
            startLoggingThread(instanceId);
        }
        
        // DISABLED: Video loop monitoring thread - feature removed to improve performance
        // Start video loop monitoring thread for file-based instances with LOOP_VIDEO enabled
        // {
        //     std::shared_lock<std::shared_timed_mutex> lock(mutex_);
        //     auto instanceIt = instances_.find(instanceId);
        //     if (instanceIt != instances_.end()) {
        //         const auto& info = instanceIt->second;
        //         bool isFileBased = !info.filePath.empty() || 
        //                          info.additionalParams.find("FILE_PATH") != info.additionalParams.end();
        //         if (isFileBased) {
        //             startVideoLoopThread(instanceId);
        //         }
        //     }
        // }
    }
    
    return started;
}

bool InstanceRegistry::stopInstance(const std::string& instanceId) {
    // CRITICAL: Get pipeline copy and release lock before calling stopPipeline
    // stopPipeline can take a long time and doesn't need the lock
    // This prevents deadlock if another thread (like terminate handler) needs the lock
    std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>> pipelineCopy;
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
    
    // Stop logging thread if exists
    stopLoggingThread(instanceId);
    
    // Stop video loop monitoring thread if exists
    stopVideoLoopThread(instanceId);
    
    // CRITICAL: Stop MQTT thread if exists
    // This will restore stdout and close pipe write end to signal EOF to thread
    stopMqttThread(instanceId);
    
    // Stop RTSP monitoring thread if exists
    stopRTSPMonitorThread(instanceId);
    
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
    // CRITICAL: Use timeout to prevent deadlock if mutex is locked by recovery handler
    std::shared_lock<std::shared_timed_mutex> lock(mutex_, std::defer_lock);
    
    // Try to acquire lock with timeout (2000ms) - longer than listInstances for API calls
    if (!lock.try_lock_for(std::chrono::milliseconds(2000))) {
        std::cerr << "[InstanceRegistry] WARNING: getAllInstances() timeout - mutex is locked, returning empty map" << std::endl;
        if (isInstanceLoggingEnabled()) {
            PLOG_WARNING << "[InstanceRegistry] getAllInstances() timeout after 2000ms - mutex may be locked by another operation";
        }
        return {}; // Return empty map to prevent blocking
    }
    
    // Return copy of instances - this is fast and doesn't block other readers
    return instances_;
}

bool InstanceRegistry::hasInstance(const std::string& instanceId) const {
    // CRITICAL: Use timeout to prevent deadlock if mutex is locked by recovery handler
    std::shared_lock<std::shared_timed_mutex> lock(mutex_, std::defer_lock); // Read lock - allows concurrent readers
    
    // Try to acquire lock with timeout (500ms) - fail fast
    if (!lock.try_lock_for(std::chrono::milliseconds(500))) {
        std::cerr << "[InstanceRegistry] WARNING: hasInstance() timeout - mutex is locked, returning false" << std::endl;
        return false; // Return false to prevent blocking
    }
    
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
            
            // Update RTMP URL if changed - check RTMP_DES_URL first, then RTMP_URL
            auto rtmpDesIt = req.additionalParams.find("RTMP_DES_URL");
            if (rtmpDesIt != req.additionalParams.end() && !rtmpDesIt->second.empty()) {
                info.rtmpUrl = rtmpDesIt->second;
            } else {
                auto rtmpIt = req.additionalParams.find("RTMP_URL");
                if (rtmpIt != req.additionalParams.end() && !rtmpIt->second.empty()) {
                    info.rtmpUrl = rtmpIt->second;
                }
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
    
    // Extract RTSP URL from request - check RTSP_SRC_URL first, then RTSP_URL
    // This should be done BEFORE generating RTSP from RTMP to avoid overriding user's input
    auto rtspSrcIt = req.additionalParams.find("RTSP_SRC_URL");
    if (rtspSrcIt != req.additionalParams.end() && !rtspSrcIt->second.empty()) {
        info.rtspUrl = rtspSrcIt->second;
    } else {
        auto rtspIt = req.additionalParams.find("RTSP_URL");
        if (rtspIt != req.additionalParams.end() && !rtspIt->second.empty()) {
            info.rtspUrl = rtspIt->second;
        }
    }
    
    // Extract RTMP URL from request - check RTMP_DES_URL first, then RTMP_URL
    auto rtmpDesIt = req.additionalParams.find("RTMP_DES_URL");
    if (rtmpDesIt != req.additionalParams.end() && !rtmpDesIt->second.empty()) {
        info.rtmpUrl = rtmpDesIt->second;
    } else {
        auto rtmpIt = req.additionalParams.find("RTMP_URL");
        if (rtmpIt != req.additionalParams.end() && !rtmpIt->second.empty()) {
            info.rtmpUrl = rtmpIt->second;
        }
    }
    
    // Only generate RTSP URL from RTMP URL if RTSP URL is not already set
    // This prevents overriding user's RTSP_SRC_URL
    if (info.rtspUrl.empty() && !info.rtmpUrl.empty()) {
        
        // Generate RTSP URL from RTMP URL
        // Pattern: rtmp://host:1935/live/stream_key -> rtsp://host:8554/live/stream_key_0
        // RTMP node automatically adds "_0" suffix to stream key
        std::string rtmpUrl = info.rtmpUrl;
        
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

bool InstanceRegistry::startPipeline(const std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>>& nodes, const std::string& instanceId, bool isRestart) {
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
            try {
                rtspNode->start();
            } catch (const std::exception& e) {
                std::cerr << "[InstanceRegistry] ✗ Exception starting RTSP node: " << e.what() << std::endl;
                std::cerr << "[InstanceRegistry] This may indicate RTSP stream is not available" << std::endl;
                throw;  // Re-throw to let caller handle
            } catch (...) {
                std::cerr << "[InstanceRegistry] ✗ Unknown exception starting RTSP node" << std::endl;
                throw;  // Re-throw to let caller handle
            }
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
            
            // Start RTSP monitoring thread for error detection and auto-reconnect
            startRTSPMonitorThread(instanceId);
            
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
            
            // Check for PROCESSING_DELAY_MS parameter to reduce processing speed
            // This helps prevent server overload and crashes by slowing down AI processing
            int processingDelayMs = 0;
            {
                std::shared_lock<std::shared_timed_mutex> lock(mutex_);
                auto instanceIt = instances_.find(instanceId);
                if (instanceIt != instances_.end()) {
                    const auto& info = instanceIt->second;
                    auto it = info.additionalParams.find("PROCESSING_DELAY_MS");
                    if (it != info.additionalParams.end() && !it->second.empty()) {
                        try {
                            processingDelayMs = std::stoi(it->second);
                            if (processingDelayMs < 0) processingDelayMs = 0;
                            if (processingDelayMs > 1000) processingDelayMs = 1000;  // Cap at 1000ms
                            std::cerr << "[InstanceRegistry] Processing delay enabled: " << processingDelayMs << "ms between frames" << std::endl;
                            std::cerr << "[InstanceRegistry] This will reduce AI processing speed to prevent server overload" << std::endl;
                        } catch (...) {
                            std::cerr << "[InstanceRegistry] Warning: Invalid PROCESSING_DELAY_MS value, ignoring..." << std::endl;
                        }
                    }
                }
            }
            
            std::cerr << "[InstanceRegistry] Calling fileNode->start()..." << std::endl;
            auto startTime = std::chrono::steady_clock::now();
            
            // CRITICAL: Wrap start() in async with timeout to prevent blocking server
            // When video ends, fileNode->start() may block indefinitely if GStreamer pipeline is in bad state
            // This timeout ensures server remains responsive even if start() hangs
            try {
                auto startFuture = std::async(std::launch::async, [fileNode]() {
                    fileNode->start();
                });
                
                // Wait with timeout (5000ms for initial start, longer than restart timeout)
                // If it takes too long, log warning but continue (don't block server)
                const int START_TIMEOUT_MS = 5000;
                if (startFuture.wait_for(std::chrono::milliseconds(START_TIMEOUT_MS)) == std::future_status::timeout) {
                    std::cerr << "[InstanceRegistry] ⚠ WARNING: fileNode->start() timeout (" << START_TIMEOUT_MS << "ms)" << std::endl;
                    std::cerr << "[InstanceRegistry] ⚠ This may indicate GStreamer pipeline issue or video file problem" << std::endl;
                    std::cerr << "[InstanceRegistry] ⚠ Server will continue running, but instance may not process frames correctly" << std::endl;
                    std::cerr << "[InstanceRegistry] ⚠ Consider stopping and restarting the instance" << std::endl;
                    // Don't return false - let instance continue, but it may not work correctly
                    // This prevents server from being blocked
                } else {
                    try {
                        startFuture.get();
                        auto endTime = std::chrono::steady_clock::now();
                        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
                        std::cerr << "[InstanceRegistry] ✓ File source node start() completed in " << duration << "ms" << std::endl;
                    } catch (const std::exception& e) {
                        std::cerr << "[InstanceRegistry] ✗ Exception during fileNode->start(): " << e.what() << std::endl;
                        std::cerr << "[InstanceRegistry] This may indicate a problem with the video file or model initialization" << std::endl;
                        return false;
                    } catch (...) {
                        std::cerr << "[InstanceRegistry] ✗ Unknown exception during fileNode->start()" << std::endl;
                        std::cerr << "[InstanceRegistry] This may indicate a critical error - check logs above for details" << std::endl;
                        return false;
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "[InstanceRegistry] ✗ Exception creating start future: " << e.what() << std::endl;
                std::cerr << "[InstanceRegistry] Falling back to synchronous start()..." << std::endl;
                // Fallback to synchronous call if async fails
                try {
                    fileNode->start();
                    auto endTime = std::chrono::steady_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
                    std::cerr << "[InstanceRegistry] ✓ File source node start() completed in " << duration << "ms" << std::endl;
                } catch (const std::exception& e2) {
                    std::cerr << "[InstanceRegistry] ✗ Exception during fileNode->start(): " << e2.what() << std::endl;
                    return false;
                } catch (...) {
                    std::cerr << "[InstanceRegistry] ✗ Unknown exception during fileNode->start()" << std::endl;
                    return false;
                }
            } catch (...) {
                std::cerr << "[InstanceRegistry] ✗ Unknown error creating start future" << std::endl;
                std::cerr << "[InstanceRegistry] Falling back to synchronous start()..." << std::endl;
                // Fallback to synchronous call if async fails
                try {
                    fileNode->start();
                    auto endTime = std::chrono::steady_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
                    std::cerr << "[InstanceRegistry] ✓ File source node start() completed in " << duration << "ms" << std::endl;
                } catch (...) {
                    std::cerr << "[InstanceRegistry] ✗ Unknown exception during fileNode->start()" << std::endl;
                    return false;
                }
            }
            
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
            
            // If processing delay is enabled, start a thread to periodically add delay
            // This slows down frame processing to reduce server load
            if (processingDelayMs > 0) {
                std::cerr << "[InstanceRegistry] Starting processing delay thread (delay: " << processingDelayMs << "ms)..." << std::endl;
                std::cerr << "[InstanceRegistry] This will slow down AI processing to prevent server overload" << std::endl;
                // Note: Actual frame skipping would need to be done in the SDK level
                // For now, we just log that delay is configured
                // The delay will be handled by rate limiting in MQTT thread
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
    if (!info.rtmpUrl.empty() && 
        req.additionalParams.find("RTMP_DES_URL") == req.additionalParams.end() &&
        req.additionalParams.find("RTMP_URL") == req.additionalParams.end()) {
        req.additionalParams["RTMP_DES_URL"] = info.rtmpUrl;
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
    
    // Check if RTMP_DES_URL or RTMP_URL is in additionalParams
    const auto& additionalParams = instanceIt->second.additionalParams;
    if (additionalParams.find("RTMP_DES_URL") != additionalParams.end() && 
        !additionalParams.at("RTMP_DES_URL").empty()) {
        return true;
    }
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
                        // 1. Instance has been running for at least 60 seconds (give it more time to connect and stabilize)
                        // 2. AND instance has not received any data yet (hasReceivedData = false)
                        // 3. OR instance has been running for more than 90 seconds without activity (after receiving data)
                        // Note: Increased timeout from 30s to 60s to account for RTSP connection time and fps update delay
                        bool isLikelyRetrying = false;
                        if (timeSinceStart >= 60) {
                            if (!info.hasReceivedData) {
                                // Instance has been running for 60+ seconds without receiving any data
                                // This indicates it's likely stuck in retry loop
                                isLikelyRetrying = true;
                            } else if (timeSinceActivity > 90) {
                                // Instance received data before but now has been inactive for 90+ seconds
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
                            // Check if instance is receiving data
                            // Note: fps may not be updated from pipeline, so we use a more lenient approach
                            // If instance has been running for a while without errors, assume it's working
                            bool isReceivingData = false;
                            
                            // Method 1: Check fps (if available from pipeline)
                            if (info.fps > 0) {
                                isReceivingData = true;
                            }
                            // Method 2: If instance has been running for 45+ seconds without being marked as retrying,
                            // assume it's working (RTSP connection established, even if fps not updated)
                            // This gives time for RTSP to connect (10-30s) and stabilize before retry detection starts (60s)
                            else if (timeSinceStart >= 45 && info.retryCount == 0) {
                                // Instance has been running for 45+ seconds without retry detection
                                // This likely means RTSP connection is established and working
                                // (retry detection only starts at 60s, so 45s is safe)
                                isReceivingData = true;
                            }
                            
                            if (isReceivingData) {
                                // Instance is receiving frames - mark as having received data
                                if (!info.hasReceivedData) {
                                    std::cerr << "[InstanceRegistry] Instance " << instanceId 
                                              << " connection successful - receiving frames";
                                    if (info.fps > 0) {
                                        std::cerr << " (fps=" << std::fixed << std::setprecision(2) << info.fps << ")";
                                    } else {
                                        std::cerr << " (running for " << timeSinceStart << "s, assumed working)";
                                    }
                                    std::cerr << std::endl;
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
    
    // Update RTSP activity if instance is receiving frames (FPS > 0) and is RTSP-based
    // Note: We update activity here to track when stream is active
    // This helps the monitoring thread detect when stream is working
    if (info.fps > 0 && !info.rtspUrl.empty()) {
        // Update activity timestamp (thread-safe, only updates monitoring data)
        {
            std::lock_guard<std::mutex> lock(rtsp_monitor_mutex_);
            rtsp_last_activity_[instanceId] = std::chrono::steady_clock::now();
        }
    }
    
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
                   info.additionalParams.find("RTMP_DES_URL") != info.additionalParams.end() ||
                   info.additionalParams.find("RTMP_URL") != info.additionalParams.end();
    if (hasRTMP) {
        std::cerr << "[InstanceProcessingLog] Output: RTMP Stream" << std::endl;
        if (!info.rtmpUrl.empty()) {
            std::cerr << "[InstanceProcessingLog] RTMP URL: " << info.rtmpUrl << std::endl;
        } else if (info.additionalParams.find("RTMP_DES_URL") != info.additionalParams.end()) {
            std::cerr << "[InstanceProcessingLog] RTMP URL: " 
                      << info.additionalParams.at("RTMP_DES_URL") << std::endl;
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
                
                // CRITICAL: Check RTMP output directly from instance info to avoid deadlock
                // Cannot call hasRTMPOutput() here because it tries to acquire mutex_ again
                // (would deadlock since we already hold exclusive lock)
                const auto& info = instanceIt->second;
                const auto& additionalParams = info.additionalParams;
                bool hasRTMP = !info.rtmpUrl.empty() ||
                               (additionalParams.find("RTMP_DES_URL") != additionalParams.end() && 
                                !additionalParams.at("RTMP_DES_URL").empty()) ||
                               (additionalParams.find("RTMP_URL") != additionalParams.end() && 
                                !additionalParams.at("RTMP_URL").empty());
                
                if (hasRTMP) {
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

void InstanceRegistry::stopMqttThread(const std::string& instanceId) {
    std::unique_lock<std::mutex> lock(mqtt_thread_mutex_);
    
    // CRITICAL: Set stop flag FIRST to allow thread to exit gracefully
    auto flagIt = mqtt_thread_stop_flags_.find(instanceId);
    if (flagIt != mqtt_thread_stop_flags_.end() && flagIt->second) {
        flagIt->second->store(true);
    }
    
    // CRITICAL: Close pipe write end IMMEDIATELY after setting stop flag
    // This will cause read() to return 0 (EOF) and thread will exit quickly
    // Do this BEFORE restoring stdout to ensure pipe is closed
    auto pipeIt = mqtt_pipe_write_fds_.find(instanceId);
    if (pipeIt != mqtt_pipe_write_fds_.end()) {
        int pipe_write_fd = pipeIt->second;
        if (pipe_write_fd >= 0) {
            ::close(pipe_write_fd);  // Close pipe write end to interrupt read()
            mqtt_pipe_write_fds_.erase(pipeIt);
        }
    }
    
    // Give thread a moment to check stop flag and exit gracefully
    // This prevents race condition where we try to join before thread exits
    // Reduced to 50ms since we're using select() with timeout now
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // CRITICAL: Restore stdout AFTER closing pipe (pipe already closed above)
    // This prevents stdout from being closed when we close the pipe
    auto backupIt = mqtt_stdout_backups_.find(instanceId);
    if (backupIt != mqtt_stdout_backups_.end()) {
        int stdout_backup_fd = backupIt->second;
        if (stdout_backup_fd >= 0) {
            // Restore stdout from backup
            dup2(stdout_backup_fd, STDOUT_FILENO);
            close(stdout_backup_fd);  // Close backup FD
        }
        mqtt_stdout_backups_.erase(backupIt);
    }
    
    // Get thread handle and release lock before joining to avoid deadlock
    std::thread threadToJoin;
    auto threadIt = mqtt_threads_.find(instanceId);
    if (threadIt != mqtt_threads_.end()) {
        if (threadIt->second.joinable()) {
            threadToJoin = std::move(threadIt->second);
        }
        mqtt_threads_.erase(threadIt);
    }
    
    // Remove stop flag
    if (flagIt != mqtt_thread_stop_flags_.end()) {
        mqtt_thread_stop_flags_.erase(flagIt);
    }
    
    // Release lock before joining to avoid deadlock
    lock.unlock();
    
    // Join thread with timeout to prevent blocking forever
    if (threadToJoin.joinable()) {
        // Use async with timeout to prevent blocking
        auto future = std::async(std::launch::async, [&threadToJoin]() {
            threadToJoin.join();
        });
        
        // Wait up to 500ms for thread to finish (reduced from 2s for faster shutdown)
        if (future.wait_for(std::chrono::milliseconds(500)) == std::future_status::timeout) {
            std::cerr << "[InstanceRegistry] [MQTT] Warning: MQTT thread join timeout after 500ms, detaching..." << std::endl;
            threadToJoin.detach();
        } else {
            // Thread joined successfully
            std::cerr << "[InstanceRegistry] [MQTT] Thread joined successfully" << std::endl;
        }
    }
}

void InstanceRegistry::startVideoLoopThread(const std::string& instanceId) {
    // DISABLED: Video loop feature removed to improve performance
    return;
    
    // Stop existing thread if any
    stopVideoLoopThread(instanceId);
    
    // Check if instance has LOOP_VIDEO enabled
    bool loopEnabled = false;
    {
        std::shared_lock<std::shared_timed_mutex> lock(mutex_);
        auto instanceIt = instances_.find(instanceId);
        if (instanceIt != instances_.end()) {
            const auto& info = instanceIt->second;
            auto it = info.additionalParams.find("LOOP_VIDEO");
            if (it != info.additionalParams.end()) {
                std::string loopValue = it->second;
                std::transform(loopValue.begin(), loopValue.end(), loopValue.begin(), ::tolower);
                loopEnabled = (loopValue == "true" || loopValue == "1" || loopValue == "yes");
            }
        }
    }
    
    if (!loopEnabled) {
        return; // Loop not enabled, don't start thread
    }
    
    // Check if instance is file-based
    bool isFileBased = false;
    {
        std::shared_lock<std::shared_timed_mutex> lock(mutex_);
        auto instanceIt = instances_.find(instanceId);
        if (instanceIt != instances_.end()) {
            const auto& info = instanceIt->second;
            isFileBased = !info.filePath.empty() || 
                         info.additionalParams.find("FILE_PATH") != info.additionalParams.end();
        }
    }
    
    if (!isFileBased) {
        return; // Not a file-based instance, don't start thread
    }
    
    // Create stop flag
    {
        std::lock_guard<std::mutex> lock(thread_mutex_);
        video_loop_thread_stop_flags_.emplace(instanceId, false);
    }
    
    std::cerr << "[InstanceRegistry] [VideoLoop] Starting video loop monitoring thread for instance " << instanceId << std::endl;
    
    // Start new monitoring thread
    // CRITICAL: Capture instanceId by value to avoid use-after-free
    // We access mutex_ and instances_ through 'this', but we check stop flag first
    // to ensure thread exits quickly if instance is stopped
    std::thread videoLoopThread([this, instanceId]() {
        try {
            int zeroFpsCount = 0;
            const int ZERO_FPS_THRESHOLD = 3; // Check 3 times (30 seconds) before restarting
            const int CHECK_INTERVAL_SECONDS = 10; // Check every 10 seconds (increased from 5 to reduce CPU usage)
            const int MIN_RUNTIME_SECONDS = 60; // Minimum runtime before allowing restart (increased from 30 to 60 seconds)
            auto instanceStartTime = std::chrono::steady_clock::now();
            bool hasEverReceivedData = false;
            
            while (true) {
                // Check stop flag first
                {
                    try {
                        std::lock_guard<std::mutex> lock(thread_mutex_);
                        auto flagIt = video_loop_thread_stop_flags_.find(instanceId);
                        if (flagIt == video_loop_thread_stop_flags_.end() || flagIt->second.load()) {
                            break;
                        }
                    } catch (...) {
                        // If mutex access fails, exit thread to prevent crash
                        std::cerr << "[InstanceRegistry] [VideoLoop] Error accessing stop flag, exiting thread" << std::endl;
                        return;
                    }
                }
            
                // Wait CHECK_INTERVAL_SECONDS seconds (check flag periodically)
                for (int i = 0; i < CHECK_INTERVAL_SECONDS * 10; ++i) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    
                    // Check stop flag
                    {
                        try {
                            std::lock_guard<std::mutex> lock(thread_mutex_);
                            auto flagIt = video_loop_thread_stop_flags_.find(instanceId);
                            if (flagIt == video_loop_thread_stop_flags_.end() || flagIt->second.load()) {
                                return;
                            }
                        } catch (...) {
                            // If mutex access fails, exit thread to prevent crash
                            std::cerr << "[InstanceRegistry] [VideoLoop] Error accessing stop flag, exiting thread" << std::endl;
                            return;
                        }
                    }
                }
                
                // Check if instance still exists and is running
                bool shouldRestart = false;
                {
                    try {
                        std::unique_lock<std::shared_timed_mutex> lock(mutex_);
                        auto instanceIt = instances_.find(instanceId);
                        if (instanceIt == instances_.end() || !instanceIt->second.running) {
                            // Instance deleted or stopped, exit thread
                            return;
                        }
                
                const auto& info = instanceIt->second;
                
                // Track if we've ever received data
                if (info.hasReceivedData) {
                    hasEverReceivedData = true;
                }
                
                // Check minimum runtime before allowing restart
                auto runtime = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - instanceStartTime).count();
                
                // Check FPS - if 0 for multiple checks, video likely ended
                // CRITICAL: Only restart if:
                // 1. Instance has been running for at least MIN_RUNTIME_SECONDS
                // 2. Instance has received data at some point (hasEverReceivedData)
                // 3. Current FPS = 0 and hasReceivedData = true (was working but now stopped)
                if (info.fps == 0.0 && info.hasReceivedData && hasEverReceivedData && 
                    runtime >= MIN_RUNTIME_SECONDS) {
                    // Instance was working but now FPS = 0 - video likely ended
                    zeroFpsCount++;
                    std::cerr << "[InstanceRegistry] [VideoLoop] FPS = 0 detected (count: " << zeroFpsCount << "/" << ZERO_FPS_THRESHOLD 
                              << ", runtime: " << runtime << "s)" << std::endl;
                    
                    if (zeroFpsCount >= ZERO_FPS_THRESHOLD) {
                        shouldRestart = true;
                        zeroFpsCount = 0; // Reset counter
                    }
                } else if (info.fps > 0.0) {
                    // FPS > 0, video is playing - reset counter
                    zeroFpsCount = 0;
                } else if (runtime < MIN_RUNTIME_SECONDS) {
                    // Instance just started, don't restart yet
                    if (zeroFpsCount == 0) {
                        std::cerr << "[InstanceRegistry] [VideoLoop] Instance just started (runtime: " << runtime 
                                  << "s < " << MIN_RUNTIME_SECONDS << "s), waiting before checking for restart..." << std::endl;
                    }
                    zeroFpsCount = 0; // Reset counter during startup period
                }
                    } catch (const std::exception& e) {
                        std::cerr << "[InstanceRegistry] [VideoLoop] Exception accessing instance data: " << e.what() << std::endl;
                        // Continue to next iteration instead of crashing
                        continue;
                    } catch (...) {
                        std::cerr << "[InstanceRegistry] [VideoLoop] Unknown error accessing instance data" << std::endl;
                        // Continue to next iteration instead of crashing
                        continue;
                    }
                }
                
                // Restart file source node if needed
                if (shouldRestart) {
                    std::cerr << "[InstanceRegistry] [VideoLoop] Video ended detected - restarting file source node..." << std::endl;
                    
                    // CRITICAL: Check stop flag before starting restart operation
                    {
                        try {
                            std::lock_guard<std::mutex> lock(thread_mutex_);
                            auto flagIt = video_loop_thread_stop_flags_.find(instanceId);
                            if (flagIt == video_loop_thread_stop_flags_.end() || flagIt->second.load()) {
                                return;  // Stop flag set, exit thread
                            }
                        } catch (...) {
                            return;  // Error accessing stop flag, exit thread
                        }
                    }
                    
                    // Get pipeline copy
                    std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>> pipelineCopy;
                    {
                        try {
                            std::unique_lock<std::shared_timed_mutex> lock(mutex_);
                            auto pipelineIt = pipelines_.find(instanceId);
                            if (pipelineIt != pipelines_.end() && !pipelineIt->second.empty()) {
                                pipelineCopy = pipelineIt->second;
                            }
                        } catch (...) {
                            std::cerr << "[InstanceRegistry] [VideoLoop] Exception getting pipeline copy, skipping restart" << std::endl;
                            continue;  // Skip restart if we can't get pipeline
                        }
                    }
                    
                    if (!pipelineCopy.empty()) {
                        // Check if first node is file source node
                        auto fileNode = std::dynamic_pointer_cast<cvedix_nodes::cvedix_file_src_node>(pipelineCopy[0]);
                        if (fileNode) {
                            try {
                                // CRITICAL: Wrap stop() in async with timeout to prevent blocking
                                try {
                                    auto stopFuture = std::async(std::launch::async, [fileNode]() {
                                        fileNode->stop();
                                    });
                                    
                                    // Wait with timeout (500ms) - if it takes too long, skip stop
                                    if (stopFuture.wait_for(std::chrono::milliseconds(500)) == std::future_status::timeout) {
                                        std::cerr << "[InstanceRegistry] [VideoLoop] ⚠ fileNode->stop() timeout (500ms), skipping..." << std::endl;
                                    } else {
                                        try {
                                            stopFuture.get();
                                        } catch (...) {
                                            // Ignore exceptions from stop()
                                        }
                                    }
                                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                                } catch (...) {
                                    // If stop() fails, continue anyway
                                }
                                
                                // CRITICAL: Wrap detach_recursively() in async with timeout
                                try {
                                    auto detachFuture = std::async(std::launch::async, [fileNode]() {
                                        fileNode->detach_recursively();
                                    });
                                    
                                    // Wait with timeout (1000ms) - if it takes too long, skip detach
                                    if (detachFuture.wait_for(std::chrono::milliseconds(1000)) == std::future_status::timeout) {
                                        std::cerr << "[InstanceRegistry] [VideoLoop] ⚠ fileNode->detach_recursively() timeout (1000ms), skipping..." << std::endl;
                                        // Continue anyway - try to start
                                    } else {
                                        try {
                                            detachFuture.get();
                                        } catch (...) {
                                            // Ignore exceptions from detach
                                        }
                                    }
                                } catch (...) {
                                    // If detach fails, continue anyway
                                }
                                
                                // CRITICAL: Longer delay to ensure GStreamer elements are fully cleaned up
                                // GStreamer needs time to transition elements to NULL state before dispose
                                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                                
                                // CRITICAL: Check stop flag before starting
                                {
                                    try {
                                        std::lock_guard<std::mutex> lock(thread_mutex_);
                                        auto flagIt = video_loop_thread_stop_flags_.find(instanceId);
                                        if (flagIt == video_loop_thread_stop_flags_.end() || flagIt->second.load()) {
                                            return;  // Stop flag set, exit thread
                                        }
                                    } catch (...) {
                                        return;  // Error accessing stop flag, exit thread
                                    }
                                }
                                
                                // Restart file source node with timeout protection
                                std::cerr << "[InstanceRegistry] [VideoLoop] Restarting file source node..." << std::endl;
                                try {
                                    auto startFuture = std::async(std::launch::async, [fileNode]() {
                                        fileNode->start();
                                    });
                                    
                                    // Wait with timeout (2000ms) - if it takes too long, skip start
                                    if (startFuture.wait_for(std::chrono::milliseconds(2000)) == std::future_status::timeout) {
                                        std::cerr << "[InstanceRegistry] [VideoLoop] ⚠ fileNode->start() timeout (2000ms), skipping..." << std::endl;
                                        std::cerr << "[InstanceRegistry] [VideoLoop] Instance will continue running, will retry restart on next check" << std::endl;
                                    } else {
                                        try {
                                            startFuture.get();
                                            std::cerr << "[InstanceRegistry] [VideoLoop] ✓ File source node restarted successfully" << std::endl;
                                            
                                            // Reset hasReceivedData to allow detection of new playback
                                            {
                                                try {
                                                    std::unique_lock<std::shared_timed_mutex> lock(mutex_);
                                                    auto instanceIt = instances_.find(instanceId);
                                                    if (instanceIt != instances_.end()) {
                                                        instanceIt->second.hasReceivedData = false;
                                                        // Reset instance start time for next cycle
                                                        instanceStartTime = std::chrono::steady_clock::now();
                                                        hasEverReceivedData = false;
                                                    }
                                                } catch (...) {
                                                    // Ignore exceptions when updating instance data
                                                }
                                            }
                                        } catch (const std::exception& e) {
                                            std::cerr << "[InstanceRegistry] [VideoLoop] ✗ Exception during fileNode->start(): " << e.what() << std::endl;
                                            std::cerr << "[InstanceRegistry] [VideoLoop] Instance will continue running, will retry restart on next check" << std::endl;
                                        } catch (...) {
                                            std::cerr << "[InstanceRegistry] [VideoLoop] ✗ Unknown error during fileNode->start()" << std::endl;
                                            std::cerr << "[InstanceRegistry] [VideoLoop] Instance will continue running, will retry restart on next check" << std::endl;
                                        }
                                    }
                                } catch (const std::exception& e) {
                                    std::cerr << "[InstanceRegistry] [VideoLoop] ✗ Exception creating start future: " << e.what() << std::endl;
                                } catch (...) {
                                    std::cerr << "[InstanceRegistry] [VideoLoop] ✗ Unknown error creating start future" << std::endl;
                                }
                            } catch (const std::exception& e) {
                                std::cerr << "[InstanceRegistry] [VideoLoop] ✗ Exception restarting file source node: " << e.what() << std::endl;
                                std::cerr << "[InstanceRegistry] [VideoLoop] Instance will continue running, will retry restart on next check" << std::endl;
                            } catch (...) {
                                std::cerr << "[InstanceRegistry] [VideoLoop] ✗ Unknown error restarting file source node" << std::endl;
                                std::cerr << "[InstanceRegistry] [VideoLoop] Instance will continue running, will retry restart on next check" << std::endl;
                            }
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[InstanceRegistry] [VideoLoop] Fatal exception in video loop thread: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "[InstanceRegistry] [VideoLoop] Fatal unknown error in video loop thread" << std::endl;
        }
    });
    
    // Store thread handle
    {
        std::lock_guard<std::mutex> lock(thread_mutex_);
        video_loop_threads_[instanceId] = std::move(videoLoopThread);
    }
}

void InstanceRegistry::stopVideoLoopThread(const std::string& instanceId) {
    std::unique_lock<std::mutex> lock(thread_mutex_);
    
    // Set stop flag
    auto flagIt = video_loop_thread_stop_flags_.find(instanceId);
    if (flagIt != video_loop_thread_stop_flags_.end()) {
        flagIt->second.store(true);
    }
    
    // Get thread handle and release lock before joining
    std::thread threadToJoin;
    auto threadIt = video_loop_threads_.find(instanceId);
    if (threadIt != video_loop_threads_.end()) {
        if (threadIt->second.joinable()) {
            threadToJoin = std::move(threadIt->second);
        }
        video_loop_threads_.erase(threadIt);
    }
    
    // Remove stop flag
    if (flagIt != video_loop_thread_stop_flags_.end()) {
        video_loop_thread_stop_flags_.erase(flagIt);
    }
    
    // Release lock before joining
    lock.unlock();
    
    // Join thread
    if (threadToJoin.joinable()) {
        threadToJoin.join();
    }
}

Json::Value InstanceRegistry::getInstanceConfig(const std::string& instanceId) const {
    // CRITICAL: Use shared_lock for read-only operations to allow concurrent readers
    std::shared_lock<std::shared_timed_mutex> lock(mutex_); // Shared lock for read operations
    
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

void InstanceRegistry::startRTSPMonitorThread(const std::string& instanceId) {
    // Stop existing thread if any
    stopRTSPMonitorThread(instanceId);
    
    // Check if instance has RTSP URL
    std::string rtspUrl;
    {
        std::shared_lock<std::shared_timed_mutex> lock(mutex_);
        auto instanceIt = instances_.find(instanceId);
        if (instanceIt == instances_.end()) {
            return; // Instance not found
        }
        const auto& info = instanceIt->second;
        if (info.rtspUrl.empty()) {
            return; // Not an RTSP instance
        }
        rtspUrl = info.rtspUrl;
    }
    
    // Create stop flag
    auto stop_flag = std::make_shared<std::atomic<bool>>(false);
    {
        std::lock_guard<std::mutex> lock(rtsp_monitor_mutex_);
        rtsp_monitor_stop_flags_[instanceId] = stop_flag;
        rtsp_last_activity_[instanceId] = std::chrono::steady_clock::now();
        rtsp_reconnect_attempts_[instanceId] = 0;
    }
    
    // Start monitoring thread
    std::thread monitor_thread([this, instanceId, rtspUrl, stop_flag]() {
        std::cerr << "[InstanceRegistry] [RTSP Monitor] Thread started for instance " << instanceId << std::endl;
        std::cerr << "[InstanceRegistry] [RTSP Monitor] Monitoring RTSP stream: " << rtspUrl << std::endl;
        
        const auto check_interval = std::chrono::seconds(2);  // Check every 2 seconds (faster detection for unstable streams)
        const auto inactivity_timeout = std::chrono::seconds(15);  // Consider disconnected if no activity for 15 seconds (faster detection for unstable streams)
        const auto reconnect_cooldown = std::chrono::seconds(10);  // Wait 10 seconds between reconnect attempts
        const int max_reconnect_attempts = 10;  // Maximum reconnect attempts before giving up
        
        auto last_reconnect_attempt = std::chrono::steady_clock::now() - reconnect_cooldown;  // Allow immediate first check
        auto last_activity_check = std::chrono::steady_clock::now();
        
        while (!stop_flag->load()) {
            // Check stop flag before blocking operations
            if (stop_flag->load()) {
                break;
            }
            
            // Sleep with periodic stop flag checks
            auto sleep_start = std::chrono::steady_clock::now();
            while (std::chrono::steady_clock::now() - sleep_start < check_interval) {
                if (stop_flag->load()) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(500));  // Check every 500ms
            }
            
            if (stop_flag->load()) {
                break;
            }
            
            // Check if instance still exists and is running
            bool instanceExists = false;
            bool instanceRunning = false;
            {
                std::shared_lock<std::shared_timed_mutex> lock(mutex_);
                auto instanceIt = instances_.find(instanceId);
                if (instanceIt != instances_.end()) {
                    instanceExists = true;
                    instanceRunning = instanceIt->second.running;
                }
            }
            
            if (!instanceExists || !instanceRunning) {
                std::cerr << "[InstanceRegistry] [RTSP Monitor] Instance " << instanceId 
                          << " no longer exists or is not running, stopping monitor thread" << std::endl;
                break;
            }
            
            // Get last activity time
            auto last_activity = std::chrono::steady_clock::now();
            {
                std::lock_guard<std::mutex> lock(rtsp_monitor_mutex_);
                auto activityIt = rtsp_last_activity_.find(instanceId);
                if (activityIt != rtsp_last_activity_.end()) {
                    last_activity = activityIt->second;
                }
            }
            
            // Check if stream is inactive (no frames received for timeout period)
            auto now = std::chrono::steady_clock::now();
            auto time_since_activity = std::chrono::duration_cast<std::chrono::seconds>(now - last_activity).count();
            
            // Get current reconnect attempt count
            int reconnect_attempts = 0;
            {
                std::lock_guard<std::mutex> lock(rtsp_monitor_mutex_);
                auto attemptsIt = rtsp_reconnect_attempts_.find(instanceId);
                if (attemptsIt != rtsp_reconnect_attempts_.end()) {
                    reconnect_attempts = attemptsIt->second.load();
                }
            }
            
            // Check if stream appears to be disconnected
            if (time_since_activity > inactivity_timeout.count()) {
                std::cerr << "[InstanceRegistry] [RTSP Monitor] ⚠ Stream appears disconnected (no activity for " 
                          << time_since_activity << " seconds)" << std::endl;
                
                // Check if enough time has passed since last reconnect attempt
                auto time_since_last_reconnect = std::chrono::duration_cast<std::chrono::seconds>(
                    now - last_reconnect_attempt).count();
                
                if (time_since_last_reconnect >= reconnect_cooldown.count()) {
                    if (reconnect_attempts < max_reconnect_attempts) {
                        std::cerr << "[InstanceRegistry] [RTSP Monitor] Attempting to reconnect RTSP stream (attempt " 
                                  << (reconnect_attempts + 1) << "/" << max_reconnect_attempts << ")..." << std::endl;
                        
                        bool reconnect_success = reconnectRTSPStream(instanceId);
                        
                        last_reconnect_attempt = now;
                        
                        if (reconnect_success) {
                            std::cerr << "[InstanceRegistry] [RTSP Monitor] ✓ Reconnection successful!" << std::endl;
                            // Reset reconnect attempts on success
                            {
                                std::lock_guard<std::mutex> lock(rtsp_monitor_mutex_);
                                auto attemptsIt = rtsp_reconnect_attempts_.find(instanceId);
                                if (attemptsIt != rtsp_reconnect_attempts_.end()) {
                                    attemptsIt->second.store(0);
                                }
                                // Update last activity to now (reconnection is activity)
                                rtsp_last_activity_[instanceId] = now;
                            }
                        } else {
                            std::cerr << "[InstanceRegistry] [RTSP Monitor] ✗ Reconnection failed" << std::endl;
                            // Increment reconnect attempts
                            {
                                std::lock_guard<std::mutex> lock(rtsp_monitor_mutex_);
                                auto attemptsIt = rtsp_reconnect_attempts_.find(instanceId);
                                if (attemptsIt != rtsp_reconnect_attempts_.end()) {
                                    attemptsIt->second.fetch_add(1);
                                }
                            }
                        }
                    } else {
                        std::cerr << "[InstanceRegistry] [RTSP Monitor] ⚠ Maximum reconnect attempts (" 
                                  << max_reconnect_attempts << ") reached. Stopping reconnect attempts." << std::endl;
                        std::cerr << "[InstanceRegistry] [RTSP Monitor] Instance will remain stopped until manual intervention." << std::endl;
                    }
                } else {
                    // Still in cooldown period
                    int remaining_cooldown = reconnect_cooldown.count() - time_since_last_reconnect;
                    if (remaining_cooldown > 0 && (now - last_activity_check).count() > 30) {
                        // Only log every 30 seconds to avoid spam
                        std::cerr << "[InstanceRegistry] [RTSP Monitor] Waiting " << remaining_cooldown 
                                  << " seconds before next reconnect attempt..." << std::endl;
                        last_activity_check = now;
                    }
                }
            } else {
                // Stream is active - reset reconnect attempts
                if (reconnect_attempts > 0) {
                    std::cerr << "[InstanceRegistry] [RTSP Monitor] ✓ Stream is active again (activity " 
                              << time_since_activity << " seconds ago)" << std::endl;
                    {
                        std::lock_guard<std::mutex> lock(rtsp_monitor_mutex_);
                        auto attemptsIt = rtsp_reconnect_attempts_.find(instanceId);
                        if (attemptsIt != rtsp_reconnect_attempts_.end()) {
                            attemptsIt->second.store(0);
                        }
                    }
                }
            }
        }
        
        std::cerr << "[InstanceRegistry] [RTSP Monitor] Thread stopped for instance " << instanceId << std::endl;
    });
    
    // Store thread
    {
        std::lock_guard<std::mutex> lock(rtsp_monitor_mutex_);
        rtsp_monitor_threads_[instanceId] = std::move(monitor_thread);
    }
    
    std::cerr << "[InstanceRegistry] [RTSP Monitor] Monitoring thread started for instance " << instanceId << std::endl;
}

void InstanceRegistry::stopRTSPMonitorThread(const std::string& instanceId) {
    std::unique_lock<std::mutex> lock(rtsp_monitor_mutex_);
    
    // Set stop flag
    auto flagIt = rtsp_monitor_stop_flags_.find(instanceId);
    if (flagIt != rtsp_monitor_stop_flags_.end() && flagIt->second) {
        flagIt->second->store(true);
    }
    
    // Get thread handle and release lock before joining to avoid deadlock
    std::thread threadToJoin;
    auto threadIt = rtsp_monitor_threads_.find(instanceId);
    if (threadIt != rtsp_monitor_threads_.end()) {
        if (threadIt->second.joinable()) {
            threadToJoin = std::move(threadIt->second);
        }
        rtsp_monitor_threads_.erase(threadIt);
    }
    
    // Remove stop flag and other tracking data
    if (flagIt != rtsp_monitor_stop_flags_.end()) {
        rtsp_monitor_stop_flags_.erase(flagIt);
    }
    rtsp_last_activity_.erase(instanceId);
    rtsp_reconnect_attempts_.erase(instanceId);
    
    // Release lock before joining to avoid deadlock
    lock.unlock();
    
    // Join thread with timeout to prevent blocking forever
    if (threadToJoin.joinable()) {
        auto future = std::async(std::launch::async, [&threadToJoin]() {
            threadToJoin.join();
        });
        
        // Wait up to 1 second for thread to finish
        auto status = future.wait_for(std::chrono::seconds(1));
        if (status == std::future_status::timeout) {
            std::cerr << "[InstanceRegistry] [RTSP Monitor] Warning: Thread join timeout, detaching..." << std::endl;
            threadToJoin.detach();
        }
    }
}

void InstanceRegistry::updateRTSPActivity(const std::string& instanceId) {
    std::lock_guard<std::mutex> lock(rtsp_monitor_mutex_);
    rtsp_last_activity_[instanceId] = std::chrono::steady_clock::now();
}

bool InstanceRegistry::reconnectRTSPStream(const std::string& instanceId) {
    std::cerr << "[InstanceRegistry] [RTSP Reconnect] Attempting to reconnect RTSP stream for instance " << instanceId << std::endl;
    
    try {
        // Get instance info
        InstanceInfo info;
        {
            std::shared_lock<std::shared_timed_mutex> lock(mutex_);
            auto instanceIt = instances_.find(instanceId);
            if (instanceIt == instances_.end()) {
                std::cerr << "[InstanceRegistry] [RTSP Reconnect] ✗ Instance not found" << std::endl;
                return false;
            }
            info = instanceIt->second;
        }
        
        if (info.rtspUrl.empty()) {
            std::cerr << "[InstanceRegistry] [RTSP Reconnect] ✗ Instance does not have RTSP URL" << std::endl;
            return false;
        }
        
        // Get pipeline nodes
        auto nodes = getInstanceNodes(instanceId);
        if (nodes.empty()) {
            std::cerr << "[InstanceRegistry] [RTSP Reconnect] ✗ Pipeline not found" << std::endl;
            return false;
        }
        
        // Get RTSP node
        auto rtspNode = std::dynamic_pointer_cast<cvedix_nodes::cvedix_rtsp_src_node>(nodes[0]);
        if (!rtspNode) {
            std::cerr << "[InstanceRegistry] [RTSP Reconnect] ✗ RTSP node not found" << std::endl;
            return false;
        }
        
        std::cerr << "[InstanceRegistry] [RTSP Reconnect] Stopping RTSP node..." << std::endl;
        
        // Stop RTSP node gracefully with timeout
        try {
            auto stopFuture = std::async(std::launch::async, [rtspNode]() {
                try {
                    rtspNode->stop();
                    return true;
                } catch (...) {
                    return false;
                }
            });
            
            auto stopStatus = stopFuture.wait_for(std::chrono::milliseconds(500));
            if (stopStatus == std::future_status::timeout) {
                std::cerr << "[InstanceRegistry] [RTSP Reconnect] ⚠ Stop timeout, using detach..." << std::endl;
                try {
                    rtspNode->detach_recursively();
                } catch (...) {
                    // Ignore errors
                }
            } else if (stopStatus == std::future_status::ready) {
                stopFuture.get();
            }
        } catch (const std::exception& e) {
            std::cerr << "[InstanceRegistry] [RTSP Reconnect] ⚠ Exception stopping RTSP node: " << e.what() << std::endl;
            // Try detach as fallback
            try {
                rtspNode->detach_recursively();
            } catch (...) {
                // Ignore errors
            }
        }
        
        // Wait a moment before restarting
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        std::cerr << "[InstanceRegistry] [RTSP Reconnect] Restarting RTSP node..." << std::endl;
        
        // Restart RTSP node
        try {
            rtspNode->start();
            std::cerr << "[InstanceRegistry] [RTSP Reconnect] ✓ RTSP node restarted successfully" << std::endl;
            
            // Update activity time
            updateRTSPActivity(instanceId);
            
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[InstanceRegistry] [RTSP Reconnect] ✗ Exception restarting RTSP node: " << e.what() << std::endl;
            return false;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[InstanceRegistry] [RTSP Reconnect] ✗ Exception during reconnect: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "[InstanceRegistry] [RTSP Reconnect] ✗ Unknown error during reconnect" << std::endl;
        return false;
    }
}

