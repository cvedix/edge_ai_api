#include "core/pipeline_builder.h"
#include <cvedix/nodes/src/cvedix_rtsp_src_node.h>
#include <cvedix/nodes/src/cvedix_file_src_node.h>
#include <cvedix/nodes/infers/cvedix_yunet_face_detector_node.h>
#include <cvedix/nodes/infers/cvedix_sface_feature_encoder_node.h>
#include <cvedix/nodes/osd/cvedix_face_osd_node_v2.h>
#include <cvedix/nodes/des/cvedix_file_des_node.h>
#include <cvedix/nodes/des/cvedix_rtmp_des_node.h>
#include <cvedix/utils/cvedix_utils.h>
#include <cvedix/objects/shapes/cvedix_size.h>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <typeinfo>
#include <exception>
#include <mutex>
#include <iomanip>
#include <cmath>
#include <set>
#include <algorithm>
#include <cctype>
// CVEDIX SDK uses experimental::filesystem, so we need to use it too for compatibility
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

// Static flag to ensure CVEDIX logger is initialized only once
static std::once_flag cvedix_init_flag;

// Initialize CVEDIX SDK logger (required before creating nodes)
static void ensureCVEDIXInitialized() {
    std::call_once(cvedix_init_flag, []() {
        try {
            // Configure GStreamer RTSP transport protocol if specified
            // This helps avoid UDP firewall issues by forcing TCP
            const char* rtspProtocols = std::getenv("GST_RTSP_PROTOCOLS");
            if (!rtspProtocols || strlen(rtspProtocols) == 0) {
                // Check if RTSP_TRANSPORT is set (alternative name)
                const char* rtspTransport = std::getenv("RTSP_TRANSPORT");
                if (rtspTransport && strlen(rtspTransport) > 0) {
                    std::string transport = rtspTransport;
                    std::transform(transport.begin(), transport.end(), transport.begin(), ::tolower);
                    if (transport == "tcp" || transport == "udp") {
                        setenv("GST_RTSP_PROTOCOLS", transport.c_str(), 0); // Don't overwrite if already set
                        std::cerr << "[PipelineBuilder] Set GST_RTSP_PROTOCOLS=" << transport 
                                  << " from RTSP_TRANSPORT environment variable" << std::endl;
                    }
                } else {
                    // Default to TCP for better firewall compatibility
                    // User can override by setting GST_RTSP_PROTOCOLS=udp if needed
                    setenv("GST_RTSP_PROTOCOLS", "tcp", 0);
                    std::cerr << "[PipelineBuilder] Set GST_RTSP_PROTOCOLS=tcp (default for firewall compatibility)" << std::endl;
                    std::cerr << "[PipelineBuilder] NOTE: To use UDP, set GST_RTSP_PROTOCOLS=udp before starting" << std::endl;
                }
            } else {
                std::cerr << "[PipelineBuilder] Using GST_RTSP_PROTOCOLS=" << rtspProtocols 
                          << " from environment" << std::endl;
            }
            
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
    
    std::cerr << "[PipelineBuilder] ========================================" << std::endl;
    std::cerr << "[PipelineBuilder] Building pipeline for solution: " << solution.solutionId << std::endl;
    std::cerr << "[PipelineBuilder] Solution name: " << solution.solutionName << std::endl;
    std::cerr << "[PipelineBuilder] Instance ID: " << instanceId << std::endl;
    std::cerr << "[PipelineBuilder] NOTE: This may be a new instance or rebuilding after stop/restart" << std::endl;
    std::cerr << "[PipelineBuilder] Pipeline will contain " << solution.pipeline.size() << " nodes:" << std::endl;
    for (size_t i = 0; i < solution.pipeline.size(); ++i) {
        const auto& nodeConfig = solution.pipeline[i];
        std::cerr << "[PipelineBuilder]   " << (i + 1) << ". " << nodeConfig.nodeType 
                  << " (" << nodeConfig.nodeName << ")" << std::endl;
    }
    std::cerr << "[PipelineBuilder] ========================================" << std::endl;
    
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
        } else if (value == "${FILE_PATH}") {
            value = getFilePath(req);
        } else if (value == "${RTMP_URL}") {
            value = getRTMPUrl(req);
        } else if (param.first == "model_path" && value == "${MODEL_PATH}") {
            // Priority: MODEL_NAME > MODEL_PATH > default
            std::string modelPath;
            
            // 1. Check MODEL_NAME first (allows user to select model by name)
            auto modelNameIt = req.additionalParams.find("MODEL_NAME");
            if (modelNameIt != req.additionalParams.end() && !modelNameIt->second.empty()) {
                std::string modelName = modelNameIt->second;
                std::string category = "face"; // Default category
                
                // Check if category is specified (format: "category:modelname" or just "modelname")
                size_t colonPos = modelName.find(':');
                if (colonPos != std::string::npos) {
                    category = modelName.substr(0, colonPos);
                    modelName = modelName.substr(colonPos + 1);
                }
                
                modelPath = resolveModelByName(modelName, category);
                if (!modelPath.empty()) {
                    std::cerr << "[PipelineBuilder] Using model by name: '" << modelNameIt->second 
                              << "' -> " << modelPath << std::endl;
                    value = modelPath;
                } else {
                    std::cerr << "[PipelineBuilder] WARNING: Model name '" << modelNameIt->second 
                              << "' not found, falling back to default" << std::endl;
                }
            }
            
            // 2. If MODEL_NAME not found or not provided, check MODEL_PATH
            if (modelPath.empty()) {
                auto it = req.additionalParams.find("MODEL_PATH");
                if (it != req.additionalParams.end() && !it->second.empty()) {
                    value = it->second;
                } else {
                    // Default to yunet.onnx - resolve path intelligently
                    value = resolveModelPath("models/face/yunet.onnx");
                }
            } else {
                value = modelPath;
            }
        } else if (param.first == "model_path" && value == "${SFACE_MODEL_PATH}") {
            // Handle SFace model path
            std::string modelPath;
            
            // 1. Check SFACE_MODEL_NAME first
            auto modelNameIt = req.additionalParams.find("SFACE_MODEL_NAME");
            if (modelNameIt != req.additionalParams.end() && !modelNameIt->second.empty()) {
                std::string modelName = modelNameIt->second;
                std::string category = "face";
                
                size_t colonPos = modelName.find(':');
                if (colonPos != std::string::npos) {
                    category = modelName.substr(0, colonPos);
                    modelName = modelName.substr(colonPos + 1);
                }
                
                modelPath = resolveModelByName(modelName, category);
                if (!modelPath.empty()) {
                    value = modelPath;
                }
            }
            
            // 2. If SFACE_MODEL_NAME not found, check SFACE_MODEL_PATH
            if (modelPath.empty()) {
                auto it = req.additionalParams.find("SFACE_MODEL_PATH");
                if (it != req.additionalParams.end() && !it->second.empty()) {
                    value = it->second;
                } else {
                    // Default to sface model - resolve path intelligently
                    value = resolveModelPath("models/face/face_recognition_sface_2021dec.onnx");
                }
            } else {
                value = modelPath;
            }
        }
        
        // Override model_path if provided in additionalParams (even if not using ${MODEL_PATH} placeholder)
        if (param.first == "model_path") {
            // Priority: MODEL_NAME > MODEL_PATH
            std::string modelPath;
            
            auto modelNameIt = req.additionalParams.find("MODEL_NAME");
            if (modelNameIt != req.additionalParams.end() && !modelNameIt->second.empty()) {
                std::string modelName = modelNameIt->second;
                std::string category = "face";
                
                size_t colonPos = modelName.find(':');
                if (colonPos != std::string::npos) {
                    category = modelName.substr(0, colonPos);
                    modelName = modelName.substr(colonPos + 1);
                }
                
                modelPath = resolveModelByName(modelName, category);
                if (!modelPath.empty()) {
                    value = modelPath;
                }
            }
            
            if (modelPath.empty()) {
                auto it = req.additionalParams.find("MODEL_PATH");
                if (it != req.additionalParams.end() && !it->second.empty()) {
                    value = it->second;
                }
            } else {
                value = modelPath;
            }
        }
        
        params[param.first] = value;
    }
    
    // Create node based on type
    try {
        if (nodeConfig.nodeType == "rtsp_src") {
            return createRTSPSourceNode(nodeName, params, req);
        } else if (nodeConfig.nodeType == "file_src") {
            return createFileSourceNode(nodeName, params, req);
        } else if (nodeConfig.nodeType == "yunet_face_detector") {
            return createFaceDetectorNode(nodeName, params, req);
        } else if (nodeConfig.nodeType == "sface_feature_encoder") {
            return createSFaceEncoderNode(nodeName, params, req);
        } else if (nodeConfig.nodeType == "face_osd_v2") {
            return createFaceOSDNode(nodeName, params);
        } else if (nodeConfig.nodeType == "file_des") {
            return createFileDestinationNode(nodeName, params, instanceId);
        } else if (nodeConfig.nodeType == "rtmp_des") {
            return createRTMPDestinationNode(nodeName, params, req);
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
        
        // Check RTSP transport protocol configuration
        const char* rtspProtocols = std::getenv("GST_RTSP_PROTOCOLS");
        std::string transportInfo = rtspProtocols ? std::string(rtspProtocols) : "tcp (default)";
        
        std::cerr << "[PipelineBuilder] ========================================" << std::endl;
        std::cerr << "[PipelineBuilder] Creating RTSP source node:" << std::endl;
        std::cerr << "[PipelineBuilder]   Name: '" << nodeName << "' (length: " << nodeName.length() << ")" << std::endl;
        std::cerr << "[PipelineBuilder]   URL: '" << rtspUrl << "' (length: " << rtspUrl.length() << ")" << std::endl;
        std::cerr << "[PipelineBuilder]   Channel: " << channel << std::endl;
        std::cerr << "[PipelineBuilder]   Resize ratio: " << std::fixed << std::setprecision(3) << resize_ratio 
                  << " (type: float, value: " << static_cast<double>(resize_ratio) << ")" << std::endl;
        std::cerr << "[PipelineBuilder]   Transport: " << transportInfo << std::endl;
        std::cerr << "[PipelineBuilder]   NOTE: If UDP is blocked by firewall, connection will retry with TCP" << std::endl;
        std::cerr << "[PipelineBuilder]   NOTE: Set GST_RTSP_PROTOCOLS=tcp to force TCP (recommended)" << std::endl;
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
        // Get parameters with defaults - resolve path intelligently
        std::string modelPath = params.count("model_path") ? 
            params.at("model_path") : 
            resolveModelPath("models/face/yunet.onnx");
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
            std::cerr << "[PipelineBuilder] ========================================" << std::endl;
            std::cerr << "[PipelineBuilder] WARNING: Model file not found!" << std::endl;
            std::cerr << "[PipelineBuilder] Expected path: " << modelPath << std::endl;
            std::cerr << "[PipelineBuilder] Absolute path: " << fs::absolute(modelFilePath).string() << std::endl;
            std::cerr << "[PipelineBuilder] ========================================" << std::endl;
            std::cerr << "[PipelineBuilder] SOLUTION: Copy your yunet.onnx file to one of these locations:" << std::endl;
            std::cerr << "[PipelineBuilder]   1. System-wide location - RECOMMENDED (FHS standard):" << std::endl;
            std::cerr << "[PipelineBuilder]      /usr/share/cvedix/cvedix_data/models/face/yunet.onnx" << std::endl;
            std::cerr << "[PipelineBuilder]      (Create: sudo mkdir -p /usr/share/cvedix/cvedix_data/models/face)" << std::endl;
            std::cerr << "[PipelineBuilder]      (Copy: sudo cp /path/to/yunet.onnx /usr/share/cvedix/cvedix_data/models/face/)" << std::endl;
            std::cerr << "[PipelineBuilder]      NOTE: /usr/share/ is for data files (FHS standard)" << std::endl;
            std::cerr << "[PipelineBuilder]   1b. Alternative (not recommended, but supported):" << std::endl;
            std::cerr << "[PipelineBuilder]      /usr/include/cvedix/cvedix_data/models/face/yunet.onnx" << std::endl;
            std::cerr << "[PipelineBuilder]      (Create: sudo mkdir -p /usr/include/cvedix/cvedix_data/models/face)" << std::endl;
            std::cerr << "[PipelineBuilder]      NOTE: /usr/include/ is for header files, not data files" << std::endl;
            std::cerr << "[PipelineBuilder]   2. SDK source location:" << std::endl;
            std::cerr << "[PipelineBuilder]      /home/pnsang/project/edge_ai_sdk/cvedix_data/models/face/yunet.onnx" << std::endl;
            std::cerr << "[PipelineBuilder]      (Copy: cp /path/to/yunet.onnx /home/pnsang/project/edge_ai_sdk/cvedix_data/models/face/)" << std::endl;
            std::cerr << "[PipelineBuilder]   3. API working directory: ./cvedix_data/models/face/yunet.onnx" << std::endl;
            std::cerr << "[PipelineBuilder]      (Create: mkdir -p ./cvedix_data/models/face)" << std::endl;
            std::cerr << "[PipelineBuilder]   4. Set environment variable CVEDIX_DATA_ROOT=/path/to/cvedix_data" << std::endl;
            std::cerr << "[PipelineBuilder]   2. Upload via API: POST /v1/core/models/upload" << std::endl;
            std::cerr << "[PipelineBuilder]      Then use MODEL_PATH in additionalParams when creating instance" << std::endl;
            std::cerr << "[PipelineBuilder]      Example: additionalParams: {\"MODEL_PATH\": \"./models/yunet.onnx\"}" << std::endl;
            std::cerr << "[PipelineBuilder] ========================================" << std::endl;
            std::cerr << "[PipelineBuilder] NOTE: Face detection will NOT work until model file is available" << std::endl;
            std::cerr << "[PipelineBuilder] NOTE: Pipeline will continue but face detection will fail" << std::endl;
            std::cerr << "[PipelineBuilder] ========================================" << std::endl;
        } else {
            std::cerr << "[PipelineBuilder] ✓ Model file found: " << fs::canonical(modelFilePath).string() << std::endl;
            std::cerr << "[PipelineBuilder]   File size: " << fs::file_size(modelFilePath) << " bytes" << std::endl;
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
            std::cerr << "[PipelineBuilder] ✓ YuNet face detector node created successfully" << std::endl;
            if (!fs::exists(modelFilePath)) {
                std::cerr << "[PipelineBuilder] ⚠ WARNING: Model file was not found, node created but may fail during inference" << std::endl;
            }
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

std::string PipelineBuilder::resolveModelPath(const std::string& relativePath) const {
    // Helper function to resolve model file paths
    // Priority:
    // 1. CVEDIX_DATA_ROOT environment variable
    // 2. CVEDIX_SDK_ROOT environment variable + /cvedix_data
    // 3. Relative to current working directory (./cvedix_data)
    // 4. Try to find SDK directory in common locations
    
    // 1. Check CVEDIX_DATA_ROOT
    const char* dataRoot = std::getenv("CVEDIX_DATA_ROOT");
    if (dataRoot && strlen(dataRoot) > 0) {
        std::string path = std::string(dataRoot);
        if (path.back() != '/') path += '/';
        path += relativePath;
        if (fs::exists(path)) {
            std::cerr << "[PipelineBuilder] Using CVEDIX_DATA_ROOT: " << path << std::endl;
            return path;
        }
    }
    
    // 2. Check CVEDIX_SDK_ROOT
    const char* sdkRoot = std::getenv("CVEDIX_SDK_ROOT");
    if (sdkRoot && strlen(sdkRoot) > 0) {
        std::string path = std::string(sdkRoot);
        if (path.back() != '/') path += '/';
        path += "cvedix_data/" + relativePath;
        if (fs::exists(path)) {
            std::cerr << "[PipelineBuilder] Using CVEDIX_SDK_ROOT: " << path << std::endl;
            return path;
        }
    }
    
    // 3. Try relative to current working directory
    std::string relativePathFull = "./cvedix_data/" + relativePath;
    if (fs::exists(relativePathFull)) {
        std::cerr << "[PipelineBuilder] Using relative path: " << fs::absolute(relativePathFull).string() << std::endl;
        return relativePathFull;
    }
    
    // 4. Try system-wide installation paths (when SDK is installed to /usr)
    // Note: /usr/share/ is preferred (FHS standard for data files)
    // /usr/include/ is for header files, but we support it as fallback
    std::vector<std::string> systemPaths = {
        "/usr/share/cvedix/cvedix_data/" + relativePath,        // Preferred (FHS standard)
        "/usr/local/share/cvedix/cvedix_data/" + relativePath,   // Local install
        "/usr/include/cvedix/cvedix_data/" + relativePath,       // Fallback (not recommended)
        "/usr/local/include/cvedix/cvedix_data/" + relativePath, // Local install fallback
    };
    
    for (const auto& path : systemPaths) {
        if (fs::exists(path)) {
            std::cerr << "[PipelineBuilder] Found in system-wide location: " << path << std::endl;
            return path;
        }
        
        // If exact file not found, try to find alternative yunet files in the same directory
        // This handles cases where file is named like "face_detection_yunet_2023mar.onnx" instead of "yunet.onnx"
        if (relativePath.find("yunet.onnx") != std::string::npos) {
            fs::path dirPath = fs::path(path).parent_path();
            if (fs::exists(dirPath) && fs::is_directory(dirPath)) {
                // Look for alternative yunet files (prefer newer versions)
                std::vector<std::string> alternatives = {
                    "face_detection_yunet_2023mar.onnx",  // Newer version (preferred)
                    "face_detection_yunet_2022mar.onnx",  // Older version
                    "yunet_2023mar.onnx",
                    "yunet_2022mar.onnx",
                };
                
                for (const auto& alt : alternatives) {
                    fs::path altPath = dirPath / alt;
                    if (fs::exists(altPath)) {
                        std::cerr << "[PipelineBuilder] Found alternative yunet model: " << altPath.string() << std::endl;
                        return altPath.string();
                    }
                }
            }
        }
    }
    
    // 5. Try common SDK source locations
    std::vector<std::string> commonPaths = {
        "/home/pnsang/project/edge_ai_sdk/cvedix_data/" + relativePath,
        "../edge_ai_sdk/cvedix_data/" + relativePath,
        "../../edge_ai_sdk/cvedix_data/" + relativePath,
    };
    
    for (const auto& path : commonPaths) {
        if (fs::exists(path)) {
            std::cerr << "[PipelineBuilder] Found in SDK directory: " << fs::absolute(path).string() << std::endl;
            return path;
        }
    }
    
    // Return default relative path (will show warning later if not found)
    std::cerr << "[PipelineBuilder] Using default path (may not exist): ./cvedix_data/" << relativePath << std::endl;
    return relativePathFull;
}

std::string PipelineBuilder::resolveModelByName(const std::string& modelName, const std::string& category) const {
    // Resolve model file by name (e.g., "yunet_2023mar", "yunet_2022mar", "yolov8n_face")
    // Supports various naming patterns and extensions
    
    // List of possible file extensions to try
    std::vector<std::string> extensions = {".onnx", ".rknn", ".weights", ".pt", ".pth", ".pb", ".tflite"};
    
    // List of possible file name patterns to try
    std::vector<std::string> patterns;
    
    // Direct match
    patterns.push_back(modelName);
    
    // Add common prefixes/suffixes
    if (modelName.find("yunet") != std::string::npos || modelName.find("face") != std::string::npos) {
        patterns.push_back("face_detection_" + modelName);
        patterns.push_back(modelName + "_face_detection");
        if (modelName.find("yunet") == std::string::npos) {
            patterns.push_back("face_detection_yunet_" + modelName);
        }
    }
    
    // Try different search locations
    std::vector<std::string> searchDirs;
    
    // 1. Check CVEDIX_DATA_ROOT
    const char* dataRoot = std::getenv("CVEDIX_DATA_ROOT");
    if (dataRoot && strlen(dataRoot) > 0) {
        std::string dir = std::string(dataRoot);
        if (dir.back() != '/') dir += '/';
        dir += "models/" + category;
        searchDirs.push_back(dir);
    }
    
    // 2. Check CVEDIX_SDK_ROOT
    const char* sdkRoot = std::getenv("CVEDIX_SDK_ROOT");
    if (sdkRoot && strlen(sdkRoot) > 0) {
        std::string dir = std::string(sdkRoot);
        if (dir.back() != '/') dir += '/';
        dir += "cvedix_data/models/" + category;
        searchDirs.push_back(dir);
    }
    
    // 3. System-wide locations
    searchDirs.push_back("/usr/share/cvedix/cvedix_data/models/" + category);
    searchDirs.push_back("/usr/local/share/cvedix/cvedix_data/models/" + category);
    searchDirs.push_back("/usr/include/cvedix/cvedix_data/models/" + category);
    searchDirs.push_back("/usr/local/include/cvedix/cvedix_data/models/" + category);
    
    // 4. SDK source locations
    searchDirs.push_back("/home/pnsang/project/edge_ai_sdk/cvedix_data/models/" + category);
    searchDirs.push_back("../edge_ai_sdk/cvedix_data/models/" + category);
    searchDirs.push_back("../../edge_ai_sdk/cvedix_data/models/" + category);
    
    // 5. Relative to current working directory
    searchDirs.push_back("./cvedix_data/models/" + category);
    searchDirs.push_back("./models"); // Also check API models directory
    
    // Search for model file
    for (const auto& dir : searchDirs) {
        fs::path dirPath(dir);
        if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
            continue;
        }
        
        // Try each pattern with each extension
        for (const auto& pattern : patterns) {
            for (const auto& ext : extensions) {
                fs::path filePath = dirPath / (pattern + ext);
                if (fs::exists(filePath)) {
                    std::cerr << "[PipelineBuilder] Found model by name '" << modelName 
                              << "' (pattern: " << pattern << ext << ") at: " 
                              << fs::canonical(filePath).string() << std::endl;
                    return fs::canonical(filePath).string();
                }
                
                // Also try case-insensitive search (list directory)
                try {
                    for (const auto& entry : fs::directory_iterator(dirPath)) {
                        if (fs::is_regular_file(entry.path())) {
                            std::string filename = entry.path().filename().string();
                            std::string filenameLower = filename;
                            std::transform(filenameLower.begin(), filenameLower.end(), filenameLower.begin(), ::tolower);
                            
                            std::string patternLower = pattern + ext;
                            std::transform(patternLower.begin(), patternLower.end(), patternLower.begin(), ::tolower);
                            
                            if (filenameLower == patternLower || 
                                filenameLower.find(patternLower) != std::string::npos) {
                                std::cerr << "[PipelineBuilder] Found model by name '" << modelName 
                                          << "' (matched: " << filename << ") at: " 
                                          << fs::canonical(entry.path()).string() << std::endl;
                                return fs::canonical(entry.path()).string();
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    // Ignore directory iteration errors
                }
            }
        }
    }
    
    return ""; // Not found
}

std::vector<std::string> PipelineBuilder::listAvailableModels(const std::string& category) const {
    std::vector<std::string> models;
    std::vector<std::string> extensions = {".onnx", ".rknn", ".weights", ".pt", ".pth", ".pb", ".tflite"};
    
    // List of search directories (same as resolveModelByName)
    std::vector<std::string> searchDirs;
    
    const char* dataRoot = std::getenv("CVEDIX_DATA_ROOT");
    if (dataRoot && strlen(dataRoot) > 0) {
        std::string dir = std::string(dataRoot);
        if (dir.back() != '/') dir += '/';
        if (category.empty()) {
            searchDirs.push_back(dir + "models");
        } else {
            searchDirs.push_back(dir + "models/" + category);
        }
    }
    
    const char* sdkRoot = std::getenv("CVEDIX_SDK_ROOT");
    if (sdkRoot && strlen(sdkRoot) > 0) {
        std::string dir = std::string(sdkRoot);
        if (dir.back() != '/') dir += '/';
        if (category.empty()) {
            searchDirs.push_back(dir + "cvedix_data/models");
        } else {
            searchDirs.push_back(dir + "cvedix_data/models/" + category);
        }
    }
    
    if (category.empty()) {
        searchDirs.push_back("/usr/share/cvedix/cvedix_data/models");
        searchDirs.push_back("/usr/local/share/cvedix/cvedix_data/models");
    } else {
        searchDirs.push_back("/usr/share/cvedix/cvedix_data/models/" + category);
        searchDirs.push_back("/usr/local/share/cvedix/cvedix_data/models/" + category);
    }
    
    searchDirs.push_back("./cvedix_data/models/" + (category.empty() ? "" : category));
    searchDirs.push_back("./models");
    
    // Collect all model files
    std::set<std::string> uniqueModels; // Use set to avoid duplicates
    
    for (const auto& dir : searchDirs) {
        fs::path dirPath(dir);
        if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
            continue;
        }
        
        try {
            for (const auto& entry : fs::directory_iterator(dirPath)) {
                if (fs::is_regular_file(entry.path())) {
                    std::string filename = entry.path().filename().string();
                    std::string ext = entry.path().extension().string();
                    
                    // Check if it's a model file
                    bool isModelFile = false;
                    for (const auto& modelExt : extensions) {
                        if (ext == modelExt || filename.find(modelExt) != std::string::npos) {
                            isModelFile = true;
                            break;
                        }
                    }
                    
                    if (isModelFile) {
                        uniqueModels.insert(fs::canonical(entry.path()).string());
                    }
                }
            }
        } catch (const std::exception& e) {
            // Ignore directory iteration errors
        }
    }
    
    // Convert set to vector
    models.assign(uniqueModels.begin(), uniqueModels.end());
    return models;
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createFileSourceNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        // Get file path from params or request
        std::string filePath = params.count("file_path") ? 
            params.at("file_path") : getFilePath(req);
        int channel = params.count("channel") ? std::stoi(params.at("channel")) : 0;
        
        // Get resize_ratio: Priority: additionalParams > params > default
        // additionalParams takes highest priority to allow runtime override
        float resizeRatio = 0.25f;  // Default to 0.25 for fixed size (320x180 from 1280x720)
        
        // Debug: Log what we have
        std::cerr << "[PipelineBuilder] DEBUG: Checking resize_ratio sources..." << std::endl;
        if (params.count("resize_ratio")) {
            std::cerr << "[PipelineBuilder] DEBUG: params has resize_ratio = " << params.at("resize_ratio") << std::endl;
        } else {
            std::cerr << "[PipelineBuilder] DEBUG: params does NOT have resize_ratio" << std::endl;
        }
        std::cerr << "[PipelineBuilder] DEBUG: additionalParams size = " << req.additionalParams.size() << std::endl;
        for (const auto& p : req.additionalParams) {
            std::cerr << "[PipelineBuilder] DEBUG: additionalParams[" << p.first << "] = " << p.second << std::endl;
        }
        
        // First check additionalParams (highest priority - allows runtime override)
        auto it = req.additionalParams.find("RESIZE_RATIO");
        if (it != req.additionalParams.end() && !it->second.empty()) {
            try {
                resizeRatio = std::stof(it->second);
                std::cerr << "[PipelineBuilder] ✓ Using RESIZE_RATIO from additionalParams: " << resizeRatio << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[PipelineBuilder] Warning: Invalid RESIZE_RATIO in additionalParams: " << it->second << ", trying params..." << std::endl;
                // Fall through to check params
                if (params.count("resize_ratio")) {
                    resizeRatio = std::stof(params.at("resize_ratio"));
                    std::cerr << "[PipelineBuilder] Using resize_ratio from solution config: " << resizeRatio << std::endl;
                }
            }
        } else {
            // RESIZE_RATIO not in additionalParams, check params
            if (params.count("resize_ratio")) {
                resizeRatio = std::stof(params.at("resize_ratio"));
                std::cerr << "[PipelineBuilder] Using resize_ratio from solution config: " << resizeRatio << std::endl;
            } else {
                std::cerr << "[PipelineBuilder] Using default resize_ratio: " << resizeRatio << std::endl;
            }
        }
        
        // Validate parameters
        if (nodeName.empty()) {
            throw std::invalid_argument("Node name cannot be empty");
        }
        if (filePath.empty()) {
            throw std::invalid_argument("File path cannot be empty");
        }
        if (resizeRatio <= 0.0f || resizeRatio > 1.0f) {
            std::cerr << "[PipelineBuilder] Warning: resize_ratio out of range (" << resizeRatio << "), using 0.25" << std::endl;
            resizeRatio = 0.25f;
        }
        
        std::cerr << "[PipelineBuilder] Creating file source node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  File path: '" << filePath << "'" << std::endl;
        std::cerr << "  Channel: " << channel << std::endl;
        std::cerr << "  Resize ratio: " << resizeRatio << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_file_src_node>(
            nodeName,
            channel,
            filePath,
            resizeRatio
        );
        
        std::cerr << "[PipelineBuilder] ✓ File source node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createFileSourceNode: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createSFaceEncoderNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        // Get model path - resolve intelligently
        std::string modelPath = params.count("model_path") ? 
            params.at("model_path") : 
            resolveModelPath("models/face/face_recognition_sface_2021dec.onnx");
        
        // Check MODEL_NAME or MODEL_PATH in additionalParams
        auto modelNameIt = req.additionalParams.find("SFACE_MODEL_NAME");
        if (modelNameIt != req.additionalParams.end() && !modelNameIt->second.empty()) {
            std::string modelName = modelNameIt->second;
            std::string category = "face";
            size_t colonPos = modelName.find(':');
            if (colonPos != std::string::npos) {
                category = modelName.substr(0, colonPos);
                modelName = modelName.substr(colonPos + 1);
            }
            std::string resolvedPath = resolveModelByName(modelName, category);
            if (!resolvedPath.empty()) {
                modelPath = resolvedPath;
            }
        } else {
            auto it = req.additionalParams.find("SFACE_MODEL_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                modelPath = it->second;
            }
        }
        
        // Validate parameters
        if (nodeName.empty()) {
            throw std::invalid_argument("Node name cannot be empty");
        }
        if (modelPath.empty()) {
            throw std::invalid_argument("Model path cannot be empty");
        }
        
        std::cerr << "[PipelineBuilder] Creating SFace encoder node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Model path: '" << modelPath << "'" << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_sface_feature_encoder_node>(
            nodeName,
            modelPath
        );
        
        std::cerr << "[PipelineBuilder] ✓ SFace encoder node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createSFaceEncoderNode: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createFaceOSDNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params) {
    
    try {
        // Validate node name
        if (nodeName.empty()) {
            throw std::invalid_argument("Node name cannot be empty");
        }
        
        std::cerr << "[PipelineBuilder] Creating face OSD v2 node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_face_osd_node_v2>(nodeName);
        
        std::cerr << "[PipelineBuilder] ✓ Face OSD v2 node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createFaceOSDNode: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createRTMPDestinationNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        // Get RTMP URL from params or request
        std::string rtmpUrl = params.count("rtmp_url") ? 
            params.at("rtmp_url") : getRTMPUrl(req);
        int channel = params.count("channel") ? std::stoi(params.at("channel")) : 0;
        
        // Validate parameters
        if (nodeName.empty()) {
            throw std::invalid_argument("Node name cannot be empty");
        }
        if (rtmpUrl.empty()) {
            throw std::invalid_argument("RTMP URL cannot be empty");
        }
        
        std::cerr << "[PipelineBuilder] Creating RTMP destination node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  RTMP URL: '" << rtmpUrl << "'" << std::endl;
        std::cerr << "  Channel: " << channel << std::endl;
        std::cerr << "  NOTE: RTMP node automatically adds '_0' suffix to stream key" << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_rtmp_des_node>(
            nodeName,
            channel,
            rtmpUrl
        );
        
        std::cerr << "[PipelineBuilder] ✓ RTMP destination node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createRTMPDestinationNode: " << e.what() << std::endl;
        throw;
    }
}

std::string PipelineBuilder::getFilePath(const CreateInstanceRequest& req) const {
    // Get file path from additionalParams
    auto it = req.additionalParams.find("FILE_PATH");
    if (it != req.additionalParams.end() && !it->second.empty()) {
        std::cerr << "[PipelineBuilder] File path from request additionalParams: '" << it->second << "'" << std::endl;
        return it->second;
    }
    
    // Default fallback
    std::cerr << "[PipelineBuilder] WARNING: Using default file path (./cvedix_data/test_video/face.mp4)" << std::endl;
    std::cerr << "[PipelineBuilder] NOTE: To use custom file path, provide 'FILE_PATH' in request body additionalParams" << std::endl;
    return "./cvedix_data/test_video/face.mp4";
}

std::string PipelineBuilder::getRTMPUrl(const CreateInstanceRequest& req) const {
    // Get RTMP URL from additionalParams
    auto it = req.additionalParams.find("RTMP_URL");
    if (it != req.additionalParams.end() && !it->second.empty()) {
        std::cerr << "[PipelineBuilder] RTMP URL from request additionalParams: '" << it->second << "'" << std::endl;
        return it->second;
    }
    
    // Default fallback
    std::cerr << "[PipelineBuilder] WARNING: Using default RTMP URL" << std::endl;
    std::cerr << "[PipelineBuilder] NOTE: To use custom RTMP URL, provide 'RTMP_URL' in request body additionalParams" << std::endl;
    return "rtmp://anhoidong.datacenter.cvedix.com:1935/live/camera_demo_1";
}

