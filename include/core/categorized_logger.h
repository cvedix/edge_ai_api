#pragma once

#include <plog/Log.h>
#include <plog/Appenders/ConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include "core/log_manager.h"
#include "core/logging_flags.h"
#include "core/env_config.h"
#include <memory>
#include <algorithm>
#include <string>

/**
 * @brief Categorized Logger with automatic log routing
 * 
 * Automatically routes logs to appropriate category based on log prefix:
 * - [API] -> api/ directory
 * - [Instance] -> instance/ directory
 * - [SDKOutput] -> sdk_output/ directory
 * - Others -> general/ directory
 */
namespace CategorizedLogger {

/**
 * @brief Initialize categorized logger
 * 
 * @param log_dir Base directory for logs (default: ./logs)
 * @param log_level Log level (default: INFO)
 * @param enable_console Whether to also log to console (default: true)
 */
inline void init(const std::string& log_dir = "",
                 plog::Severity log_level = plog::info,
                 bool enable_console = true) {
    
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
    
    // Initialize log manager
    LogManager::init(log_dir);
    
    // Get console appender if enabled
    static plog::ConsoleAppender<plog::TxtFormatter> consoleAppender;
    
    // Initialize PLOG with console appender
    plog::Logger<0>* logger = nullptr;
    if (enable_console) {
        logger = &plog::init(log_level, &consoleAppender);
    } else {
        logger = &plog::init(log_level);
    }
    
    // Add appenders based on logging flags
    if (isApiLoggingEnabled()) {
        auto* api_appender = LogManager::getAppender(LogManager::Category::API);
        if (api_appender) {
            logger->addAppender(api_appender);
        }
    }
    
    if (isInstanceLoggingEnabled()) {
        auto* instance_appender = LogManager::getAppender(LogManager::Category::INSTANCE);
        if (instance_appender) {
            logger->addAppender(instance_appender);
        }
    }
    
    if (isSdkOutputLoggingEnabled()) {
        auto* sdk_appender = LogManager::getAppender(LogManager::Category::SDK_OUTPUT);
        if (sdk_appender) {
            logger->addAppender(sdk_appender);
        }
    }
    
    // Always add general appender
    auto* general_appender = LogManager::getAppender(LogManager::Category::GENERAL);
    if (general_appender) {
        logger->addAppender(general_appender);
    }
    
    std::string log_dir_display = log_dir.empty() ? "./logs" : log_dir;
    PLOG_INFO << "========================================";
    PLOG_INFO << "Categorized Logger initialized";
    PLOG_INFO << "Log directory: " << log_dir_display;
    PLOG_INFO << "Log categories:";
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "  - API logs: " << LogManager::getCategoryDir(LogManager::Category::API);
    }
    if (isInstanceLoggingEnabled()) {
        PLOG_INFO << "  - Instance logs: " << LogManager::getCategoryDir(LogManager::Category::INSTANCE);
    }
    if (isSdkOutputLoggingEnabled()) {
        PLOG_INFO << "  - SDK output logs: " << LogManager::getCategoryDir(LogManager::Category::SDK_OUTPUT);
    }
    PLOG_INFO << "  - General logs: " << LogManager::getCategoryDir(LogManager::Category::GENERAL);
    PLOG_INFO << "Log rotation: Daily (YYYY-MM-DD format)";
    PLOG_INFO << "Cleanup: Monthly (auto-delete logs older than 30 days)";
    PLOG_INFO << "Disk space monitoring: Enabled (cleanup when > 85% full)";
    PLOG_INFO << "========================================";
}

/**
 * @brief Shutdown logger and cleanup thread
 */
inline void shutdown() {
    LogManager::stopCleanupThread();
}

} // namespace CategorizedLogger

