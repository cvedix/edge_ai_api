#pragma once

#include "instances/instance_info.h"
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
    // Thread management for video loop monitoring threads
    std::unordered_map<std::string, std::atomic<bool>> video_loop_thread_stop_flags_;
    std::unordered_map<std::string, std::thread> video_loop_threads_;
    mutable std::mutex thread_mutex_; // Separate mutex for thread management to avoid deadlock
    
    // Thread management for MQTT JSON reader threads (prevent memory leaks and segmentation faults)
    // Use shared_ptr to atomic<bool> to avoid capturing 'this' in thread lambda
    std::unordered_map<std::string, std::shared_ptr<std::atomic<bool>>> mqtt_thread_stop_flags_;
    std::unordered_map<std::string, std::thread> mqtt_threads_;
    std::unordered_map<std::string, int> mqtt_pipe_write_fds_; // Track pipe write FDs to close them on stop
    std::unordered_map<std::string, int> mqtt_stdout_backups_; // Track stdout backups to restore on stop
    mutable std::mutex mqtt_thread_mutex_; // Separate mutex for MQTT thread management
    
    // Thread management for RTSP connection monitoring and auto-reconnect
    std::unordered_map<std::string, std::shared_ptr<std::atomic<bool>>> rtsp_monitor_stop_flags_;
    std::unordered_map<std::string, std::thread> rtsp_monitor_threads_;
    mutable std::unordered_map<std::string, std::chrono::steady_clock::time_point> rtsp_last_activity_; // Track last frame received time (mutable for const methods)
    std::unordered_map<std::string, std::atomic<int>> rtsp_reconnect_attempts_; // Track reconnect attempts
    mutable std::mutex rtsp_monitor_mutex_; // Separate mutex for RTSP monitor thread management
    
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
     * @param nodes Pipeline nodes to start
     * @param instanceId Instance ID for accessing instance parameters
     * @param isRestart If true, this is a restart (use longer delays for model initialization)
     */
    bool startPipeline(const std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>>& nodes, const std::string& instanceId, bool isRestart = false);
    
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
     * @brief Stop and cleanup MQTT thread for an instance
     * @param instanceId Instance ID
     */
    void stopMqttThread(const std::string& instanceId);
    
    /**
     * @brief Start logging thread for an instance (if needed)
     * @param instanceId Instance ID
     */
    void startLoggingThread(const std::string& instanceId);
    
    /**
     * @brief Start video loop monitoring thread for file-based instances
     * @param instanceId Instance ID
     */
    void startVideoLoopThread(const std::string& instanceId);
    
    /**
     * @brief Stop video loop monitoring thread for an instance
     * @param instanceId Instance ID
     */
    void stopVideoLoopThread(const std::string& instanceId);
    
    /**
     * @brief Start RTSP connection monitoring thread for an instance
     * Monitors RTSP connection status and auto-reconnects if stream is lost
     * @param instanceId Instance ID
     */
    void startRTSPMonitorThread(const std::string& instanceId);
    
    /**
     * @brief Stop RTSP connection monitoring thread for an instance
     * @param instanceId Instance ID
     */
    void stopRTSPMonitorThread(const std::string& instanceId);
    
    /**
     * @brief Update RTSP last activity time (called when frame is received)
     * @param instanceId Instance ID
     */
    void updateRTSPActivity(const std::string& instanceId);
    
    /**
     * @brief Attempt to reconnect RTSP stream for an instance
     * @param instanceId Instance ID
     * @return true if reconnection was successful
     */
    bool reconnectRTSPStream(const std::string& instanceId);
};

