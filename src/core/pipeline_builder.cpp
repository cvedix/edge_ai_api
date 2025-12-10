#include "core/pipeline_builder.h"
#include "config/system_config.h"
#include "core/platform_detector.h"
#include <cvedix/nodes/src/cvedix_rtsp_src_node.h>
#include <cvedix/nodes/src/cvedix_file_src_node.h>
#include <cvedix/nodes/src/cvedix_app_src_node.h>
#include <cvedix/nodes/src/cvedix_image_src_node.h>
#include <cvedix/nodes/src/cvedix_rtmp_src_node.h>
#include <cvedix/nodes/src/cvedix_udp_src_node.h>
#include <cvedix/nodes/infers/cvedix_yunet_face_detector_node.h>
#include <cvedix/nodes/infers/cvedix_sface_feature_encoder_node.h>
#include <cvedix/nodes/osd/cvedix_face_osd_node_v2.h>
#include <cvedix/nodes/des/cvedix_file_des_node.h>
#include <cvedix/nodes/des/cvedix_rtmp_des_node.h>
#include <cvedix/nodes/des/cvedix_screen_des_node.h>
#include <cvedix/nodes/track/cvedix_sort_track_node.h>
#include <cvedix/nodes/ba/cvedix_ba_crossline_node.h>
#include <cvedix/nodes/osd/cvedix_ba_crossline_osd_node.h>
#include <cvedix/utils/cvedix_utils.h>
#include <cvedix/objects/shapes/cvedix_size.h>
#include <cvedix/objects/shapes/cvedix_point.h>
#include <cvedix/objects/shapes/cvedix_line.h>

// TensorRT Inference Nodes
#ifdef CVEDIX_WITH_TRT
#include <cvedix/nodes/infers/cvedix_trt_yolov8_detector.h>
#include <cvedix/nodes/infers/cvedix_trt_yolov8_seg_detector.h>
#include <cvedix/nodes/infers/cvedix_trt_yolov8_pose_detector.h>
#include <cvedix/nodes/infers/cvedix_trt_yolov8_classifier.h>
#include <cvedix/nodes/infers/cvedix_trt_vehicle_detector.h>
#include <cvedix/nodes/infers/cvedix_trt_vehicle_plate_detector.h>
#include <cvedix/nodes/infers/cvedix_trt_vehicle_plate_detector_v2.h>
#include <cvedix/nodes/infers/cvedix_trt_vehicle_color_classifier.h>
#include <cvedix/nodes/infers/cvedix_trt_vehicle_type_classifier.h>
#include <cvedix/nodes/infers/cvedix_trt_vehicle_feature_encoder.h>
#include <cvedix/nodes/infers/cvedix_trt_vehicle_scanner.h>
#endif

// RKNN Inference Nodes
#ifdef CVEDIX_WITH_RKNN
#include <cvedix/nodes/infers/cvedix_rknn_yolov8_detector_node.h>
#include <cvedix/nodes/infers/cvedix_rknn_yolov11_detector_node.h>
#include <cvedix/nodes/infers/cvedix_rknn_face_detector_node.h>
#endif

// Other Inference Nodes
#include <cvedix/nodes/infers/cvedix_yolo_detector_node.h>
#include <cvedix/nodes/infers/cvedix_yolov11_detector_node.h>
#include <cvedix/nodes/infers/cvedix_enet_seg_node.h>
#include <cvedix/nodes/infers/cvedix_mask_rcnn_detector_node.h>
#include <cvedix/nodes/infers/cvedix_openpose_detector_node.h>
#include <cvedix/nodes/infers/cvedix_classifier_node.h>
// Note: cvedix_feature_encoder_node is abstract - use cvedix_sface_feature_encoder_node or cvedix_trt_vehicle_feature_encoder instead
// #include <cvedix/nodes/infers/cvedix_feature_encoder_node.h>
#include <cvedix/nodes/infers/cvedix_lane_detector_node.h>
#include <cvedix/nodes/infers/cvedix_face_swap_node.h>
#include <cvedix/nodes/infers/cvedix_insight_face_recognition_node.h>
#ifdef CVEDIX_WITH_LLM
#include <cvedix/nodes/infers/cvedix_mllm_analyser_node.h>
#endif
#ifdef CVEDIX_WITH_PADDLE
#include <cvedix/nodes/infers/cvedix_ppocr_text_detector_node.h>
#endif
#include <cvedix/nodes/infers/cvedix_restoration_node.h>

// TensorRT Additional Inference Nodes
#ifdef CVEDIX_WITH_TRT
#include <cvedix/nodes/infers/cvedix_trt_insight_face_recognition_node.h>
#endif
#include <vector>

// Broker Nodes
// Temporarily disabled JSON broker nodes due to cereal dependency issue
// TODO: Re-enable after fixing cereal symlink or CVEDIX SDK update
// #include <cvedix/nodes/broker/cvedix_json_console_broker_node.h>
// #include <cvedix/nodes/broker/cvedix_json_enhanced_console_broker_node.h>
#ifdef CVEDIX_WITH_MQTT
// #include <cvedix/nodes/broker/cvedix_json_mqtt_broker_node.h>
#endif
#ifdef CVEDIX_WITH_KAFKA
// #include <cvedix/nodes/broker/cvedix_json_kafka_broker_node.h>
#endif
// Temporarily disabled all broker nodes due to cereal dependency issue
// TODO: Re-enable after fixing cereal symlink or CVEDIX SDK update
// #include <cvedix/nodes/broker/cvedix_xml_file_broker_node.h>
// #include <cvedix/nodes/broker/cvedix_xml_socket_broker_node.h>
// #include <cvedix/nodes/broker/cvedix_msg_broker_node.h>
// #include <cvedix/nodes/broker/cvedix_ba_socket_broker_node.h>
// #include <cvedix/nodes/broker/cvedix_embeddings_socket_broker_node.h>
// #include <cvedix/nodes/broker/cvedix_embeddings_properties_socket_broker_node.h>
// #include <cvedix/nodes/broker/cvedix_plate_socket_broker_node.h>
// #include <cvedix/nodes/broker/cvedix_expr_socket_broker_node.h>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <fstream>
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

// Helper function to select decoder from priority list
static std::string selectDecoderFromPriority(const std::string& defaultDecoder) {
    try {
        auto& systemConfig = SystemConfig::getInstance();
        auto decoderList = systemConfig.getDecoderPriorityList();
        
        if (decoderList.empty()) {
            return defaultDecoder;
        }
        
        // Map decoder priority names to GStreamer decoder names
        std::map<std::string, std::string> decoderMap = {
            {"blaize.auto", "avdec_h264"},  // Blaize hardware decoder (fallback to software)
            {"rockchip", "mppvideodec"},     // Rockchip MPP decoder
            {"nvidia.1", "nvh264dec"},       // NVIDIA hardware decoder
            {"intel.1", "qsvh264dec"},       // Intel QuickSync decoder
            {"software", "avdec_h264"}       // Software decoder
        };
        
        // Try decoders in priority order
        for (const auto& priorityDecoder : decoderList) {
            auto it = decoderMap.find(priorityDecoder);
            if (it != decoderMap.end()) {
                // Check if decoder is available (simple check - could be enhanced)
                // For now, return first available in priority order
                std::cerr << "[PipelineBuilder] Selected decoder from priority: " 
                         << priorityDecoder << " -> " << it->second << std::endl;
                return it->second;
            }
        }
        
        // If no match, return default
        return defaultDecoder;
    } catch (...) {
        // On error, return default
        return defaultDecoder;
    }
}

// Helper function to get GStreamer pipeline from config
// Note: Currently not used but available for future integration
[[maybe_unused]] static std::string getGStreamerPipelineForPlatform() {
    try {
        auto& systemConfig = SystemConfig::getInstance();
        std::string platform = PlatformDetector::detectPlatform();
        std::string pipeline = systemConfig.getGStreamerPipeline(platform);
        
        if (!pipeline.empty()) {
            std::cerr << "[PipelineBuilder] Using GStreamer pipeline from config for platform '" 
                     << platform << "': " << pipeline << std::endl;
            return pipeline;
        }
        
        // Fallback to auto if platform-specific not found
        pipeline = systemConfig.getGStreamerPipeline("auto");
        if (!pipeline.empty()) {
            std::cerr << "[PipelineBuilder] Using GStreamer pipeline from config (auto): " 
                     << pipeline << std::endl;
            return pipeline;
        }
    } catch (...) {
        // On error, return empty (will use default)
    }
    
    return "";
}

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
            
            // Apply GStreamer plugin ranks from config
            try {
                auto& systemConfig = SystemConfig::getInstance();
                // Get all plugin ranks from config
                // Note: GStreamer plugin ranks are set via environment variables
                // Format: GST_PLUGIN_FEATURE_RANK=plugin1:rank1,plugin2:rank2,...
                std::vector<std::string> pluginRanks;
                std::vector<std::string> knownPlugins = {
                    "nvv4l2decoder", "nvjpegdec", "nvjpegenc", "nvvidconv",
                    "msdkvpp", "vaapipostproc", "vpldec",
                    "qsv", "qsvh265dec", "qsvh264dec", "qsvh265enc", "qsvh264enc",
                    "amfh264dec", "amfh265dec", "amfhvp9dec", "amfhav1dec",
                    "nvh264dec", "nvh265dec", "nvh264enc", "nvh265enc",
                    "nvvp9dec", "nvvp9enc",
                    "nvmpeg4videodec", "nvmpeg2videodec", "nvmpegvideodec",
                    "mpph264enc", "mpph265enc", "mppvp8enc", "mppjpegenc",
                    "mppvideodec", "mppjpegdec"
                };
                
                for (const auto& plugin : knownPlugins) {
                    std::string rank = systemConfig.getGStreamerPluginRank(plugin);
                    if (!rank.empty()) {
                        pluginRanks.push_back(plugin + ":" + rank);
                    }
                }
                
                if (!pluginRanks.empty()) {
                    std::string rankEnv = "";
                    for (size_t i = 0; i < pluginRanks.size(); ++i) {
                        if (i > 0) rankEnv += ",";
                        rankEnv += pluginRanks[i];
                    }
                    setenv("GST_PLUGIN_FEATURE_RANK", rankEnv.c_str(), 0);
                    std::cerr << "[PipelineBuilder] Set GStreamer plugin ranks from config: " 
                             << pluginRanks.size() << " plugins" << std::endl;
                }
            } catch (...) {
                std::cerr << "[PipelineBuilder] Warning: Failed to apply GStreamer plugin ranks from config" << std::endl;
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
    std::vector<std::string> nodeTypes; // Track node types for connection logic
    
    // Build nodes in pipeline order
    for (const auto& nodeConfig : solution.pipeline) {
        try {
            std::cerr << "[PipelineBuilder] Creating node: " << nodeConfig.nodeType 
                      << " (" << nodeConfig.nodeName << ")" << std::endl;
            
            auto node = createNode(nodeConfig, req, instanceId);
            if (node) {
                nodes.push_back(node);
                nodeTypes.push_back(nodeConfig.nodeType);
                
                // Connect to previous node
                // attach_to() expects a vector of shared_ptr, not a raw pointer
                if (nodes.size() > 1) {
                    // Check if current node is a destination node
                    bool isDestNode = (nodeConfig.nodeType == "file_des" || 
                                      nodeConfig.nodeType == "rtmp_des" || 
                                      nodeConfig.nodeType == "screen_des");
                    
                    // Find the node to attach to
                    // If current node is a destination node and previous node is also a destination node,
                    // attach to the node before the previous one (to allow multiple destinations from same source)
                    size_t attachIndex = nodes.size() - 2;
                    if (isDestNode && attachIndex > 0) {
                        // Check if previous node is also a destination node
                        bool prevIsDestNode = (nodeTypes[attachIndex] == "file_des" || 
                                              nodeTypes[attachIndex] == "rtmp_des" || 
                                              nodeTypes[attachIndex] == "screen_des");
                        if (prevIsDestNode) {
                            // Find the last non-destination node
                            for (int i = static_cast<int>(attachIndex) - 1; i >= 0; --i) {
                                bool nodeIsDest = (nodeTypes[i] == "file_des" || 
                                                  nodeTypes[i] == "rtmp_des" || 
                                                  nodeTypes[i] == "screen_des");
                                if (!nodeIsDest) {
                                    attachIndex = i;
                                    break;
                                }
                            }
                        }
                    }
                    
                    if (attachIndex < nodes.size()) {
                        node->attach_to({nodes[attachIndex]});
                    } else {
                        node->attach_to({nodes[nodes.size() - 2]});
                    }
                }
                std::cerr << "[PipelineBuilder] Successfully created and connected node: " 
                          << nodeConfig.nodeType << std::endl;
            } else {
                // Check if this is an optional node that can be skipped
                bool isOptionalNode = (nodeConfig.nodeType == "screen_des" || 
                                     nodeConfig.nodeType == "json_console_broker" ||
                                     nodeConfig.nodeType == "json_enhanced_console_broker" ||
                                     nodeConfig.nodeType == "json_mqtt_broker" ||
                                     nodeConfig.nodeType == "json_kafka_broker" ||
                                     nodeConfig.nodeType == "xml_file_broker" ||
                                     nodeConfig.nodeType == "xml_socket_broker" ||
                                     nodeConfig.nodeType == "msg_broker" ||
                                     nodeConfig.nodeType == "ba_socket_broker" ||
                                     nodeConfig.nodeType == "embeddings_socket_broker" ||
                                     nodeConfig.nodeType == "embeddings_properties_socket_broker" ||
                                     nodeConfig.nodeType == "plate_socket_broker" ||
                                     nodeConfig.nodeType == "expr_socket_broker");
                
                if (isOptionalNode) {
                    std::cerr << "[PipelineBuilder] Skipping optional node: " << nodeConfig.nodeType 
                              << " (node creation returned nullptr)" << std::endl;
                    // Continue to next node without throwing exception
                } else {
                    std::cerr << "[PipelineBuilder] Failed to create node: " << nodeConfig.nodeType 
                              << " (createNode returned nullptr)" << std::endl;
                    throw std::runtime_error("Failed to create node: " + nodeConfig.nodeType);
                }
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
    
    // Auto-add file_des node if RECORD_PATH is set in additionalParams
    auto recordPathIt = req.additionalParams.find("RECORD_PATH");
    if (recordPathIt != req.additionalParams.end() && !recordPathIt->second.empty()) {
        std::string recordPath = recordPathIt->second;
        std::cerr << "[PipelineBuilder] RECORD_PATH detected: " << recordPath << std::endl;
        std::cerr << "[PipelineBuilder] Auto-adding file_des node for recording..." << std::endl;
        
        try {
            // Create file_des node config
            SolutionConfig::NodeConfig fileDesConfig;
            fileDesConfig.nodeType = "file_des";
            fileDesConfig.nodeName = "file_des_record_{instanceId}";
            fileDesConfig.parameters["save_dir"] = recordPath;
            fileDesConfig.parameters["name_prefix"] = "record";
            fileDesConfig.parameters["max_duration"] = "10"; // 10 minutes per file
            fileDesConfig.parameters["osd"] = "true"; // Include OSD overlay
            
            // Substitute {instanceId} in node name
            std::string fileDesNodeName = fileDesConfig.nodeName;
            size_t pos = fileDesNodeName.find("{instanceId}");
            while (pos != std::string::npos) {
                fileDesNodeName.replace(pos, 12, instanceId);
                pos = fileDesNodeName.find("{instanceId}", pos + instanceId.length());
            }
            
            // Create file_des node
            auto fileDesNode = createFileDestinationNode(
                fileDesNodeName,
                fileDesConfig.parameters,
                instanceId
            );
            
            if (fileDesNode && !nodes.empty()) {
                // Connect to last node in pipeline
                fileDesNode->attach_to({nodes.back()});
                nodes.push_back(fileDesNode);
                std::cerr << "[PipelineBuilder] ✓ Auto-added file_des node for recording to: " << recordPath << std::endl;
            } else {
                std::cerr << "[PipelineBuilder] ⚠ Failed to create file_des node for recording" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "[PipelineBuilder] ⚠ Exception auto-adding file_des node: " << e.what() << std::endl;
            std::cerr << "[PipelineBuilder] ⚠ Recording will not be available, but pipeline will continue" << std::endl;
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
        } else if (value == "${WEIGHTS_PATH}") {
            auto it = req.additionalParams.find("WEIGHTS_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                value = it->second;
            }
        } else if (value == "${CONFIG_PATH}") {
            auto it = req.additionalParams.find("CONFIG_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                value = it->second;
            }
        } else if (value == "${LABELS_PATH}") {
            auto it = req.additionalParams.find("LABELS_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                value = it->second;
            }
        } else if (value == "${ENABLE_SCREEN_DES}") {
            auto it = req.additionalParams.find("ENABLE_SCREEN_DES");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                value = it->second;
            } else {
                // Default: empty string means enabled if display available
                value = "";
            }
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
        // Source nodes
        if (nodeConfig.nodeType == "rtsp_src") {
            return createRTSPSourceNode(nodeName, params, req);
        } else if (nodeConfig.nodeType == "file_src") {
            return createFileSourceNode(nodeName, params, req);
        } else if (nodeConfig.nodeType == "app_src") {
            return createAppSourceNode(nodeName, params, req);
        } else if (nodeConfig.nodeType == "image_src") {
            return createImageSourceNode(nodeName, params, req);
        } else if (nodeConfig.nodeType == "rtmp_src") {
            return createRTMPSourceNode(nodeName, params, req);
        } else if (nodeConfig.nodeType == "udp_src") {
            return createUDPSourceNode(nodeName, params, req);
        }
        // Face detection nodes
        else if (nodeConfig.nodeType == "yunet_face_detector") {
            return createFaceDetectorNode(nodeName, params, req);
        } else if (nodeConfig.nodeType == "sface_feature_encoder") {
            return createSFaceEncoderNode(nodeName, params, req);
        } else if (nodeConfig.nodeType == "face_osd_v2") {
            return createFaceOSDNode(nodeName, params);
        }
#ifdef CVEDIX_WITH_TRT
        // TensorRT YOLOv8 nodes
        else if (nodeConfig.nodeType == "trt_yolov8_detector") {
            return createTRTYOLOv8DetectorNode(nodeName, params, req);
        } else if (nodeConfig.nodeType == "trt_yolov8_seg_detector") {
            return createTRTYOLOv8SegDetectorNode(nodeName, params, req);
        } else if (nodeConfig.nodeType == "trt_yolov8_pose_detector") {
            return createTRTYOLOv8PoseDetectorNode(nodeName, params, req);
        } else if (nodeConfig.nodeType == "trt_yolov8_classifier") {
            return createTRTYOLOv8ClassifierNode(nodeName, params, req);
        }
        // TensorRT Vehicle nodes
        else if (nodeConfig.nodeType == "trt_vehicle_detector") {
            return createTRTVehicleDetectorNode(nodeName, params, req);
        } else if (nodeConfig.nodeType == "trt_vehicle_plate_detector") {
            return createTRTVehiclePlateDetectorNode(nodeName, params, req);
        } else if (nodeConfig.nodeType == "trt_vehicle_plate_detector_v2") {
            return createTRTVehiclePlateDetectorV2Node(nodeName, params, req);
        } else if (nodeConfig.nodeType == "trt_vehicle_color_classifier") {
            return createTRTVehicleColorClassifierNode(nodeName, params, req);
        } else if (nodeConfig.nodeType == "trt_vehicle_type_classifier") {
            return createTRTVehicleTypeClassifierNode(nodeName, params, req);
        } else if (nodeConfig.nodeType == "trt_vehicle_feature_encoder") {
            return createTRTVehicleFeatureEncoderNode(nodeName, params, req);
        } else if (nodeConfig.nodeType == "trt_vehicle_scanner") {
            return createTRTVehicleScannerNode(nodeName, params, req);
        } else if (nodeConfig.nodeType == "trt_insight_face_recognition") {
            return createTRTInsightFaceRecognitionNode(nodeName, params, req);
        }
#endif
#ifdef CVEDIX_WITH_RKNN
        // RKNN nodes
        else if (nodeConfig.nodeType == "rknn_yolov8_detector") {
            return createRKNNYOLOv8DetectorNode(nodeName, params, req);
        } else if (nodeConfig.nodeType == "rknn_yolov11_detector") {
            return createRKNNYOLOv11DetectorNode(nodeName, params, req);
        } else if (nodeConfig.nodeType == "rknn_face_detector") {
            return createRKNNFaceDetectorNode(nodeName, params, req);
        }
#endif
        // Other inference nodes
        else if (nodeConfig.nodeType == "yolo_detector") {
            return createYOLODetectorNode(nodeName, params, req);
        } else if (nodeConfig.nodeType == "yolov11_detector") {
            return createYOLOv11DetectorNode(nodeName, params, req);
        } else if (nodeConfig.nodeType == "enet_seg") {
            return createENetSegNode(nodeName, params, req);
        } else if (nodeConfig.nodeType == "mask_rcnn_detector") {
            return createMaskRCNNDetectorNode(nodeName, params, req);
        } else if (nodeConfig.nodeType == "openpose_detector") {
            return createOpenPoseDetectorNode(nodeName, params, req);
        } else if (nodeConfig.nodeType == "classifier") {
            return createClassifierNode(nodeName, params, req);
        } else if (nodeConfig.nodeType == "feature_encoder") {
            std::cerr << "[PipelineBuilder] feature_encoder node type is not supported (abstract class). Use 'sface_feature_encoder' or 'trt_vehicle_feature_encoder' instead." << std::endl;
            throw std::runtime_error("feature_encoder node type is not supported. Use 'sface_feature_encoder' or 'trt_vehicle_feature_encoder' instead.");
        } else if (nodeConfig.nodeType == "lane_detector") {
            return createLaneDetectorNode(nodeName, params, req);
        } else if (nodeConfig.nodeType == "face_swap") {
            return createFaceSwapNode(nodeName, params, req);
        } else if (nodeConfig.nodeType == "insight_face_recognition") {
            return createInsightFaceRecognitionNode(nodeName, params, req);
        } else if (nodeConfig.nodeType == "mllm_analyser") {
            return createMLLMAnalyserNode(nodeName, params, req);
#ifdef CVEDIX_WITH_PADDLE
        } else if (nodeConfig.nodeType == "ppocr_text_detector") {
            return createPaddleOCRTextDetectorNode(nodeName, params, req);
#endif
        } else if (nodeConfig.nodeType == "restoration") {
            return createRestorationNode(nodeName, params, req);
        }
        // Tracking nodes
        else if (nodeConfig.nodeType == "sort_track") {
            return createSORTTrackNode(nodeName, params);
        }
        // Behavior Analysis nodes
        else if (nodeConfig.nodeType == "ba_crossline") {
            return createBACrosslineNode(nodeName, params);
        }
        // OSD nodes
        else if (nodeConfig.nodeType == "ba_crossline_osd") {
            return createBACrosslineOSDNode(nodeName, params);
        }
        // Broker nodes
        // Temporarily disabled JSON broker nodes due to cereal dependency issue
        else if (nodeConfig.nodeType == "json_console_broker") {
            std::cerr << "[PipelineBuilder] json_console_broker is temporarily disabled due to cereal dependency issue" << std::endl;
            return nullptr;
        } else if (nodeConfig.nodeType == "json_enhanced_console_broker") {
            std::cerr << "[PipelineBuilder] json_enhanced_console_broker is temporarily disabled due to cereal dependency issue" << std::endl;
            return nullptr;
        } else if (nodeConfig.nodeType == "json_mqtt_broker") {
            std::cerr << "[PipelineBuilder] json_mqtt_broker is temporarily disabled due to cereal dependency issue" << std::endl;
            return nullptr;
        } else if (nodeConfig.nodeType == "json_kafka_broker") {
            std::cerr << "[PipelineBuilder] json_kafka_broker is temporarily disabled due to cereal dependency issue" << std::endl;
            return nullptr;
        } else if (nodeConfig.nodeType == "xml_file_broker") {
            std::cerr << "[PipelineBuilder] xml_file_broker is temporarily disabled due to cereal dependency issue" << std::endl;
            return nullptr;
        } else if (nodeConfig.nodeType == "xml_socket_broker") {
            std::cerr << "[PipelineBuilder] xml_socket_broker is temporarily disabled due to cereal dependency issue" << std::endl;
            return nullptr;
        } else if (nodeConfig.nodeType == "msg_broker") {
            std::cerr << "[PipelineBuilder] msg_broker is temporarily disabled due to cereal dependency issue" << std::endl;
            return nullptr;
        } else if (nodeConfig.nodeType == "ba_socket_broker") {
            std::cerr << "[PipelineBuilder] ba_socket_broker is temporarily disabled due to cereal dependency issue" << std::endl;
            return nullptr;
        } else if (nodeConfig.nodeType == "embeddings_socket_broker") {
            std::cerr << "[PipelineBuilder] embeddings_socket_broker is temporarily disabled due to cereal dependency issue" << std::endl;
            return nullptr;
        } else if (nodeConfig.nodeType == "embeddings_properties_socket_broker") {
            std::cerr << "[PipelineBuilder] embeddings_properties_socket_broker is temporarily disabled due to cereal dependency issue" << std::endl;
            return nullptr;
        } else if (nodeConfig.nodeType == "plate_socket_broker") {
            std::cerr << "[PipelineBuilder] plate_socket_broker is temporarily disabled due to cereal dependency issue" << std::endl;
            return nullptr;
        } else if (nodeConfig.nodeType == "expr_socket_broker") {
            std::cerr << "[PipelineBuilder] expr_socket_broker is temporarily disabled due to cereal dependency issue" << std::endl;
            return nullptr;
        }
        // Destination nodes
        else if (nodeConfig.nodeType == "file_des") {
            return createFileDestinationNode(nodeName, params, instanceId);
        } else if (nodeConfig.nodeType == "rtmp_des") {
            return createRTMPDestinationNode(nodeName, params, req);
        } else if (nodeConfig.nodeType == "screen_des") {
            return createScreenDestinationNode(nodeName, params);
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

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createScreenDestinationNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params) {
    
    try {
        // Check if screen_des is explicitly disabled via parameter
        if (params.count("enabled")) {
            std::string enabledStr = params.at("enabled");
            // Convert to lowercase for case-insensitive comparison
            std::transform(enabledStr.begin(), enabledStr.end(), enabledStr.begin(), ::tolower);
            if (enabledStr == "false" || enabledStr == "0" || enabledStr == "no" || enabledStr == "off") {
                std::cerr << "[PipelineBuilder] screen_des node skipped: Explicitly disabled via parameter (enabled=false)" << std::endl;
                return nullptr;
            }
        }
        
        // Check if display is available
        const char *display = std::getenv("DISPLAY");
        const char *wayland = std::getenv("WAYLAND_DISPLAY");
        
        if (!display && !wayland) {
            std::cerr << "[PipelineBuilder] screen_des node skipped: No DISPLAY or WAYLAND_DISPLAY environment variable found" << std::endl;
            std::cerr << "[PipelineBuilder] NOTE: screen_des requires a display to work. Set DISPLAY or WAYLAND_DISPLAY to enable." << std::endl;
            return nullptr;
        }
        
        // Additional check for X11 display
        if (display && display[0] != '\0') {
            std::string displayStr(display);
            if (displayStr[0] == ':') {
                std::string socketPath = "/tmp/.X11-unix/X" + displayStr.substr(1);
                // Check if X11 socket exists (basic check)
                std::ifstream socketFile(socketPath);
                if (!socketFile.good()) {
                    std::cerr << "[PipelineBuilder] screen_des node skipped: X11 socket not found at " << socketPath << std::endl;
                    std::cerr << "[PipelineBuilder] NOTE: X server may not be running or accessible" << std::endl;
                    return nullptr;
                }
            }
        }
        
        int channel = params.count("channel") ? std::stoi(params.at("channel")) : 0;
        
        if (nodeName.empty()) {
            throw std::invalid_argument("Node name cannot be empty");
        }
        
        std::cerr << "[PipelineBuilder] Creating screen destination node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Channel: " << channel << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_screen_des_node>(
            nodeName,
            channel
        );
        
        std::cerr << "[PipelineBuilder] ✓ Screen destination node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createScreenDestinationNode: " << e.what() << std::endl;
        std::cerr << "[PipelineBuilder] screen_des node will be skipped due to error" << std::endl;
        return nullptr; // Return nullptr instead of throwing to allow pipeline to continue
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createSORTTrackNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params) {
    
    try {
        if (nodeName.empty()) {
            throw std::invalid_argument("Node name cannot be empty");
        }
        
        std::cerr << "[PipelineBuilder] Creating SORT tracker node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_sort_track_node>(nodeName);
        
        std::cerr << "[PipelineBuilder] ✓ SORT tracker node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createSORTTrackNode: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createBACrosslineNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params) {
    
    try {
        if (nodeName.empty()) {
            throw std::invalid_argument("Node name cannot be empty");
        }
        
        // Parse lines from parameters
        // Expected format: "channel:start_x,start_y:end_x,end_y" or JSON-like format
        // For simplicity, we'll support a format like: "0:0,250:700,220"
        // Or use separate parameters: line_channel, line_start_x, line_start_y, line_end_x, line_end_y
        std::map<int, cvedix_objects::cvedix_line> lines;
        
        // Check if we have line parameters
        if (params.count("line_channel") && params.count("line_start_x") && 
            params.count("line_start_y") && params.count("line_end_x") && params.count("line_end_y")) {
            int channel = std::stoi(params.at("line_channel"));
            int start_x = std::stoi(params.at("line_start_x"));
            int start_y = std::stoi(params.at("line_start_y"));
            int end_x = std::stoi(params.at("line_end_x"));
            int end_y = std::stoi(params.at("line_end_y"));
            
            cvedix_objects::cvedix_point start(start_x, start_y);
            cvedix_objects::cvedix_point end(end_x, end_y);
            lines[channel] = cvedix_objects::cvedix_line(start, end);
        } else {
            // Default line for channel 0 (from sample)
            cvedix_objects::cvedix_point start(0, 250);
            cvedix_objects::cvedix_point end(700, 220);
            lines[0] = cvedix_objects::cvedix_line(start, end);
            std::cerr << "[PipelineBuilder] Using default line configuration (channel 0: (0,250) -> (700,220))" << std::endl;
        }
        
        std::cerr << "[PipelineBuilder] Creating BA crossline node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Lines configured for " << lines.size() << " channel(s)" << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_ba_crossline_node>(nodeName, lines);
        
        std::cerr << "[PipelineBuilder] ✓ BA crossline node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createBACrosslineNode: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createBACrosslineOSDNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params) {
    
    try {
        if (nodeName.empty()) {
            throw std::invalid_argument("Node name cannot be empty");
        }
        
        std::cerr << "[PipelineBuilder] Creating BA crossline OSD node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_ba_crossline_osd_node>(nodeName);
        
        std::cerr << "[PipelineBuilder] ✓ BA crossline OSD node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createBACrosslineOSDNode: " << e.what() << std::endl;
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

// ========== TensorRT YOLOv8 Nodes Implementation ==========

#ifdef CVEDIX_WITH_TRT
std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createTRTYOLOv8DetectorNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string modelPath = params.count("model_path") ? params.at("model_path") : "";
        std::string labelsPath = params.count("labels_path") ? params.at("labels_path") : "";
        
        // Get from additionalParams if not in params
        if (modelPath.empty()) {
            auto it = req.additionalParams.find("MODEL_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                modelPath = it->second;
            }
        }
        if (labelsPath.empty()) {
            auto it = req.additionalParams.find("LABELS_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                labelsPath = it->second;
            }
        }
        
        if (nodeName.empty()) {
            throw std::invalid_argument("Node name cannot be empty");
        }
        if (modelPath.empty()) {
            throw std::invalid_argument("Model path cannot be empty for TRT YOLOv8 detector");
        }
        
        std::cerr << "[PipelineBuilder] Creating TRT YOLOv8 detector node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Model path: '" << modelPath << "'" << std::endl;
        if (!labelsPath.empty()) {
            std::cerr << "  Labels path: '" << labelsPath << "'" << std::endl;
        }
        
        auto node = std::make_shared<cvedix_nodes::cvedix_trt_yolov8_detector>(
            nodeName,
            modelPath,
            labelsPath
        );
        
        std::cerr << "[PipelineBuilder] ✓ TRT YOLOv8 detector node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createTRTYOLOv8DetectorNode: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createTRTYOLOv8SegDetectorNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string modelPath = params.count("model_path") ? params.at("model_path") : "";
        std::string labelsPath = params.count("labels_path") ? params.at("labels_path") : "";
        
        if (modelPath.empty()) {
            auto it = req.additionalParams.find("MODEL_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                modelPath = it->second;
            }
        }
        if (labelsPath.empty()) {
            auto it = req.additionalParams.find("LABELS_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                labelsPath = it->second;
            }
        }
        
        if (nodeName.empty() || modelPath.empty()) {
            throw std::invalid_argument("Node name and model path are required");
        }
        
        std::cerr << "[PipelineBuilder] Creating TRT YOLOv8 segmentation detector node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Model path: '" << modelPath << "'" << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_trt_yolov8_seg_detector>(
            nodeName,
            modelPath,
            labelsPath
        );
        
        std::cerr << "[PipelineBuilder] ✓ TRT YOLOv8 segmentation detector node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createTRTYOLOv8SegDetectorNode: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createTRTYOLOv8PoseDetectorNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string modelPath = params.count("model_path") ? params.at("model_path") : "";
        if (modelPath.empty()) {
            auto it = req.additionalParams.find("MODEL_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                modelPath = it->second;
            }
        }
        
        if (nodeName.empty() || modelPath.empty()) {
            throw std::invalid_argument("Node name and model path are required");
        }
        
        std::cerr << "[PipelineBuilder] Creating TRT YOLOv8 pose detector node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Model path: '" << modelPath << "'" << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_trt_yolov8_pose_detector>(
            nodeName,
            modelPath
        );
        
        std::cerr << "[PipelineBuilder] ✓ TRT YOLOv8 pose detector node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createTRTYOLOv8PoseDetectorNode: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createTRTYOLOv8ClassifierNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string modelPath = params.count("model_path") ? params.at("model_path") : "";
        std::string labelsPath = params.count("labels_path") ? params.at("labels_path") : "";
        
        // Parse class_ids_applied_to from params (comma-separated)
        std::vector<int> classIdsAppliedTo;
        if (params.count("class_ids_applied_to")) {
            std::string idsStr = params.at("class_ids_applied_to");
            std::istringstream iss(idsStr);
            std::string token;
            while (std::getline(iss, token, ',')) {
                try {
                    classIdsAppliedTo.push_back(std::stoi(token));
                } catch (...) {
                    // Ignore invalid tokens
                }
            }
        }
        
        int minWidth = params.count("min_width_applied_to") ? std::stoi(params.at("min_width_applied_to")) : 0;
        int minHeight = params.count("min_height_applied_to") ? std::stoi(params.at("min_height_applied_to")) : 0;
        
        if (modelPath.empty()) {
            auto it = req.additionalParams.find("MODEL_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                modelPath = it->second;
            }
        }
        
        if (nodeName.empty() || modelPath.empty()) {
            throw std::invalid_argument("Node name and model path are required");
        }
        
        std::cerr << "[PipelineBuilder] Creating TRT YOLOv8 classifier node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Model path: '" << modelPath << "'" << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_trt_yolov8_classifier>(
            nodeName,
            modelPath,
            labelsPath,
            classIdsAppliedTo,
            minWidth,
            minHeight
        );
        
        std::cerr << "[PipelineBuilder] ✓ TRT YOLOv8 classifier node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createTRTYOLOv8ClassifierNode: " << e.what() << std::endl;
        throw;
    }
}

// ========== TensorRT Vehicle Nodes Implementation ==========

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createTRTVehicleDetectorNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string modelPath = params.count("model_path") ? params.at("model_path") : "";
        if (modelPath.empty()) {
            auto it = req.additionalParams.find("MODEL_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                modelPath = it->second;
            }
        }
        
        if (nodeName.empty() || modelPath.empty()) {
            throw std::invalid_argument("Node name and model path are required");
        }
        
        std::cerr << "[PipelineBuilder] Creating TRT vehicle detector node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Model path: '" << modelPath << "'" << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_trt_vehicle_detector>(
            nodeName,
            modelPath
        );
        
        std::cerr << "[PipelineBuilder] ✓ TRT vehicle detector node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createTRTVehicleDetectorNode: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createTRTVehiclePlateDetectorNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string detModelPath = params.count("det_model_path") ? params.at("det_model_path") : "";
        std::string recModelPath = params.count("rec_model_path") ? params.at("rec_model_path") : "";
        
        if (detModelPath.empty()) {
            auto it = req.additionalParams.find("DET_MODEL_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                detModelPath = it->second;
            }
        }
        if (recModelPath.empty()) {
            auto it = req.additionalParams.find("REC_MODEL_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                recModelPath = it->second;
            }
        }
        
        if (nodeName.empty() || detModelPath.empty()) {
            throw std::invalid_argument("Node name and detection model path are required");
        }
        
        std::cerr << "[PipelineBuilder] Creating TRT vehicle plate detector node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Detection model path: '" << detModelPath << "'" << std::endl;
        if (!recModelPath.empty()) {
            std::cerr << "  Recognition model path: '" << recModelPath << "'" << std::endl;
        }
        
        auto node = std::make_shared<cvedix_nodes::cvedix_trt_vehicle_plate_detector>(
            nodeName,
            detModelPath,
            recModelPath
        );
        
        std::cerr << "[PipelineBuilder] ✓ TRT vehicle plate detector node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createTRTVehiclePlateDetectorNode: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createTRTVehiclePlateDetectorV2Node(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string detModelPath = params.count("det_model_path") ? params.at("det_model_path") : "";
        std::string recModelPath = params.count("rec_model_path") ? params.at("rec_model_path") : "";
        
        if (detModelPath.empty()) {
            auto it = req.additionalParams.find("DET_MODEL_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                detModelPath = it->second;
            }
        }
        if (recModelPath.empty()) {
            auto it = req.additionalParams.find("REC_MODEL_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                recModelPath = it->second;
            }
        }
        
        if (nodeName.empty() || detModelPath.empty()) {
            throw std::invalid_argument("Node name and detection model path are required");
        }
        
        std::cerr << "[PipelineBuilder] Creating TRT vehicle plate detector v2 node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Detection model path: '" << detModelPath << "'" << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_trt_vehicle_plate_detector_v2>(
            nodeName,
            detModelPath,
            recModelPath
        );
        
        std::cerr << "[PipelineBuilder] ✓ TRT vehicle plate detector v2 node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createTRTVehiclePlateDetectorV2Node: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createTRTVehicleColorClassifierNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string modelPath = params.count("model_path") ? params.at("model_path") : "";
        
        // Parse class_ids_applied_to
        std::vector<int> classIdsAppliedTo;
        if (params.count("class_ids_applied_to")) {
            std::string idsStr = params.at("class_ids_applied_to");
            std::istringstream iss(idsStr);
            std::string token;
            while (std::getline(iss, token, ',')) {
                try {
                    classIdsAppliedTo.push_back(std::stoi(token));
                } catch (...) {}
            }
        }
        
        int minWidth = params.count("min_width_applied_to") ? std::stoi(params.at("min_width_applied_to")) : 0;
        int minHeight = params.count("min_height_applied_to") ? std::stoi(params.at("min_height_applied_to")) : 0;
        
        if (modelPath.empty()) {
            auto it = req.additionalParams.find("MODEL_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                modelPath = it->second;
            }
        }
        
        if (nodeName.empty() || modelPath.empty()) {
            throw std::invalid_argument("Node name and model path are required");
        }
        
        std::cerr << "[PipelineBuilder] Creating TRT vehicle color classifier node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Model path: '" << modelPath << "'" << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_trt_vehicle_color_classifier>(
            nodeName,
            modelPath,
            classIdsAppliedTo,
            minWidth,
            minHeight
        );
        
        std::cerr << "[PipelineBuilder] ✓ TRT vehicle color classifier node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createTRTVehicleColorClassifierNode: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createTRTVehicleTypeClassifierNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string modelPath = params.count("model_path") ? params.at("model_path") : "";
        
        std::vector<int> classIdsAppliedTo;
        if (params.count("class_ids_applied_to")) {
            std::string idsStr = params.at("class_ids_applied_to");
            std::istringstream iss(idsStr);
            std::string token;
            while (std::getline(iss, token, ',')) {
                try {
                    classIdsAppliedTo.push_back(std::stoi(token));
                } catch (...) {}
            }
        }
        
        int minWidth = params.count("min_width_applied_to") ? std::stoi(params.at("min_width_applied_to")) : 0;
        int minHeight = params.count("min_height_applied_to") ? std::stoi(params.at("min_height_applied_to")) : 0;
        
        if (modelPath.empty()) {
            auto it = req.additionalParams.find("MODEL_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                modelPath = it->second;
            }
        }
        
        if (nodeName.empty() || modelPath.empty()) {
            throw std::invalid_argument("Node name and model path are required");
        }
        
        std::cerr << "[PipelineBuilder] Creating TRT vehicle type classifier node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Model path: '" << modelPath << "'" << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_trt_vehicle_type_classifier>(
            nodeName,
            modelPath,
            classIdsAppliedTo,
            minWidth,
            minHeight
        );
        
        std::cerr << "[PipelineBuilder] ✓ TRT vehicle type classifier node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createTRTVehicleTypeClassifierNode: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createTRTVehicleFeatureEncoderNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string modelPath = params.count("model_path") ? params.at("model_path") : "";
        
        std::vector<int> classIdsAppliedTo;
        if (params.count("class_ids_applied_to")) {
            std::string idsStr = params.at("class_ids_applied_to");
            std::istringstream iss(idsStr);
            std::string token;
            while (std::getline(iss, token, ',')) {
                try {
                    classIdsAppliedTo.push_back(std::stoi(token));
                } catch (...) {}
            }
        }
        
        int minWidth = params.count("min_width_applied_to") ? std::stoi(params.at("min_width_applied_to")) : 0;
        int minHeight = params.count("min_height_applied_to") ? std::stoi(params.at("min_height_applied_to")) : 0;
        
        if (modelPath.empty()) {
            auto it = req.additionalParams.find("MODEL_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                modelPath = it->second;
            }
        }
        
        if (nodeName.empty() || modelPath.empty()) {
            throw std::invalid_argument("Node name and model path are required");
        }
        
        std::cerr << "[PipelineBuilder] Creating TRT vehicle feature encoder node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Model path: '" << modelPath << "'" << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_trt_vehicle_feature_encoder>(
            nodeName,
            modelPath,
            classIdsAppliedTo,
            minWidth,
            minHeight
        );
        
        std::cerr << "[PipelineBuilder] ✓ TRT vehicle feature encoder node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createTRTVehicleFeatureEncoderNode: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createTRTVehicleScannerNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string modelPath = params.count("model_path") ? params.at("model_path") : "";
        if (modelPath.empty()) {
            auto it = req.additionalParams.find("MODEL_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                modelPath = it->second;
            }
        }
        
        if (nodeName.empty() || modelPath.empty()) {
            throw std::invalid_argument("Node name and model path are required");
        }
        
        std::cerr << "[PipelineBuilder] Creating TRT vehicle scanner node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Model path: '" << modelPath << "'" << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_trt_vehicle_scanner>(
            nodeName,
            modelPath
        );
        
        std::cerr << "[PipelineBuilder] ✓ TRT vehicle scanner node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createTRTVehicleScannerNode: " << e.what() << std::endl;
        throw;
    }
}
#endif // CVEDIX_WITH_TRT

// ========== RKNN Nodes Implementation ==========

#ifdef CVEDIX_WITH_RKNN
std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createRKNNYOLOv8DetectorNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string modelPath = params.count("model_path") ? params.at("model_path") : "";
        float scoreThreshold = params.count("score_threshold") ? std::stof(params.at("score_threshold")) : 0.5f;
        float nmsThreshold = params.count("nms_threshold") ? std::stof(params.at("nms_threshold")) : 0.5f;
        int inputWidth = params.count("input_width") ? std::stoi(params.at("input_width")) : 640;
        int inputHeight = params.count("input_height") ? std::stoi(params.at("input_height")) : 640;
        int numClasses = params.count("num_classes") ? std::stoi(params.at("num_classes")) : 80;
        
        if (modelPath.empty()) {
            auto it = req.additionalParams.find("MODEL_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                modelPath = it->second;
            }
        }
        
        if (nodeName.empty() || modelPath.empty()) {
            throw std::invalid_argument("Node name and model path are required");
        }
        
        std::cerr << "[PipelineBuilder] Creating RKNN YOLOv8 detector node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Model path: '" << modelPath << "'" << std::endl;
        std::cerr << "  Score threshold: " << scoreThreshold << std::endl;
        std::cerr << "  NMS threshold: " << nmsThreshold << std::endl;
        std::cerr << "  Input size: " << inputWidth << "x" << inputHeight << std::endl;
        std::cerr << "  Num classes: " << numClasses << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_rknn_yolov8_detector_node>(
            nodeName,
            modelPath,
            scoreThreshold,
            nmsThreshold,
            inputWidth,
            inputHeight,
            numClasses
        );
        
        std::cerr << "[PipelineBuilder] ✓ RKNN YOLOv8 detector node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createRKNNYOLOv8DetectorNode: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createRKNNFaceDetectorNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string modelPath = params.count("model_path") ? params.at("model_path") : "";
        float scoreThreshold = params.count("score_threshold") ? std::stof(params.at("score_threshold")) : 0.5f;
        float nmsThreshold = params.count("nms_threshold") ? std::stof(params.at("nms_threshold")) : 0.5f;
        int inputWidth = params.count("input_width") ? std::stoi(params.at("input_width")) : 640;
        int inputHeight = params.count("input_height") ? std::stoi(params.at("input_height")) : 640;
        int topK = params.count("top_k") ? std::stoi(params.at("top_k")) : 50;
        
        if (modelPath.empty()) {
            auto it = req.additionalParams.find("MODEL_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                modelPath = it->second;
            }
        }
        
        if (nodeName.empty() || modelPath.empty()) {
            throw std::invalid_argument("Node name and model path are required");
        }
        
        std::cerr << "[PipelineBuilder] Creating RKNN face detector node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Model path: '" << modelPath << "'" << std::endl;
        std::cerr << "  Score threshold: " << scoreThreshold << std::endl;
        std::cerr << "  NMS threshold: " << nmsThreshold << std::endl;
        std::cerr << "  Input size: " << inputWidth << "x" << inputHeight << std::endl;
        std::cerr << "  Top K: " << topK << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_rknn_face_detector_node>(
            nodeName,
            modelPath,
            scoreThreshold,
            nmsThreshold,
            inputWidth,
            inputHeight,
            topK
        );
        
        std::cerr << "[PipelineBuilder] ✓ RKNN face detector node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createRKNNFaceDetectorNode: " << e.what() << std::endl;
        throw;
    }
}
#endif // CVEDIX_WITH_RKNN

// ========== Other Inference Nodes Implementation ==========

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createYOLODetectorNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        // Support both naming conventions: weights_path/config_path and model_path/model_config_path
        std::string modelPath = "";
        if (params.count("weights_path")) {
            modelPath = params.at("weights_path");
        } else if (params.count("model_path")) {
            modelPath = params.at("model_path");
        }
        
        std::string modelConfigPath = "";
        if (params.count("config_path")) {
            modelConfigPath = params.at("config_path");
        } else if (params.count("model_config_path")) {
            modelConfigPath = params.at("model_config_path");
        }
        
        std::string labelsPath = params.count("labels_path") ? params.at("labels_path") : "";
        int inputWidth = params.count("input_width") ? std::stoi(params.at("input_width")) : 416;
        int inputHeight = params.count("input_height") ? std::stoi(params.at("input_height")) : 416;
        int batchSize = params.count("batch_size") ? std::stoi(params.at("batch_size")) : 1;
        int classIdOffset = params.count("class_id_offset") ? std::stoi(params.at("class_id_offset")) : 0;
        float scoreThreshold = params.count("score_threshold") ? std::stof(params.at("score_threshold")) : 0.5f;
        float confidenceThreshold = params.count("confidence_threshold") ? std::stof(params.at("confidence_threshold")) : 0.5f;
        float nmsThreshold = params.count("nms_threshold") ? std::stof(params.at("nms_threshold")) : 0.5f;
        
        // Try to get from additionalParams if still empty
        if (modelPath.empty()) {
            auto it = req.additionalParams.find("WEIGHTS_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                modelPath = it->second;
            } else {
                it = req.additionalParams.find("MODEL_PATH");
                if (it != req.additionalParams.end() && !it->second.empty()) {
                    modelPath = it->second;
                }
            }
        }
        if (modelConfigPath.empty()) {
            auto it = req.additionalParams.find("CONFIG_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                modelConfigPath = it->second;
            }
        }
        if (labelsPath.empty()) {
            auto it = req.additionalParams.find("LABELS_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                labelsPath = it->second;
            }
        }
        
        if (nodeName.empty() || modelPath.empty()) {
            throw std::invalid_argument("Node name and model path are required");
        }
        
        std::cerr << "[PipelineBuilder] Creating YOLO detector node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Model path: '" << modelPath << "'" << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_yolo_detector_node>(
            nodeName,
            modelPath,
            modelConfigPath,
            labelsPath,
            inputWidth,
            inputHeight,
            batchSize,
            classIdOffset,
            scoreThreshold,
            confidenceThreshold,
            nmsThreshold
        );
        
        std::cerr << "[PipelineBuilder] ✓ YOLO detector node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createYOLODetectorNode: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createENetSegNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string modelPath = params.count("model_path") ? params.at("model_path") : "";
        std::string modelConfigPath = params.count("model_config_path") ? params.at("model_config_path") : "";
        std::string labelsPath = params.count("labels_path") ? params.at("labels_path") : "";
        int inputWidth = params.count("input_width") ? std::stoi(params.at("input_width")) : 1024;
        int inputHeight = params.count("input_height") ? std::stoi(params.at("input_height")) : 512;
        int batchSize = params.count("batch_size") ? std::stoi(params.at("batch_size")) : 1;
        int classIdOffset = params.count("class_id_offset") ? std::stoi(params.at("class_id_offset")) : 0;
        
        if (modelPath.empty()) {
            auto it = req.additionalParams.find("MODEL_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                modelPath = it->second;
            }
        }
        
        if (nodeName.empty() || modelPath.empty()) {
            throw std::invalid_argument("Node name and model path are required");
        }
        
        std::cerr << "[PipelineBuilder] Creating ENet segmentation node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Model path: '" << modelPath << "'" << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_enet_seg_node>(
            nodeName,
            modelPath,
            modelConfigPath,
            labelsPath,
            inputWidth,
            inputHeight,
            batchSize,
            classIdOffset
        );
        
        std::cerr << "[PipelineBuilder] ✓ ENet segmentation node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createENetSegNode: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createMaskRCNNDetectorNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string modelPath = params.count("model_path") ? params.at("model_path") : "";
        std::string modelConfigPath = params.count("model_config_path") ? params.at("model_config_path") : "";
        std::string labelsPath = params.count("labels_path") ? params.at("labels_path") : "";
        int inputWidth = params.count("input_width") ? std::stoi(params.at("input_width")) : 416;
        int inputHeight = params.count("input_height") ? std::stoi(params.at("input_height")) : 416;
        int batchSize = params.count("batch_size") ? std::stoi(params.at("batch_size")) : 1;
        int classIdOffset = params.count("class_id_offset") ? std::stoi(params.at("class_id_offset")) : 0;
        float scoreThreshold = params.count("score_threshold") ? std::stof(params.at("score_threshold")) : 0.5f;
        
        if (modelPath.empty()) {
            auto it = req.additionalParams.find("MODEL_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                modelPath = it->second;
            }
        }
        
        if (nodeName.empty() || modelPath.empty()) {
            throw std::invalid_argument("Node name and model path are required");
        }
        
        std::cerr << "[PipelineBuilder] Creating Mask RCNN detector node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Model path: '" << modelPath << "'" << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_mask_rcnn_detector_node>(
            nodeName,
            modelPath,
            modelConfigPath,
            labelsPath,
            inputWidth,
            inputHeight,
            batchSize,
            classIdOffset,
            scoreThreshold
        );
        
        std::cerr << "[PipelineBuilder] ✓ Mask RCNN detector node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createMaskRCNNDetectorNode: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createOpenPoseDetectorNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string modelPath = params.count("model_path") ? params.at("model_path") : "";
        std::string modelConfigPath = params.count("model_config_path") ? params.at("model_config_path") : "";
        std::string labelsPath = params.count("labels_path") ? params.at("labels_path") : "";
        int inputWidth = params.count("input_width") ? std::stoi(params.at("input_width")) : 368;
        int inputHeight = params.count("input_height") ? std::stoi(params.at("input_height")) : 368;
        int batchSize = params.count("batch_size") ? std::stoi(params.at("batch_size")) : 1;
        int classIdOffset = params.count("class_id_offset") ? std::stoi(params.at("class_id_offset")) : 0;
        float scoreThreshold = params.count("score_threshold") ? std::stof(params.at("score_threshold")) : 0.1f;
        
        if (modelPath.empty()) {
            auto it = req.additionalParams.find("MODEL_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                modelPath = it->second;
            }
        }
        
        if (nodeName.empty() || modelPath.empty()) {
            throw std::invalid_argument("Node name and model path are required");
        }
        
        std::cerr << "[PipelineBuilder] Creating OpenPose detector node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Model path: '" << modelPath << "'" << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_openpose_detector_node>(
            nodeName,
            modelPath,
            modelConfigPath,
            labelsPath,
            inputWidth,
            inputHeight,
            batchSize,
            classIdOffset,
            scoreThreshold
        );
        
        std::cerr << "[PipelineBuilder] ✓ OpenPose detector node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createOpenPoseDetectorNode: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createClassifierNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string modelPath = params.count("model_path") ? params.at("model_path") : "";
        std::string modelConfigPath = params.count("model_config_path") ? params.at("model_config_path") : "";
        std::string labelsPath = params.count("labels_path") ? params.at("labels_path") : "";
        int inputWidth = params.count("input_width") ? std::stoi(params.at("input_width")) : 128;
        int inputHeight = params.count("input_height") ? std::stoi(params.at("input_height")) : 128;
        int batchSize = params.count("batch_size") ? std::stoi(params.at("batch_size")) : 1;
        
        // Parse class_ids_applied_to
        std::vector<int> classIdsAppliedTo;
        if (params.count("class_ids_applied_to")) {
            std::string idsStr = params.at("class_ids_applied_to");
            std::istringstream iss(idsStr);
            std::string token;
            while (std::getline(iss, token, ',')) {
                try {
                    classIdsAppliedTo.push_back(std::stoi(token));
                } catch (...) {}
            }
        }
        
        int minWidth = params.count("min_width_applied_to") ? std::stoi(params.at("min_width_applied_to")) : 0;
        int minHeight = params.count("min_height_applied_to") ? std::stoi(params.at("min_height_applied_to")) : 0;
        int cropPadding = params.count("crop_padding") ? std::stoi(params.at("crop_padding")) : 10;
        bool needSoftmax = params.count("need_softmax") ? (params.at("need_softmax") == "true" || params.at("need_softmax") == "1") : true;
        
        if (modelPath.empty()) {
            auto it = req.additionalParams.find("MODEL_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                modelPath = it->second;
            }
        }
        
        if (nodeName.empty() || modelPath.empty()) {
            throw std::invalid_argument("Node name and model path are required");
        }
        
        std::cerr << "[PipelineBuilder] Creating classifier node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Model path: '" << modelPath << "'" << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_classifier_node>(
            nodeName,
            modelPath,
            modelConfigPath,
            labelsPath,
            inputWidth,
            inputHeight,
            batchSize,
            classIdsAppliedTo,
            minWidth,
            minHeight,
            cropPadding,
            needSoftmax
        );
        
        std::cerr << "[PipelineBuilder] ✓ Classifier node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createClassifierNode: " << e.what() << std::endl;
        throw;
    }
}

// Note: createFeatureEncoderNode is disabled because cvedix_feature_encoder_node is abstract
// Use createSFaceEncoderNode (for "sface_feature_encoder") or createTRTVehicleFeatureEncoderNode (for "trt_vehicle_feature_encoder") instead
/*
std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createFeatureEncoderNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string modelPath = params.count("model_path") ? params.at("model_path") : "";
        
        if (modelPath.empty()) {
            auto it = req.additionalParams.find("MODEL_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                modelPath = it->second;
            }
        }
        
        if (nodeName.empty() || modelPath.empty()) {
            throw std::invalid_argument("Node name and model path are required");
        }
        
        std::cerr << "[PipelineBuilder] Creating feature encoder node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Model path: '" << modelPath << "'" << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_feature_encoder_node>(
            nodeName,
            modelPath
        );
        
        std::cerr << "[PipelineBuilder] ✓ Feature encoder node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createFeatureEncoderNode: " << e.what() << std::endl;
        throw;
    }
}
*/

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createLaneDetectorNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string modelPath = params.count("model_path") ? params.at("model_path") : "";
        std::string modelConfigPath = params.count("model_config_path") ? params.at("model_config_path") : "";
        std::string labelsPath = params.count("labels_path") ? params.at("labels_path") : "";
        int inputWidth = params.count("input_width") ? std::stoi(params.at("input_width")) : 736;
        int inputHeight = params.count("input_height") ? std::stoi(params.at("input_height")) : 416;
        int batchSize = params.count("batch_size") ? std::stoi(params.at("batch_size")) : 1;
        int classIdOffset = params.count("class_id_offset") ? std::stoi(params.at("class_id_offset")) : 0;
        
        if (modelPath.empty()) {
            auto it = req.additionalParams.find("MODEL_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                modelPath = it->second;
            }
        }
        
        if (nodeName.empty() || modelPath.empty()) {
            throw std::invalid_argument("Node name and model path are required");
        }
        
        std::cerr << "[PipelineBuilder] Creating lane detector node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Model path: '" << modelPath << "'" << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_lane_detector_node>(
            nodeName,
            modelPath,
            modelConfigPath,
            labelsPath,
            inputWidth,
            inputHeight,
            batchSize,
            classIdOffset
        );
        
        std::cerr << "[PipelineBuilder] ✓ Lane detector node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createLaneDetectorNode: " << e.what() << std::endl;
        throw;
    }
}

#ifdef CVEDIX_WITH_PADDLE
std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createPaddleOCRTextDetectorNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string detModelDir = params.count("det_model_dir") ? params.at("det_model_dir") : "";
        std::string clsModelDir = params.count("cls_model_dir") ? params.at("cls_model_dir") : "";
        std::string recModelDir = params.count("rec_model_dir") ? params.at("rec_model_dir") : "";
        std::string recCharDictPath = params.count("rec_char_dict_path") ? params.at("rec_char_dict_path") : "";
        
        if (nodeName.empty()) {
            throw std::invalid_argument("Node name is required");
        }
        
        std::cerr << "[PipelineBuilder] Creating PaddleOCR text detector node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        if (!detModelDir.empty()) {
            std::cerr << "  Detection model dir: '" << detModelDir << "'" << std::endl;
        }
        if (!recModelDir.empty()) {
            std::cerr << "  Recognition model dir: '" << recModelDir << "'" << std::endl;
        }
        
        auto node = std::make_shared<cvedix_nodes::cvedix_ppocr_text_detector_node>(
            nodeName,
            detModelDir,
            clsModelDir,
            recModelDir,
            recCharDictPath
        );
        
        std::cerr << "[PipelineBuilder] ✓ PaddleOCR text detector node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createPaddleOCRTextDetectorNode: " << e.what() << std::endl;
        throw;
    }
}
#endif

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createRestorationNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string bgRestorationModel = params.count("bg_restoration_model") ? params.at("bg_restoration_model") : "";
        std::string faceRestorationModel = params.count("face_restoration_model") ? params.at("face_restoration_model") : "";
        bool restorationToOSD = params.count("restoration_to_osd") ? (params.at("restoration_to_osd") == "true" || params.at("restoration_to_osd") == "1") : true;
        
        if (bgRestorationModel.empty()) {
            auto it = req.additionalParams.find("BG_RESTORATION_MODEL");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                bgRestorationModel = it->second;
            }
        }
        
        if (nodeName.empty() || bgRestorationModel.empty()) {
            throw std::invalid_argument("Node name and background restoration model path are required");
        }
        
        std::cerr << "[PipelineBuilder] Creating restoration node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Background restoration model: '" << bgRestorationModel << "'" << std::endl;
        if (!faceRestorationModel.empty()) {
            std::cerr << "  Face restoration model: '" << faceRestorationModel << "'" << std::endl;
        }
        
        auto node = std::make_shared<cvedix_nodes::cvedix_restoration_node>(
            nodeName,
            bgRestorationModel,
            faceRestorationModel,
            restorationToOSD
        );
        
        std::cerr << "[PipelineBuilder] ✓ Restoration node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createRestorationNode: " << e.what() << std::endl;
        throw;
    }
}

// ========== New Inference Nodes Implementation ==========

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createYOLOv11DetectorNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string modelPath = params.count("model_path") ? params.at("model_path") : "";
        
        if (modelPath.empty()) {
            auto it = req.additionalParams.find("MODEL_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                modelPath = it->second;
            }
        }
        
        if (nodeName.empty() || modelPath.empty()) {
            throw std::invalid_argument("Node name and model path are required");
        }
        
        std::cerr << "[PipelineBuilder] Creating YOLOv11 detector node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Model path: '" << modelPath << "'" << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_yolov11_detector_node>(
            nodeName,
            modelPath
        );
        
        std::cerr << "[PipelineBuilder] ✓ YOLOv11 detector node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createYOLOv11DetectorNode: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createFaceSwapNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string yunetFaceDetectModel = params.count("yunet_face_detect_model") ? params.at("yunet_face_detect_model") : "";
        std::string buffaloLFaceEncodingModel = params.count("buffalo_l_face_encoding_model") ? params.at("buffalo_l_face_encoding_model") : "";
        std::string emapFileForEmbeddings = params.count("emap_file_for_embeddings") ? params.at("emap_file_for_embeddings") : "";
        std::string insightfaceSwapModel = params.count("insightface_swap_model") ? params.at("insightface_swap_model") : "";
        std::string swapSourceImage = params.count("swap_source_image") ? params.at("swap_source_image") : "";
        int swapSourceFaceIndex = params.count("swap_source_face_index") ? std::stoi(params.at("swap_source_face_index")) : 0;
        int minFaceWH = params.count("min_face_w_h") ? std::stoi(params.at("min_face_w_h")) : 50;
        bool swapOnOSD = params.count("swap_on_osd") ? (params.at("swap_on_osd") == "true" || params.at("swap_on_osd") == "1") : true;
        bool actAsPrimaryDetector = params.count("act_as_primary_detector") ? (params.at("act_as_primary_detector") == "true" || params.at("act_as_primary_detector") == "1") : false;
        
        // Try to get from additionalParams if still empty
        if (yunetFaceDetectModel.empty()) {
            auto it = req.additionalParams.find("YUNET_FACE_DETECT_MODEL");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                yunetFaceDetectModel = it->second;
            }
        }
        if (buffaloLFaceEncodingModel.empty()) {
            auto it = req.additionalParams.find("BUFFALO_L_FACE_ENCODING_MODEL");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                buffaloLFaceEncodingModel = it->second;
            }
        }
        if (emapFileForEmbeddings.empty()) {
            auto it = req.additionalParams.find("EMAP_FILE_FOR_EMBEDDINGS");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                emapFileForEmbeddings = it->second;
            }
        }
        if (insightfaceSwapModel.empty()) {
            auto it = req.additionalParams.find("INSIGHTFACE_SWAP_MODEL");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                insightfaceSwapModel = it->second;
            }
        }
        if (swapSourceImage.empty()) {
            auto it = req.additionalParams.find("SWAP_SOURCE_IMAGE");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                swapSourceImage = it->second;
            }
        }
        
        if (nodeName.empty() || yunetFaceDetectModel.empty() || buffaloLFaceEncodingModel.empty() || 
            emapFileForEmbeddings.empty() || insightfaceSwapModel.empty() || swapSourceImage.empty()) {
            throw std::invalid_argument("Node name and all model paths are required for face swap node");
        }
        
        std::cerr << "[PipelineBuilder] Creating face swap node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  YUNet face detect model: '" << yunetFaceDetectModel << "'" << std::endl;
        std::cerr << "  Buffalo L face encoding model: '" << buffaloLFaceEncodingModel << "'" << std::endl;
        std::cerr << "  EMap file for embeddings: '" << emapFileForEmbeddings << "'" << std::endl;
        std::cerr << "  InsightFace swap model: '" << insightfaceSwapModel << "'" << std::endl;
        std::cerr << "  Swap source image: '" << swapSourceImage << "'" << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_face_swap_node>(
            nodeName,
            yunetFaceDetectModel,
            buffaloLFaceEncodingModel,
            emapFileForEmbeddings,
            insightfaceSwapModel,
            swapSourceImage,
            swapSourceFaceIndex,
            minFaceWH,
            swapOnOSD,
            actAsPrimaryDetector
        );
        
        std::cerr << "[PipelineBuilder] ✓ Face swap node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createFaceSwapNode: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createInsightFaceRecognitionNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string modelPath = params.count("model_path") ? params.at("model_path") : "";
        
        if (modelPath.empty()) {
            auto it = req.additionalParams.find("MODEL_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                modelPath = it->second;
            }
        }
        
        if (nodeName.empty() || modelPath.empty()) {
            throw std::invalid_argument("Node name and model path are required");
        }
        
        std::cerr << "[PipelineBuilder] Creating InsightFace recognition node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Model path: '" << modelPath << "'" << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_insight_face_recognition_node>(
            nodeName,
            modelPath
        );
        
        std::cerr << "[PipelineBuilder] ✓ InsightFace recognition node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createInsightFaceRecognitionNode: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createMLLMAnalyserNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
#ifdef CVEDIX_WITH_LLM
    try {
        std::string modelName = params.count("model_name") ? params.at("model_name") : "";
        std::string prompt = params.count("prompt") ? params.at("prompt") : "";
        std::string apiBaseUrl = params.count("api_base_url") ? params.at("api_base_url") : "";
        std::string apiKey = params.count("api_key") ? params.at("api_key") : "";
        
        // Try to get from additionalParams if still empty
        if (modelName.empty()) {
            auto it = req.additionalParams.find("MODEL_NAME");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                modelName = it->second;
            }
        }
        if (prompt.empty()) {
            auto it = req.additionalParams.find("PROMPT");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                prompt = it->second;
            }
        }
        if (apiBaseUrl.empty()) {
            auto it = req.additionalParams.find("API_BASE_URL");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                apiBaseUrl = it->second;
            }
        }
        if (apiKey.empty()) {
            auto it = req.additionalParams.find("API_KEY");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                apiKey = it->second;
            }
        }
        
        // Parse backend type
        std::string backendTypeStr = params.count("backend_type") ? params.at("backend_type") : "ollama";
        llmlib::LLMBackendType backendType = llmlib::LLMBackendType::Ollama;
        if (backendTypeStr == "openai") {
            backendType = llmlib::LLMBackendType::OpenAI;
        } else if (backendTypeStr == "anthropic") {
            backendType = llmlib::LLMBackendType::Anthropic;
        }
        
        if (nodeName.empty() || modelName.empty() || prompt.empty() || apiBaseUrl.empty()) {
            throw std::invalid_argument("Node name, model name, prompt, and API base URL are required");
        }
        
        std::cerr << "[PipelineBuilder] Creating MLLM analyser node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Model name: '" << modelName << "'" << std::endl;
        std::cerr << "  Prompt: '" << prompt << "'" << std::endl;
        std::cerr << "  API base URL: '" << apiBaseUrl << "'" << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_mllm_analyser_node>(
            nodeName,
            modelName,
            prompt,
            apiBaseUrl,
            apiKey,
            backendType
        );
        
        std::cerr << "[PipelineBuilder] ✓ MLLM analyser node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createMLLMAnalyserNode: " << e.what() << std::endl;
        throw;
    }
#else
    throw std::runtime_error("MLLM analyser node is not available (CVEDIX_WITH_LLM not defined)");
#endif
}

#ifdef CVEDIX_WITH_RKNN
std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createRKNNYOLOv11DetectorNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string modelPath = params.count("model_path") ? params.at("model_path") : "";
        float scoreThreshold = params.count("score_threshold") ? std::stof(params.at("score_threshold")) : 0.5f;
        float nmsThreshold = params.count("nms_threshold") ? std::stof(params.at("nms_threshold")) : 0.5f;
        int inputWidth = params.count("input_width") ? std::stoi(params.at("input_width")) : 640;
        int inputHeight = params.count("input_height") ? std::stoi(params.at("input_height")) : 640;
        int numClasses = params.count("num_classes") ? std::stoi(params.at("num_classes")) : 80;
        
        if (modelPath.empty()) {
            auto it = req.additionalParams.find("MODEL_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                modelPath = it->second;
            }
        }
        
        if (nodeName.empty() || modelPath.empty()) {
            throw std::invalid_argument("Node name and model path are required");
        }
        
        std::cerr << "[PipelineBuilder] Creating RKNN YOLOv11 detector node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Model path: '" << modelPath << "'" << std::endl;
        std::cerr << "  Score threshold: " << scoreThreshold << std::endl;
        std::cerr << "  NMS threshold: " << nmsThreshold << std::endl;
        std::cerr << "  Input size: " << inputWidth << "x" << inputHeight << std::endl;
        std::cerr << "  Num classes: " << numClasses << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_rknn_yolov11_detector_node>(
            nodeName,
            modelPath,
            scoreThreshold,
            nmsThreshold,
            inputWidth,
            inputHeight,
            numClasses
        );
        
        std::cerr << "[PipelineBuilder] ✓ RKNN YOLOv11 detector node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createRKNNYOLOv11DetectorNode: " << e.what() << std::endl;
        throw;
    }
}
#endif // CVEDIX_WITH_RKNN

#ifdef CVEDIX_WITH_TRT
std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createTRTInsightFaceRecognitionNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string modelPath = params.count("model_path") ? params.at("model_path") : "";
        
        if (modelPath.empty()) {
            auto it = req.additionalParams.find("MODEL_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                modelPath = it->second;
            }
        }
        
        if (nodeName.empty() || modelPath.empty()) {
            throw std::invalid_argument("Node name and model path are required");
        }
        
        std::cerr << "[PipelineBuilder] Creating TensorRT InsightFace recognition node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Model path: '" << modelPath << "'" << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_trt_insight_face_recognition_node>(
            nodeName,
            modelPath
        );
        
        std::cerr << "[PipelineBuilder] ✓ TensorRT InsightFace recognition node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createTRTInsightFaceRecognitionNode: " << e.what() << std::endl;
        throw;
    }
}
#endif // CVEDIX_WITH_TRT

// ========== Source Nodes Implementation ==========

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createAppSourceNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        int channelIndex = params.count("channel") ? std::stoi(params.at("channel")) : 0;
        
        if (nodeName.empty()) {
            throw std::invalid_argument("Node name cannot be empty");
        }
        
        std::cerr << "[PipelineBuilder] Creating app source node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Channel index: " << channelIndex << std::endl;
        std::cerr << "  NOTE: Use push_frames() method to push frames into pipeline" << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_app_src_node>(
            nodeName,
            channelIndex
        );
        
        std::cerr << "[PipelineBuilder] ✓ App source node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createAppSourceNode: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createImageSourceNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        int channelIndex = params.count("channel") ? std::stoi(params.at("channel")) : 0;
        std::string portOrLocation = params.count("port_or_location") ? params.at("port_or_location") : "";
        int interval = params.count("interval") ? std::stoi(params.at("interval")) : 1;
        float resizeRatio = params.count("resize_ratio") ? std::stof(params.at("resize_ratio")) : 1.0f;
        bool cycle = params.count("cycle") ? (params.at("cycle") == "true" || params.at("cycle") == "1") : true;
        // Get decoder from config priority list if not specified
        std::string defaultDecoder = "jpegdec";
        std::string gstDecoderName = params.count("gst_decoder_name") ? params.at("gst_decoder_name") : selectDecoderFromPriority(defaultDecoder);
        
        // Get from additionalParams if not in params
        if (portOrLocation.empty()) {
            auto it = req.additionalParams.find("IMAGE_SRC_PORT_OR_LOCATION");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                portOrLocation = it->second;
            } else {
                // Default: use file path pattern
                portOrLocation = "./cvedix_data/test_images/%d.jpg";
            }
        }
        
        if (nodeName.empty()) {
            throw std::invalid_argument("Node name cannot be empty");
        }
        if (portOrLocation.empty()) {
            throw std::invalid_argument("port_or_location cannot be empty");
        }
        
        // Validate resize_ratio
        if (resizeRatio <= 0.0f || resizeRatio > 1.0f) {
            std::cerr << "[PipelineBuilder] Warning: resize_ratio out of range, using 1.0" << std::endl;
            resizeRatio = 1.0f;
        }
        
        std::cerr << "[PipelineBuilder] Creating image source node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Channel index: " << channelIndex << std::endl;
        std::cerr << "  Port/Location: '" << portOrLocation << "'" << std::endl;
        std::cerr << "  Interval: " << interval << " seconds" << std::endl;
        std::cerr << "  Resize ratio: " << resizeRatio << std::endl;
        std::cerr << "  Cycle: " << (cycle ? "true" : "false") << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_image_src_node>(
            nodeName,
            channelIndex,
            portOrLocation,
            interval,
            resizeRatio,
            cycle,
            gstDecoderName
        );
        
        std::cerr << "[PipelineBuilder] ✓ Image source node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createImageSourceNode: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createRTMPSourceNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        int channelIndex = params.count("channel") ? std::stoi(params.at("channel")) : 0;
        std::string rtmpUrl = params.count("rtmp_url") ? params.at("rtmp_url") : "";
        float resizeRatio = params.count("resize_ratio") ? std::stof(params.at("resize_ratio")) : 1.0f;
        // Get decoder from config priority list if not specified
        std::string defaultDecoder = "avdec_h264";
        std::string gstDecoderName = params.count("gst_decoder_name") ? params.at("gst_decoder_name") : selectDecoderFromPriority(defaultDecoder);
        int skipInterval = params.count("skip_interval") ? std::stoi(params.at("skip_interval")) : 0;
        
        // Get from additionalParams if not in params
        if (rtmpUrl.empty()) {
            auto it = req.additionalParams.find("RTMP_SRC_URL");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                rtmpUrl = it->second;
            } else {
                // Default fake data
                rtmpUrl = "rtmp://localhost:1935/live/stream";
            }
        }
        
        if (nodeName.empty()) {
            throw std::invalid_argument("Node name cannot be empty");
        }
        if (rtmpUrl.empty()) {
            throw std::invalid_argument("RTMP URL cannot be empty");
        }
        
        // Validate resize_ratio
        if (resizeRatio <= 0.0f || resizeRatio > 1.0f) {
            std::cerr << "[PipelineBuilder] Warning: resize_ratio out of range, using 1.0" << std::endl;
            resizeRatio = 1.0f;
        }
        
        std::cerr << "[PipelineBuilder] Creating RTMP source node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Channel index: " << channelIndex << std::endl;
        std::cerr << "  RTMP URL: '" << rtmpUrl << "'" << std::endl;
        std::cerr << "  Resize ratio: " << resizeRatio << std::endl;
        std::cerr << "  Skip interval: " << skipInterval << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_rtmp_src_node>(
            nodeName,
            channelIndex,
            rtmpUrl,
            resizeRatio,
            gstDecoderName,
            skipInterval
        );
        
        std::cerr << "[PipelineBuilder] ✓ RTMP source node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createRTMPSourceNode: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createUDPSourceNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        int channelIndex = params.count("channel") ? std::stoi(params.at("channel")) : 0;
        int port = params.count("port") ? std::stoi(params.at("port")) : 6000;
        float resizeRatio = params.count("resize_ratio") ? std::stof(params.at("resize_ratio")) : 1.0f;
        // Get decoder from config priority list if not specified
        std::string defaultDecoder = "avdec_h264";
        std::string gstDecoderName = params.count("gst_decoder_name") ? params.at("gst_decoder_name") : selectDecoderFromPriority(defaultDecoder);
        int skipInterval = params.count("skip_interval") ? std::stoi(params.at("skip_interval")) : 0;
        
        // Get from additionalParams if not in params
        if (params.count("port") == 0) {
            auto it = req.additionalParams.find("UDP_PORT");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                try {
                    port = std::stoi(it->second);
                } catch (...) {
                    std::cerr << "[PipelineBuilder] Warning: Invalid UDP_PORT, using default 6000" << std::endl;
                }
            }
        }
        
        if (nodeName.empty()) {
            throw std::invalid_argument("Node name cannot be empty");
        }
        if (port <= 0 || port > 65535) {
            throw std::invalid_argument("UDP port must be between 1 and 65535");
        }
        
        // Validate resize_ratio
        if (resizeRatio <= 0.0f || resizeRatio > 1.0f) {
            std::cerr << "[PipelineBuilder] Warning: resize_ratio out of range, using 1.0" << std::endl;
            resizeRatio = 1.0f;
        }
        
        std::cerr << "[PipelineBuilder] Creating UDP source node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Channel index: " << channelIndex << std::endl;
        std::cerr << "  Port: " << port << std::endl;
        std::cerr << "  Resize ratio: " << resizeRatio << std::endl;
        std::cerr << "  Skip interval: " << skipInterval << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_udp_src_node>(
            nodeName,
            channelIndex,
            port,
            resizeRatio,
            gstDecoderName,
            skipInterval
        );
        
        std::cerr << "[PipelineBuilder] ✓ UDP source node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createUDPSourceNode: " << e.what() << std::endl;
        throw;
    }
}

// ========== Broker Nodes Implementation ==========

// Temporarily disabled JSON broker nodes due to cereal dependency issue
// TODO: Re-enable after fixing cereal symlink or CVEDIX SDK update
/*
std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createJSONConsoleBrokerNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        // Parse broke_for enum
        std::string brokeForStr = params.count("broke_for") ? params.at("broke_for") : "NORMAL";
        cvedix_nodes::cvedix_broke_for brokeFor = cvedix_nodes::cvedix_broke_for::NORMAL;
        
        if (brokeForStr == "FACE") {
            brokeFor = cvedix_nodes::cvedix_broke_for::FACE;
        } else if (brokeForStr == "TEXT") {
            brokeFor = cvedix_nodes::cvedix_broke_for::TEXT;
        } else if (brokeForStr == "POSE") {
            brokeFor = cvedix_nodes::cvedix_broke_for::POSE;
        } else if (brokeForStr == "POSE") {
            brokeFor = cvedix_nodes::cvedix_broke_for::POSE;
        } else if (brokeForStr == "NORMAL") {
            brokeFor = cvedix_nodes::cvedix_broke_for::NORMAL;
        }
        
        int warnThreshold = params.count("broking_cache_warn_threshold") ? std::stoi(params.at("broking_cache_warn_threshold")) : 50;
        int ignoreThreshold = params.count("broking_cache_ignore_threshold") ? std::stoi(params.at("broking_cache_ignore_threshold")) : 200;
        
        if (nodeName.empty()) {
            throw std::invalid_argument("Node name cannot be empty");
        }
        
        std::cerr << "[PipelineBuilder] Creating JSON console broker node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Broke for: " << brokeForStr << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_json_console_broker_node>(
            nodeName,
            brokeFor,
            warnThreshold,
            ignoreThreshold
        );
        
        std::cerr << "[PipelineBuilder] ✓ JSON console broker node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createJSONConsoleBrokerNode: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createJSONEnhancedConsoleBrokerNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string brokeForStr = params.count("broke_for") ? params.at("broke_for") : "NORMAL";
        cvedix_nodes::cvedix_broke_for brokeFor = cvedix_nodes::cvedix_broke_for::NORMAL;
        
        if (brokeForStr == "FACE") {
            brokeFor = cvedix_nodes::cvedix_broke_for::FACE;
        } else if (brokeForStr == "TEXT") {
            brokeFor = cvedix_nodes::cvedix_broke_for::TEXT;
        } else if (brokeForStr == "POSE") {
            brokeFor = cvedix_nodes::cvedix_broke_for::POSE;
        }
        
        int warnThreshold = params.count("broking_cache_warn_threshold") ? std::stoi(params.at("broking_cache_warn_threshold")) : 50;
        int ignoreThreshold = params.count("broking_cache_ignore_threshold") ? std::stoi(params.at("broking_cache_ignore_threshold")) : 200;
        bool encodeFullFrame = params.count("encode_full_frame") ? (params.at("encode_full_frame") == "true" || params.at("encode_full_frame") == "1") : false;
        
        if (nodeName.empty()) {
            throw std::invalid_argument("Node name cannot be empty");
        }
        
        std::cerr << "[PipelineBuilder] Creating JSON enhanced console broker node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Broke for: " << brokeForStr << std::endl;
        std::cerr << "  Encode full frame: " << (encodeFullFrame ? "true" : "false") << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_json_enhanced_console_broker_node>(
            nodeName,
            brokeFor,
            warnThreshold,
            ignoreThreshold,
            encodeFullFrame
        );
        
        std::cerr << "[PipelineBuilder] ✓ JSON enhanced console broker node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createJSONEnhancedConsoleBrokerNode: " << e.what() << std::endl;
        throw;
    }
}

#ifdef CVEDIX_WITH_MQTT
std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createJSONMQTTBrokerNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string brokeForStr = params.count("broke_for") ? params.at("broke_for") : "NORMAL";
        cvedix_nodes::cvedix_broke_for brokeFor = cvedix_nodes::cvedix_broke_for::NORMAL;
        
        if (brokeForStr == "FACE") {
            brokeFor = cvedix_nodes::cvedix_broke_for::FACE;
        } else if (brokeForStr == "TEXT") {
            brokeFor = cvedix_nodes::cvedix_broke_for::TEXT;
        } else if (brokeForStr == "POSE") {
            brokeFor = cvedix_nodes::cvedix_broke_for::POSE;
        }
        
        // MQTT broker node requires custom callbacks - use fake/default for now
        // In real implementation, these should be provided via additionalParams or config
        std::cerr << "[PipelineBuilder] Creating JSON MQTT broker node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Broke for: " << brokeForStr << std::endl;
        std::cerr << "  NOTE: MQTT broker requires custom callbacks - using default implementation" << std::endl;
        
        // Create with default/null callbacks (fake data)
        auto node = std::make_shared<cvedix_nodes::cvedix_json_mqtt_broker_node>(
            nodeName,
            brokeFor,
            nullptr,  // json_transformer (nullptr = use original JSON)
            nullptr   // mqtt_publisher (nullptr = will need to be set later or use default)
        );
        
        std::cerr << "[PipelineBuilder] ✓ JSON MQTT broker node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createJSONMQTTBrokerNode: " << e.what() << std::endl;
        throw;
    }
}
#endif

#ifdef CVEDIX_WITH_KAFKA
std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createJSONKafkaBrokerNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string kafkaServers = params.count("kafka_servers") ? params.at("kafka_servers") : "127.0.0.1:9092";
        std::string topicName = params.count("topic_name") ? params.at("topic_name") : "videopipe_topic";
        std::string brokeForStr = params.count("broke_for") ? params.at("broke_for") : "NORMAL";
        cvedix_nodes::cvedix_broke_for brokeFor = cvedix_nodes::cvedix_broke_for::NORMAL;
        
        // Get from additionalParams if not in params
        if (params.count("kafka_servers") == 0) {
            auto it = req.additionalParams.find("KAFKA_SERVERS");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                kafkaServers = it->second;
            }
        }
        if (params.count("topic_name") == 0) {
            auto it = req.additionalParams.find("KAFKA_TOPIC");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                topicName = it->second;
            }
        }
        
        if (brokeForStr == "FACE") {
            brokeFor = cvedix_nodes::cvedix_broke_for::FACE;
        } else if (brokeForStr == "TEXT") {
            brokeFor = cvedix_nodes::cvedix_broke_for::TEXT;
        } else if (brokeForStr == "POSE") {
            brokeFor = cvedix_nodes::cvedix_broke_for::POSE;
        }
        
        int warnThreshold = params.count("broking_cache_warn_threshold") ? std::stoi(params.at("broking_cache_warn_threshold")) : 50;
        int ignoreThreshold = params.count("broking_cache_ignore_threshold") ? std::stoi(params.at("broking_cache_ignore_threshold")) : 200;
        
        if (nodeName.empty()) {
            throw std::invalid_argument("Node name cannot be empty");
        }
        
        std::cerr << "[PipelineBuilder] Creating JSON Kafka broker node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Kafka servers: '" << kafkaServers << "'" << std::endl;
        std::cerr << "  Topic name: '" << topicName << "'" << std::endl;
        std::cerr << "  Broke for: " << brokeForStr << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_json_kafka_broker_node>(
            nodeName,
            kafkaServers,
            topicName,
            brokeFor,
            warnThreshold,
            ignoreThreshold
        );
        
        std::cerr << "[PipelineBuilder] ✓ JSON Kafka broker node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createJSONKafkaBrokerNode: " << e.what() << std::endl;
        throw;
    }
}
#endif
*/
// Stub implementations to avoid linker errors
std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createJSONConsoleBrokerNode(
    const std::string&, const std::map<std::string, std::string>&, const CreateInstanceRequest&) {
    return nullptr;
}
std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createJSONEnhancedConsoleBrokerNode(
    const std::string&, const std::map<std::string, std::string>&, const CreateInstanceRequest&) {
    return nullptr;
}
#ifdef CVEDIX_WITH_MQTT
std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createJSONMQTTBrokerNode(
    const std::string&, const std::map<std::string, std::string>&, const CreateInstanceRequest&) {
    return nullptr;
}
#endif
#ifdef CVEDIX_WITH_KAFKA
std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createJSONKafkaBrokerNode(
    const std::string&, const std::map<std::string, std::string>&, const CreateInstanceRequest&) {
    return nullptr;
}
#endif

// Temporarily disabled all broker node implementations due to cereal dependency issue
/*
std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createXMLFileBrokerNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string brokeForStr = params.count("broke_for") ? params.at("broke_for") : "NORMAL";
        cvedix_nodes::cvedix_broke_for brokeFor = cvedix_nodes::cvedix_broke_for::NORMAL;
        
        if (brokeForStr == "FACE") {
            brokeFor = cvedix_nodes::cvedix_broke_for::FACE;
        } else if (brokeForStr == "TEXT") {
            brokeFor = cvedix_nodes::cvedix_broke_for::TEXT;
        } else if (brokeForStr == "POSE") {
            brokeFor = cvedix_nodes::cvedix_broke_for::POSE;
        }
        
        std::string filePath = params.count("file_path") ? params.at("file_path") : "";
        if (filePath.empty()) {
            auto it = req.additionalParams.find("XML_FILE_PATH");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                filePath = it->second;
            } else {
                // Default fake data
                filePath = "./output/broker_output.xml";
            }
        }
        
        int warnThreshold = params.count("broking_cache_warn_threshold") ? std::stoi(params.at("broking_cache_warn_threshold")) : 50;
        int ignoreThreshold = params.count("broking_cache_ignore_threshold") ? std::stoi(params.at("broking_cache_ignore_threshold")) : 200;
        
        if (nodeName.empty()) {
            throw std::invalid_argument("Node name cannot be empty");
        }
        
        std::cerr << "[PipelineBuilder] Creating XML file broker node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  File path: '" << filePath << "'" << std::endl;
        std::cerr << "  Broke for: " << brokeForStr << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_xml_file_broker_node>(
            nodeName,
            brokeFor,
            filePath,
            warnThreshold,
            ignoreThreshold
        );
        
        std::cerr << "[PipelineBuilder] ✓ XML file broker node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createXMLFileBrokerNode: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createXMLSocketBrokerNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string desIp = params.count("des_ip") ? params.at("des_ip") : "";
        int desPort = params.count("des_port") ? std::stoi(params.at("des_port")) : 0;
        std::string brokeForStr = params.count("broke_for") ? params.at("broke_for") : "NORMAL";
        cvedix_nodes::cvedix_broke_for brokeFor = cvedix_nodes::cvedix_broke_for::NORMAL;
        
        // Get from additionalParams if not in params
        if (desIp.empty()) {
            auto it = req.additionalParams.find("XML_SOCKET_IP");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                desIp = it->second;
            } else {
                // Default fake data
                desIp = "127.0.0.1";
            }
        }
        if (desPort == 0) {
            auto it = req.additionalParams.find("XML_SOCKET_PORT");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                try {
                    desPort = std::stoi(it->second);
                } catch (...) {
                    desPort = 8080; // Default fake data
                }
            } else {
                desPort = 8080; // Default fake data
            }
        }
        
        if (brokeForStr == "FACE") {
            brokeFor = cvedix_nodes::cvedix_broke_for::FACE;
        } else if (brokeForStr == "TEXT") {
            brokeFor = cvedix_nodes::cvedix_broke_for::TEXT;
        } else if (brokeForStr == "POSE") {
            brokeFor = cvedix_nodes::cvedix_broke_for::POSE;
        }
        
        int warnThreshold = params.count("broking_cache_warn_threshold") ? std::stoi(params.at("broking_cache_warn_threshold")) : 50;
        int ignoreThreshold = params.count("broking_cache_ignore_threshold") ? std::stoi(params.at("broking_cache_ignore_threshold")) : 200;
        
        if (nodeName.empty()) {
            throw std::invalid_argument("Node name cannot be empty");
        }
        
        std::cerr << "[PipelineBuilder] Creating XML socket broker node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Destination IP: '" << desIp << "'" << std::endl;
        std::cerr << "  Destination port: " << desPort << std::endl;
        std::cerr << "  Broke for: " << brokeForStr << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_xml_socket_broker_node>(
            nodeName,
            desIp,
            desPort,
            brokeFor,
            warnThreshold,
            ignoreThreshold
        );
        
        std::cerr << "[PipelineBuilder] ✓ XML socket broker node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createXMLSocketBrokerNode: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createMessageBrokerNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string brokeForStr = params.count("broke_for") ? params.at("broke_for") : "NORMAL";
        cvedix_nodes::cvedix_broke_for brokeFor = cvedix_nodes::cvedix_broke_for::NORMAL;
        
        if (brokeForStr == "FACE") {
            brokeFor = cvedix_nodes::cvedix_broke_for::FACE;
        } else if (brokeForStr == "TEXT") {
            brokeFor = cvedix_nodes::cvedix_broke_for::TEXT;
        } else if (brokeForStr == "POSE") {
            brokeFor = cvedix_nodes::cvedix_broke_for::POSE;
        }
        
        int warnThreshold = params.count("broking_cache_warn_threshold") ? std::stoi(params.at("broking_cache_warn_threshold")) : 50;
        int ignoreThreshold = params.count("broking_cache_ignore_threshold") ? std::stoi(params.at("broking_cache_ignore_threshold")) : 200;
        
        if (nodeName.empty()) {
            throw std::invalid_argument("Node name cannot be empty");
        }
        
        std::cerr << "[PipelineBuilder] Creating message broker node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Broke for: " << brokeForStr << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_msg_broker_node>(
            nodeName,
            brokeFor,
            warnThreshold,
            ignoreThreshold
        );
        
        std::cerr << "[PipelineBuilder] ✓ Message broker node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createMessageBrokerNode: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createBASocketBrokerNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string desIp = params.count("des_ip") ? params.at("des_ip") : "";
        int desPort = params.count("des_port") ? std::stoi(params.at("des_port")) : 0;
        std::string brokeForStr = params.count("broke_for") ? params.at("broke_for") : "NORMAL";
        cvedix_nodes::cvedix_broke_for brokeFor = cvedix_nodes::cvedix_broke_for::NORMAL;
        
        // Get from additionalParams if not in params
        if (desIp.empty()) {
            auto it = req.additionalParams.find("BA_SOCKET_IP");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                desIp = it->second;
            } else {
                desIp = "127.0.0.1"; // Default fake data
            }
        }
        if (desPort == 0) {
            auto it = req.additionalParams.find("BA_SOCKET_PORT");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                try {
                    desPort = std::stoi(it->second);
                } catch (...) {
                    desPort = 8080; // Default fake data
                }
            } else {
                desPort = 8080; // Default fake data
            }
        }
        
        if (brokeForStr == "FACE") {
            brokeFor = cvedix_nodes::cvedix_broke_for::FACE;
        } else if (brokeForStr == "TEXT") {
            brokeFor = cvedix_nodes::cvedix_broke_for::TEXT;
        } else if (brokeForStr == "POSE") {
            brokeFor = cvedix_nodes::cvedix_broke_for::POSE;
        }
        
        int warnThreshold = params.count("broking_cache_warn_threshold") ? std::stoi(params.at("broking_cache_warn_threshold")) : 50;
        int ignoreThreshold = params.count("broking_cache_ignore_threshold") ? std::stoi(params.at("broking_cache_ignore_threshold")) : 200;
        
        if (nodeName.empty()) {
            throw std::invalid_argument("Node name cannot be empty");
        }
        
        std::cerr << "[PipelineBuilder] Creating BA socket broker node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Destination IP: '" << desIp << "'" << std::endl;
        std::cerr << "  Destination port: " << desPort << std::endl;
        std::cerr << "  Broke for: " << brokeForStr << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_ba_socket_broker_node>(
            nodeName,
            desIp,
            desPort,
            brokeFor,
            warnThreshold,
            ignoreThreshold
        );
        
        std::cerr << "[PipelineBuilder] ✓ BA socket broker node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createBASocketBrokerNode: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createEmbeddingsSocketBrokerNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string desIp = params.count("des_ip") ? params.at("des_ip") : "";
        int desPort = params.count("des_port") ? std::stoi(params.at("des_port")) : 0;
        std::string croppedDir = params.count("cropped_dir") ? params.at("cropped_dir") : "cropped_images";
        int minCropWidth = params.count("min_crop_width") ? std::stoi(params.at("min_crop_width")) : 50;
        int minCropHeight = params.count("min_crop_height") ? std::stoi(params.at("min_crop_height")) : 50;
        std::string brokeForStr = params.count("broke_for") ? params.at("broke_for") : "NORMAL";
        cvedix_nodes::cvedix_broke_for brokeFor = cvedix_nodes::cvedix_broke_for::NORMAL;
        bool onlyForTracked = params.count("only_for_tracked") ? (params.at("only_for_tracked") == "true" || params.at("only_for_tracked") == "1") : false;
        
        // Get from additionalParams if not in params
        if (desIp.empty()) {
            auto it = req.additionalParams.find("EMBEDDINGS_SOCKET_IP");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                desIp = it->second;
            } else {
                desIp = "127.0.0.1"; // Default fake data
            }
        }
        if (desPort == 0) {
            auto it = req.additionalParams.find("EMBEDDINGS_SOCKET_PORT");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                try {
                    desPort = std::stoi(it->second);
                } catch (...) {
                    desPort = 8080; // Default fake data
                }
            } else {
                desPort = 8080; // Default fake data
            }
        }
        
        if (brokeForStr == "FACE") {
            brokeFor = cvedix_nodes::cvedix_broke_for::FACE;
        } else if (brokeForStr == "TEXT") {
            brokeFor = cvedix_nodes::cvedix_broke_for::TEXT;
        } else if (brokeForStr == "POSE") {
            brokeFor = cvedix_nodes::cvedix_broke_for::POSE;
        }
        
        int warnThreshold = params.count("broking_cache_warn_threshold") ? std::stoi(params.at("broking_cache_warn_threshold")) : 50;
        int ignoreThreshold = params.count("broking_cache_ignore_threshold") ? std::stoi(params.at("broking_cache_ignore_threshold")) : 200;
        
        if (nodeName.empty()) {
            throw std::invalid_argument("Node name cannot be empty");
        }
        
        std::cerr << "[PipelineBuilder] Creating embeddings socket broker node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Destination IP: '" << desIp << "'" << std::endl;
        std::cerr << "  Destination port: " << desPort << std::endl;
        std::cerr << "  Cropped dir: '" << croppedDir << "'" << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_embeddings_socket_broker_node>(
            nodeName,
            desIp,
            desPort,
            croppedDir,
            minCropWidth,
            minCropHeight,
            brokeFor,
            onlyForTracked,
            warnThreshold,
            ignoreThreshold
        );
        
        std::cerr << "[PipelineBuilder] ✓ Embeddings socket broker node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createEmbeddingsSocketBrokerNode: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createEmbeddingsPropertiesSocketBrokerNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string desIp = params.count("des_ip") ? params.at("des_ip") : "";
        int desPort = params.count("des_port") ? std::stoi(params.at("des_port")) : 0;
        std::string croppedDir = params.count("cropped_dir") ? params.at("cropped_dir") : "cropped_images";
        int minCropWidth = params.count("min_crop_width") ? std::stoi(params.at("min_crop_width")) : 50;
        int minCropHeight = params.count("min_crop_height") ? std::stoi(params.at("min_crop_height")) : 50;
        std::string brokeForStr = params.count("broke_for") ? params.at("broke_for") : "NORMAL";
        cvedix_nodes::cvedix_broke_for brokeFor = cvedix_nodes::cvedix_broke_for::NORMAL;
        bool onlyForTracked = params.count("only_for_tracked") ? (params.at("only_for_tracked") == "true" || params.at("only_for_tracked") == "1") : false;
        
        // Get from additionalParams if not in params
        if (desIp.empty()) {
            auto it = req.additionalParams.find("EMBEDDINGS_PROPERTIES_SOCKET_IP");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                desIp = it->second;
            } else {
                desIp = "127.0.0.1"; // Default fake data
            }
        }
        if (desPort == 0) {
            auto it = req.additionalParams.find("EMBEDDINGS_PROPERTIES_SOCKET_PORT");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                try {
                    desPort = std::stoi(it->second);
                } catch (...) {
                    desPort = 8080; // Default fake data
                }
            } else {
                desPort = 8080; // Default fake data
            }
        }
        
        if (brokeForStr == "FACE") {
            brokeFor = cvedix_nodes::cvedix_broke_for::FACE;
        } else if (brokeForStr == "TEXT") {
            brokeFor = cvedix_nodes::cvedix_broke_for::TEXT;
        } else if (brokeForStr == "POSE") {
            brokeFor = cvedix_nodes::cvedix_broke_for::POSE;
        }
        
        int warnThreshold = params.count("broking_cache_warn_threshold") ? std::stoi(params.at("broking_cache_warn_threshold")) : 50;
        int ignoreThreshold = params.count("broking_cache_ignore_threshold") ? std::stoi(params.at("broking_cache_ignore_threshold")) : 200;
        
        if (nodeName.empty()) {
            throw std::invalid_argument("Node name cannot be empty");
        }
        
        std::cerr << "[PipelineBuilder] Creating embeddings properties socket broker node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Destination IP: '" << desIp << "'" << std::endl;
        std::cerr << "  Destination port: " << desPort << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_embeddings_properties_socket_broker_node>(
            nodeName,
            desIp,
            desPort,
            croppedDir,
            minCropWidth,
            minCropHeight,
            brokeFor,
            onlyForTracked,
            warnThreshold,
            ignoreThreshold
        );
        
        std::cerr << "[PipelineBuilder] ✓ Embeddings properties socket broker node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createEmbeddingsPropertiesSocketBrokerNode: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createPlateSocketBrokerNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string desIp = params.count("des_ip") ? params.at("des_ip") : "";
        int desPort = params.count("des_port") ? std::stoi(params.at("des_port")) : 0;
        std::string platesDir = params.count("plates_dir") ? params.at("plates_dir") : "plate_images";
        int minCropWidth = params.count("min_crop_width") ? std::stoi(params.at("min_crop_width")) : 100;
        int minCropHeight = params.count("min_crop_height") ? std::stoi(params.at("min_crop_height")) : 0;
        std::string brokeForStr = params.count("broke_for") ? params.at("broke_for") : "NORMAL";
        cvedix_nodes::cvedix_broke_for brokeFor = cvedix_nodes::cvedix_broke_for::NORMAL;
        bool onlyForTracked = params.count("only_for_tracked") ? (params.at("only_for_tracked") == "true" || params.at("only_for_tracked") == "1") : true;
        
        // Get from additionalParams if not in params
        if (desIp.empty()) {
            auto it = req.additionalParams.find("PLATE_SOCKET_IP");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                desIp = it->second;
            } else {
                desIp = "127.0.0.1"; // Default fake data
            }
        }
        if (desPort == 0) {
            auto it = req.additionalParams.find("PLATE_SOCKET_PORT");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                try {
                    desPort = std::stoi(it->second);
                } catch (...) {
                    desPort = 8080; // Default fake data
                }
            } else {
                desPort = 8080; // Default fake data
            }
        }
        
        if (brokeForStr == "FACE") {
            brokeFor = cvedix_nodes::cvedix_broke_for::FACE;
        } else if (brokeForStr == "TEXT") {
            brokeFor = cvedix_nodes::cvedix_broke_for::TEXT;
        } else if (brokeForStr == "POSE") {
            brokeFor = cvedix_nodes::cvedix_broke_for::POSE;
        }
        
        int warnThreshold = params.count("broking_cache_warn_threshold") ? std::stoi(params.at("broking_cache_warn_threshold")) : 50;
        int ignoreThreshold = params.count("broking_cache_ignore_threshold") ? std::stoi(params.at("broking_cache_ignore_threshold")) : 200;
        
        if (nodeName.empty()) {
            throw std::invalid_argument("Node name cannot be empty");
        }
        
        std::cerr << "[PipelineBuilder] Creating plate socket broker node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Destination IP: '" << desIp << "'" << std::endl;
        std::cerr << "  Destination port: " << desPort << std::endl;
        std::cerr << "  Plates dir: '" << platesDir << "'" << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_plate_socket_broker_node>(
            nodeName,
            desIp,
            desPort,
            platesDir,
            minCropWidth,
            minCropHeight,
            brokeFor,
            onlyForTracked,
            warnThreshold,
            ignoreThreshold
        );
        
        std::cerr << "[PipelineBuilder] ✓ Plate socket broker node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createPlateSocketBrokerNode: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createExpressionSocketBrokerNode(
    const std::string& nodeName,
    const std::map<std::string, std::string>& params,
    const CreateInstanceRequest& req) {
    
    try {
        std::string desIp = params.count("des_ip") ? params.at("des_ip") : "";
        int desPort = params.count("des_port") ? std::stoi(params.at("des_port")) : 0;
        std::string screenshotDir = params.count("screenshot_dir") ? params.at("screenshot_dir") : "screenshot_images";
        std::string brokeForStr = params.count("broke_for") ? params.at("broke_for") : "TEXT";
        cvedix_nodes::cvedix_broke_for brokeFor = cvedix_nodes::cvedix_broke_for::TEXT;
        
        // Get from additionalParams if not in params
        if (desIp.empty()) {
            auto it = req.additionalParams.find("EXPR_SOCKET_IP");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                desIp = it->second;
            } else {
                desIp = "127.0.0.1"; // Default fake data
            }
        }
        if (desPort == 0) {
            auto it = req.additionalParams.find("EXPR_SOCKET_PORT");
            if (it != req.additionalParams.end() && !it->second.empty()) {
                try {
                    desPort = std::stoi(it->second);
                } catch (...) {
                    desPort = 8080; // Default fake data
                }
            } else {
                desPort = 8080; // Default fake data
            }
        }
        
        if (brokeForStr == "FACE") {
            brokeFor = cvedix_nodes::cvedix_broke_for::FACE;
        } else if (brokeForStr == "TEXT") {
            brokeFor = cvedix_nodes::cvedix_broke_for::TEXT;
        } else if (brokeForStr == "POSE") {
            brokeFor = cvedix_nodes::cvedix_broke_for::POSE;
        } else if (brokeForStr == "POSE") {
            brokeFor = cvedix_nodes::cvedix_broke_for::POSE;
        } else if (brokeForStr == "NORMAL") {
            brokeFor = cvedix_nodes::cvedix_broke_for::NORMAL;
        }
        
        int warnThreshold = params.count("broking_cache_warn_threshold") ? std::stoi(params.at("broking_cache_warn_threshold")) : 50;
        int ignoreThreshold = params.count("broking_cache_ignore_threshold") ? std::stoi(params.at("broking_cache_ignore_threshold")) : 200;
        
        if (nodeName.empty()) {
            throw std::invalid_argument("Node name cannot be empty");
        }
        
        std::cerr << "[PipelineBuilder] Creating expression socket broker node:" << std::endl;
        std::cerr << "  Name: '" << nodeName << "'" << std::endl;
        std::cerr << "  Destination IP: '" << desIp << "'" << std::endl;
        std::cerr << "  Destination port: " << desPort << std::endl;
        std::cerr << "  Screenshot dir: '" << screenshotDir << "'" << std::endl;
        
        auto node = std::make_shared<cvedix_nodes::cvedix_expr_socket_broker_node>(
            nodeName,
            desIp,
            desPort,
            screenshotDir,
            brokeFor,
            warnThreshold,
            ignoreThreshold
        );
        
        std::cerr << "[PipelineBuilder] ✓ Expression socket broker node created successfully" << std::endl;
        return node;
    } catch (const std::exception& e) {
        std::cerr << "[PipelineBuilder] Exception in createExpressionSocketBrokerNode: " << e.what() << std::endl;
        throw;
    }
}
*/
// Stub implementations for all broker nodes to avoid linker errors
std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createXMLFileBrokerNode(
    const std::string&, const std::map<std::string, std::string>&, const CreateInstanceRequest&) {
    return nullptr;
}
std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createXMLSocketBrokerNode(
    const std::string&, const std::map<std::string, std::string>&, const CreateInstanceRequest&) {
    return nullptr;
}
std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createMessageBrokerNode(
    const std::string&, const std::map<std::string, std::string>&, const CreateInstanceRequest&) {
    return nullptr;
}
std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createBASocketBrokerNode(
    const std::string&, const std::map<std::string, std::string>&, const CreateInstanceRequest&) {
    return nullptr;
}
std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createEmbeddingsSocketBrokerNode(
    const std::string&, const std::map<std::string, std::string>&, const CreateInstanceRequest&) {
    return nullptr;
}
std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createEmbeddingsPropertiesSocketBrokerNode(
    const std::string&, const std::map<std::string, std::string>&, const CreateInstanceRequest&) {
    return nullptr;
}
std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createPlateSocketBrokerNode(
    const std::string&, const std::map<std::string, std::string>&, const CreateInstanceRequest&) {
    return nullptr;
}
std::shared_ptr<cvedix_nodes::cvedix_node> PipelineBuilder::createExpressionSocketBrokerNode(
    const std::string&, const std::map<std::string, std::string>&, const CreateInstanceRequest&) {
    return nullptr;
}


