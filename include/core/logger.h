#pragma once

#include <plog/Log.h>
#include <plog/Appenders/DailyRollingFileAppender.h>
#include <plog/Appenders/ConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Init.h>
#include <string>
#include <filesystem>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include "core/env_config.h"

/**
 * @brief Logger utility with daily log rotation using Plog
 * 
 * Provides a simple interface to initialize Plog with daily log rotation.
 * Logs are written to files named: log_YYYY-MM-DD.txt
 * 
 * Usage:
 *   Logger::init();  // Initialize with default settings
 *   PLOG_INFO << "Your log message";
 *   PLOG_ERROR << "Error message";
 */

namespace Logger {

/**
 * @brief Initialize Plog logger with daily rotation
 * 
 * Creates log directory if it doesn't exist and initializes Plog
 * with a daily rolling file appender that creates a new file each day.
 * 
 * @param log_dir Directory to store log files (default: ./logs)
 * @param log_level Log level (default: INFO)
 * @param max_days Maximum number of days to keep log files (0 = keep forever, default: 30)
 * @param roll_at_hour Hour of day to roll log file (0-23, default: 0 = midnight)
 * @param enable_console Whether to also log to console (default: true)
 */
inline void init(const std::string& log_dir = "", 
                 plog::Severity log_level = plog::info,
                 int max_days = 30,
                 int roll_at_hour = 0,
                 bool enable_console = true) {
    
    // Get log directory from environment or use default
    std::string log_directory = log_dir.empty() 
        ? EnvConfig::getString("LOG_DIR", "./logs") 
        : log_dir;
    
    // Get max_days from environment if available
    int env_max_days = EnvConfig::getInt("LOG_MAX_DAYS", max_days, 0, 365);
    max_days = env_max_days;
    
    // Create log directory if it doesn't exist
    try {
        std::filesystem::create_directories(log_directory);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Failed to create log directory '" << log_directory 
                  << "': " << e.what() << std::endl;
        std::cerr << "Logs will be written to current directory." << std::endl;
        log_directory = ".";
    }
    
    // Get log level from environment if not specified
    std::string log_level_str = EnvConfig::getString("LOG_LEVEL", "");
    if (!log_level_str.empty()) {
        std::string upper = log_level_str;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        
        if (upper == "NONE") log_level = plog::none;
        else if (upper == "FATAL") log_level = plog::fatal;
        else if (upper == "ERROR") log_level = plog::error;
        else if (upper == "WARNING" || upper == "WARN") log_level = plog::warning;
        else if (upper == "INFO") log_level = plog::info;
        else if (upper == "DEBUG") log_level = plog::debug;
        else if (upper == "VERBOSE") log_level = plog::verbose;
    }
    
    // Build log file path
    // DailyRollingFileAppender will automatically append date to filename
    std::string log_file_path = log_directory;
    if (!log_directory.empty() && log_directory.back() != '/') {
        log_file_path += "/";
    }
    log_file_path += "log.txt";
    
    // Initialize Plog with daily rolling file appender
    // DailyRollingFileAppender automatically creates new files daily
    // Format: log_YYYY-MM-DD.txt
    static plog::DailyRollingFileAppender<plog::TxtFormatter> dailyFileAppender(
        log_file_path.c_str(), 
        max_days,      // Maximum days to keep old log files (0 = keep forever)
        roll_at_hour   // Hour of day to roll (0 = midnight)
    );
    
    // Initialize with console appender if enabled
    if (enable_console) {
        static plog::ConsoleAppender<plog::TxtFormatter> consoleAppender;
        plog::init(log_level, &consoleAppender).addAppender(&dailyFileAppender);
    } else {
        plog::init(log_level, &dailyFileAppender);
    }
    
    PLOG_INFO << "========================================";
    PLOG_INFO << "Logger initialized";
    PLOG_INFO << "Log directory: " << log_directory;
    PLOG_INFO << "Log file pattern: " << log_file_path << " (will be: log_YYYY-MM-DD.txt)";
    PLOG_INFO << "Log level: " << plog::severityToString(log_level);
    PLOG_INFO << "Max days to keep: " << (max_days == 0 ? "unlimited" : std::to_string(max_days));
    PLOG_INFO << "Roll at hour: " << roll_at_hour << ":00";
    PLOG_INFO << "Console logging: " << (enable_console ? "enabled" : "disabled");
    PLOG_INFO << "========================================";
}

/**
 * @brief Get current log file path
 * 
 * Returns the path to today's log file based on the log directory.
 * DailyRollingFileAppender creates files with pattern: log_YYYY-MM-DD.txt
 * 
 * @param log_dir Log directory (default: ./logs)
 * @return Full path to today's log file
 */
inline std::string getCurrentLogFile(const std::string& log_dir = "./logs") {
    std::time_t now = std::time(nullptr);
    std::tm* timeinfo = std::localtime(&now);
    
    std::ostringstream filename;
    filename << log_dir;
    if (!log_dir.empty() && log_dir.back() != '/') {
        filename << "/";
    }
    filename << "log_" 
             << std::setfill('0') << std::setw(4) << (1900 + timeinfo->tm_year) << "-"
             << std::setfill('0') << std::setw(2) << (timeinfo->tm_mon + 1) << "-"
             << std::setfill('0') << std::setw(2) << timeinfo->tm_mday
             << ".txt";
    
    return filename.str();
}

} // namespace Logger

