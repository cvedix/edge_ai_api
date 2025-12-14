#include "instances/queue_monitor.h"
#include "instances/instance_registry.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <regex>
#include <sstream>

QueueMonitor::QueueMonitor() {
    // Default configuration - aggressive thresholds to prevent deadlock
    auto_clear_threshold_ = 20.0;  // 20 warnings per second (reduced from 50)
    monitoring_window_ = 3;  // 3 seconds window (reduced from 5)
    max_warnings_before_clear_ = 20;  // Clear after 20 warnings (reduced from 100)
}

QueueMonitor::~QueueMonitor() {
    stopMonitoring();
}

void QueueMonitor::recordQueueFullWarning(const std::string& instanceId, const std::string& nodeName) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    auto& stats = instance_stats_[instanceId];
    stats.warning_count.fetch_add(1);
    stats.drop_count.fetch_add(1);
    
    auto now = std::chrono::steady_clock::now();
    
    if (stats.warning_count.load() == 1) {
        stats.first_warning_time = now;
    }
    stats.last_warning_time = now;
    stats.is_monitoring = true;
    
    // Log immediately for first few warnings to help debugging
    int count = stats.warning_count.load();
    if (count <= 5) {
        std::cerr << "[QueueMonitor] Instance " << instanceId 
                 << " queue full warning #" << count 
                 << " from node: " << nodeName << std::endl;
    }
    
    // Log if threshold exceeded
    if (count > 0 && count % 10 == 0) {  // Reduced from 50 to 10 for faster detection
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - stats.first_warning_time).count();
        if (elapsed > 0) {
            double rate = static_cast<double>(count) / elapsed;
            std::cerr << "[QueueMonitor] Instance " << instanceId 
                     << " queue full warnings: " << count 
                     << " in " << elapsed << "s (rate: " << rate << " warnings/s)" << std::endl;
            
            if (rate > auto_clear_threshold_) {
                std::cerr << "[QueueMonitor] WARNING: Queue full rate (" << rate 
                         << " warnings/s) exceeds threshold (" << auto_clear_threshold_ 
                         << "). Consider clearing queue or reducing frame rate." << std::endl;
            }
        }
    }
}

bool QueueMonitor::shouldClearQueue(const std::string& instanceId) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    auto it = instance_stats_.find(instanceId);
    if (it == instance_stats_.end()) {
        return false;
    }
    
    auto& stats = it->second;
    int count = stats.warning_count.load();
    
    // Clear if warning count exceeds threshold
    if (count >= max_warnings_before_clear_) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - stats.first_warning_time).count();
        
        if (elapsed > 0) {
            double rate = static_cast<double>(count) / elapsed;
            if (rate > auto_clear_threshold_) {
                std::cerr << "[QueueMonitor] Queue clearing recommended for instance " 
                         << instanceId << " (rate: " << rate << " warnings/s)" << std::endl;
                return true;
            }
        }
    }
    
    return false;
}

void QueueMonitor::clearStats(const std::string& instanceId) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    instance_stats_.erase(instanceId);
}

QueueMonitor::QueueStats* QueueMonitor::getStats(const std::string& instanceId) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    auto it = instance_stats_.find(instanceId);
    if (it != instance_stats_.end()) {
        return &it->second;
    }
    return nullptr;
}

void QueueMonitor::startMonitoring() {
    if (running_.load()) {
        return;
    }
    
    running_.store(true);
    monitoring_thread_ = std::thread(&QueueMonitor::monitoringThread, this);
    std::cerr << "[QueueMonitor] Started queue monitoring thread" << std::endl;
}

void QueueMonitor::stopMonitoring() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }
    std::cerr << "[QueueMonitor] Stopped queue monitoring thread" << std::endl;
}

void QueueMonitor::setAutoClearThreshold(double threshold) {
    auto_clear_threshold_ = threshold;
    std::cerr << "[QueueMonitor] Auto-clear threshold set to " << threshold << " warnings/s" << std::endl;
}

void QueueMonitor::setMonitoringWindow(int window) {
    monitoring_window_ = window;
    std::cerr << "[QueueMonitor] Monitoring window set to " << window << " seconds" << std::endl;
}

void QueueMonitor::monitoringThread() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(monitoring_window_));
        
        if (!running_.load()) {
            break;
        }
        
        // Parse log file if enabled
        if (log_parsing_enabled_.load()) {
            parseLogFile();
        }
        
        // Check all instances for queue issues
        std::lock_guard<std::mutex> lock(stats_mutex_);
        auto now = std::chrono::steady_clock::now();
        
        for (auto& [instanceId, stats] : instance_stats_) {
            if (!stats.is_monitoring) {
                continue;
            }
            
            int count = stats.warning_count.load();
            if (count == 0) {
                continue;
            }
            
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - stats.first_warning_time).count();
            
            if (elapsed > 0) {
                double rate = static_cast<double>(count) / elapsed;
                
                // Log periodic status
                if (count % 100 == 0) {
                    std::cerr << "[QueueMonitor] Instance " << instanceId 
                             << ": " << count << " warnings in " << elapsed 
                             << "s (rate: " << rate << " warnings/s)" << std::endl;
                }
                
                // Auto-clear recommendation
                if (rate > auto_clear_threshold_ && count >= max_warnings_before_clear_) {
                    std::cerr << "[QueueMonitor] RECOMMENDATION: Clear queue for instance " 
                             << instanceId << " (rate: " << rate 
                             << " warnings/s exceeds threshold: " << auto_clear_threshold_ << ")" << std::endl;
                }
            }
            
            // Reset stats if no warnings for a while
            auto time_since_last = std::chrono::duration_cast<std::chrono::seconds>(
                now - stats.last_warning_time).count();
            if (time_since_last > 30) {
                stats.is_monitoring = false;
                stats.warning_count.store(0);
            }
        }
    }
}

std::string QueueMonitor::parseLogLine(const std::string& logLine) {
    // Pattern: [Warn] [node_name] queue full, dropping meta!
    // Node name format: node_type_instanceId (e.g., yolo_detector_90d86cfb-46fa-4214-8a3c-e7c737ccf369)
    // Extract node name from log line
    std::regex pattern(R"(\[Warn\].*?\[([^\]]+)\].*?queue full.*?dropping meta)");
    std::smatch match;
    
    if (std::regex_search(logLine, match, pattern)) {
        if (match.size() > 1) {
            std::string nodeName = match[1].str();
            
            // Extract instance ID from node name
            // Node name format: node_type_instanceId
            // Try to find UUID pattern (8-4-4-4-12 hex digits)
            std::regex uuidPattern(R"([a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12})");
            std::smatch uuidMatch;
            
            if (std::regex_search(nodeName, uuidMatch, uuidPattern)) {
                return uuidMatch[0].str();
            }
            
            // Fallback: Try to find instance ID from node_to_instance_map
            std::lock_guard<std::mutex> lock(node_map_mutex_);
            auto it = node_to_instance_map_.find(nodeName);
            if (it != node_to_instance_map_.end()) {
                return it->second;
            }
        }
    }
    
    return "";
}

void QueueMonitor::enableLogParsing(const std::string& logFilePath) {
    log_file_path_ = logFilePath;
    log_parsing_enabled_.store(true);
    std::cerr << "[QueueMonitor] Log parsing enabled for: " << logFilePath << std::endl;
}

void QueueMonitor::parseLogFile() {
    if (log_file_path_.empty()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(log_file_mutex_);
    
    static std::ifstream log_file(log_file_path_);
    static std::streampos last_position = 0;
    
    if (!log_file.is_open()) {
        log_file.open(log_file_path_);
        if (!log_file.is_open()) {
            return;
        }
        // Seek to end to read only new lines
        log_file.seekg(0, std::ios::end);
        last_position = log_file.tellg();
        return;
    }
    
    // Read only new lines since last check
    log_file.seekg(last_position);
    std::string line;
    while (std::getline(log_file, line)) {
        std::string instanceId = parseLogLine(line);
        if (!instanceId.empty()) {
            // Extract node name from line
            std::regex nodePattern(R"(\[([^\]]+)\])");
            std::smatch nodeMatch;
            if (std::regex_search(line, nodeMatch, nodePattern) && nodeMatch.size() > 1) {
                std::string nodeName = nodeMatch[1].str();
                recordQueueFullWarning(instanceId, nodeName);
            }
        }
    }
    
    // Update position for next read
    last_position = log_file.tellg();
}

void QueueMonitor::updateNodeToInstanceMap() {
    // This will be called periodically to update node-to-instance mapping
    // For now, we extract instance ID from node name pattern: node_type_instanceId
    // Example: yolo_detector_90d86cfb-46fa-4214-8a3c-e7c737ccf369
    // This is a simple approach - can be improved with actual instance registry integration
}

