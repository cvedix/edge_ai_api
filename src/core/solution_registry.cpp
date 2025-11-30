#include "core/solution_registry.h"
#include <algorithm>

void SolutionRegistry::registerSolution(const SolutionConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    solutions_[config.solutionId] = config;
}

std::optional<SolutionConfig> SolutionRegistry::getSolution(const std::string& solutionId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = solutions_.find(solutionId);
    if (it != solutions_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<std::string> SolutionRegistry::listSolutions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    result.reserve(solutions_.size());
    for (const auto& pair : solutions_) {
        result.push_back(pair.first);
    }
    return result;
}

bool SolutionRegistry::hasSolution(const std::string& solutionId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return solutions_.find(solutionId) != solutions_.end();
}

void SolutionRegistry::initializeDefaultSolutions() {
    registerFaceDetectionSolution();
    registerObjectDetectionSolution();  // Add YOLO-based solution
}

void SolutionRegistry::registerFaceDetectionSolution() {
    SolutionConfig config;
    config.solutionId = "face_detection";
    config.solutionName = "Face Detection";
    config.solutionType = "face_detection";
    
    // RTSP Source Node
    SolutionConfig::NodeConfig rtspSrc;
    rtspSrc.nodeType = "rtsp_src";
    rtspSrc.nodeName = "rtsp_src_{instanceId}";
    rtspSrc.parameters["rtsp_url"] = "${RTSP_URL}";
    rtspSrc.parameters["channel"] = "0";
    rtspSrc.parameters["resize_ratio"] = "1.0";  // Use resize_ratio instead of fps (must be > 0 and <= 1.0)
    config.pipeline.push_back(rtspSrc);
    
    // YuNet Face Detector Node
    SolutionConfig::NodeConfig faceDetector;
    faceDetector.nodeType = "yunet_face_detector";
    faceDetector.nodeName = "face_detector_{instanceId}";
    // Use ${MODEL_PATH} placeholder - can be overridden via additionalParams["MODEL_PATH"] in request
    // Default to yunet.onnx if not provided
    faceDetector.parameters["model_path"] = "${MODEL_PATH}";
    faceDetector.parameters["score_threshold"] = "${detectionSensitivity}";
    faceDetector.parameters["nms_threshold"] = "0.5";
    faceDetector.parameters["top_k"] = "50";
    config.pipeline.push_back(faceDetector);
    
    // File Destination Node
    SolutionConfig::NodeConfig fileDes;
    fileDes.nodeType = "file_des";
    fileDes.nodeName = "file_des_{instanceId}";
    fileDes.parameters["save_dir"] = "./output/{instanceId}";
    fileDes.parameters["name_prefix"] = "face_detection";
    fileDes.parameters["osd"] = "true";
    config.pipeline.push_back(fileDes);
    
    // Default configurations
    config.defaults["detectorMode"] = "SmartDetection";
    config.defaults["detectionSensitivity"] = "0.7";
    config.defaults["sensorModality"] = "RGB";
    
    registerSolution(config);
}

void SolutionRegistry::registerObjectDetectionSolution() {
    SolutionConfig config;
    config.solutionId = "object_detection";
    config.solutionName = "Object Detection (YOLO)";
    config.solutionType = "object_detection";
    
    // RTSP Source Node
    SolutionConfig::NodeConfig rtspSrc;
    rtspSrc.nodeType = "rtsp_src";
    rtspSrc.nodeName = "rtsp_src_{instanceId}";
    rtspSrc.parameters["rtsp_url"] = "${RTSP_URL}";
    rtspSrc.parameters["channel"] = "0";
    rtspSrc.parameters["resize_ratio"] = "1.0";
    config.pipeline.push_back(rtspSrc);
    
    // YOLO Detector Node (commented out - need to implement createYOLODetectorNode)
    // To use YOLO, you need to:
    // 1. Add "yolo_detector" case in PipelineBuilder::createNode()
    // 2. Implement createYOLODetectorNode() in PipelineBuilder
    // 3. Uncomment the code below
    /*
    SolutionConfig::NodeConfig yoloDetector;
    yoloDetector.nodeType = "yolo_detector";
    yoloDetector.nodeName = "yolo_detector_{instanceId}";
    yoloDetector.parameters["weights_path"] = "${MODEL_PATH}";
    yoloDetector.parameters["config_path"] = "${CONFIG_PATH}";
    yoloDetector.parameters["labels_path"] = "${LABELS_PATH}";
    config.pipeline.push_back(yoloDetector);
    */
    
    // File Destination Node
    SolutionConfig::NodeConfig fileDes;
    fileDes.nodeType = "file_des";
    fileDes.nodeName = "file_des_{instanceId}";
    fileDes.parameters["save_dir"] = "./output/{instanceId}";
    fileDes.parameters["name_prefix"] = "object_detection";
    fileDes.parameters["osd"] = "true";
    config.pipeline.push_back(fileDes);
    
    // Default configurations
    config.defaults["detectorMode"] = "SmartDetection";
    config.defaults["detectionSensitivity"] = "0.7";
    config.defaults["sensorModality"] = "RGB";
    
    registerSolution(config);
}

