#include "solutions/solution_registry.h"
#include <algorithm>
#include <iostream>

void SolutionRegistry::registerSolution(const SolutionConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if solution already exists and is a default solution
    auto it = solutions_.find(config.solutionId);
    if (it != solutions_.end() && it->second.isDefault) {
        // Cannot override default solutions
        std::cerr << "[SolutionRegistry] Warning: Attempted to override default solution: " 
                  << config.solutionId << ". Ignoring registration." << std::endl;
        return;
    }
    
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

std::unordered_map<std::string, SolutionConfig> SolutionRegistry::getAllSolutions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return solutions_;
}

bool SolutionRegistry::updateSolution(const SolutionConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = solutions_.find(config.solutionId);
    if (it == solutions_.end()) {
        return false; // Solution doesn't exist
    }
    
    // Check if it's a default solution - default solutions cannot be updated
    if (it->second.isDefault) {
        return false;
    }
    
    // Update the solution
    solutions_[config.solutionId] = config;
    return true;
}

bool SolutionRegistry::deleteSolution(const std::string& solutionId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = solutions_.find(solutionId);
    if (it == solutions_.end()) {
        return false; // Solution doesn't exist
    }
    
    // Check if it's a default solution - default solutions cannot be deleted
    if (it->second.isDefault) {
        return false;
    }
    
    // Delete the solution
    solutions_.erase(it);
    return true;
}

bool SolutionRegistry::isDefaultSolution(const std::string& solutionId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = solutions_.find(solutionId);
    if (it != solutions_.end()) {
        return it->second.isDefault;
    }
    return false;
}

void SolutionRegistry::initializeDefaultSolutions() {
    registerFaceDetectionSolution();
    registerFaceDetectionFileSolution();  // Add face detection with file source
    registerObjectDetectionSolution();  // Add YOLO-based solution
    registerFaceDetectionRTMPSolution();  // Add face detection with RTMP streaming
    registerBACrosslineSolution();  // Add behavior analysis crossline solution
}

void SolutionRegistry::registerFaceDetectionSolution() {
    SolutionConfig config;
    config.solutionId = "face_detection";
    config.solutionName = "Face Detection";
    config.solutionType = "face_detection";
    config.isDefault = true;
    
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

void SolutionRegistry::registerFaceDetectionFileSolution() {
    SolutionConfig config;
    config.solutionId = "face_detection_file";
    config.solutionName = "Face Detection with File Source";
    config.solutionType = "face_detection";
    config.isDefault = true;
    
    // File Source Node
    SolutionConfig::NodeConfig fileSrc;
    fileSrc.nodeType = "file_src";
    fileSrc.nodeName = "file_src_{instanceId}";
    fileSrc.parameters["file_path"] = "${FILE_PATH}";
    fileSrc.parameters["channel"] = "0";
    fileSrc.parameters["resize_ratio"] = "1.0";  // Use resize_ratio instead of fps (must be > 0 and <= 1.0)
    config.pipeline.push_back(fileSrc);
    
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
    config.isDefault = true;
    
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

void SolutionRegistry::registerFaceDetectionRTMPSolution() {
    SolutionConfig config;
    config.solutionId = "face_detection_rtmp";
    config.solutionName = "Face Detection with RTMP Streaming";
    config.solutionType = "face_detection";
    config.isDefault = true;
    
    // File Source Node
    SolutionConfig::NodeConfig fileSrc;
    fileSrc.nodeType = "file_src";
    fileSrc.nodeName = "file_src_{instanceId}";
    fileSrc.parameters["file_path"] = "${FILE_PATH}";
    fileSrc.parameters["channel"] = "0";
    // IMPORTANT: Use resize_ratio = 1.0 (no resize) if video already has fixed resolution
    // This prevents double-resizing which can cause shape mismatch errors
    // If your video is already re-encoded with fixed resolution (e.g., 640x360), use 1.0
    // If your video has variable resolution, re-encode it first, then use 1.0
    // 
    // Alternative: If you must resize, use ratios that produce even dimensions:
    // - 0.5 = 1280x720 -> 640x360 (for original 1280x720 videos)
    // - 0.25 = 1280x720 -> 320x180 (smaller, faster)
    // - 0.125 = 1280x720 -> 160x90 (very small)
    //
    // CRITICAL: The best solution is to re-encode video with fixed resolution,
    // then use resize_ratio = 1.0 to avoid any resize operations
    fileSrc.parameters["resize_ratio"] = "1.0";
    config.pipeline.push_back(fileSrc);
    
    // YuNet Face Detector Node
    // NOTE: YuNet 2022mar model may have issues with dynamic input sizes
    // If you encounter shape mismatch errors, consider using YuNet 2023mar model
    // which has better support for variable input sizes
    SolutionConfig::NodeConfig faceDetector;
    faceDetector.nodeType = "yunet_face_detector";
    faceDetector.nodeName = "yunet_face_detector_{instanceId}";
    faceDetector.parameters["model_path"] = "${MODEL_PATH}";
    faceDetector.parameters["score_threshold"] = "${detectionSensitivity}";
    faceDetector.parameters["nms_threshold"] = "0.5";
    faceDetector.parameters["top_k"] = "50";
    config.pipeline.push_back(faceDetector);
    
    // SFace Feature Encoder Node
    SolutionConfig::NodeConfig sfaceEncoder;
    sfaceEncoder.nodeType = "sface_feature_encoder";
    sfaceEncoder.nodeName = "sface_face_encoder_{instanceId}";
    sfaceEncoder.parameters["model_path"] = "${SFACE_MODEL_PATH}";
    config.pipeline.push_back(sfaceEncoder);
    
    // Face OSD v2 Node
    SolutionConfig::NodeConfig faceOSD;
    faceOSD.nodeType = "face_osd_v2";
    faceOSD.nodeName = "osd_{instanceId}";
    config.pipeline.push_back(faceOSD);
    
    // RTMP Destination Node
    SolutionConfig::NodeConfig rtmpDes;
    rtmpDes.nodeType = "rtmp_des";
    rtmpDes.nodeName = "rtmp_des_{instanceId}";
    rtmpDes.parameters["rtmp_url"] = "${RTMP_URL}";
    rtmpDes.parameters["channel"] = "0";
    config.pipeline.push_back(rtmpDes);
    
    // Default configurations
    config.defaults["detectorMode"] = "SmartDetection";
    config.defaults["detectionSensitivity"] = "Low";
    config.defaults["sensorModality"] = "RGB";
    
    registerSolution(config);
}

void SolutionRegistry::registerBACrosslineSolution() {
    SolutionConfig config;
    config.solutionId = "ba_crossline";
    config.solutionName = "Behavior Analysis - Crossline Detection";
    config.solutionType = "behavior_analysis";
    config.isDefault = true;
    
    // File Source Node
    SolutionConfig::NodeConfig fileSrc;
    fileSrc.nodeType = "file_src";
    fileSrc.nodeName = "file_src_{instanceId}";
    fileSrc.parameters["file_path"] = "${FILE_PATH}";
    fileSrc.parameters["channel"] = "0";
    fileSrc.parameters["resize_ratio"] = "0.4";
    config.pipeline.push_back(fileSrc);
    
    // YOLO Detector Node
    SolutionConfig::NodeConfig yoloDetector;
    yoloDetector.nodeType = "yolo_detector";
    yoloDetector.nodeName = "yolo_detector_{instanceId}";
    yoloDetector.parameters["weights_path"] = "${WEIGHTS_PATH}";
    yoloDetector.parameters["config_path"] = "${CONFIG_PATH}";
    yoloDetector.parameters["labels_path"] = "${LABELS_PATH}";
    config.pipeline.push_back(yoloDetector);
    
    // SORT Tracker Node
    SolutionConfig::NodeConfig sortTrack;
    sortTrack.nodeType = "sort_track";
    sortTrack.nodeName = "sort_tracker_{instanceId}";
    config.pipeline.push_back(sortTrack);
    
    // BA Crossline Node
    SolutionConfig::NodeConfig baCrossline;
    baCrossline.nodeType = "ba_crossline";
    baCrossline.nodeName = "ba_crossline_{instanceId}";
    baCrossline.parameters["line_channel"] = "0";
    baCrossline.parameters["line_start_x"] = "0";
    baCrossline.parameters["line_start_y"] = "250";
    baCrossline.parameters["line_end_x"] = "700";
    baCrossline.parameters["line_end_y"] = "220";
    config.pipeline.push_back(baCrossline);
    
    // BA Crossline OSD Node
    SolutionConfig::NodeConfig baCrosslineOSD;
    baCrosslineOSD.nodeType = "ba_crossline_osd";
    baCrosslineOSD.nodeName = "osd_{instanceId}";
    config.pipeline.push_back(baCrosslineOSD);
    
    // Screen Destination Node (optional - can be disabled via ENABLE_SCREEN_DES parameter)
    SolutionConfig::NodeConfig screenDes;
    screenDes.nodeType = "screen_des";
    screenDes.nodeName = "screen_des_{instanceId}";
    screenDes.parameters["channel"] = "0";
    screenDes.parameters["enabled"] = "${ENABLE_SCREEN_DES}";  // Default: empty (enabled if display available), set to "false" to disable
    config.pipeline.push_back(screenDes);
    
    // RTMP Destination Node
    SolutionConfig::NodeConfig rtmpDes;
    rtmpDes.nodeType = "rtmp_des";
    rtmpDes.nodeName = "rtmp_des_{instanceId}";
    rtmpDes.parameters["rtmp_url"] = "${RTMP_URL}";
    rtmpDes.parameters["channel"] = "0";
    config.pipeline.push_back(rtmpDes);
    
    // Default configurations
    config.defaults["detectorMode"] = "SmartDetection";
    config.defaults["detectionSensitivity"] = "0.7";
    config.defaults["sensorModality"] = "RGB";
    
    registerSolution(config);
}

