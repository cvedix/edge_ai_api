#pragma once

#include "instances/instance_info.h"
#include "instances/instance_statistics.h"
#include "models/create_instance_request.h"
#include "solutions/solution_registry.h"
#include "core/pipeline_builder.h"
#include "instances/instance_storage.h"
#include <string>
#include <unordered_map>
#include <optional>
#include <mutex>
#include <shared_mutex>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <chrono>
#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>

// Forward declarations
namespace cvedix_nodes {
    class cvedix_node;
}

/**
 * @brief Instance Registry
 * 
 * Manages AI instances and their pipelines.
 * Handles creation, deletion, starting, and stopping of instances.
 */
class InstanceRegistry {
public:
    /**
     * @brief Constructor
     * @param solutionRegistry Reference to solution registry
     * @param pipelineBuilder Reference to pipeline builder
     * @param instanceStorage Reference to instance storage
     */
    InstanceRegistry(
        SolutionRegistry& solutionRegistry,
        PipelineBuilder& pipelineBuilder,
        InstanceStorage& instanceStorage
    );
    
    /**
     * @brief Create a new instance
     * @param req Create instance request
     * @return Instance ID if successful, empty string otherwise
     */
    std::string createInstance(const CreateInstanceRequest& req);
    
    /**
     * @brief Delete an instance
     * @param instanceId Instance ID
     * @return true if successful
     */
    bool deleteInstance(const std::string& instanceId);
    
    /**
     * @brief Get instance information
     * @param instanceId Instance ID
     * @return Instance info if found, empty optional otherwise
     */
    std::optional<InstanceInfo> getInstance(const std::string& instanceId) const;
    
    /**
     * @brief Start an instance (start pipeline)
     * @param instanceId Instance ID
     * @param skipAutoStop If true, skip auto-stop of running instance (for restart scenario)
     * @return true if successful
     */
    bool startInstance(const std::string& instanceId, bool skipAutoStop = false);
    
    /**
     * @brief Stop an instance (stop pipeline)
     * @param instanceId Instance ID
     * @return true if successful
     */
    bool stopInstance(const std::string& instanceId);
    
    /**
     * @brief Update instance information
     * @param instanceId Instance ID
     * @param req Update instance request
     * @return true if successful
     */
    bool updateInstance(const std::string& instanceId, const class UpdateInstanceRequest& req);
    
    /**
     * @brief Update instance from JSON config (direct config update)
     * @param instanceId Instance ID
     * @param configJson JSON config to merge (PascalCase format matching instance_detail.txt)
     * @return true if successful
     */
    bool updateInstanceFromConfig(const std::string& instanceId, const class Json::Value& configJson);
    
    /**
     * @brief List all instance IDs
     * @return Vector of instance IDs
     */
    std::vector<std::string> listInstances() const;
    
    /**
     * @brief Get total count of instances
     * @return Number of instances
     */
    int getInstanceCount() const;
    
    /**
     * @brief Get all instances info in one lock acquisition (optimized for list operations)
     * @return Map of instance ID to InstanceInfo
     */
    std::unordered_map<std::string, InstanceInfo> getAllInstances() const;
    
    /**
     * @brief Check if instance exists
     * @param instanceId Instance ID
     * @return true if instance exists
     */
    bool hasInstance(const std::string& instanceId) const;
    
    /**
     * @brief Load all persistent instances from storage
     */
    void loadPersistentInstances();
    
    /**
     * @brief Check if instance has RTMP output
     * @param instanceId Instance ID
     * @return true if instance has RTMP output
     */
    bool hasRTMPOutput(const std::string& instanceId) const;
    
    /**
     * @brief Get source nodes from all running instances (for debug/analysis board)
     * @return Vector of source nodes from running instances
     */
    std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>> getSourceNodesFromRunningInstances() const;
    
    /**
     * @brief Get pipeline nodes for an instance (for shutdown/force detach)
     * @param instanceId Instance ID
     * @return Vector of pipeline nodes if instance exists and has pipeline, empty vector otherwise
     */
    std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>> getInstanceNodes(const std::string& instanceId) const;
    
    /**
     * @brief Check and increment retry counter for instances stuck in retry loop
     * This should be called periodically to monitor instances
     * @return Number of instances that reached retry limit and were stopped
     */
    int checkAndHandleRetryLimits();
    
    /**
     * @brief Get instance config as JSON (config format, not state)
     * @param instanceId Instance ID
     * @return JSON config if instance exists, empty JSON otherwise
     */
    Json::Value getInstanceConfig(const std::string& instanceId) const;
    
    /**
     * @brief Get instance statistics
     * @param instanceId Instance ID
     * @return Statistics info if instance exists and is running, empty optional otherwise
     * @note This method may update tracker with latest FPS/resolution information
     */
    std::optional<InstanceStatistics> getInstanceStatistics(const std::string& instanceId);
    
    /**
     * @brief Get last frame from instance (cached frame)
     * @param instanceId Instance ID
     * @return Base64-encoded JPEG frame string, empty string if no frame available
     */
    std::string getLastFrame(const std::string& instanceId) const;
    
private:
    SolutionRegistry& solution_registry_;
    PipelineBuilder& pipeline_builder_;
    InstanceStorage& instance_storage_;
    
    mutable std::shared_timed_mutex mutex_; // Use shared_timed_mutex to allow multiple concurrent readers (getAllInstances) while writers (start/stop) are exclusive, and support timeout operations
    std::unordered_map<std::string, InstanceInfo> instances_;
    std::unordered_map<std::string, std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>>> pipelines_;
    
    // Thread management for logging threads (prevent memory leaks from detached threads)
    std::unordered_map<std::string, std::atomic<bool>> logging_thread_stop_flags_;
    std::unordered_map<std::string, std::thread> logging_threads_;
    mutable std::mutex thread_mutex_; // Separate mutex for thread management to avoid deadlock
    
    // Statistics tracking per instance
    struct InstanceStatsTracker {
        std::chrono::steady_clock::time_point start_time;  // For elapsed time calculation
        std::chrono::system_clock::time_point start_time_system;  // For Unix timestamp
        uint64_t frames_processed = 0;
        uint64_t dropped_frames = 0;
        double last_fps = 0.0;
        std::chrono::steady_clock::time_point last_fps_update;
        uint64_t frame_count_since_last_update = 0;
        std::string resolution;  // Current processing resolution
        std::string source_resolution;  // Source resolution
        std::string format;  // Frame format
        size_t max_queue_size_seen = 0;  // Maximum queue size observed
        size_t current_queue_size = 0;   // Current queue size (from last hook callback)
        uint64_t expected_frames_from_source = 0;  // Expected frames based on source FPS
    };
    
    mutable std::unordered_map<std::string, InstanceStatsTracker> statistics_trackers_;
    
    // Frame cache per instance
    struct FrameCache {
        cv::Mat frame;  // OSD frame (processed frame with overlays)
        std::chrono::steady_clock::time_point timestamp;
        bool has_frame = false;
    };
    
    mutable std::unordered_map<std::string, FrameCache> frame_caches_;
    mutable std::mutex frame_cache_mutex_; // Separate mutex for frame cache to avoid deadlock
    
    /**
     * @brief Update frame cache for an instance
     * @param instanceId Instance ID
     * @param frame Frame to cache (will be copied)
     */
    void updateFrameCache(const std::string& instanceId, const cv::Mat& frame);
    
    /**
     * @brief Setup frame capture hook for pipeline
     * @param instanceId Instance ID
     * @param nodes Pipeline nodes
     */
    void setupFrameCaptureHook(const std::string& instanceId, const std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>>& nodes);
    
    /**
     * @brief Setup queue size tracking hook for pipeline nodes
     * @param instanceId Instance ID
     * @param nodes Pipeline nodes
     */
    void setupQueueSizeTrackingHook(const std::string& instanceId, const std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>>& nodes);
    
    /**
     * @brief Encode cv::Mat frame to JPEG base64 string
     * @param frame Frame to encode
     * @param jpegQuality JPEG quality (1-100, default 85)
     * @return Base64-encoded JPEG string
     */
    std::string encodeFrameToBase64(const cv::Mat& frame, int jpegQuality = 85) const;
    
    /**
     * @brief Create InstanceInfo from request
     */
    InstanceInfo createInstanceInfo(
        const std::string& instanceId,
        const CreateInstanceRequest& req,
        const SolutionConfig* solution
    ) const;
    
    /**
     * @brief Wait for DNN models to be ready using exponential backoff
     * @param nodes Pipeline nodes to check
     * @param maxWaitMs Maximum wait time in milliseconds. Use -1 for unlimited wait (no timeout)
     */
    void waitForModelsReady(const std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>>& nodes, int maxWaitMs);
    
    /**
     * @brief Start pipeline nodes
     * @param instanceId Instance ID (for frame capture hook setup)
     * @param nodes Pipeline nodes to start
     * @param isRestart If true, this is a restart (use longer delays for model initialization)
     */
    bool startPipeline(const std::string& instanceId, const std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>>& nodes, bool isRestart = false);
    
    /**
     * @brief Stop and cleanup pipeline nodes
     * @param nodes Pipeline nodes to stop
     * @param isDeletion If true, this is for deletion (full cleanup). If false, just stop (can restart).
     */
    void stopPipeline(const std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>>& nodes, bool isDeletion = false);
    
    /**
     * @brief Rebuild pipeline from instance info (for instances loaded from storage)
     * @param instanceId Instance ID
     * @return true if pipeline was rebuilt successfully
     */
    bool rebuildPipelineFromInstanceInfo(const std::string& instanceId);
    
    /**
     * @brief Log processing results for instance (for instances without RTMP output)
     * @param instanceId Instance ID
     */
    void logProcessingResults(const std::string& instanceId) const;
    
    /**
     * @brief Stop and cleanup logging thread for an instance
     * @param instanceId Instance ID
     */
    void stopLoggingThread(const std::string& instanceId);
    
    /**
     * @brief Start logging thread for an instance (if needed)
     * @param instanceId Instance ID
     */
    void startLoggingThread(const std::string& instanceId);
};

