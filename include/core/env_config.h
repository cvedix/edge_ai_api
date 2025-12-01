#pragma once

#include <cstdlib>
#include <string>
#include <cstdint>
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <climits>

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

} // namespace EnvConfig

