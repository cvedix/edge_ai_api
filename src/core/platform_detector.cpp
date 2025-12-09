#include "core/platform_detector.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>

namespace PlatformDetector {

bool isJetson() {
    // Check for Jetson by looking for NVIDIA Tegra in /proc/device-tree/model
    try {
        std::ifstream modelFile("/proc/device-tree/model");
        if (modelFile.is_open()) {
            std::string model;
            std::getline(modelFile, model);
            modelFile.close();
            
            // Convert to lowercase for comparison
            std::transform(model.begin(), model.end(), model.begin(), ::tolower);
            
            // Check for Jetson keywords
            if (model.find("jetson") != std::string::npos ||
                model.find("tegra") != std::string::npos) {
                return true;
            }
        }
    } catch (...) {
        // Ignore errors
    }
    
    // Also check /sys/firmware/devicetree/base/model
    try {
        std::ifstream modelFile("/sys/firmware/devicetree/base/model");
        if (modelFile.is_open()) {
            std::string model;
            std::getline(modelFile, model);
            modelFile.close();
            
            std::transform(model.begin(), model.end(), model.begin(), ::tolower);
            if (model.find("jetson") != std::string::npos ||
                model.find("tegra") != std::string::npos) {
                return true;
            }
        }
    } catch (...) {
        // Ignore errors
    }
    
    return false;
}

bool isNVIDIA() {
    // Check for NVIDIA GPU by looking for nvidia-smi or /dev/nvidia*
    // First check if nvidia-smi exists and works
    FILE* pipe = popen("nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null | head -1", "r");
    if (pipe) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            pclose(pipe);
            // If we got output, NVIDIA GPU is present
            return strlen(buffer) > 0;
        }
        pclose(pipe);
    }
    
    // Fallback: check for /dev/nvidia0
    std::ifstream nvidiaDev("/dev/nvidia0");
    if (nvidiaDev.good()) {
        nvidiaDev.close();
        return true;
    }
    
    return false;
}

bool isMSDK() {
    // Check for Intel MSDK by looking for libmfx or intel_gpu_top
    // Check if libmfx is available
    FILE* pipe = popen("ldconfig -p 2>/dev/null | grep -i libmfx", "r");
    if (pipe) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            pclose(pipe);
            return strlen(buffer) > 0;
        }
        pclose(pipe);
    }
    
    // Check for intel_gpu_top
    pipe = popen("which intel_gpu_top 2>/dev/null", "r");
    if (pipe) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            pclose(pipe);
            return strlen(buffer) > 0;
        }
        pclose(pipe);
    }
    
    return false;
}

bool isVAAPI() {
    // Check for VAAPI by looking for libva or /dev/dri/renderD*
    // Check if libva is available
    FILE* pipe = popen("ldconfig -p 2>/dev/null | grep -i libva", "r");
    if (pipe) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            pclose(pipe);
            return strlen(buffer) > 0;
        }
        pclose(pipe);
    }
    
    // Check for /dev/dri/renderD* devices
    pipe = popen("ls /dev/dri/renderD* 2>/dev/null | head -1", "r");
    if (pipe) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            pclose(pipe);
            return strlen(buffer) > 0;
        }
        pclose(pipe);
    }
    
    return false;
}

std::string detectPlatform() {
    // Priority order: jetson > nvidia > msdk > vaapi > auto
    if (isJetson()) {
        return "jetson";
    }
    
    if (isNVIDIA()) {
        return "nvidia";
    }
    
    if (isMSDK()) {
        return "msdk";
    }
    
    if (isVAAPI()) {
        return "vaapi";
    }
    
    // Default to auto
    return "auto";
}

} // namespace PlatformDetector

