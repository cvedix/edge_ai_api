# Hướng dẫn Thêm/Cập nhật Default Solutions

## Tổng quan

Khi chạy project, **4 default solutions** sẽ tự động có sẵn:
1. `face_detection`
2. `face_detection_file`
3. `object_detection`
4. `face_detection_rtmp`

Để thêm hoặc cập nhật default solutions, bạn cần sửa code trong `src/solutions/solution_registry.cpp`.

---

## Cách Thêm Default Solution Mới

### Bước 1: Tạo hàm register mới

Mở file `src/solutions/solution_registry.cpp` và thêm hàm mới sau các hàm register hiện có:

```cpp
void SolutionRegistry::registerYourNewSolution() {
    SolutionConfig config;
    config.solutionId = "your_solution_id";
    config.solutionName = "Your Solution Name";
    config.solutionType = "your_solution_type";
    config.isDefault = true;  // QUAN TRỌNG: Phải set = true
    
    // Thêm các nodes vào pipeline
    // Node 1: Source
    SolutionConfig::NodeConfig sourceNode;
    sourceNode.nodeType = "rtsp_src";  // hoặc "file_src", etc.
    sourceNode.nodeName = "source_{instanceId}";
    sourceNode.parameters["rtsp_url"] = "${RTSP_URL}";
    sourceNode.parameters["channel"] = "0";
    sourceNode.parameters["resize_ratio"] = "1.0";
    config.pipeline.push_back(sourceNode);
    
    // Node 2: Detector/Processor
    SolutionConfig::NodeConfig detectorNode;
    detectorNode.nodeType = "yunet_face_detector";  // hoặc node type khác
    detectorNode.nodeName = "detector_{instanceId}";
    detectorNode.parameters["model_path"] = "${MODEL_PATH}";
    detectorNode.parameters["score_threshold"] = "${detectionSensitivity}";
    // ... thêm các parameters khác
    config.pipeline.push_back(detectorNode);
    
    // Node 3: Destination
    SolutionConfig::NodeConfig destNode;
    destNode.nodeType = "file_des";  // hoặc "rtmp_des", etc.
    destNode.nodeName = "destination_{instanceId}";
    destNode.parameters["save_dir"] = "./output/{instanceId}";
    destNode.parameters["name_prefix"] = "your_prefix";
    destNode.parameters["osd"] = "true";
    config.pipeline.push_back(destNode);
    
    // Default configurations
    config.defaults["detectorMode"] = "SmartDetection";
    config.defaults["detectionSensitivity"] = "0.7";
    config.defaults["sensorModality"] = "RGB";
    
    registerSolution(config);
}
```

### Bước 2: Khai báo hàm trong header

Mở file `include/solutions/solution_registry.h` và thêm khai báo trong phần `private:`:

```cpp
private:
    // ... các hàm khác ...
    
    /**
     * @brief Register your new solution
     */
    void registerYourNewSolution();
```

### Bước 3: Gọi hàm trong initializeDefaultSolutions()

Trong file `src/solutions/solution_registry.cpp`, sửa hàm `initializeDefaultSolutions()`:

```cpp
void SolutionRegistry::initializeDefaultSolutions() {
    registerFaceDetectionSolution();
    registerFaceDetectionFileSolution();
    registerObjectDetectionSolution();
    registerFaceDetectionRTMPSolution();
    registerYourNewSolution();  // ← Thêm dòng này
}
```

### Bước 4: Rebuild và test

```bash
# Rebuild project
cd /home/pnsang/project/edge_ai_api
mkdir -p build && cd build
cmake ..
make

# Test: Khởi động ứng dụng và kiểm tra
# Solution mới sẽ tự động có sẵn
curl http://localhost:8080/v1/core/solutions
```

---

## Cách Cập nhật Default Solution Hiện có

### Ví dụ: Cập nhật `face_detection`

Mở file `src/solutions/solution_registry.cpp` và tìm hàm `registerFaceDetectionSolution()`:

```cpp
void SolutionRegistry::registerFaceDetectionSolution() {
    SolutionConfig config;
    config.solutionId = "face_detection";
    config.solutionName = "Face Detection";
    config.solutionType = "face_detection";
    config.isDefault = true;
    
    // ... pipeline nodes ...
    
    // Để thay đổi default values, sửa phần defaults:
    config.defaults["detectorMode"] = "SmartDetection";
    config.defaults["detectionSensitivity"] = "0.8";  // ← Thay đổi từ 0.7 → 0.8
    config.defaults["sensorModality"] = "RGB";
    
    registerSolution(config);
}
```

**Lưu ý**: Sau khi sửa, cần rebuild project để thay đổi có hiệu lực.

---

## Template: Tạo Default Solution Mới

Sử dụng template sau để tạo nhanh một default solution mới:

```cpp
void SolutionRegistry::register[YourSolutionName]Solution() {
    SolutionConfig config;
    config.solutionId = "[solution_id]";
    config.solutionName = "[Solution Name]";
    config.solutionType = "[solution_type]";
    config.isDefault = true;  // ← QUAN TRỌNG
    
    // ===== SOURCE NODE =====
    SolutionConfig::NodeConfig sourceNode;
    sourceNode.nodeType = "[rtsp_src|file_src|...]";
    sourceNode.nodeName = "[node_name]_{instanceId}";
    sourceNode.parameters["[param1]"] = "[value1]";
    sourceNode.parameters["[param2]"] = "[value2]";
    config.pipeline.push_back(sourceNode);
    
    // ===== PROCESSOR NODE =====
    SolutionConfig::NodeConfig processorNode;
    processorNode.nodeType = "[yunet_face_detector|yolo_detector|...]";
    processorNode.nodeName = "[node_name]_{instanceId}";
    processorNode.parameters["[param1]"] = "[value1]";
    processorNode.parameters["[param2]"] = "[value2]";
    config.pipeline.push_back(processorNode);
    
    // ===== DESTINATION NODE =====
    SolutionConfig::NodeConfig destNode;
    destNode.nodeType = "[file_des|rtmp_des|...]";
    destNode.nodeName = "[node_name]_{instanceId}";
    destNode.parameters["[param1]"] = "[value1]";
    destNode.parameters["[param2]"] = "[value2]";
    config.pipeline.push_back(destNode);
    
    // ===== DEFAULTS =====
    config.defaults["detectorMode"] = "SmartDetection";
    config.defaults["detectionSensitivity"] = "0.7";
    config.defaults["sensorModality"] = "RGB";
    
    registerSolution(config);
}
```

---

## Các Node Types Được Hỗ trợ

### Source Nodes:
- `rtsp_src` - RTSP stream source
- `file_src` - File video source

### Detector/Processor Nodes:
- `yunet_face_detector` - YuNet face detector
- `yolo_detector` - YOLO object detector (chưa implement)
- `sface_feature_encoder` - SFace feature encoder
- `face_osd_v2` - Face OSD v2

### Destination Nodes:
- `file_des` - File destination (save to disk)
- `rtmp_des` - RTMP streaming destination

---

## Variables và Placeholders

Các placeholders có thể sử dụng trong parameters:

- `${RTSP_URL}` - RTSP URL (từ request)
- `${FILE_PATH}` - File path (từ request)
- `${MODEL_PATH}` - Model path (từ request)
- `${SFACE_MODEL_PATH}` - SFace model path (từ request)
- `${RTMP_URL}` - RTMP URL (từ request)
- `${detectionSensitivity}` - Detection sensitivity (từ defaults hoặc request)
- `{instanceId}` - Instance ID (tự động thay thế)

---

## Checklist Khi Thêm/Cập nhật Default Solution

- [ ] Tạo hàm `register[Name]Solution()` trong `solution_registry.cpp`
- [ ] Khai báo hàm trong `solution_registry.h` (phần private)
- [ ] Gọi hàm trong `initializeDefaultSolutions()`
- [ ] Set `config.isDefault = true`
- [ ] Đảm bảo `solutionId` không trùng với solution khác
- [ ] Test solution bằng cách tạo instance
- [ ] Cập nhật file `docs/default_solutions_backup.json` (nếu cần)
- [ ] Cập nhật tài liệu `docs/DEFAULT_SOLUTIONS_REFERENCE.md`

---

## Ví dụ Thực tế: Thêm Solution Mới

### Ví dụ: Thêm "Face Detection với Webcam"

```cpp
void SolutionRegistry::registerFaceDetectionWebcamSolution() {
    SolutionConfig config;
    config.solutionId = "face_detection_webcam";
    config.solutionName = "Face Detection with Webcam";
    config.solutionType = "face_detection";
    config.isDefault = true;
    
    // Webcam Source Node
    SolutionConfig::NodeConfig webcamSrc;
    webcamSrc.nodeType = "webcam_src";  // Giả sử có node này
    webcamSrc.nodeName = "webcam_src_{instanceId}";
    webcamSrc.parameters["device_id"] = "${WEBCAM_DEVICE_ID}";
    webcamSrc.parameters["channel"] = "0";
    webcamSrc.parameters["resize_ratio"] = "1.0";
    config.pipeline.push_back(webcamSrc);
    
    // YuNet Face Detector
    SolutionConfig::NodeConfig faceDetector;
    faceDetector.nodeType = "yunet_face_detector";
    faceDetector.nodeName = "face_detector_{instanceId}";
    faceDetector.parameters["model_path"] = "${MODEL_PATH}";
    faceDetector.parameters["score_threshold"] = "${detectionSensitivity}";
    faceDetector.parameters["nms_threshold"] = "0.5";
    faceDetector.parameters["top_k"] = "50";
    config.pipeline.push_back(faceDetector);
    
    // File Destination
    SolutionConfig::NodeConfig fileDes;
    fileDes.nodeType = "file_des";
    fileDes.nodeName = "file_des_{instanceId}";
    fileDes.parameters["save_dir"] = "./output/{instanceId}";
    fileDes.parameters["name_prefix"] = "webcam_face_detection";
    fileDes.parameters["osd"] = "true";
    config.pipeline.push_back(fileDes);
    
    // Defaults
    config.defaults["detectorMode"] = "SmartDetection";
    config.defaults["detectionSensitivity"] = "0.7";
    config.defaults["sensorModality"] = "RGB";
    
    registerSolution(config);
}
```

Sau đó thêm vào `initializeDefaultSolutions()`:
```cpp
void SolutionRegistry::initializeDefaultSolutions() {
    registerFaceDetectionSolution();
    registerFaceDetectionFileSolution();
    registerObjectDetectionSolution();
    registerFaceDetectionRTMPSolution();
    registerFaceDetectionWebcamSolution();  // ← Thêm dòng này
}
```

---

## Lưu ý Quan trọng

1. **Default solutions không được lưu vào storage**: Chúng chỉ tồn tại trong memory khi ứng dụng chạy
2. **Phải rebuild**: Sau khi sửa code, phải rebuild project để thay đổi có hiệu lực
3. **Không thể xóa qua API**: Default solutions được bảo vệ, không thể xóa qua API
4. **ID phải unique**: Đảm bảo `solutionId` không trùng với solution khác
5. **isDefault = true**: Luôn nhớ set `config.isDefault = true` cho default solutions

---

## Kiểm tra Sau Khi Thêm/Cập nhật

```bash
# 1. Rebuild project
cd build && make

# 2. Khởi động ứng dụng
./edge_ai_api

# 3. Kiểm tra solutions có sẵn
curl http://localhost:8080/v1/core/solutions | jq

# 4. Kiểm tra solution mới
curl http://localhost:8080/v1/core/solutions/[your_solution_id] | jq

# 5. Test tạo instance với solution mới
curl -X POST http://localhost:8080/v1/core/instances \
  -H "Content-Type: application/json" \
  -d '{
    "instanceId": "test_instance",
    "solutionId": "[your_solution_id]",
    ...
  }'
```

---

## Tài liệu Liên quan

- [DEFAULT_SOLUTIONS_REFERENCE.md](./DEFAULT_SOLUTIONS_REFERENCE.md) - Tham khảo các default solutions hiện có
- [SOLUTION_SECURITY.md](./SOLUTION_SECURITY.md) - Bảo mật default solutions
- [default_solutions_backup.json](./default_solutions_backup.json) - Backup JSON của các default solutions

