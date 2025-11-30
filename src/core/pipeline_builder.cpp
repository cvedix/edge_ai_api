#include "core/pipeline_builder.h"
#include <cvedix/nodes/src/cvedix_rtsp_src_node.h>
#include <cvedix/nodes/infers/cvedix_yunet_face_detector_node.h>
#include <cvedix/nodes/des/cvedix_file_des_node.h>
#include <cvedix/utils/cvedix_utils.h>
#include <cvedix/objects/shapes/cvedix_size.h>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <typeinfo>
#include <exception>
#include <mutex>
#include <iomanip>
#include <cmath>
// CVEDIX SDK uses experimental::filesystem, so we need to use it too for compatibility
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

// Static flag to ensure CVEDIX logger is initialized only once
static std::once_flag cvedix_init_flag;

// Initialize CVEDIX SDK logger (required before creating nodes)
static void ensureCVEDIXInitialized() {
    std::call_once(cvedix_init_flag, []() {
        try {
            CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
            CVEDIX_LOGGER_INIT();
            std::cerr << "[PipelineBuilder] CVEDIX SDK logger initialized" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[PipelineBuilder] Warning: Failed to initialize CVEDIX logger: " 
                      << e.what() << std::endl;
        } catch (...) {
            std::cerr << "[PipelineBuilder] Warning: Unknown error initializing CVEDIX logger" << std::endl;
        }
    });
}

std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>> PipelineBuilder::buildPipeline(
    const SolutionConfig& solution,
    const CreateInstanceRequest& req,
    const std::string& instanceId) {
    
    // Ensure CVEDIX SDK is initialized before creating nodes
    ensureCVEDIXInitialized();
    
    std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>> nodes;
    
    // Build nodes in pipeline order
    for (const auto& nodeConfig : solution.pipeline) {
        try {
            std::cerr << "[PipelineBuilder] Creating node: " << nodeConfig.nodeType 
                      << " (" << nodeConfig.nodeName << ")" << std::endl;
            
            auto node = createNode(nodeConfig, req, instanceId);
            if (node) {
                nodes.push_back(node);
                
                // Connect to previous node
                // attach_to() expects a vector of shared_ptr, not a raw pointer
                if (nodes.size() > 1) {
                    node->attach_to({nodes[nodes.size() - 2]});
                }
                std::cerr << "[PipelineBuilder] Successfully created and connected node: " 
                          << nodeConfig.nodeType << std::endl;
            } else {
                std::cerr << "[PipelineBuilder] Failed to create node: " << nodeConfig.nodeType 
                          << " (createNode returned nullptr)" << std::endl;
                throw std::runtime_error("Failed to create node: " + nodeConfig.nodeType);
            }
        } catch (const std::exception& e) {
            std::cerr << "[PipelineBuilder] Exception creating node " << nodeConfig.nodeType 
                      << ": " << e.what() << std::endl;
            throw; // Re-throw to be caught by caller
        } catch (...) {
            std::cerr << "[PipelineBuilder] Unknown exception creating node: " 
                      << nodeConfig.nodeType << std::endl;
            throw std::runtime_error("Unknown error creating node: " + nodeConfig.nodeType);
        }
    }
    
    std::cerr << "[PipelineBuilder] Successfully built pipeline with " << nodes.size() 
              << " nodes" << std::endl;
    return nodes;
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createNode(
    const SolutionConfig::NodeConfig& nodeConfig,
    const CreateInstanceRequest& req,
    const std::string& instanceId) {
    
    // Get node name with instanceId substituted
    std::string nodeName = nodeConfig.nodeName;
    std::string placeholder = "{instanceId}";
    size_t pos = nodeName.find(placeholder);
    while (pos != std::string::npos) {
        nodeName.replace(pos, placeholder.length(), instanceId);
        pos = nodeName.find(placeholder, pos + instanceId.length());
    }
    
    // Debug: log the final node name
    std::cerr << "[PipelineBuilder] Original node name template: '" << nodeConfig.nodeName << "'" << std::endl;
    std::cerr << "[PipelineBuilder] Node name after substitution: '" << nodeName << "'" << std::endl;
    
    // Build parameter map with substitutions
    std::map<std::string, std::string> params;
    for (const auto& param : nodeConfig.parameters) {
        std::string value = param.second;
        
        // Replace {instanceId}
        std::string placeholder = "{instanceId}";
        size_t pos = value.find(placeholder);
        while (pos != std::string::npos) {
            value.replace(pos, placeholder.length(), instanceId);
            pos = value.find(placeholder, pos + instanceId.length());
        }
        
        // Replace ${variable} from request
        // Map detectionSensitivity to threshold value
        if (param.first == "score_threshold" && value == "${detectionSensitivity}") {
            double threshold = mapDetectionSensitivity(req.detectionSensitivity);
            std::ostringstream oss;
            oss << threshold;
            value = oss.str();
        } else if (value == "${frameRateLimit}") {
            std::ostringstream oss;
            oss << req.frameRateLimit;
            value = oss.str();
        } else if (value == "${RTSP_URL}") {
            value = getRTSPUrl(req);
        } else if (param.first == "model_path" && value == "${MODEL_PATH}") {
            // Allow override model_path from additionalParams
            auto it = req.additionalParams.find("MODEL_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                value = it->second;
            } else {
                // Default to yunet.onnx if MODEL_PATH not provided
                value = "./cvedix_data/models/face/yunet.onnx";
            }
        }
        
        // Override model_path if provided in additionalParams (even if not using ${MODEL_PATH} placeholder)
        if (param.first == "model_path") {
            auto it = req.additionalParams.find("MODEL_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                value = it->second;
            }
        }
        
        params[param.first] = value;
    }
    
    // Create node based on type
    try {
        if (nodeConfig.nodeType == "rtsp_src") {
            return createRTSPSourceNode(nodeName, params, req);
        } else if (nodeConfig.nodeType == "yunet_face_detector") {
            return createFaceDetectorNode(nodeName, params, req);
        } else if (nodeConfig.nodeType == "file_des") {
            return createFileDestinationNode(nodeName, params, instanceId);
        } else {
            std::cerr << "[PipelineBuilder] Unknown node type: " << nodeConfig.nodeType << std::endl;
            throw std::runtime_error("Unknown node type: " + nodeConfig.nodeType);
        }
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Error in createNode for type " << nodeConfig.nodeType 
                  << ": " << e.what() << std::endl;
        throw;
    } catch (...) {
        std::cerr << "[PipelineBuilder] Unknown error in createNode for type " 
                  << nodeConfig.nodeType << std::endl;
        throw std::runtime_error("Unknown error creating node type: " + nodeConfig.nodeType);
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createRTSPSourceNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string rtspUrl = params.count("rtsp_url") ? params.at("rtsp_url") : getRTSPUrl(req);
        int channel = params.count("channel") ? std::stoi(params.at("channel")) : 0;
        
        // cvedix_rtsp_src_node constructor: (name, channel, rtsp_url, resize_ratio)
        // resize_ratio must be > 0.0 and <= 1.0
        float resize_ratio = 1.0f; // Default to no resize
        
        // Check if resize_ratio is specified in params
        if (params.count("resize_ratio")) {
            resize_ratio = std::stof(params.at("resize_ratio"));
        } else if (params.count("scale")) {
            resize_ratio = std::stof(params.at("scale"));
        }
        
        // Validate resize_ratio (must be > 0 and <= 1.0)
        // Note: Assertion in SDK checks: resize_ratio > 0 && resize_ratio <= 1.0
        // So we need: 0 < resize_ratio <= 1.0
        if (resize_ratio <= 0.0f) {
            std::cerr << "[PipelineBuilder] Invalid resize_ratio (<= 0): " << resize_ratio 
                      << ", using default 0.5" << std::endl;
            resize_ratio = 0.5f; // Use a safe default
        }
        if (resize_ratio > 1.0f) {
            std::cerr << "[PipelineBuilder] Invalid resize_ratio (> 1.0): " << resize_ratio 
                      << ", using 1.0" << std::endl;
            resize_ratio = 1.0f;
        }
        
        // Final validation before creating node
        if (nodeName.empty()) {
            throw std::invalid_argument("Node name cannot be empty");
        }
        if (rtspUrl.empty()) {
            throw std::invalid_argument("RTSP URL cannot be empty");
        }
        // Final check: must be strictly > 0 and <= 1.0
        if (resize_ratio <= 0.0f || resize_ratio > 1.0f) {
            throw std::invalid_argument("resize_ratio must be > 0.0 and <= 1.0, got: " + std::to_string(resize_ratio));
        }
        
        std::cerr << "[PipelineBuilder] ========================================" << std::endl;
        std::cerr << "[PipelineBuilder] Creating RTSP source node:" << std::endl;
        std::cerr << "[PipelineBuilder]   Name: '" << nodeName << "' (length: " << nodeName.length() << ")" << std::endl;
        std::cerr << "[PipelineBuilder]   URL: '" << rtspUrl << "' (length: " << rtspUrl.length() << ")" << std::endl;
        std::cerr << "[PipelineBuilder]   Channel: " << channel << std::endl;
        std::cerr << "[PipelineBuilder]   Resize ratio: " << std::fixed << std::setprecision(3) << resize_ratio 
                  << " (type: float, value: " << static_cast<double>(resize_ratio) << ")" << std::endl;
        std::cerr << "[PipelineBuilder] ========================================" << std::endl;
        
        // Double-check resize_ratio is valid float
        if (std::isnan(resize_ratio) || std::isinf(resize_ratio)) {
            std::cerr << "[PipelineBuilder] ERROR: resize_ratio is NaN or Inf!" << std::endl;
            throw std::invalid_argument("resize_ratio is NaN or Inf");
        }
        
        // Ensure resize_ratio is strictly > 0 (not == 0) and <= 1.0
        if (resize_ratio <= 0.0f) {
            std::cerr << "[PipelineBuilder] ERROR: resize_ratio must be > 0.0, got: " << resize_ratio << std::endl;
            resize_ratio = 0.1f; // Use minimum valid value
            std::cerr << "[PipelineBuilder] Using minimum valid value: " << resize_ratio << std::endl;
        }
        if (resize_ratio > 1.0f) {
            std::cerr << "[PipelineBuilder] ERROR: resize_ratio must be <= 1.0, got: " << resize_ratio << std::endl;
            resize_ratio = 1.0f;
            std::cerr << "[PipelineBuilder] Using maximum valid value: " << resize_ratio << std::endl;
        }
        
        // Create node - wrap in try-catch to catch any assertion failures
        std::shared_ptr<cvedix_nodes::cvedix_rtsp_src_node> node;
        try {
            std::cerr << "[PipelineBuilder] Calling cvedix_rtsp_src_node constructor..." << std::endl;
            node = std::make_shared<cvedix_nodes::cvedix_rtsp_src_node>(
                nodeName,
                channel,
                rtspUrl,
                resize_ratio
            );
            std::cerr << "[PipelineBuilder] RTSP source node created successfully" << std::endl;
        } catch (const std::bad_alloc& e) {
            std::cerr << "[PipelineBuilder] Memory allocation failed: " << e.what() << std::endl;
            throw;
        } catch (const std::exception& e) {
            std::cerr << "[PipelineBuilder] Standard exception in constructor: " << e.what() << std::endl;
            throw;
        } catch (...) {
            // This might catch assertion failures on some systems
            std::cerr << "[PipelineBuilder] Non-standard exception in cvedix_rtsp_src_node constructor" << std::endl;
            std::cerr << "[PipelineBuilder] Parameters were: name='" << nodeName 
                      << "', channel=" << channel << ", url='" << rtspUrl 
                      << "', resize_ratio=" << resize_ratio << std::endl;
            throw std::runtime_error("Failed to create RTSP source node - check parameters and CVEDIX SDK");
        }
        
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createRTSPSourceNode: " << e.what() << std::endl;
        throw;
    } catch (...) {
        std::cerr << "[PipelineBuilder] Unknown exception in createRTSPSourceNode" << std::endl;
        throw std::runtime_error("Unknown error creating RTSP source node");
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createFaceDetectorNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        // Get parameters with defaults
        std::string modelPath = params.count("model_path") ? params.at("model_path") : "./cvedix_data/models/face/yunet.onnx";
        float scoreThreshold = params.count("score_threshold") ? 
            static_cast<float>(std::stod(params.at("score_threshold"))) : 
            static_cast<float>(mapDetectionSensitivity(req.detectionSensitivity));
        float nmsThreshold = params.count("nms_threshold") ? 
            static_cast<float>(std::stod(params.at("nms_threshold"))) : 0.5f;
        int topK = params.count("top_k") ? std::stoi(params.at("top_k")) : 50;
        
        // Validate node name
        if (nodeName.empty()) {
            throw std::invalid_argument("Node name cannot be empty");
        }
        
        // Validate model path
        if (modelPath.empty()) {
            throw std::invalid_argument("Model path cannot be empty");
        }
        
        // Check if model file exists (warning only, SDK will also check)
        fs::path modelFilePath(modelPath);
        if (!fs::exists(modelFilePath)) {
            std::cerr << "[PipelineBuilder] WARNING: Model file not found at: " << modelPath << std::endl;
            std::cerr << "[PipelineBuilder] WARNING: Face detection may not work until model is uploaded or path is corrected" << std::endl;
            std::cerr << "[PipelineBuilder] WARNING: You can upload a model file via POST /v1/core/models/upload" << std::endl;
            std::cerr << "[PipelineBuilder] WARNING: Then use MODEL_PATH in additionalParams when creating instance" << std::endl;
        } else {
            std::cerr << "[PipelineBuilder] Model file found: " << fs::canonical(modelFilePath).string() << std::endl;
        }
        
        // Validate thresholds
        if (scoreThreshold < 0.0f || scoreThreshold > 1.0f) {
            std::cerr << "[PipelineBuilder] Warning: score_threshold out of range [0,1]: " 
                      << scoreThreshold << ", clamping to [0,1]" << std::endl;
            scoreThreshold = std::max(0.0f, std::min(1.0f, scoreThreshold));
        }
        if (nmsThreshold < 0.0f || nmsThreshold > 1.0f) {
            std::cerr << "[PipelineBuilder] Warning: nms_threshold out of range [0,1]: " 
                      << nmsThreshold << ", clamping to [0,1]" << std::endl;
            nmsThreshold = std::max(0.0f, std::min(1.0f, nmsThreshold));
        }
        if (topK < 0) {
            std::cerr << "[PipelineBuilder] Warning: top_k must be >= 0, got: " 
                      << topK << ", using default 50" << std::endl;
            topK = 50;
        }
        
        std::cerr << "[PipelineBuilder] Creating YuNet face detector node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Model path: '" << modelPath << "'" << std::endl;
        std::cerr << "  Score threshold: " << scoreThreshold << std::endl;
        std::cerr << "  NMS threshold: " << nmsThreshold << std::endl;
        std::cerr << "  Top K: " << topK << std::endl;
        
        // Create the YuNet face detector node
        std::shared_ptr<cvedix_nodes::cvedix_yunet_face_detector_node> node;
        try {
            std::cerr << "[PipelineBuilder] Calling cvedix_yunet_face_detector_node constructor..." << std::endl;
            node = std::make_shared<cvedix_nodes::cvedix_yunet_face_detector_node>(
                nodeName,
                modelPath,
                scoreThreshold,
                nmsThreshold,
                topK
            );
            std::cerr << "[PipelineBuilder] YuNet face detector node created successfully" << std::endl;
        } catch (const std::bad_alloc& e) {
            std::cerr << "[PipelineBuilder] Memory allocation failed: " << e.what() << std::endl;
            throw;
        } catch (const std::exception& e) {
            std::cerr << "[PipelineBuilder] Standard exception in constructor: " << e.what() << std::endl;
            throw;
        } catch (...) {
            std::cerr << "[PipelineBuilder] Non-standard exception in cvedix_yunet_face_detector_node constructor" << std::endl;
            std::cerr << "[PipelineBuilder] Parameters were: name='" << nodeName 
                      << "', model_path='" << modelPath 
                      << "', score_threshold=" << scoreThreshold
                      << ", nms_threshold=" << nmsThreshold
                      << ", top_k=" << topK << std::endl;
            throw std::runtime_error("Failed to create YuNet face detector node - check parameters and CVEDIX SDK");
        }
        
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createFaceDetectorNode: " << e.what() << std::endl;
        throw;
    } catch (...) {
        std::cerr << "[PipelineBuilder] Unknown exception in createFaceDetectorNode" << std::endl;
        throw std::runtime_error("Unknown error creating face detector node");
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createFileDestinationNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const std::string& instanceId) {
    
    try {
        // Get parameters with defaults
        // Constructor: cvedix_file_des_node(node_name, channel_index, save_dir, name_prefix, 
        //                                   max_duration_for_single_file, resolution_w_h, 
        //                                   bitrate, osd, gst_encoder_name)
        int channelIndex = params.count("channel") ? std::stoi(params.at("channel")) : 0;
        std::string saveDir = params.count("save_dir") ? params.at("save_dir") : "./output/" + instanceId;
        std::string namePrefix = params.count("name_prefix") ? params.at("name_prefix") : "output";
        int maxDuration = params.count("max_duration") ? std::stoi(params.at("max_duration")) : 2; // minutes
        int bitrate = params.count("bitrate") ? std::stoi(params.at("bitrate")) : 1024;
        bool osd = params.count("osd") && (params.at("osd") == "true" || params.at("osd") == "1");
        std::string encoderName = params.count("encoder") ? params.at("encoder") : "x264enc";
        
        // Parse resolution if provided (format: "widthxheight" or "width,height")
        cvedix_objects::cvedix_size resolution = {};
        if (params.count("resolution")) {
            std::string resStr = params.at("resolution");
            size_t xPos = resStr.find('x');
            size_t commaPos = resStr.find(',');
            if (xPos != std::string::npos) {
                resolution.width = std::stoi(resStr.substr(0, xPos));
                resolution.height = std::stoi(resStr.substr(xPos + 1));
            } else if (commaPos != std::string::npos) {
                resolution.width = std::stoi(resStr.substr(0, commaPos));
                resolution.height = std::stoi(resStr.substr(commaPos + 1));
            }
        }
        
        // Validate node name
        if (nodeName.empty()) {
            throw std::invalid_argument("Node name cannot be empty");
        }
        
        // Validate save directory
        if (saveDir.empty()) {
            throw std::invalid_argument("Save directory cannot be empty");
        }
        
        // Ensure save directory exists (required by cvedix_file_des_node)
        // Use experimental::filesystem to match CVEDIX SDK
        fs::path saveDirPath(saveDir);
        if (!fs::exists(saveDirPath)) {
            std::cerr << "[PipelineBuilder] Creating save directory: " << saveDir << std::endl;
            fs::create_directories(saveDirPath);
        }
        
        // Validate channel index
        if (channelIndex < 0) {
            std::cerr << "[PipelineBuilder] Warning: channel_index < 0: " << channelIndex 
                      << ", using 0" << std::endl;
            channelIndex = 0;
        }
        
        // Validate max duration
        if (maxDuration <= 0) {
            std::cerr << "[PipelineBuilder] Warning: max_duration <= 0: " << maxDuration 
                      << ", using default 2 minutes" << std::endl;
            maxDuration = 2;
        }
        
        // Validate bitrate
        if (bitrate <= 0) {
            std::cerr << "[PipelineBuilder] Warning: bitrate <= 0: " << bitrate 
                      << ", using default 1024" << std::endl;
            bitrate = 1024;
        }
        
        std::cerr << "[PipelineBuilder] Creating file destination node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Channel index: " << channelIndex << std::endl;
        std::cerr << "  Save directory: '" << saveDir << "'" << std::endl;
        std::cerr << "  Name prefix: '" << namePrefix << "'" << std::endl;
        std::cerr << "  Max duration: " << maxDuration << " minutes" << std::endl;
        std::cerr << "  Bitrate: " << bitrate << std::endl;
        std::cerr << "  OSD: " << (osd ? "true" : "false") << std::endl;
        std::cerr << "  Encoder: '" << encoderName << "'" << std::endl;
        if (resolution.width > 0 && resolution.height > 0) {
            std::cerr << "  Resolution: " << resolution.width << "x" << resolution.height << std::endl;
        }
        
        // Create the file destination node
        std::shared_ptr<cvedix_nodes::cvedix_file_des_node> node;
        try {
            std::cerr << "[PipelineBuilder] Calling cvedix_file_des_node constructor..." << std::endl;
            node = std::make_shared<cvedix_nodes::cvedix_file_des_node>(
                nodeName,
                channelIndex,
                saveDir,
                namePrefix,
                maxDuration,
                resolution,
                bitrate,
                osd,
                encoderName
            );
            std::cerr << "[PipelineBuilder] File destination node created successfully" << std::endl;
        } catch (const std::bad_alloc& e) {
            std::cerr << "[PipelineBuilder] Memory allocation failed: " << e.what() << std::endl;
            throw;
        } catch (const std::exception& e) {
            std::cerr << "[PipelineBuilder] Standard exception in constructor: " << e.what() << std::endl;
            throw;
        } catch (...) {
            std::cerr << "[PipelineBuilder] Non-standard exception in cvedix_file_des_node constructor" << std::endl;
            std::cerr << "[PipelineBuilder] Parameters were: name='" << nodeName 
                      << "', channel=" << channelIndex
                      << ", save_dir='" << saveDir 
                      << "', name_prefix='" << namePrefix
                      << "', max_duration=" << maxDuration
                      << ", bitrate=" << bitrate
                      << ", osd=" << osd
                      << ", encoder='" << encoderName << "'" << std::endl;
            throw std::runtime_error("Failed to create file destination node - check parameters and CVEDIX SDK");
        }
        
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createFileDestinationNode: " << e.what() << std::endl;
        throw;
    } catch (...) {
        std::cerr << "[PipelineBuilder] Unknown exception in createFileDestinationNode" << std::endl;
        throw std::runtime_error("Unknown error creating file destination node");
    }
}

double PipelineBuilder::mapDetectionSensitivity(const std::string& sensitivity) const {
    if (sensitivity == "Low") {
        return 0.5;
    } else if (sensitivity == "Medium") {
        return 0.7;
    } else if (sensitivity == "High") {
        return 0.9;
    }
    return 0.7; // Default to Medium
}

std::string PipelineBuilder::getRTSPUrl(const CreateInstanceRequest& req) const {
    // Get RTSP URL from additionalParams
    auto it = req.additionalParams.find("RTSP_URL");
    if (it != req.additionalParams.end() && !it->second.empty()) {
        std::cerr << "[PipelineBuilder] RTSP URL from request additionalParams: '" << it->second << "'" << std::endl;
        return it->second;
    }
    
    // Default or from environment
    const char* envUrl = std::getenv("RTSP_URL");
    if (envUrl && strlen(envUrl) > 0) {
        std::cerr << "[PipelineBuilder] RTSP URL from environment variable: '" << envUrl << "'" << std::endl;
        return std::string(envUrl);
    }
    
    // Default fallback
    std::cerr << "[PipelineBuilder] WARNING: Using default RTSP URL (rtsp://localhost:8554/stream)" << std::endl;
    std::cerr << "[PipelineBuilder] NOTE: To use custom RTSP URL, provide 'RTSP_URL' in request body or set RTSP_URL environment variable" << std::endl;
    return "rtsp://localhost:8554/stream";
}

