#pragma once

#include "models/solution_config.h"
#include "models/create_instance_request.h"
#include "instances/instance_info.h"
#include <memory>
#include <vector>
#include <string>

// Forward declarations for CVEDIX SDK nodes
namespace cvedix_nodes {
    class cvedix_node;
}

/**
 * @brief Pipeline Builder
 * 
 * Builds CVEDIX SDK pipelines from solution configurations and instance requests.
 */
class PipelineBuilder {
public:
    /**
     * @brief Build pipeline from solution config and request
     * @param solution Solution configuration
     * @param req Create instance request
     * @param instanceId Instance ID for node naming
     * @return Vector of pipeline nodes (connected in order)
     */
    std::vector<std::shared_ptr<cvedix_nodes::cvedix_node>> buildPipeline(
        const SolutionConfig& solution,
        const CreateInstanceRequest& req,
        const std::string& instanceId
    );
    
private:
    /**
     * @brief Create a node from node configuration
     * @param nodeConfig Node configuration
     * @param req Create instance request
     * @param instanceId Instance ID
     * @return Created node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createNode(
        const SolutionConfig::NodeConfig& nodeConfig,
        const CreateInstanceRequest& req,
        const std::string& instanceId
    );
    
    // ========== Source Nodes ==========
    
    /**
     * @brief Create RTSP source node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createRTSPSourceNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create file source node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createFileSourceNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create app source node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createAppSourceNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create image source node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createImageSourceNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create RTMP source node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createRTMPSourceNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create UDP source node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createUDPSourceNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create FFmpeg source node (for HLS, HTTP streams, etc.)
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createFFmpegSourceNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Detect input type from URI/path
     * @param uri URI or file path
     * @return Input type: "rtsp", "rtmp", "hls", "http", "file"
     */
    std::string detectInputType(const std::string& uri) const;
    
    /**
     * @brief Create face detector node (YuNet)
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createFaceDetectorNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create file destination node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createFileDestinationNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const std::string& instanceId
    );
    
    /**
     * @brief Create SFace feature encoder node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createSFaceEncoderNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create face OSD v2 node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createFaceOSDNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params
    );
    
    /**
     * @brief Create OSD v3 node (for masks and labels)
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createOSDv3Node(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create RTMP destination node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createRTMPDestinationNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create screen destination node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createScreenDestinationNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params
    );
    
    /**
     * @brief Create app destination node (for frame capture)
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createAppDesNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params
    );
    
    // ========== Tracking Nodes ==========
    
    /**
     * @brief Create SORT tracker node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createSORTTrackNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params
    );
    
    // ========== Behavior Analysis Nodes ==========
    
    /**
     * @brief Create BA crossline node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createBACrosslineNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params
    );
    
    /**
     * @brief Create BA crossline OSD node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createBACrosslineOSDNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params
    );
    
    // ========== Broker Nodes ==========
    
    /**
     * @brief Create JSON console broker node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createJSONConsoleBrokerNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create JSON enhanced console broker node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createJSONEnhancedConsoleBrokerNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create JSON MQTT broker node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createJSONMQTTBrokerNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create JSON Crossline MQTT broker node (custom formatting for ba_crossline)
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createJSONCrosslineMQTTBrokerNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create JSON Kafka broker node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createJSONKafkaBrokerNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create XML file broker node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createXMLFileBrokerNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create XML socket broker node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createXMLSocketBrokerNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create message broker node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createMessageBrokerNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create BA socket broker node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createBASocketBrokerNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create embeddings socket broker node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createEmbeddingsSocketBrokerNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create embeddings properties socket broker node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createEmbeddingsPropertiesSocketBrokerNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create plate socket broker node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createPlateSocketBrokerNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create expression socket broker node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createExpressionSocketBrokerNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    // ========== TensorRT Inference Nodes ==========
    
    /**
     * @brief Create TensorRT YOLOv8 detector node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createTRTYOLOv8DetectorNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create TensorRT YOLOv8 segmentation detector node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createTRTYOLOv8SegDetectorNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create TensorRT YOLOv8 pose detector node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createTRTYOLOv8PoseDetectorNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create TensorRT YOLOv8 classifier node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createTRTYOLOv8ClassifierNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create TensorRT vehicle detector node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createTRTVehicleDetectorNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create TensorRT vehicle plate detector node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createTRTVehiclePlateDetectorNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create TensorRT vehicle plate detector v2 node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createTRTVehiclePlateDetectorV2Node(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create TensorRT vehicle color classifier node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createTRTVehicleColorClassifierNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create TensorRT vehicle type classifier node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createTRTVehicleTypeClassifierNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create TensorRT vehicle feature encoder node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createTRTVehicleFeatureEncoderNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create TensorRT vehicle scanner node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createTRTVehicleScannerNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    // ========== RKNN Inference Nodes ==========
    
    /**
     * @brief Create RKNN YOLOv8 detector node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createRKNNYOLOv8DetectorNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create RKNN face detector node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createRKNNFaceDetectorNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    // ========== Other Inference Nodes ==========
    
    /**
     * @brief Create YOLO detector node (OpenCV DNN)
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createYOLODetectorNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create ENet segmentation node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createENetSegNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create Mask RCNN detector node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createMaskRCNNDetectorNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create OpenPose detector node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createOpenPoseDetectorNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create classifier node (generic)
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createClassifierNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create feature encoder node (generic)
     * @note DISABLED: cvedix_feature_encoder_node is abstract. Use createSFaceEncoderNode or createTRTVehicleFeatureEncoderNode instead.
     */
    // std::shared_ptr<cvedix_nodes::cvedix_node> createFeatureEncoderNode(
    //     const std::string& nodeName,
    //     const std::map<std::string, std::string>& params,
    //     const CreateInstanceRequest& req
    // );
    
    /**
     * @brief Create lane detector node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createLaneDetectorNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create PaddleOCR text detector node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createPaddleOCRTextDetectorNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create restoration node (Real-ESRGAN)
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createRestorationNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create YOLOv11 detector node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createYOLOv11DetectorNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create face swap node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createFaceSwapNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create InsightFace recognition node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createInsightFaceRecognitionNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
    /**
     * @brief Create MLLM analyser node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createMLLMAnalyserNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
    
#ifdef CVEDIX_WITH_RKNN
    /**
     * @brief Create RKNN YOLOv11 detector node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createRKNNYOLOv11DetectorNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
#endif
    
#ifdef CVEDIX_WITH_TRT
    /**
     * @brief Create TensorRT InsightFace recognition node
     */
    std::shared_ptr<cvedix_nodes::cvedix_node> createTRTInsightFaceRecognitionNode(
        const std::string& nodeName,
        const std::map<std::string, std::string>& params,
        const CreateInstanceRequest& req
    );
#endif
    
    /**
     * @brief Map detection sensitivity to threshold value
     * @param sensitivity "Low", "Medium", or "High"
     * @return Threshold value (0.0-1.0)
     */
    double mapDetectionSensitivity(const std::string& sensitivity) const;
    
    /**
     * @brief Get file path from request (for file source)
     */
    std::string getFilePath(const CreateInstanceRequest& req) const;
    
    /**
     * @brief Get RTMP URL from request
     */
    std::string getRTMPUrl(const CreateInstanceRequest& req) const;
    
    /**
     * @brief Get RTSP URL from request
     */
    std::string getRTSPUrl(const CreateInstanceRequest& req) const;
    
    /**
     * @brief Resolve model file path (supports CVEDIX_DATA_ROOT, CVEDIX_SDK_ROOT, or relative paths)
     * @param relativePath Relative path from cvedix_data/models (e.g., "face/yunet.onnx")
     * @return Resolved absolute or relative path
     */
    std::string resolveModelPath(const std::string& relativePath) const;
    
    /**
     * @brief Resolve model file by name (e.g., "yunet_2023mar", "yunet_2022mar", "yolov8n_face")
     * @param modelName Model name (without extension)
     * @param category Model category (e.g., "face", "object") - defaults to "face"
     * @return Resolved absolute or relative path, or empty string if not found
     */
    std::string resolveModelByName(const std::string& modelName, const std::string& category = "face") const;
    
    /**
     * @brief List available models in system directories
     * @param category Model category (e.g., "face", "object") - empty string for all categories
     * @return Vector of model file paths
     */
    std::vector<std::string> listAvailableModels(const std::string& category = "") const;
};

