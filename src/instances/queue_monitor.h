#pragma once

#include <string>
#include <map>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <memory>
#include <regex>

/**
 * @brief Monitor queue status for instances and proactively clear queues
 * 
 * Monitors queue full warnings and takes action to prevent deadlock:
 * - Tracks queue full warning frequency
 * - Automatically clears/restarts nodes when queue is consistently full
 * - Prevents deadlock by proactive queue management
 */
class QueueMonitor {
public:
    struct QueueStats {
        std::atomic<int> warning_count{0};
        std::atomic<int> drop_count{0};
        std::chrono::steady_clock::time_point last_warning_time;
        std::chrono::steady_clock::time_point first_warning_time;
        bool is_monitoring{false};
    };

    static QueueMonitor& getInstance() {
        static QueueMonitor instance;
        return instance;
    }

    /**
     * @brief Record a queue full warning for an instance
     * @param instanceId Instance ID
     * @param nodeName Node name that has queue full
     */
    void recordQueueFullWarning(const std::string& instanceId, const std::string& nodeName);

    /**
     * @brief Check if instance needs queue clearing
     * @param instanceId Instance ID
     * @return true if queue should be cleared
     */
    bool shouldClearQueue(const std::string& instanceId);

    /**
     * @brief Clear queue stats for an instance
     * @param instanceId Instance ID
     */
    void clearStats(const std::string& instanceId);

    /**
     * @brief Get queue stats for an instance
     * @param instanceId Instance ID
     * @return QueueStats or nullptr if not found
     */
    QueueStats* getStats(const std::string& instanceId);

    /**
     * @brief Start monitoring thread
     */
    void startMonitoring();

    /**
     * @brief Stop monitoring thread
     */
    void stopMonitoring();

    /**
     * @brief Set threshold for auto-clear (warnings per second)
     * @param threshold Warnings per second threshold
     */
    void setAutoClearThreshold(double threshold);

    /**
     * @brief Set time window for monitoring (seconds)
     * @param window Time window in seconds
     */
    void setMonitoringWindow(int window);

    /**
     * @brief Parse log line to detect queue full warnings
     * @param logLine Log line to parse
     * @return Instance ID if found, empty string otherwise
     */
    std::string parseLogLine(const std::string& logLine);

    /**
     * @brief Enable log file parsing
     * @param logFilePath Path to log file to parse
     */
    void enableLogParsing(const std::string& logFilePath);

private:
    QueueMonitor();
    ~QueueMonitor();

    void monitoringThread();

    std::map<std::string, QueueStats> instance_stats_;
    std::mutex stats_mutex_;
    std::atomic<bool> running_{false};
    std::thread monitoring_thread_;
    
    // Configuration
    double auto_clear_threshold_{50.0};  // Warnings per second
    int monitoring_window_{5};  // Seconds
    int max_warnings_before_clear_{100};  // Max warnings before clearing
    
    // Log parsing
    std::string log_file_path_;
    std::atomic<bool> log_parsing_enabled_{false};
    std::mutex log_file_mutex_;
    
    // Map node names to instance IDs (for log parsing)
    std::map<std::string, std::string> node_to_instance_map_;
    std::mutex node_map_mutex_;
    
    void parseLogFile();
    void updateNodeToInstanceMap();
};

