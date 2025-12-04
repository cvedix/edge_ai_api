#include "core/log_manager.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>
#include <filesystem>
#include <sys/statvfs.h>
#include <unistd.h>

namespace fs = std::filesystem;

// Static member definitions
std::string LogManager::base_dir_ = "./logs";
int LogManager::max_disk_usage_percent_ = 85;
int LogManager::cleanup_interval_hours_ = 24;

std::unique_ptr<plog::RollingFileAppender<plog::TxtFormatter>> LogManager::api_appender_;
std::unique_ptr<plog::RollingFileAppender<plog::TxtFormatter>> LogManager::instance_appender_;
std::unique_ptr<plog::RollingFileAppender<plog::TxtFormatter>> LogManager::sdk_output_appender_;
std::unique_ptr<plog::RollingFileAppender<plog::TxtFormatter>> LogManager::general_appender_;

std::unique_ptr<std::thread> LogManager::cleanup_thread_;
std::atomic<bool> LogManager::cleanup_running_{false};
std::mutex LogManager::cleanup_mutex_;

void LogManager::init(const std::string& base_dir, int max_disk_usage_percent, int cleanup_interval_hours) {
    std::lock_guard<std::mutex> lock(cleanup_mutex_);
    
    // Get base directory from environment or use provided/default
    base_dir_ = base_dir.empty() 
        ? EnvConfig::getString("LOG_DIR", "./logs")
        : base_dir;
    
    // Get max disk usage from environment
    max_disk_usage_percent_ = EnvConfig::getInt("LOG_MAX_DISK_USAGE_PERCENT", max_disk_usage_percent, 50, 95);
    
    // Get cleanup interval from environment (in hours)
    cleanup_interval_hours_ = EnvConfig::getInt("LOG_CLEANUP_INTERVAL_HOURS", cleanup_interval_hours, 1, 168);
    
    // Create directory structure
    createDirectories();
    
    // Get today's date string
    std::string date_str = getDateString();
    
    // Max file size: 50MB per file
    size_t max_file_size = 50 * 1024 * 1024;
    // Max files: 0 = unlimited (we handle cleanup manually)
    int max_files = 0;
    
    // Initialize appenders for each category
    // Only create appenders if their respective logging flags are enabled
    if (isApiLoggingEnabled()) {
        std::string api_log_path = getLogFilePath(Category::API, date_str);
        api_appender_ = std::make_unique<plog::RollingFileAppender<plog::TxtFormatter>>(
            api_log_path.c_str(), max_file_size, max_files);
    }
    
    if (isInstanceLoggingEnabled()) {
        std::string instance_log_path = getLogFilePath(Category::INSTANCE, date_str);
        instance_appender_ = std::make_unique<plog::RollingFileAppender<plog::TxtFormatter>>(
            instance_log_path.c_str(), max_file_size, max_files);
    }
    
    if (isSdkOutputLoggingEnabled()) {
        std::string sdk_log_path = getLogFilePath(Category::SDK_OUTPUT, date_str);
        sdk_output_appender_ = std::make_unique<plog::RollingFileAppender<plog::TxtFormatter>>(
            sdk_log_path.c_str(), max_file_size, max_files);
    }
    
    // General appender (always created for general logs)
    std::string general_log_path = getLogFilePath(Category::GENERAL, date_str);
    general_appender_ = std::make_unique<plog::RollingFileAppender<plog::TxtFormatter>>(
        general_log_path.c_str(), max_file_size, max_files);
    
    // Start cleanup thread
    startCleanupThread();
}

plog::RollingFileAppender<plog::TxtFormatter>* LogManager::getAppender(Category category) {
    switch (category) {
        case Category::API:
            return api_appender_.get();
        case Category::INSTANCE:
            return instance_appender_.get();
        case Category::SDK_OUTPUT:
            return sdk_output_appender_.get();
        case Category::GENERAL:
        default:
            return general_appender_.get();
    }
}

std::string LogManager::getCategoryDir(Category category) {
    std::string category_dir = base_dir_;
    if (!base_dir_.empty() && base_dir_.back() != '/') {
        category_dir += "/";
    }
    
    switch (category) {
        case Category::API:
            category_dir += "api";
            break;
        case Category::INSTANCE:
            category_dir += "instance";
            break;
        case Category::SDK_OUTPUT:
            category_dir += "sdk_output";
            break;
        case Category::GENERAL:
        default:
            category_dir += "general";
            break;
    }
    
    return category_dir;
}

std::string LogManager::getCurrentLogFile(Category category) {
    std::string date_str = getDateString();
    return getLogFilePath(category, date_str);
}

void LogManager::startCleanupThread() {
    if (cleanup_running_.load()) {
        return; // Already running
    }
    
    cleanup_running_ = true;
    cleanup_thread_ = std::make_unique<std::thread>(cleanupThreadFunc);
}

void LogManager::stopCleanupThread() {
    cleanup_running_ = false;
    if (cleanup_thread_ && cleanup_thread_->joinable()) {
        cleanup_thread_->join();
    }
    cleanup_thread_.reset();
}

void LogManager::performCleanup() {
    std::lock_guard<std::mutex> lock(cleanup_mutex_);
    
    // Cleanup old logs (older than 1 month)
    cleanupOldLogs();
    
    // Check disk space and cleanup if needed
    cleanupOnLowDiskSpace();
}

double LogManager::getDiskUsagePercent(const std::string& path) {
    struct statvfs stat;
    if (statvfs(path.c_str(), &stat) != 0) {
        return -1.0; // Error
    }
    
    uint64_t total = stat.f_blocks * stat.f_frsize;
    uint64_t available = stat.f_bavail * stat.f_frsize;
    uint64_t used = total - available;
    
    if (total == 0) {
        return -1.0;
    }
    
    return (static_cast<double>(used) / static_cast<double>(total)) * 100.0;
}

uint64_t LogManager::getDirectorySize(const std::string& dir_path) {
    uint64_t total_size = 0;
    
    try {
        if (!fs::exists(dir_path) || !fs::is_directory(dir_path)) {
            return 0;
        }
        
        for (const auto& entry : fs::recursive_directory_iterator(dir_path)) {
            if (fs::is_regular_file(entry)) {
                total_size += fs::file_size(entry);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[LogManager] Error calculating directory size: " << e.what() << std::endl;
    }
    
    return total_size;
}

void LogManager::createDirectories() {
    try {
        // Create base directory
        fs::create_directories(base_dir_);
        
        // Create category directories
        fs::create_directories(getCategoryDir(Category::API));
        fs::create_directories(getCategoryDir(Category::INSTANCE));
        fs::create_directories(getCategoryDir(Category::SDK_OUTPUT));
        fs::create_directories(getCategoryDir(Category::GENERAL));
    } catch (const std::exception& e) {
        std::cerr << "[LogManager] Warning: Failed to create log directories: " << e.what() << std::endl;
    }
}

std::string LogManager::getDateString() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&time_t);
    
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << (tm->tm_year + 1900) << "-"
        << std::setw(2) << (tm->tm_mon + 1) << "-"
        << std::setw(2) << tm->tm_mday;
    
    return oss.str();
}

std::string LogManager::getLogFilePath(Category category, const std::string& date_str) {
    std::string category_dir = getCategoryDir(category);
    return category_dir + "/" + date_str + ".log";
}

void LogManager::cleanupOldLogs() {
    // Delete logs older than 30 days (1 month)
    int days_to_keep = EnvConfig::getInt("LOG_RETENTION_DAYS", 30, 1, 365);
    
    std::vector<std::string> categories = {
        getCategoryDir(Category::API),
        getCategoryDir(Category::INSTANCE),
        getCategoryDir(Category::SDK_OUTPUT),
        getCategoryDir(Category::GENERAL)
    };
    
    for (const auto& category_dir : categories) {
        try {
            deleteOldFiles(category_dir, days_to_keep);
        } catch (const std::exception& e) {
            std::cerr << "[LogManager] Error cleaning up old logs in " << category_dir 
                      << ": " << e.what() << std::endl;
        }
    }
}

void LogManager::cleanupOnLowDiskSpace() {
    double disk_usage = getDiskUsagePercent(base_dir_);
    
    if (disk_usage < 0) {
        return; // Error getting disk usage
    }
    
    if (disk_usage >= max_disk_usage_percent_) {
        std::cerr << "[LogManager] Warning: Disk usage is " << disk_usage 
                  << "%, exceeding threshold of " << max_disk_usage_percent_ 
                  << "%. Starting aggressive cleanup..." << std::endl;
        
        // Delete logs older than 7 days when disk is nearly full
        std::vector<std::string> categories = {
            getCategoryDir(Category::API),
            getCategoryDir(Category::INSTANCE),
            getCategoryDir(Category::SDK_OUTPUT),
            getCategoryDir(Category::GENERAL)
        };
        
        for (const auto& category_dir : categories) {
            try {
                deleteOldFiles(category_dir, 7); // Keep only last 7 days
            } catch (const std::exception& e) {
                std::cerr << "[LogManager] Error in aggressive cleanup: " << e.what() << std::endl;
            }
        }
        
        // Check again after cleanup
        disk_usage = getDiskUsagePercent(base_dir_);
        if (disk_usage >= max_disk_usage_percent_) {
            std::cerr << "[LogManager] Warning: Disk usage still high (" << disk_usage 
                      << "%) after cleanup. Consider manual cleanup or increasing disk space." << std::endl;
        }
    }
}

void LogManager::cleanupThreadFunc() {
    while (cleanup_running_.load()) {
        // Sleep for cleanup interval
        for (int i = 0; i < cleanup_interval_hours_ && cleanup_running_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::hours(1));
        }
        
        if (!cleanup_running_.load()) {
            break;
        }
        
        // Perform cleanup
        performCleanup();
    }
}

void LogManager::deleteOldFiles(const std::string& dir_path, int days_old) {
    if (!fs::exists(dir_path) || !fs::is_directory(dir_path)) {
        return;
    }
    
    auto now = std::chrono::system_clock::now();
    int deleted_count = 0;
    uint64_t freed_bytes = 0;
    
    try {
        for (const auto& entry : fs::directory_iterator(dir_path)) {
            if (fs::is_regular_file(entry)) {
                int age_days = getFileAgeDays(entry.path());
                
                if (age_days > days_old) {
                    uint64_t file_size = fs::file_size(entry.path());
                    fs::remove(entry.path());
                    deleted_count++;
                    freed_bytes += file_size;
                }
            }
        }
        
        if (deleted_count > 0) {
            std::cerr << "[LogManager] Cleaned up " << deleted_count << " log file(s) older than " 
                      << days_old << " days in " << dir_path 
                      << ", freed " << (freed_bytes / (1024 * 1024)) << " MB" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[LogManager] Error deleting old files: " << e.what() << std::endl;
    }
}

int LogManager::getFileAgeDays(const fs::path& file_path) {
    try {
        auto file_time = fs::last_write_time(file_path);
        auto now = std::chrono::system_clock::now();
        
        // Convert file_time to system_clock::time_point
        auto file_time_tp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            file_time - fs::file_time_type::clock::now() + now);
        
        auto duration = now - file_time_tp;
        auto days = std::chrono::duration_cast<std::chrono::hours>(duration).count() / 24;
        
        return static_cast<int>(days);
    } catch (const std::exception& e) {
        return -1; // Error
    }
}

