#pragma once

#include <cstdlib>
#include <string>
#include <cstdint>
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <climits>
#include <filesystem>

/**
 * @brief Helper functions to parse environment variables
 * 
 * Provides utilities to read and parse environment variables
 * with default values and validation.
 */
namespace EnvConfig {

/**
 * @brief Get string environment variable
 * @param name Variable name
 * @param default_value Default value if not set
 * @return String value or default
 */
inline std::string getString(const char* name, const std::string& default_value = "") {
    const char* value = std::getenv(name);
    return value ? std::string(value) : default_value;
}

/**
 * @brief Get integer environment variable
 * @param name Variable name
 * @param default_value Default value if not set
 * @param min_value Minimum allowed value (optional)
 * @param max_value Maximum allowed value (optional)
 * @return Integer value or default
 */
inline int getInt(const char* name, int default_value, int min_value = INT32_MIN, int max_value = INT32_MAX) {
    const char* value = std::getenv(name);
    if (!value) {
        return default_value;
    }
    
    try {
        int int_value = std::stoi(value);
        if (int_value < min_value || int_value > max_value) {
            std::cerr << "Warning: " << name << "=" << value 
                     << " is out of range [" << min_value << ", " << max_value 
                     << "]. Using default: " << default_value << std::endl;
            return default_value;
        }
        return int_value;
    } catch (const std::exception& e) {
        std::cerr << "Warning: Invalid " << name << "='" << value 
                 << "': " << e.what() << ". Using default: " << default_value << std::endl;
        return default_value;
    }
}

/**
 * @brief Get unsigned integer environment variable
 * @param name Variable name
 * @param default_value Default value if not set
 * @param max_value Maximum allowed value (optional)
 * @return Unsigned integer value or default
 */
inline uint32_t getUInt32(const char* name, uint32_t default_value, uint32_t max_value = UINT32_MAX) {
    const char* value = std::getenv(name);
    if (!value) {
        return default_value;
    }
    
    try {
        int int_value = std::stoi(value);
        if (int_value < 0) {
            std::cerr << "Warning: " << name << "=" << value 
                     << " must be non-negative. Using default: " << default_value << std::endl;
            return default_value;
        }
        uint32_t uint_value = static_cast<uint32_t>(int_value);
        if (uint_value > max_value) {
            std::cerr << "Warning: " << name << "=" << value 
                     << " exceeds maximum " << max_value 
                     << ". Using default: " << default_value << std::endl;
            return default_value;
        }
        return uint_value;
    } catch (const std::exception& e) {
        std::cerr << "Warning: Invalid " << name << "='" << value 
                 << "': " << e.what() << ". Using default: " << default_value << std::endl;
        return default_value;
    }
}

/**
 * @brief Get size_t environment variable (for sizes, limits)
 * @param name Variable name
 * @param default_value Default value if not set
 * @return size_t value or default
 */
inline size_t getSizeT(const char* name, size_t default_value) {
    const char* value = std::getenv(name);
    if (!value) {
        return default_value;
    }
    
    try {
        int int_value = std::stoi(value);
        if (int_value < 0) {
            std::cerr << "Warning: " << name << "=" << value 
                     << " must be non-negative. Using default: " << default_value << std::endl;
            return default_value;
        }
        return static_cast<size_t>(int_value);
    } catch (const std::exception& e) {
        std::cerr << "Warning: Invalid " << name << "='" << value 
                 << "': " << e.what() << ". Using default: " << default_value << std::endl;
        return default_value;
    }
}

/**
 * @brief Get double environment variable
 * @param name Variable name
 * @param default_value Default value if not set
 * @param min_value Minimum allowed value (optional)
 * @param max_value Maximum allowed value (optional)
 * @return Double value or default
 */
inline double getDouble(const char* name, double default_value, double min_value = -1e10, double max_value = 1e10) {
    const char* value = std::getenv(name);
    if (!value) {
        return default_value;
    }
    
    try {
        double double_value = std::stod(value);
        if (double_value < min_value || double_value > max_value) {
            std::cerr << "Warning: " << name << "=" << value 
                     << " is out of range [" << min_value << ", " << max_value 
                     << "]. Using default: " << default_value << std::endl;
            return default_value;
        }
        return double_value;
    } catch (const std::exception& e) {
        std::cerr << "Warning: Invalid " << name << "='" << value 
                 << "': " << e.what() << ". Using default: " << default_value << std::endl;
        return default_value;
    }
}

/**
 * @brief Get boolean environment variable
 * @param name Variable name
 * @param default_value Default value if not set
 * @return Boolean value or default
 */
inline bool getBool(const char* name, bool default_value) {
    const char* value = std::getenv(name);
    if (!value) {
        return default_value;
    }
    
    std::string str_value = value;
    std::transform(str_value.begin(), str_value.end(), str_value.begin(), ::tolower);
    
    if (str_value == "1" || str_value == "true" || str_value == "yes" || str_value == "on") {
        return true;
    } else if (str_value == "0" || str_value == "false" || str_value == "no" || str_value == "off") {
        return false;
    }
    
    std::cerr << "Warning: Invalid " << name << "='" << value 
             << "'. Expected boolean (true/false, 1/0, yes/no, on/off). Using default: " 
             << (default_value ? "true" : "false") << std::endl;
    return default_value;
}

/**
 * @brief Parse log level from string
 * Note: This function requires drogon headers to be included
 * @param level_str Log level string (TRACE, DEBUG, INFO, WARN, ERROR)
 * @param default_level Default log level
 * @return Log level enum value
 */
inline int parseLogLevelInt(const std::string& level_str, int default_level) {
    if (level_str.empty()) {
        return default_level;
    }
    
    std::string upper = level_str;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    
    // Map to trantor::Logger constants (defined in drogon)
    if (upper == "TRACE") return 0;  // kTrace
    if (upper == "DEBUG") return 1;  // kDebug
    if (upper == "INFO") return 2;   // kInfo
    if (upper == "WARN") return 3;   // kWarn
    if (upper == "ERROR") return 4;  // kError
    
    std::cerr << "Warning: Invalid LOG_LEVEL='" << level_str 
             << "'. Using default: INFO" << std::endl;
    return default_level;
}

/**
 * @brief Resolve data directory path intelligently
 * 
 * Priority:
 * 1. Environment variable (if set) - highest priority
 * 2. Use /var/lib/edge_ai_api/{subdir} as default
 * 
 * Directory will be created automatically if it doesn't exist.
 * 
 * @param env_var_name Environment variable name (e.g., "SOLUTIONS_DIR")
 * @param subdir Subdirectory name under /var/lib/edge_ai_api (e.g., "solutions")
 * @return Resolved directory path
 */
inline std::string resolveDataDir(const char* env_var_name, const std::string& subdir) {
    // 1. Check environment variable first (highest priority)
    const char* env_value = std::getenv(env_var_name);
    if (env_value && strlen(env_value) > 0) {
        std::string path = std::string(env_value);
        // Ensure directory exists
        try {
            std::filesystem::create_directories(path);
        } catch (...) {
            // Log error but continue - storage classes will handle creation
        }
        return path;
    }
    
    // 2. Use /var/lib/edge_ai_api/{subdir} as default
    std::string default_path = "/var/lib/edge_ai_api/" + subdir;
    
    // Ensure directory exists
    try {
        std::filesystem::create_directories(default_path);
    } catch (const std::exception& e) {
        // If can't create /var/lib (need root), try user directory as fallback
        const char* home = std::getenv("HOME");
        if (home) {
            std::string fallback_path = std::string(home) + "/.local/share/edge_ai_api/" + subdir;
            try {
                std::filesystem::create_directories(fallback_path);
                std::cerr << "[EnvConfig] Cannot create " << default_path 
                         << " (permission denied), using " << fallback_path << std::endl;
                return fallback_path;
            } catch (...) {
                // Last resort: use current directory
                std::cerr << "[EnvConfig] Warning: Cannot create data directories, using current directory" << std::endl;
                return "./" + subdir;
            }
        }
        // Last resort: use current directory
        std::cerr << "[EnvConfig] Warning: Cannot create " << default_path 
                 << ", using current directory" << std::endl;
        return "./" + subdir;
    }
    
    return default_path;
}

/**
 * @brief Resolve config file path intelligently with 3-tier fallback
 * 
 * Priority:
 * 1. CONFIG_FILE environment variable (if set) - highest priority
 * 2. Try paths in order:
 *    - ./config.json (current directory)
 *    - /opt/edge_ai_api/config/config.json (production)
 *    - /etc/edge_ai_api/config.json (system)
 *    - ~/.config/edge_ai_api/config.json (user config - fallback)
 *    - ./config.json (last resort)
 * 
 * Parent directories will be created automatically if needed.
 * 
 * @return Resolved config file path
 */
inline std::string resolveConfigPath() {
    // Priority 1: Environment variable (highest priority)
    const char* env_config_file = std::getenv("CONFIG_FILE");
    if (env_config_file && strlen(env_config_file) > 0) {
        std::string path = std::string(env_config_file);
        // Create parent directory if needed
        try {
            std::filesystem::path filePath(path);
            if (filePath.has_parent_path()) {
                std::filesystem::create_directories(filePath.parent_path());
            }
            std::cerr << "[EnvConfig] Using config file from CONFIG_FILE: " << path << std::endl;
            return path;
        } catch (const std::filesystem::filesystem_error& e) {
            if (e.code() == std::errc::permission_denied) {
                std::cerr << "[EnvConfig] ⚠ Cannot create directory for " << path 
                         << " (permission denied), trying fallback..." << std::endl;
            } else {
                std::cerr << "[EnvConfig] ⚠ Error with CONFIG_FILE path " << path 
                         << ": " << e.what() << ", trying fallback..." << std::endl;
            }
        } catch (...) {
            std::cerr << "[EnvConfig] ⚠ Error with CONFIG_FILE path " << path 
                     << ", trying fallback..." << std::endl;
        }
    }
    
    // Priority 2: Try paths in order with fallback pattern
    // Tier 1: Current directory (always accessible)
    std::string current_dir_path = "./config.json";
    if (std::filesystem::exists(current_dir_path)) {
        std::cerr << "[EnvConfig] ✓ Found existing config file: " << current_dir_path 
                 << " (current directory)" << std::endl;
        return current_dir_path;
    }
    
    // Tier 2: Production path (/opt/edge_ai_api/config/config.json)
    std::string production_path = "/opt/edge_ai_api/config/config.json";
    if (std::filesystem::exists(production_path)) {
        std::cerr << "[EnvConfig] ✓ Found existing config file: " << production_path 
                 << " (production)" << std::endl;
        return production_path;
    }
    
    // Try to create production directory
    try {
        std::filesystem::path filePath(production_path);
        if (filePath.has_parent_path()) {
            std::filesystem::create_directories(filePath.parent_path());
        }
        std::cerr << "[EnvConfig] ✓ Created directory and will use: " << production_path 
                 << " (production)" << std::endl;
        return production_path;
    } catch (const std::filesystem::filesystem_error& e) {
        if (e.code() == std::errc::permission_denied) {
            std::cerr << "[EnvConfig] ⚠ Cannot create " << production_path 
                     << " (permission denied), trying fallback..." << std::endl;
        } else {
            std::cerr << "[EnvConfig] ⚠ Error creating " << production_path 
                     << ": " << e.what() << ", trying fallback..." << std::endl;
        }
    } catch (...) {
        std::cerr << "[EnvConfig] ⚠ Error with " << production_path << ", trying fallback..." << std::endl;
    }
    
    // Tier 3: System path (/etc/edge_ai_api/config.json)
    std::string system_path = "/etc/edge_ai_api/config.json";
    if (std::filesystem::exists(system_path)) {
        std::cerr << "[EnvConfig] ✓ Found existing config file: " << system_path 
                 << " (system)" << std::endl;
        return system_path;
    }
    
    // Try to create system directory
    try {
        std::filesystem::path filePath(system_path);
        if (filePath.has_parent_path()) {
            std::filesystem::create_directories(filePath.parent_path());
        }
        std::cerr << "[EnvConfig] ✓ Created directory and will use: " << system_path 
                 << " (system)" << std::endl;
        return system_path;
    } catch (const std::filesystem::filesystem_error& e) {
        if (e.code() == std::errc::permission_denied) {
            std::cerr << "[EnvConfig] ⚠ Cannot create " << system_path 
                     << " (permission denied), trying fallback..." << std::endl;
        } else {
            std::cerr << "[EnvConfig] ⚠ Error creating " << system_path 
                     << ": " << e.what() << ", trying fallback..." << std::endl;
        }
    } catch (...) {
        std::cerr << "[EnvConfig] ⚠ Error with " << system_path << ", trying fallback..." << std::endl;
    }
    
    // Fallback 1: User config directory (~/.config/edge_ai_api/config.json)
    const char* home = std::getenv("HOME");
    if (home) {
        std::string user_config = std::string(home) + "/.config/edge_ai_api/config.json";
        try {
            std::filesystem::path filePath(user_config);
            if (filePath.has_parent_path()) {
                std::filesystem::create_directories(filePath.parent_path());
            }
            std::cerr << "[EnvConfig] ✓ Using fallback user config: " << user_config << std::endl;
            return user_config;
        } catch (...) {
            std::cerr << "[EnvConfig] ⚠ Cannot create user config directory, using last resort..." << std::endl;
        }
    }
    
    // Last resort: Current directory (always works)
    std::cerr << "[EnvConfig] ✓ Using last resort: ./config.json (current directory)" << std::endl;
    std::cerr << "[EnvConfig] ℹ Note: To use production path, run: sudo mkdir -p /opt/edge_ai_api/config" << std::endl;
    return "./config.json";
}

} // namespace EnvConfig

