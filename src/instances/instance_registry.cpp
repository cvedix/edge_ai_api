#include "instances/instance_registry.h"
#include "core/uuid_generator.h"
#include <cvedix/cvedix_version.h>
#include <cvedix/nodes/src/cvedix_rtsp_src_node.h>
#include <cvedix/nodes/src/cvedix_file_src_node.h>
#include <cvedix/nodes/infers/cvedix_yunet_face_detector_node.h>
#include <cvedix/nodes/infers/cvedix_sface_feature_encoder_node.h>
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
            std::cerr << "[InstanceRegistry] ========================================" << std::endl;
            std::cerr << "[InstanceRegistry] Auto-starting pipeline for instance " << instanceId << std::endl;
            std::cerr << "[InstanceRegistry] ========================================" << std::endl;
            
            // Wait for DNN models to be ready using exponential backoff
            // This is more reliable than fixed delay as it adapts to model loading time
            waitForModelsReady(pipeline, 2000); // Max 2 seconds
            
            try {
                if (startPipeline(pipeline)) {
                    info.running = true;
                    instances_[instanceId] = info;
                    std::cerr << "[InstanceRegistry] ========================================" << std::endl;
                    std::cerr << "[InstanceRegistry] ✓ Pipeline started successfully for instance " << instanceId << std::endl;
                    std::cerr << "[InstanceRegistry] NOTE: If RTSP connection fails, the node will retry automatically" << std::endl;
                    std::cerr << "[InstanceRegistry] NOTE: Monitor logs above for RTSP connection status" << std::endl;
                    std::cerr << "[InstanceRegistry] ========================================" << std::endl;
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
    
    std::cerr << "[InstanceRegistry] ========================================" << std::endl;
    std::cerr << "[InstanceRegistry] Deleting instance " << instanceId << "..." << std::endl;
    std::cerr << "[InstanceRegistry] ========================================" << std::endl;
    
    // Stop pipeline if running (cleanup before deletion)
    auto pipelineIt = pipelines_.find(instanceId);
    if (pipelineIt != pipelines_.end()) {
        std::cerr << "[InstanceRegistry] Stopping pipeline before deletion..." << std::endl;
        stopPipeline(pipelineIt->second, true);  // true = deletion, cleanup everything
        pipelines_.erase(pipelineIt);
        std::cerr << "[InstanceRegistry] Pipeline stopped and removed" << std::endl;
    }
    
    // Delete from storage
    if (it->second.persistent) {
        std::cerr << "[InstanceRegistry] Removing persistent instance from storage..." << std::endl;
        instance_storage_.deleteInstance(instanceId);
    }
    
    instances_.erase(it);
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

bool InstanceRegistry::startInstance(const std::string& instanceId) {
    // CRITICAL: Release lock before calling waitForModelsReady and startPipeline
    // These functions can take a long time (30 seconds!) and don't need the lock
    // This prevents deadlock if another thread needs the lock
    std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>> pipelineCopy;
    bool needRebuild = false;
    bool alreadyRunning = false;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto instanceIt = instances_.find(instanceId);
        if (instanceIt == instances_.end()) {
            return false;
        }
        
        // Check if already running
        if (instanceIt->second.running) {
            std::cerr << "[InstanceRegistry] Instance " << instanceId << " is already running" << std::endl;
            return true;
        }
        
        // Check if pipeline exists and is valid
        // After stop, pipeline is cleared from map, so we always need to rebuild
        auto pipelineIt = pipelines_.find(instanceId);
        
        if (pipelineIt == pipelines_.end()) {
            needRebuild = true;
            std::cerr << "[InstanceRegistry] Pipeline not found for instance " << instanceId 
                      << ", rebuilding from instance info..." << std::endl;
        } else if (pipelineIt->second.empty()) {
            needRebuild = true;
            std::cerr << "[InstanceRegistry] Pipeline is empty for instance " << instanceId 
                      << ", rebuilding..." << std::endl;
        } else {
            // After detach_recursively(), nodes cannot be restarted
            // Always rebuild to ensure fresh pipeline
            std::cerr << "[InstanceRegistry] Pipeline exists but may be invalid after previous stop, rebuilding to ensure fresh pipeline..." << std::endl;
            needRebuild = true;
        }
        
        if (needRebuild) {
            // Release lock before rebuilding (rebuildPipelineFromInstanceInfo may need lock internally)
            // Actually, rebuildPipelineFromInstanceInfo doesn't use lock, so we can call it here
            // But we'll release lock before the long operations
        } else {
            // Copy pipeline before releasing lock
            pipelineCopy = pipelineIt->second;
        }
    } // Release lock here - rebuild and wait operations don't need it
    
    if (needRebuild) {
        if (!rebuildPipelineFromInstanceInfo(instanceId)) {
            std::cerr << "[InstanceRegistry] ✗ Failed to rebuild pipeline for instance " << instanceId << std::endl;
            return false;
        }
        
        // Get pipeline copy after rebuild (need lock briefly)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto pipelineIt = pipelines_.find(instanceId);
            if (pipelineIt == pipelines_.end() || pipelineIt->second.empty()) {
                std::cerr << "[InstanceRegistry] ✗ Pipeline rebuild failed or returned empty pipeline" << std::endl;
                return false;
            }
            pipelineCopy = pipelineIt->second;
        } // Release lock again
        
        std::cerr << "[InstanceRegistry] ✓ Pipeline rebuilt successfully" << std::endl;
        
        // IMPORTANT: When restarting, we need extra time because:
        // 1. Model is being loaded again (may have cache/state issues)
        // 2. OpenCV DNN may need time to clear old state
        // 3. File source needs to reset to beginning of file
        // Use adaptive waiting with longer timeout for restart scenarios
        // NOTE: This is called WITHOUT holding the lock to prevent deadlock
        std::cerr << "[InstanceRegistry] Waiting for models to be ready (adaptive, up to 30 seconds)..." << std::endl;
        std::cerr << "[InstanceRegistry] This ensures OpenCV DNN clears any cached state and models are fully initialized" << std::endl;
        waitForModelsReady(pipelineCopy, 30000); // Max 30 seconds for restart
    }
    
    std::cerr << "[InstanceRegistry] ========================================" << std::endl;
    std::cerr << "[InstanceRegistry] Starting instance " << instanceId << "..." << std::endl;
    std::cerr << "[InstanceRegistry] ========================================" << std::endl;
    
    // Call startPipeline without holding the lock
    bool started = startPipeline(pipelineCopy);
    
    // Update running status (need lock briefly)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto instanceIt = instances_.find(instanceId);
        if (instanceIt != instances_.end()) {
            if (started) {
                instanceIt->second.running = true;
                std::cerr << "[InstanceRegistry] ✓ Instance " << instanceId << " started successfully" << std::endl;
            } else {
                std::cerr << "[InstanceRegistry] ✗ Failed to start instance " << instanceId << std::endl;
            }
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
        
        // Copy pipeline before releasing lock
        pipelineCopy = pipelineIt->second;
        
        // Mark as not running immediately (before stopPipeline)
        instanceIt->second.running = false;
        
        // Remove from pipelines map immediately to prevent other threads from accessing it
        pipelines_.erase(pipelineIt);
    } // Release lock here - stopPipeline doesn't need it
    
    std::cerr << "[InstanceRegistry] ========================================" << std::endl;
    std::cerr << "[InstanceRegistry] Stopping instance " << instanceId << "..." << std::endl;
    std::cerr << "[InstanceRegistry] ========================================" << std::endl;
    
    // Now call stopPipeline without holding the lock
    // This prevents deadlock if stopPipeline takes a long time
    try {
        stopPipeline(pipelineCopy, false);  // false = not deletion, just stop
    } catch (const std::exception& e) {
        std::cerr << "[InstanceRegistry] Exception in stopPipeline: " << e.what() << std::endl;
        // Continue anyway - pipeline is already removed from map
    } catch (...) {
        std::cerr << "[InstanceRegistry] Unknown exception in stopPipeline" << std::endl;
        // Continue anyway - pipeline is already removed from map
    }
    
    std::cerr << "[InstanceRegistry] ✓ Instance " << instanceId << " stopped successfully" << std::endl;
    std::cerr << "[InstanceRegistry] NOTE: Pipeline will be automatically rebuilt when you start this instance again" << std::endl;
    std::cerr << "[InstanceRegistry] ========================================" << std::endl;
    
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
    
    // Use exponential backoff with adaptive waiting
    // Start with small delays and increase gradually until maxWaitMs
    std::cerr << "[InstanceRegistry] Waiting for DNN models to initialize (adaptive, max " << maxWaitMs << "ms)..." << std::endl;
    int currentDelay = 200; // Start with 200ms
    int totalWaited = 0;
    int attempt = 0;
    // Calculate max attempts needed: for 30s (30000ms) with exponential backoff up to 1600ms per attempt
    // Need at least 30000/1600 = ~19 attempts, add buffer for safety
    const int maxAttempts = (maxWaitMs / 1600) + 10; // Dynamic based on maxWaitMs
    
    // Calculate number of attempts needed to reach maxWaitMs
    // With exponential backoff: 200, 400, 800, 1600, 1600, 1600...
    while (totalWaited < maxWaitMs && attempt < maxAttempts) {
        int delayThisRound = std::min(currentDelay, maxWaitMs - totalWaited);
        std::cerr << "[InstanceRegistry]   Attempt " << (attempt + 1) << ": waiting " << delayThisRound << "ms (total: " << totalWaited << "ms / " << maxWaitMs << "ms)..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(delayThisRound));
        totalWaited += delayThisRound;
        
        // Exponential backoff: double the delay each time, cap at 1600ms per attempt
        currentDelay = std::min(currentDelay * 2, 1600);
        attempt++;
        
        // For shorter waits (create scenario), can exit early
        // For longer waits (restart scenario), wait closer to maxWaitMs
        if (maxWaitMs <= 2000 && totalWaited >= 1000) {
            // For create scenario (2s max), exit after 1s if models usually ready
            std::cerr << "[InstanceRegistry]   Models should be ready now (waited " << totalWaited << "ms)" << std::endl;
            break;
        }
    }
    
    std::cerr << "[InstanceRegistry] ✓ Models initialization wait completed (total: " << totalWaited << "ms / " << maxWaitMs << "ms)" << std::endl;
}

bool InstanceRegistry::startPipeline(const std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>>& nodes) {
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
            
            // Additional small delay to ensure file source and model pipeline are fully synchronized
            // Note: We already waited for models after rebuild, but add small delay here for final sync
            std::cerr << "[InstanceRegistry] Final synchronization delay before starting file source (200ms)..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            
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
            
            // CRITICAL: Add delay after start to allow model to process first frame
            // This prevents shape mismatch errors that occur when model receives frames
            // before it's fully ready to process them
            std::cerr << "[InstanceRegistry] Waiting for model to process first frame (1 second)..." << std::endl;
            std::cerr << "[InstanceRegistry] This prevents shape mismatch errors during initial frame processing" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            
            // NOTE: Shape mismatch errors may still occur after this point if:
            // 1. Video has inconsistent frame sizes
            // 2. Model (especially YuNet 2022mar) doesn't handle dynamic input well
            // 3. Resize ratio doesn't produce consistent dimensions
            // If this happens, SIGABRT handler will catch it and stop the instance
            std::cerr << "[InstanceRegistry] File source pipeline started successfully" << std::endl;
            std::cerr << "[InstanceRegistry] NOTE: If you see shape mismatch errors, consider:" << std::endl;
            std::cerr << "[InstanceRegistry]   1. Using YuNet 2023mar model (better dynamic input support)" << std::endl;
            std::cerr << "[InstanceRegistry]   2. Ensuring video has consistent resolution" << std::endl;
            std::cerr << "[InstanceRegistry]   3. Re-encoding video to fixed resolution if needed" << std::endl;
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
        
        // Give GStreamer time to properly cleanup after detach
        // This helps reduce warnings about VideoWriter finalization
        if (isDeletion) {
            std::cerr << "[InstanceRegistry] Waiting for GStreamer cleanup..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            std::cerr << "[InstanceRegistry] Pipeline stopped and detached (deletion)" << std::endl;
            std::cerr << "[InstanceRegistry] NOTE: GStreamer warnings about VideoWriter finalization are normal during cleanup" << std::endl;
        } else {
            // Note: We detach nodes but keep them in the pipeline vector
            // This allows the pipeline to be rebuilt when restarting
            // The nodes will be recreated when startInstance is called if needed
            std::cerr << "[InstanceRegistry] Pipeline stopped (nodes detached but kept for potential restart)" << std::endl;
            std::cerr << "[InstanceRegistry] NOTE: Pipeline will be automatically rebuilt when restarting" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[InstanceRegistry] Exception in stopPipeline: " << e.what() << std::endl;
        std::cerr << "[InstanceRegistry] NOTE: GStreamer warnings during cleanup are usually harmless" << std::endl;
    }
}

bool InstanceRegistry::rebuildPipelineFromInstanceInfo(const std::string& instanceId) {
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
    
    // Restore additional parameters from InstanceInfo
    // Use originator.address as RTSP URL if available
    if (!info.originator.address.empty()) {
        req.additionalParams["RTSP_URL"] = info.originator.address;
    }
    
    // Restore RTMP URL if available
    if (!info.rtmpUrl.empty()) {
        req.additionalParams["RTMP_URL"] = info.rtmpUrl;
    }
    
    // Restore FILE_PATH if available
    if (!info.filePath.empty()) {
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

