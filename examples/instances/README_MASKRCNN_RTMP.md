# Hướng Dẫn Test MaskRCNN với RTMP Output

File mẫu này (`example_mask_rcnn_rtmp.json`) là một ví dụ để tạo instance MaskRCNN với RTMP streaming output, tương tự như `example_face_detection_rtmp.json`.

## ✅ Solution Đã Được Tạo

**Đã có solution sẵn cho MaskRCNN:**

1. **`mask_rcnn_detection`** - MaskRCNN với file source và file output
2. **`mask_rcnn_rtmp`** - MaskRCNN với file source và RTMP output ⭐

File mẫu `example_mask_rcnn_rtmp.json` sử dụng solution `mask_rcnn_rtmp` đã được đăng ký trong hệ thống.

## 📋 Các Solutions Có Sẵn

### 1. mask_rcnn_detection
- **Input**: File video
- **Output**: File video với segmentation results
- **Pipeline**: File Source → MaskRCNN Detector → OSD v3 → File Destination

### 2. mask_rcnn_rtmp ⭐
- **Input**: File video
- **Output**: RTMP stream với segmentation results
- **Pipeline**: File Source → MaskRCNN Detector → OSD v3 → RTMP Destination

## 🔧 Cách Sử Dụng

### Sử dụng solution mask_rcnn_rtmp (Khuyến nghị)

File mẫu đã được cấu hình sẵn với solution `mask_rcnn_rtmp`:

```bash
curl -X POST http://localhost:8080/v1/instances \
  -H "Content-Type: application/json" \
  -d @examples/instances/example_mask_rcnn_rtmp.json
```

### Sử dụng solution mask_rcnn_detection (File output)

Nếu bạn chỉ cần file output:

```bash
curl -X POST http://localhost:8080/v1/instances \
  -H "Content-Type: application/json" \
  -d @examples/instances/infer_nodes/example_mask_rcnn.json
```

## 📝 Các Phương Án Khác (Không Cần Thiết Nữa)

**Lưu ý:** Các phương án dưới đây chỉ cần thiết nếu bạn muốn tùy chỉnh pipeline. Với hầu hết trường hợp, sử dụng solution `mask_rcnn_rtmp` là đủ.

### Phương án 1: Sử dụng Custom Pipeline (Khuyến nghị)

Tạo instance với custom pipeline bằng cách định nghĩa các nodes riêng lẻ:

```json
{
  "name": "mask_rcnn_rtmp_custom",
  "group": "segmentation",
  "pipeline": [
    {
      "nodeType": "file_src",
      "nodeName": "file_src_0",
      "parameters": {
        "file_path": "./cvedix_data/test_video/mask_rcnn.mp4",
        "channel": "0",
        "resize_ratio": "1.0"
      }
    },
    {
      "nodeType": "mask_rcnn_detector",
      "nodeName": "mask_rcnn_detector_0",
      "parameters": {
        "model_path": "./cvedix_data/models/mask_rcnn/frozen_inference_graph.pb",
        "model_config_path": "./cvedix_data/models/mask_rcnn/mask_rcnn_inception_v2_coco_2018_01_28.pbtxt",
        "labels_path": "./cvedix_data/models/coco_80classes.txt",
        "input_width": "416",
        "input_height": "416",
        "score_threshold": "0.5"
      }
    },
    {
      "nodeType": "osd_v3",
      "nodeName": "osd_v3_0",
      "parameters": {
        "font_path": "./cvedix_data/font/NotoSansCJKsc-Medium.otf"
      }
    },
    {
      "nodeType": "rtmp_des",
      "nodeName": "rtmp_des_0",
      "parameters": {
        "rtmp_url": "rtmp://localhost:1935/live/mask_rcnn_demo",
        "channel": "0"
      }
    }
  ],
  "persistent": true,
  "autoStart": false
}
```

### Phương án 2: Tạo Solution Mới

Thêm solution `mask_rcnn_rtmp` vào `src/solutions/solution_registry.cpp`:

```cpp
void SolutionRegistry::registerMaskRCNNRTMPSolution() {
    SolutionConfig config;
    config.solutionId = "mask_rcnn_rtmp";
    config.solutionName = "MaskRCNN with RTMP Streaming";
    config.solutionType = "segmentation";
    config.isDefault = true;
    
    // File Source Node
    SolutionConfig::NodeConfig fileSrc;
    fileSrc.nodeType = "file_src";
    fileSrc.nodeName = "file_src_{instanceId}";
    fileSrc.parameters["file_path"] = "${FILE_PATH}";
    fileSrc.parameters["channel"] = "0";
    fileSrc.parameters["resize_ratio"] = "${RESIZE_RATIO}";
    config.pipeline.push_back(fileSrc);
    
    // MaskRCNN Detector Node
    SolutionConfig::NodeConfig maskRCNN;
    maskRCNN.nodeType = "mask_rcnn_detector";
    maskRCNN.nodeName = "mask_rcnn_detector_{instanceId}";
    maskRCNN.parameters["model_path"] = "${MODEL_PATH}";
    maskRCNN.parameters["model_config_path"] = "${MODEL_CONFIG_PATH}";
    maskRCNN.parameters["labels_path"] = "${LABELS_PATH}";
    maskRCNN.parameters["input_width"] = "${INPUT_WIDTH}";
    maskRCNN.parameters["input_height"] = "${INPUT_HEIGHT}";
    maskRCNN.parameters["score_threshold"] = "${SCORE_THRESHOLD}";
    config.pipeline.push_back(maskRCNN);
    
    // OSD v3 Node
    SolutionConfig::NodeConfig osd;
    osd.nodeType = "osd_v3";
    osd.nodeName = "osd_v3_{instanceId}";
    osd.parameters["font_path"] = "./cvedix_data/font/NotoSansCJKsc-Medium.otf";
    config.pipeline.push_back(osd);
    
    // RTMP Destination Node
    SolutionConfig::NodeConfig rtmpDes;
    rtmpDes.nodeType = "rtmp_des";
    rtmpDes.nodeName = "rtmp_des_{instanceId}";
    rtmpDes.parameters["rtmp_url"] = "${RTMP_URL}";
    rtmpDes.parameters["channel"] = "0";
    config.pipeline.push_back(rtmpDes);
    
    // Default configurations
    config.defaults["detectorMode"] = "SmartDetection";
    config.defaults["detectionSensitivity"] = "Medium";
    config.defaults["sensorModality"] = "RGB";
    
    registerSolution(config);
}
```

Sau đó đăng ký trong `initializeDefaultSolutions()`:

```cpp
void SolutionRegistry::initializeDefaultSolutions() {
    // ... existing solutions ...
    registerMaskRCNNRTMPSolution();
}
```

### Phương án 3: Sử dụng File Output (Hiện tại)

File mẫu `example_mask_rcnn_rtmp.json` hiện tại sử dụng solution `mask_rcnn_detection` với file output. Bạn có thể:

1. Chạy instance với file output
2. Sau đó stream file output lên RTMP bằng công cụ khác (ffmpeg, etc.)

## 📝 Cách Sử Dụng File Mẫu

### 1. Chỉnh sửa đường dẫn

Mở file `example_mask_rcnn_rtmp.json` và chỉnh sửa các tham số:

```json
{
  "additionalParams": {
    "FILE_PATH": "./cvedix_data/test_video/mask_rcnn.mp4",  // Đường dẫn video
    "RTMP_URL": "rtmp://localhost:1935/live/mask_rcnn_demo",  // RTMP server URL
    "MODEL_PATH": "./cvedix_data/models/mask_rcnn/frozen_inference_graph.pb",
    "MODEL_CONFIG_PATH": "./cvedix_data/models/mask_rcnn/mask_rcnn_inception_v2_coco_2018_01_28.pbtxt",
    "LABELS_PATH": "./cvedix_data/models/coco_80classes.txt",
    "INPUT_WIDTH": "416",
    "INPUT_HEIGHT": "416",
    "SCORE_THRESHOLD": "0.5",
    "RESIZE_RATIO": "1.0"
  }
}
```

### 2. Tạo instance

```bash
curl -X POST http://localhost:8080/v1/instances \
  -H "Content-Type: application/json" \
  -d @examples/instances/example_mask_rcnn_rtmp.json
```

### 3. Kiểm tra status

```bash
curl http://localhost:8080/v1/instances/mask_rcnn_rtmp_demo
```

### 4. Start instance (nếu autoStart = false)

```bash
curl -X POST http://localhost:8080/v1/instances/mask_rcnn_rtmp_demo/start
```

### 5. Dừng instance

```bash
curl -X POST http://localhost:8080/v1/instances/mask_rcnn_rtmp_demo/stop
```

### 6. Xóa instance

```bash
curl -X DELETE http://localhost:8080/v1/instances/mask_rcnn_rtmp_demo
```

## 🔧 Yêu Cầu

1. **Model files:**
   - `frozen_inference_graph.pb` hoặc `mask_rcnn_inception_v2_coco.pb`
   - `mask_rcnn_inception_v2_coco_2018_01_28.pbtxt`
   - `coco_80classes.txt`

2. **Video test:**
   - `mask_rcnn.mp4` hoặc video có nhiều đối tượng

3. **RTMP Server:**
   - Nginx-RTMP hoặc RTMP server khác
   - URL format: `rtmp://host:port/app/stream`

## 📚 Tài Liệu Tham Khảo

- `example_face_detection_rtmp.json` - Mẫu face detection với RTMP
- `examples/instances/infer_nodes/example_mask_rcnn.json` - Mẫu MaskRCNN cơ bản
- `src/solutions/solution_registry.cpp` - Đăng ký solutions
- `sample/mask_rcnn_sample.cpp` - Sample code C++

## 💡 Tips

1. **Test với file output trước** để đảm bảo MaskRCNN hoạt động đúng
2. **Kiểm tra RTMP server** trước khi stream
3. **Giảm input size** (416x416 → 320x320) nếu tốc độ chậm
4. **Sử dụng GPU** nếu có để tăng tốc độ inference

