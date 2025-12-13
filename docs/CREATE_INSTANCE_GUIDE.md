# Hướng dẫn Tạo Instance - Chi tiết từng Case

## Mục lục

1. [Giới thiệu](#giới-thiệu)
2. [Tổng quan](#tổng-quan)
3. [Cấu trúc Request](#cấu-trúc-request)
4. [Inference Nodes (Detector)](#inference-nodes-detector)
5. [Source Nodes (Input)](#source-nodes-input)
6. [Broker Nodes (Output)](#broker-nodes-output)
7. [Pipeline Hoàn chỉnh](#pipeline-hoàn-chỉnh)
8. [Kiểm tra và Testing](#kiểm-tra-và-testing)

---

## Giới thiệu

### Pipeline là gì?

**Pipeline** (luồng xử lý) là một chuỗi các **nodes** (nút xử lý) được kết nối với nhau để xử lý dữ liệu video/ảnh theo một quy trình nhất định. Mỗi node thực hiện một chức năng cụ thể và truyền kết quả cho node tiếp theo.

**Ví dụ Pipeline đơn giản**:
```
[RTSP Source] → [Face Detector] → [Feature Encoder] → [JSON Broker] → [RTMP Destination]
     ↓                ↓                    ↓                  ↓              ↓
  Nhận video    Phát hiện khuôn mặt   Mã hóa đặc trưng   Xuất JSON    Gửi stream
```

**Các thành phần Pipeline**:
1. **Source Nodes** (Input): Nhận dữ liệu đầu vào
   - RTSP stream, File video, Image, RTMP, UDP, Application frames
2. **Inference Nodes** (Processing): Xử lý AI/ML
   - Object detection, Face detection, Classification, Segmentation, Pose estimation, etc.
3. **OSD Nodes** (Overlay): Vẽ thông tin lên frame
   - Face OSD, Object OSD, Text overlay
4. **Broker Nodes** (Output): Xuất kết quả dưới dạng messages
   - JSON (Console, MQTT, Kafka), XML (File, Socket), Socket brokers
5. **Destination Nodes** (Output): Xuất video stream/file
   - RTMP stream, File video, Screen display

### Bài toán Tạo Instance giải quyết vấn đề gì?

**Instance** là một phiên bản cụ thể của pipeline với các tham số cụ thể. Tạo instance cho phép:

1. **Tái sử dụng Solution Config**: 
   - Một solution config (định nghĩa pipeline) có thể được sử dụng để tạo nhiều instances khác nhau
   - Mỗi instance có thể có tham số riêng (model path, RTSP URL, output destination, etc.)

2. **Quản lý nhiều AI tasks đồng thời**:
   - Chạy nhiều instances cùng lúc trên cùng một server
   - Mỗi instance xử lý một nguồn video/ảnh khác nhau
   - Mỗi instance có thể sử dụng model AI khác nhau

3. **Linh hoạt trong cấu hình**:
   - Không cần recompile code khi thay đổi tham số
   - Cấu hình qua API, dễ dàng tự động hóa
   - Hỗ trợ persistent storage để khôi phục instances sau khi restart

4. **Tách biệt môi trường**:
   - Mỗi instance có ID riêng, có thể start/stop độc lập
   - Logs và output được tách biệt theo instance
   - Dễ dàng monitoring và debugging

**Ví dụ thực tế**:
```
Solution: "face_detection_pipeline"
  ├─ Instance 1: Camera 1 → Face Detection → MQTT Broker
  ├─ Instance 2: Camera 2 → Face Detection → Kafka Broker  
  └─ Instance 3: Video File → Face Detection → XML File
```

### Chức năng hiện tại Code đang hỗ trợ

Hệ thống hiện tại hỗ trợ đầy đủ các loại nodes sau:

#### 1. Source Nodes (Input) - 6 nodes ✅
- `rtsp_src`: Nhận video từ RTSP stream
- `file_src`: Đọc video từ file
- `app_src`: Nhận frames từ application code (push_frames)
- `image_src`: Đọc ảnh từ file hoặc UDP
- `rtmp_src`: Nhận video từ RTMP stream
- `udp_src`: Nhận video từ UDP stream

#### 2. Inference Nodes (Detector/Processing) - 23 nodes ✅

**TensorRT Nodes** (11 nodes):
- `trt_yolov8_detector`: YOLOv8 object detection
- `trt_yolov8_seg_detector`: YOLOv8 instance segmentation
- `trt_yolov8_pose_detector`: YOLOv8 pose estimation
- `trt_yolov8_classifier`: YOLOv8 classification
- `trt_vehicle_detector`: Vehicle detection
- `trt_vehicle_plate_detector`: Vehicle plate detection
- `trt_vehicle_plate_detector_v2`: Vehicle plate detection v2
- `trt_vehicle_color_classifier`: Vehicle color classification
- `trt_vehicle_type_classifier`: Vehicle type classification
- `trt_vehicle_feature_encoder`: Vehicle feature encoding
- `trt_vehicle_scanner`: Vehicle scanning

**RKNN Nodes** (2 nodes):
- `rknn_yolov8_detector`: RKNN YOLOv8 detection
- `rknn_face_detector`: RKNN face detection

**Other Inference Nodes** (10 nodes):
- `yunet_face_detector`: YuNet face detection
- `sface_feature_encoder`: SFace feature encoding
- `yolo_detector`: YOLO detector (OpenCV DNN)
- `enet_seg`: ENet semantic segmentation
- `mask_rcnn_detector`: Mask RCNN instance segmentation
- `openpose_detector`: OpenPose pose estimation
- `classifier`: Generic image classifier
- `feature_encoder`: Generic feature encoder
- `lane_detector`: Lane detection
- `ppocr_text_detector`: PaddleOCR text detection
- `restoration`: Image restoration (Real-ESRGAN)

#### 3. Broker Nodes (Output Messages) - 12 nodes ✅
- `json_console_broker`: JSON console output
- `json_enhanced_console_broker`: JSON enhanced console output
- `json_mqtt_broker`: JSON MQTT broker
- `json_kafka_broker`: JSON Kafka broker
- `xml_file_broker`: XML file output
- `xml_socket_broker`: XML socket output
- `msg_broker`: Message broker
- `ba_socket_broker`: BA socket broker
- `embeddings_socket_broker`: Embeddings socket broker
- `embeddings_properties_socket_broker`: Embeddings properties socket broker
- `plate_socket_broker`: Plate socket broker
- `expr_socket_broker`: Expression socket broker

#### 4. Destination Nodes (Output Stream/File) - 2 nodes ✅
- `file_des`: Lưu video vào file
- `rtmp_des`: Gửi video qua RTMP stream

#### 5. OSD Nodes (Overlay) - Được hỗ trợ qua SDK
- Face OSD, Object OSD, Text overlay (sử dụng trực tiếp từ SDK)

### Tổng kết Chức năng

| Loại Node | Số lượng | Trạng thái |
|-----------|----------|------------|
| **Source Nodes** | 6 | ✅ 100% |
| **Inference Nodes** | 23 | ✅ 100% |
| **Broker Nodes** | 12 | ✅ 100% |
| **Destination Nodes** | 2 | ✅ (có thể mở rộng) |
| **Tổng** | **43 nodes** | ✅ **Đầy đủ cho yêu cầu chính** |

### Workflow Tạo Instance

```
1. User gửi Request → API POST /v1/core/instance
   ↓
2. Parse Request → Validate parameters
   ↓
3. Get Solution Config → Load pipeline definition
   ↓
4. Build Pipeline → Tạo các nodes theo config
   ↓
5. Create Instance → Lưu instance info
   ↓
6. Auto Start (nếu enabled) → Start pipeline
   ↓
7. Return Instance ID → User có thể quản lý instance
```

### Use Cases Hỗ trợ

1. **Video Surveillance**:
   - RTSP camera → Face/Vehicle detection → MQTT/Kafka broker
   
2. **Video Processing**:
   - File video → Object detection → RTMP stream
   
3. **Real-time Analytics**:
   - Multiple RTSP streams → Multiple detectors → Multiple brokers
   
4. **Edge AI Applications**:
   - Camera input → AI inference → Cloud messaging
   
5. **Batch Processing**:
   - Image sequence → Classification → File output

---

## Tổng quan

API `POST /v1/core/instance` cho phép tạo các AI instances với các loại nodes khác nhau:
- **Inference Nodes**: Phát hiện, phân loại, nhận diện (Detector)
- **Source Nodes**: Nguồn input (Input)
- **Broker Nodes**: Xuất kết quả (Output)

### Quy trình chung

1. **Tạo Solution Config** (nếu chưa có)
   - Định nghĩa pipeline với các node types
   - Đăng ký solution vào hệ thống

2. **Tạo Instance**
   - Gọi API `POST /v1/core/instance`
   - Cung cấp solution ID và parameters

3. **Kiểm tra Instance**
   - Kiểm tra trạng thái instance
   - Xem logs để verify

---

## Cấu trúc Request

### Request Body Cơ bản

```json
{
  "name": "instance_name",
  "group": "group_name",
  "solution": "solution_id",
  "persistent": true,
  "autoStart": true,
  "detectionSensitivity": "Medium",
  "additionalParams": {
    "PARAM_NAME": "value"
  }
}
```

### Các Field Quan trọng

**Fields Cơ bản**:
- `name` (required): Tên instance, pattern: `^[A-Za-z0-9 -_]+$`
- `group` (optional): Nhóm instance, pattern: `^[A-Za-z0-9 -_]+$`
- `solution` (optional): Solution ID đã được đăng ký
- `persistent` (optional): Lưu instance vào file để load lại khi restart (default: false)
- `autoStart` (optional): Tự động khởi động pipeline khi tạo instance (default: false)

**Fields Detector (Nhận diện AI)**:
- `detectionSensitivity` (optional): Độ nhạy phát hiện - "Low", "Medium", "High", "Normal", "Slow" (default: "Low")
- `detectorMode` (optional): Chế độ detector - "SmartDetection", "FullRegionInference", "MosaicInference" (default: "SmartDetection")
- `detectorModelFile` (optional): Tên model file (ví dụ: "pva_det_full_frame_512") - hệ thống tự tìm trong thư mục models
- `animalConfidenceThreshold` (optional): Ngưỡng độ tin cậy cho động vật (0.0-1.0)
- `personConfidenceThreshold` (optional): Ngưỡng độ tin cậy cho người (0.0-1.0)
- `vehicleConfidenceThreshold` (optional): Ngưỡng độ tin cậy cho xe cộ (0.0-1.0)
- `faceConfidenceThreshold` (optional): Ngưỡng độ tin cậy cho khuôn mặt (0.0-1.0)
- `licensePlateConfidenceThreshold` (optional): Ngưỡng độ tin cậy cho biển số (0.0-1.0)
- `confThreshold` (optional): Ngưỡng độ tin cậy chung (0.0-1.0)
- `detectorThermalModelFile` (optional): Tên model file cho camera nhiệt (ví dụ: "pva_det_mosaic_320")

**Fields Performance (Hiệu năng)**:
- `performanceMode` (optional): Chế độ hiệu năng - "Balanced", "Performance", "Saved" (default: "Balanced")
- `frameRateLimit` (optional): Giới hạn tốc độ khung hình (FPS)
- `recommendedFrameRate` (optional): Tốc độ khung hình được khuyến nghị (FPS)
- `inputPixelLimit` (optional): Giới hạn số pixel đầu vào (ví dụ: 2000000 cho Full HD)

**Fields SolutionManager (Quản lý Solution)**:
- `metadataMode` (optional): Gửi metadata về vật thể nhận diện (default: false)
- `statisticsMode` (optional): Chạy thống kê (default: false)
- `diagnosticsMode` (optional): Gửi dữ liệu chẩn đoán lỗi (default: false)
- `debugMode` (optional): Bật chế độ debug (default: false)

**Fields Input (Đầu vào)**:
- `inputOrientation` (optional): Hướng xoay input (0-3)
- `inputPixelLimit` (optional): Giới hạn số pixel đầu vào

**Fields Additional Parameters**:
- `additionalParams`: Các tham số cho nodes (MODEL_PATH, RTSP_URL, FILE_PATH, RTMP_URL, etc.)

---

## Inference Nodes (Detector)

### Case 1: TensorRT YOLOv8 Detector

**Solution Config** (cần tạo trước):
```json
{
  "solutionId": "trt_yolov8_detection",
  "solutionName": "TensorRT YOLOv8 Detection",
  "pipeline": [
    {
      "nodeType": "rtsp_src",
      "nodeName": "rtsp_src_{instanceId}",
      "parameters": {
        "rtsp_url": "${RTSP_URL}",
        "channel": "0"
      }
    },
    {
      "nodeType": "trt_yolov8_detector",
      "nodeName": "yolov8_detector_{instanceId}",
      "parameters": {
        "model_path": "${MODEL_PATH}",
        "labels_path": "${LABELS_PATH}"
      }
    },
    {
      "nodeType": "rtmp_des",
      "nodeName": "rtmp_des_{instanceId}",
      "parameters": {
        "rtmp_url": "${RTMP_URL}",
        "channel": "0"
      }
    }
  ]
}
```

**Create Instance Request**:
```json
{
  "name": "yolov8_detector_1",
  "group": "detection",
  "solution": "trt_yolov8_detection",
  "persistent": true,
  "autoStart": true,
  "detectionSensitivity": "Medium",
  "additionalParams": {
    "RTSP_URL": "rtsp://localhost:8554/stream",
    "MODEL_PATH": "./cvedix_data/models/trt/others/yolov8s_v8.5.engine",
    "LABELS_PATH": "./cvedix_data/models/coco_80classes.txt",
    "RTMP_URL": "rtmp://server.com:1935/live/stream"
  }
}
```

**Cách test**:
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @examples/instances/infer_nodes/example_trt_yolov8_detector.json
```

---

### Case 2: TensorRT Vehicle Detector

**Solution Config**:
```json
{
  "solutionId": "trt_vehicle_detection",
  "solutionName": "TensorRT Vehicle Detection",
  "pipeline": [
    {
      "nodeType": "file_src",
      "nodeName": "file_src_{instanceId}",
      "parameters": {
        "file_path": "${FILE_PATH}",
        "channel": "0",
        "resize_ratio": "0.5"
      }
    },
    {
      "nodeType": "trt_vehicle_detector",
      "nodeName": "vehicle_detector_{instanceId}",
      "parameters": {
        "model_path": "${MODEL_PATH}"
      }
    },
    {
      "nodeType": "file_des",
      "nodeName": "file_des_{instanceId}",
      "parameters": {
        "channel": "0",
        "save_dir": "./output/{instanceId}"
      }
    }
  ]
}
```

**Create Instance Request**:
```json
{
  "name": "vehicle_detector_1",
  "group": "vehicle",
  "solution": "trt_vehicle_detection",
  "persistent": true,
  "autoStart": true,
  "additionalParams": {
    "FILE_PATH": "./cvedix_data/test_video/plate.mp4",
    "MODEL_PATH": "./cvedix_data/models/trt/vehicle/vehicle_v8.5.trt"
  }
}
```

**Cách test**:
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @examples/instances/infer_nodes/example_trt_vehicle_detector.json
```

---

### Case 3: RKNN Face Detector

**Solution Config**:
```json
{
  "solutionId": "rknn_face_detection",
  "solutionName": "RKNN Face Detection",
  "pipeline": [
    {
      "nodeType": "file_src",
      "nodeName": "file_src_{instanceId}",
      "parameters": {
        "file_path": "${FILE_PATH}",
        "channel": "0",
        "resize_ratio": "0.6"
      }
    },
    {
      "nodeType": "rknn_face_detector",
      "nodeName": "rknn_face_detector_{instanceId}",
      "parameters": {
        "model_path": "${MODEL_PATH}",
        "score_threshold": "${SCORE_THRESHOLD}",
        "nms_threshold": "${NMS_THRESHOLD}",
        "input_width": "${INPUT_WIDTH}",
        "input_height": "${INPUT_HEIGHT}",
        "num_classes": "${NUM_CLASSES}"
      }
    },
    {
      "nodeType": "rtmp_des",
      "nodeName": "rtmp_des_{instanceId}",
      "parameters": {
        "rtmp_url": "${RTMP_URL}",
        "channel": "0"
      }
    }
  ]
}
```

**Create Instance Request**:
```json
{
  "name": "rknn_face_detector_1",
  "group": "face",
  "solution": "rknn_face_detection",
  "persistent": true,
  "autoStart": true,
  "additionalParams": {
    "FILE_PATH": "./cvedix_data/test_video/face_multis.mp4",
    "MODEL_PATH": "./cvedix_data/models/face/yolov8n_face_detection.rknn",
    "SCORE_THRESHOLD": "0.5",
    "NMS_THRESHOLD": "0.5",
    "INPUT_WIDTH": "640",
    "INPUT_HEIGHT": "640",
    "NUM_CLASSES": "1",
    "RTMP_URL": "rtmp://server.com:1935/live/rknn_face"
  }
}
```

**Cách test**:
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @examples/instances/infer_nodes/example_rknn_face_detector.json
```

---

### Case 4: YOLO Detector (OpenCV DNN)

**Solution Config**:
```json
{
  "solutionId": "yolo_detection",
  "solutionName": "YOLO Detection",
  "pipeline": [
    {
      "nodeType": "rtsp_src",
      "nodeName": "rtsp_src_{instanceId}",
      "parameters": {
        "rtsp_url": "${RTSP_URL}",
        "channel": "0"
      }
    },
    {
      "nodeType": "yolo_detector",
      "nodeName": "yolo_detector_{instanceId}",
      "parameters": {
        "model_path": "${MODEL_PATH}",
        "model_config_path": "${MODEL_CONFIG_PATH}",
        "labels_path": "${LABELS_PATH}",
        "input_width": "${INPUT_WIDTH}",
        "input_height": "${INPUT_HEIGHT}",
        "score_threshold": "${SCORE_THRESHOLD}",
        "confidence_threshold": "${CONFIDENCE_THRESHOLD}",
        "nms_threshold": "${NMS_THRESHOLD}"
      }
    },
    {
      "nodeType": "rtmp_des",
      "nodeName": "rtmp_des_{instanceId}",
      "parameters": {
        "rtmp_url": "${RTMP_URL}",
        "channel": "0"
      }
    }
  ]
}
```

**Create Instance Request**:
```json
{
  "name": "yolo_detector_1",
  "group": "detection",
  "solution": "yolo_detection",
  "persistent": true,
  "autoStart": true,
  "additionalParams": {
    "RTSP_URL": "rtsp://localhost:8554/stream",
    "MODEL_PATH": "./cvedix_data/models/yolo/yolov3.weights",
    "MODEL_CONFIG_PATH": "./cvedix_data/models/yolo/yolov3.cfg",
    "LABELS_PATH": "./cvedix_data/models/coco_80classes.txt",
    "INPUT_WIDTH": "416",
    "INPUT_HEIGHT": "416",
    "SCORE_THRESHOLD": "0.5",
    "CONFIDENCE_THRESHOLD": "0.5",
    "NMS_THRESHOLD": "0.5",
    "RTMP_URL": "rtmp://server.com:1935/live/yolo"
  }
}
```

**Cách test**:
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @examples/instances/infer_nodes/example_yolo_detector.json
```

---

## Source Nodes (Input)

### Case 5: App Source

**Solution Config**:
```json
{
  "solutionId": "app_source_detection",
  "solutionName": "App Source Detection",
  "pipeline": [
    {
      "nodeType": "app_src",
      "nodeName": "app_src_{instanceId}",
      "parameters": {
        "channel": "${CHANNEL}"
      }
    },
    {
      "nodeType": "yunet_face_detector",
      "nodeName": "face_detector_{instanceId}",
      "parameters": {
        "model_path": "${MODEL_PATH}",
        "score_threshold": "${detectionSensitivity}"
      }
    },
    {
      "nodeType": "rtmp_des",
      "nodeName": "rtmp_des_{instanceId}",
      "parameters": {
        "rtmp_url": "${RTMP_URL}",
        "channel": "0"
      }
    }
  ]
}
```

**Create Instance Request**:
```json
{
  "name": "app_src_demo",
  "group": "source",
  "solution": "app_source_detection",
  "persistent": true,
  "autoStart": false,
  "detectionSensitivity": "Medium",
  "additionalParams": {
    "MODEL_PATH": "./cvedix_data/models/face/face_detection_yunet_2022mar.onnx",
    "CHANNEL": "0",
    "RTMP_URL": "rtmp://server.com:1935/live/app_src"
  }
}
```

**Lưu ý**: 
- `autoStart: false` vì app_src cần push frames từ code
- Sử dụng `push_frames()` method để push frames vào pipeline

**Cách test**:
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @examples/instances/infer_nodes/example_app_src.json
```

---

### Case 6: Image Source

**Solution Config**:
```json
{
  "solutionId": "image_source_detection",
  "solutionName": "Image Source Detection",
  "pipeline": [
    {
      "nodeType": "image_src",
      "nodeName": "image_src_{instanceId}",
      "parameters": {
        "port_or_location": "${IMAGE_SRC_PORT_OR_LOCATION}",
        "interval": "${INTERVAL}",
        "resize_ratio": "${RESIZE_RATIO}",
        "cycle": "${CYCLE}"
      }
    },
    {
      "nodeType": "trt_vehicle_detector",
      "nodeName": "vehicle_detector_{instanceId}",
      "parameters": {
        "model_path": "${MODEL_PATH}"
      }
    },
    {
      "nodeType": "file_des",
      "nodeName": "file_des_{instanceId}",
      "parameters": {
        "channel": "0",
        "save_dir": "./output/{instanceId}"
      }
    }
  ]
}
```

**Create Instance Request**:
```json
{
  "name": "image_src_demo",
  "group": "source",
  "solution": "image_source_detection",
  "persistent": true,
  "autoStart": true,
  "additionalParams": {
    "IMAGE_SRC_PORT_OR_LOCATION": "./cvedix_data/test_images/vehicle/%d.jpg",
    "INTERVAL": "1",
    "RESIZE_RATIO": "0.4",
    "CYCLE": "true",
    "MODEL_PATH": "./cvedix_data/models/trt/vehicle/vehicle_v8.5.trt"
  }
}
```

**Lưu ý**:
- `port_or_location` có thể là:
  - File pattern: `"./images/%d.jpg"` (đọc từ file)
  - UDP port: `"6000"` (nhận từ UDP)

**Cách test**:
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @examples/instances/infer_nodes/example_image_src.json
```

---

### Case 7: RTMP Source

**Solution Config**:
```json
{
  "solutionId": "rtmp_source_detection",
  "solutionName": "RTMP Source Detection",
  "pipeline": [
    {
      "nodeType": "rtmp_src",
      "nodeName": "rtmp_src_{instanceId}",
      "parameters": {
        "rtmp_url": "${RTMP_SRC_URL}",
        "channel": "0",
        "resize_ratio": "${RESIZE_RATIO}",
        "skip_interval": "${SKIP_INTERVAL}"
      }
    },
    {
      "nodeType": "yunet_face_detector",
      "nodeName": "face_detector_{instanceId}",
      "parameters": {
        "model_path": "${MODEL_PATH}",
        "score_threshold": "${detectionSensitivity}"
      }
    },
    {
      "nodeType": "rtmp_des",
      "nodeName": "rtmp_des_{instanceId}",
      "parameters": {
        "rtmp_url": "${RTMP_URL}",
        "channel": "0"
      }
    }
  ]
}
```

**Create Instance Request**:
```json
{
  "name": "rtmp_src_demo",
  "group": "source",
  "solution": "rtmp_source_detection",
  "persistent": true,
  "autoStart": true,
  "additionalParams": {
    "RTMP_SRC_URL": "rtmp://source.com:1935/live/input_stream",
    "RESIZE_RATIO": "1.0",
    "SKIP_INTERVAL": "0",
    "MODEL_PATH": "./cvedix_data/models/face/face_detection_yunet_2022mar.onnx",
    "RTMP_URL": "rtmp://server.com:1935/live/output_stream"
  }
}
```

**Cách test**:
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @examples/instances/infer_nodes/example_rtmp_src.json
```

---

### Case 8: UDP Source

**Solution Config**:
```json
{
  "solutionId": "udp_source_detection",
  "solutionName": "UDP Source Detection",
  "pipeline": [
    {
      "nodeType": "udp_src",
      "nodeName": "udp_src_{instanceId}",
      "parameters": {
        "port": "${UDP_PORT}",
        "channel": "0",
        "resize_ratio": "${RESIZE_RATIO}",
        "skip_interval": "${SKIP_INTERVAL}"
      }
    },
    {
      "nodeType": "yunet_face_detector",
      "nodeName": "face_detector_{instanceId}",
      "parameters": {
        "model_path": "${MODEL_PATH}",
        "score_threshold": "${detectionSensitivity}"
      }
    },
    {
      "nodeType": "rtmp_des",
      "nodeName": "rtmp_des_{instanceId}",
      "parameters": {
        "rtmp_url": "${RTMP_URL}",
        "channel": "0"
      }
    }
  ]
}
```

**Create Instance Request**:
```json
{
  "name": "udp_src_demo",
  "group": "source",
  "solution": "udp_source_detection",
  "persistent": true,
  "autoStart": true,
  "additionalParams": {
    "UDP_PORT": "6000",
    "RESIZE_RATIO": "1.0",
    "SKIP_INTERVAL": "0",
    "MODEL_PATH": "./cvedix_data/models/face/face_detection_yunet_2022mar.onnx",
    "RTMP_URL": "rtmp://server.com:1935/live/udp_stream"
  }
}
```

**Cách test**:
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @examples/instances/infer_nodes/example_udp_src.json
```

---

## Broker Nodes (Output)

### Case 9: JSON Console Broker

**Solution Config**:
```json
{
  "solutionId": "face_detection_with_json_broker",
  "solutionName": "Face Detection with JSON Console Broker",
  "pipeline": [
    {
      "nodeType": "file_src",
      "nodeName": "file_src_{instanceId}",
      "parameters": {
        "file_path": "${FILE_PATH}",
        "channel": "0",
        "resize_ratio": "0.6"
      }
    },
    {
      "nodeType": "yunet_face_detector",
      "nodeName": "face_detector_{instanceId}",
      "parameters": {
        "model_path": "${MODEL_PATH}",
        "score_threshold": "${detectionSensitivity}"
      }
    },
    {
      "nodeType": "sface_feature_encoder",
      "nodeName": "sface_encoder_{instanceId}",
      "parameters": {
        "model_path": "${SFACE_MODEL_PATH}"
      }
    },
    {
      "nodeType": "json_console_broker",
      "nodeName": "json_console_broker_{instanceId}",
      "parameters": {
        "broke_for": "${BROKE_FOR}"
      }
    },
    {
      "nodeType": "rtmp_des",
      "nodeName": "rtmp_des_{instanceId}",
      "parameters": {
        "rtmp_url": "${RTMP_URL}",
        "channel": "0"
      }
    }
  ]
}
```

**Create Instance Request**:
```json
{
  "name": "json_console_broker_demo",
  "group": "broker",
  "solution": "face_detection_with_json_broker",
  "persistent": true,
  "autoStart": true,
  "detectionSensitivity": "Medium",
  "additionalParams": {
    "FILE_PATH": "./cvedix_data/test_video/face.mp4",
    "MODEL_PATH": "./cvedix_data/models/face/face_detection_yunet_2022mar.onnx",
    "SFACE_MODEL_PATH": "./cvedix_data/models/face/face_recognition_sface_2021dec.onnx",
    "BROKE_FOR": "FACE",
    "RTMP_URL": "rtmp://server.com:1935/live/face"
  }
}
```

**Cách test**:
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @examples/instances/infer_nodes/example_json_console_broker.json
```

**Kiểm tra**: Xem console output để thấy JSON messages được in ra

---

### Case 10: JSON MQTT Broker

**Solution Config**:
```json
{
  "solutionId": "face_detection_with_mqtt_broker",
  "solutionName": "Face Detection with MQTT Broker",
  "pipeline": [
    {
      "nodeType": "file_src",
      "nodeName": "file_src_{instanceId}",
      "parameters": {
        "file_path": "${FILE_PATH}",
        "channel": "0",
        "resize_ratio": "0.6"
      }
    },
    {
      "nodeType": "yunet_face_detector",
      "nodeName": "face_detector_{instanceId}",
      "parameters": {
        "model_path": "${MODEL_PATH}",
        "score_threshold": "${detectionSensitivity}"
      }
    },
    {
      "nodeType": "sface_feature_encoder",
      "nodeName": "sface_encoder_{instanceId}",
      "parameters": {
        "model_path": "${SFACE_MODEL_PATH}"
      }
    },
    {
      "nodeType": "json_mqtt_broker",
      "nodeName": "json_mqtt_broker_{instanceId}",
      "parameters": {
        "broke_for": "${BROKE_FOR}"
      }
    },
    {
      "nodeType": "rtmp_des",
      "nodeName": "rtmp_des_{instanceId}",
      "parameters": {
        "rtmp_url": "${RTMP_URL}",
        "channel": "0"
      }
    }
  ]
}
```

**Create Instance Request**:
```json
{
  "name": "json_mqtt_broker_demo",
  "group": "broker",
  "solution": "face_detection_with_mqtt_broker",
  "persistent": true,
  "autoStart": true,
  "detectionSensitivity": "Medium",
  "additionalParams": {
    "FILE_PATH": "./cvedix_data/test_video/face.mp4",
    "MODEL_PATH": "./cvedix_data/models/face/face_detection_yunet_2022mar.onnx",
    "SFACE_MODEL_PATH": "./cvedix_data/models/face/face_recognition_sface_2021dec.onnx",
    "BROKE_FOR": "FACE",
    "MQTT_BROKER_URL": "localhost",
    "MQTT_PORT": "1883",
    "MQTT_TOPIC": "events",
    "RTMP_URL": "rtmp://server.com:1935/live/face"
  }
}
```

**Lưu ý**: 
- MQTT broker node yêu cầu custom callbacks (được cung cấp bởi SDK)
- Cần compile với `CVEDIX_WITH_MQTT=ON`

**Cách test**:
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @examples/instances/infer_nodes/example_json_mqtt_broker.json
```

**Kiểm tra**: Subscribe đến MQTT topic để nhận JSON messages

---

### Case 11: JSON Kafka Broker

**Solution Config**:
```json
{
  "solutionId": "face_detection_with_kafka_broker",
  "solutionName": "Face Detection with Kafka Broker",
  "pipeline": [
    {
      "nodeType": "file_src",
      "nodeName": "file_src_{instanceId}",
      "parameters": {
        "file_path": "${FILE_PATH}",
        "channel": "0",
        "resize_ratio": "0.6"
      }
    },
    {
      "nodeType": "yunet_face_detector",
      "nodeName": "face_detector_{instanceId}",
      "parameters": {
        "model_path": "${MODEL_PATH}",
        "score_threshold": "${detectionSensitivity}"
      }
    },
    {
      "nodeType": "sface_feature_encoder",
      "nodeName": "sface_encoder_{instanceId}",
      "parameters": {
        "model_path": "${SFACE_MODEL_PATH}"
      }
    },
    {
      "nodeType": "json_kafka_broker",
      "nodeName": "json_kafka_broker_{instanceId}",
      "parameters": {
        "kafka_servers": "${KAFKA_SERVERS}",
        "topic_name": "${KAFKA_TOPIC}",
        "broke_for": "${BROKE_FOR}"
      }
    },
    {
      "nodeType": "rtmp_des",
      "nodeName": "rtmp_des_{instanceId}",
      "parameters": {
        "rtmp_url": "${RTMP_URL}",
        "channel": "0"
      }
    }
  ]
}
```

**Create Instance Request**:
```json
{
  "name": "json_kafka_broker_demo",
  "group": "broker",
  "solution": "face_detection_with_kafka_broker",
  "persistent": true,
  "autoStart": true,
  "detectionSensitivity": "Medium",
  "additionalParams": {
    "FILE_PATH": "./cvedix_data/test_video/face.mp4",
    "MODEL_PATH": "./cvedix_data/models/face/face_detection_yunet_2022mar.onnx",
    "SFACE_MODEL_PATH": "./cvedix_data/models/face/face_recognition_sface_2021dec.onnx",
    "BROKE_FOR": "FACE",
    "KAFKA_SERVERS": "192.168.77.87:9092",
    "KAFKA_TOPIC": "videopipe_topic",
    "RTMP_URL": "rtmp://server.com:1935/live/face"
  }
}
```

**Lưu ý**: 
- Cần compile với `CVEDIX_WITH_KAFKA=ON`
- Cần Kafka server đang chạy

**Cách test**:
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @examples/instances/infer_nodes/example_json_kafka_broker.json
```

**Kiểm tra**: Consume từ Kafka topic để nhận JSON messages

---

### Case 12: XML File Broker

**Solution Config**:
```json
{
  "solutionId": "face_detection_with_xml_broker",
  "solutionName": "Face Detection with XML File Broker",
  "pipeline": [
    {
      "nodeType": "file_src",
      "nodeName": "file_src_{instanceId}",
      "parameters": {
        "file_path": "${FILE_PATH}",
        "channel": "0",
        "resize_ratio": "0.6"
      }
    },
    {
      "nodeType": "yunet_face_detector",
      "nodeName": "face_detector_{instanceId}",
      "parameters": {
        "model_path": "${MODEL_PATH}",
        "score_threshold": "${detectionSensitivity}"
      }
    },
    {
      "nodeType": "sface_feature_encoder",
      "nodeName": "sface_encoder_{instanceId}",
      "parameters": {
        "model_path": "${SFACE_MODEL_PATH}"
      }
    },
    {
      "nodeType": "xml_file_broker",
      "nodeName": "xml_file_broker_{instanceId}",
      "parameters": {
        "file_path": "${XML_FILE_PATH}",
        "broke_for": "${BROKE_FOR}"
      }
    },
    {
      "nodeType": "rtmp_des",
      "nodeName": "rtmp_des_{instanceId}",
      "parameters": {
        "rtmp_url": "${RTMP_URL}",
        "channel": "0"
      }
    }
  ]
}
```

**Create Instance Request**:
```json
{
  "name": "xml_file_broker_demo",
  "group": "broker",
  "solution": "face_detection_with_xml_broker",
  "persistent": true,
  "autoStart": true,
  "detectionSensitivity": "Medium",
  "additionalParams": {
    "FILE_PATH": "./cvedix_data/test_video/face.mp4",
    "MODEL_PATH": "./cvedix_data/models/face/face_detection_yunet_2022mar.onnx",
    "SFACE_MODEL_PATH": "./cvedix_data/models/face/face_recognition_sface_2021dec.onnx",
    "BROKE_FOR": "FACE",
    "XML_FILE_PATH": "./output/broker_output.xml",
    "RTMP_URL": "rtmp://server.com:1935/live/face"
  }
}
```

**Cách test**:
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @examples/instances/infer_nodes/example_xml_file_broker.json
```

**Kiểm tra**: Xem file XML tại đường dẫn đã chỉ định

---

### Case 13: XML Socket Broker

**Solution Config**:
```json
{
  "solutionId": "face_detection_with_xml_socket_broker",
  "solutionName": "Face Detection with XML Socket Broker",
  "pipeline": [
    {
      "nodeType": "file_src",
      "nodeName": "file_src_{instanceId}",
      "parameters": {
        "file_path": "${FILE_PATH}",
        "channel": "0",
        "resize_ratio": "0.6"
      }
    },
    {
      "nodeType": "yunet_face_detector",
      "nodeName": "face_detector_{instanceId}",
      "parameters": {
        "model_path": "${MODEL_PATH}",
        "score_threshold": "${detectionSensitivity}"
      }
    },
    {
      "nodeType": "sface_feature_encoder",
      "nodeName": "sface_encoder_{instanceId}",
      "parameters": {
        "model_path": "${SFACE_MODEL_PATH}"
      }
    },
    {
      "nodeType": "xml_socket_broker",
      "nodeName": "xml_socket_broker_{instanceId}",
      "parameters": {
        "des_ip": "${XML_SOCKET_IP}",
        "des_port": "${XML_SOCKET_PORT}",
        "broke_for": "${BROKE_FOR}"
      }
    },
    {
      "nodeType": "rtmp_des",
      "nodeName": "rtmp_des_{instanceId}",
      "parameters": {
        "rtmp_url": "${RTMP_URL}",
        "channel": "0"
      }
    }
  ]
}
```

**Create Instance Request**:
```json
{
  "name": "xml_socket_broker_demo",
  "group": "broker",
  "solution": "face_detection_with_xml_socket_broker",
  "persistent": true,
  "autoStart": true,
  "detectionSensitivity": "Medium",
  "additionalParams": {
    "FILE_PATH": "./cvedix_data/test_video/face.mp4",
    "MODEL_PATH": "./cvedix_data/models/face/face_detection_yunet_2022mar.onnx",
    "SFACE_MODEL_PATH": "./cvedix_data/models/face/face_recognition_sface_2021dec.onnx",
    "BROKE_FOR": "FACE",
    "XML_SOCKET_IP": "127.0.0.1",
    "XML_SOCKET_PORT": "8080",
    "RTMP_URL": "rtmp://server.com:1935/live/face"
  }
}
```

**Cách test**:
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @examples/instances/infer_nodes/example_xml_socket_broker.json
```

**Kiểm tra**: Tạo socket listener để nhận XML messages

---

### Case 14: Embeddings Socket Broker

**Solution Config**:
```json
{
  "solutionId": "face_detection_with_embeddings_socket_broker",
  "solutionName": "Face Detection with Embeddings Socket Broker",
  "pipeline": [
    {
      "nodeType": "file_src",
      "nodeName": "file_src_{instanceId}",
      "parameters": {
        "file_path": "${FILE_PATH}",
        "channel": "0",
        "resize_ratio": "0.6"
      }
    },
    {
      "nodeType": "yunet_face_detector",
      "nodeName": "face_detector_{instanceId}",
      "parameters": {
        "model_path": "${MODEL_PATH}",
        "score_threshold": "${detectionSensitivity}"
      }
    },
    {
      "nodeType": "sface_feature_encoder",
      "nodeName": "sface_encoder_{instanceId}",
      "parameters": {
        "model_path": "${SFACE_MODEL_PATH}"
      }
    },
    {
      "nodeType": "embeddings_socket_broker",
      "nodeName": "embeddings_socket_broker_{instanceId}",
      "parameters": {
        "des_ip": "${EMBEDDINGS_SOCKET_IP}",
        "des_port": "${EMBEDDINGS_SOCKET_PORT}",
        "cropped_dir": "${CROPPED_DIR}",
        "min_crop_width": "${MIN_CROP_WIDTH}",
        "min_crop_height": "${MIN_CROP_HEIGHT}",
        "only_for_tracked": "${ONLY_FOR_TRACKED}",
        "broke_for": "${BROKE_FOR}"
      }
    },
    {
      "nodeType": "rtmp_des",
      "nodeName": "rtmp_des_{instanceId}",
      "parameters": {
        "rtmp_url": "${RTMP_URL}",
        "channel": "0"
      }
    }
  ]
}
```

**Create Instance Request**:
```json
{
  "name": "embeddings_socket_broker_demo",
  "group": "broker",
  "solution": "face_detection_with_embeddings_socket_broker",
  "persistent": true,
  "autoStart": true,
  "detectionSensitivity": "Medium",
  "additionalParams": {
    "FILE_PATH": "./cvedix_data/test_video/face.mp4",
    "MODEL_PATH": "./cvedix_data/models/face/face_detection_yunet_2022mar.onnx",
    "SFACE_MODEL_PATH": "./cvedix_data/models/face/face_recognition_sface_2021dec.onnx",
    "BROKE_FOR": "FACE",
    "EMBEDDINGS_SOCKET_IP": "127.0.0.1",
    "EMBEDDINGS_SOCKET_PORT": "8080",
    "CROPPED_DIR": "cropped_images",
    "MIN_CROP_WIDTH": "50",
    "MIN_CROP_HEIGHT": "50",
    "ONLY_FOR_TRACKED": "false",
    "RTMP_URL": "rtmp://server.com:1935/live/face"
  }
}
```

**Cách test**:
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @examples/instances/infer_nodes/example_embeddings_socket_broker.json
```

---

### Case 15: Plate Socket Broker

**Solution Config**:
```json
{
  "solutionId": "plate_detection_with_socket_broker",
  "solutionName": "Plate Detection with Socket Broker",
  "pipeline": [
    {
      "nodeType": "file_src",
      "nodeName": "file_src_{instanceId}",
      "parameters": {
        "file_path": "${FILE_PATH}",
        "channel": "0",
        "resize_ratio": "0.5"
      }
    },
    {
      "nodeType": "trt_vehicle_plate_detector_v2",
      "nodeName": "plate_detector_{instanceId}",
      "parameters": {
        "det_model_path": "${DET_MODEL_PATH}",
        "rec_model_path": "${REC_MODEL_PATH}"
      }
    },
    {
      "nodeType": "plate_socket_broker",
      "nodeName": "plate_socket_broker_{instanceId}",
      "parameters": {
        "des_ip": "${PLATE_SOCKET_IP}",
        "des_port": "${PLATE_SOCKET_PORT}",
        "plates_dir": "${PLATES_DIR}",
        "min_crop_width": "${MIN_CROP_WIDTH}",
        "only_for_tracked": "${ONLY_FOR_TRACKED}",
        "broke_for": "${BROKE_FOR}"
      }
    },
    {
      "nodeType": "rtmp_des",
      "nodeName": "rtmp_des_{instanceId}",
      "parameters": {
        "rtmp_url": "${RTMP_URL}",
        "channel": "0"
      }
    }
  ]
}
```

**Create Instance Request**:
```json
{
  "name": "plate_socket_broker_demo",
  "group": "broker",
  "solution": "plate_detection_with_socket_broker",
  "persistent": true,
  "autoStart": true,
  "detectionSensitivity": "Medium",
  "additionalParams": {
    "FILE_PATH": "./cvedix_data/test_video/plate.mp4",
    "DET_MODEL_PATH": "./cvedix_data/models/trt/plate/det_v8.5.trt",
    "REC_MODEL_PATH": "./cvedix_data/models/trt/plate/rec_v8.5.trt",
    "BROKE_FOR": "NORMAL",
    "PLATE_SOCKET_IP": "127.0.0.1",
    "PLATE_SOCKET_PORT": "8080",
    "PLATES_DIR": "plate_images",
    "MIN_CROP_WIDTH": "100",
    "ONLY_FOR_TRACKED": "true",
    "RTMP_URL": "rtmp://server.com:1935/live/plate"
  }
}
```

**Cách test**:
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @examples/instances/infer_nodes/example_plate_socket_broker.json
```

---

## Pipeline Hoàn chỉnh

### Case 16: Face Detection với Multiple Brokers

**Solution Config**:
```json
{
  "solutionId": "face_detection_multi_broker",
  "solutionName": "Face Detection with Multiple Brokers",
  "pipeline": [
    {
      "nodeType": "rtsp_src",
      "nodeName": "rtsp_src_{instanceId}",
      "parameters": {
        "rtsp_url": "${RTSP_URL}",
        "channel": "0"
      }
    },
    {
      "nodeType": "yunet_face_detector",
      "nodeName": "face_detector_{instanceId}",
      "parameters": {
        "model_path": "${MODEL_PATH}",
        "score_threshold": "${detectionSensitivity}"
      }
    },
    {
      "nodeType": "sface_feature_encoder",
      "nodeName": "sface_encoder_{instanceId}",
      "parameters": {
        "model_path": "${SFACE_MODEL_PATH}"
      }
    },
    {
      "nodeType": "json_console_broker",
      "nodeName": "json_console_broker_{instanceId}",
      "parameters": {
        "broke_for": "${BROKE_FOR}"
      }
    },
    {
      "nodeType": "json_kafka_broker",
      "nodeName": "json_kafka_broker_{instanceId}",
      "parameters": {
        "kafka_servers": "${KAFKA_SERVERS}",
        "topic_name": "${KAFKA_TOPIC}",
        "broke_for": "${BROKE_FOR}"
      }
    },
    {
      "nodeType": "xml_file_broker",
      "nodeName": "xml_file_broker_{instanceId}",
      "parameters": {
        "file_path": "${XML_FILE_PATH}",
        "broke_for": "${BROKE_FOR}"
      }
    },
    {
      "nodeType": "rtmp_des",
      "nodeName": "rtmp_des_{instanceId}",
      "parameters": {
        "rtmp_url": "${RTMP_URL}",
        "channel": "0"
      }
    }
  ]
}
```

**Create Instance Request**:
```json
{
  "name": "face_multi_broker_demo",
  "group": "broker",
  "solution": "face_detection_multi_broker",
  "persistent": true,
  "autoStart": true,
  "detectionSensitivity": "Medium",
  "additionalParams": {
    "RTSP_URL": "rtsp://localhost:8554/stream",
    "MODEL_PATH": "./cvedix_data/models/face/face_detection_yunet_2022mar.onnx",
    "SFACE_MODEL_PATH": "./cvedix_data/models/face/face_recognition_sface_2021dec.onnx",
    "BROKE_FOR": "FACE",
    "KAFKA_SERVERS": "192.168.77.87:9092",
    "KAFKA_TOPIC": "videopipe_topic",
    "XML_FILE_PATH": "./output/broker_output.xml",
    "RTMP_URL": "rtmp://server.com:1935/live/face"
  }
}
```

---

## Example với Cấu hình Đầy đủ

### Case 17: Instance với Tất cả Các Fields

**Create Instance Request** (theo format từ instance_detail.txt):
```json
{
  "name": "CAMERA FACE",
  "group": "security",
  "solution": "securt",
  "persistent": true,
  "autoStart": false,
  "detectionSensitivity": "High",
  "detectorMode": "FullRegionInference",
  "performanceMode": "Balanced",
  "frameRateLimit": 15,
  "recommendedFrameRate": 5,
  "inputPixelLimit": 2000000,
  "metadataMode": true,
  "statisticsMode": true,
  "diagnosticsMode": false,
  "debugMode": false,
  "detectorModelFile": "pva_det_full_frame_512",
  "animalConfidenceThreshold": 0.3,
  "personConfidenceThreshold": 0.3,
  "vehicleConfidenceThreshold": 0.3,
  "faceConfidenceThreshold": 0.1,
  "licensePlateConfidenceThreshold": 0.1,
  "confThreshold": 0.2,
  "detectorThermalModelFile": "pva_det_mosaic_320",
  "additionalParams": {
    "RTSP_URL": "rtsp://localhost:8554/live/vanphong",
    "MODEL_PATH": "./cvedix_data/models/detection/pva_det_full_frame_512.trt"
  }
}
```

**Giải thích các Fields**:
- `detectorModelFile`: Tên model file, hệ thống sẽ tự tìm trong thư mục models
- `animalConfidenceThreshold: 0.3`: Chỉ báo động khi độ tin cậy phát hiện động vật >= 30%
- `faceConfidenceThreshold: 0.1`: Chỉ báo động khi độ tin cậy phát hiện khuôn mặt >= 10% (thấp để không bỏ sót)
- `frameRateLimit: 15`: Giới hạn xử lý tối đa 15 FPS
- `recommendedFrameRate: 5`: Hệ thống khuyến nghị 5 FPS để tối ưu tài nguyên
- `inputPixelLimit: 2000000`: Giới hạn input tối đa 2,000,000 pixels (khoảng Full HD 1080p)

**Cách test**:
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @examples/instances/example_full_config.json
```

---

## Kiểm tra và Testing

### Bước 1: Tạo Instance

```bash
# Sử dụng example file
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @examples/instances/infer_nodes/example_trt_yolov8_detector.json

# Hoặc sử dụng inline JSON
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d '{
    "name": "test_instance",
    "solution": "trt_yolov8_detection",
    "autoStart": true,
    "additionalParams": {
      "RTSP_URL": "rtsp://localhost:8554/stream",
      "MODEL_PATH": "./models/yolov8.engine"
    }
  }'
```

**Response thành công**:
```json
{
  "instanceId": "550e8400-e29b-41d4-a716-446655440000",
  "displayName": "test_instance",
  "solutionId": "trt_yolov8_detection",
  "running": true,
  "loaded": true
}
```

### Bước 2: Kiểm tra Trạng thái Instance

```bash
# Lấy thông tin instance
curl http://localhost:8080/v1/core/instance/{instanceId}

# List tất cả instances
curl http://localhost:8080/v1/core/instances
```

### Bước 3: Kiểm tra Logs

```bash
# Xem logs của instance
tail -f build/logs/general/$(date +%Y-%m-%d).log | grep {instanceId}

# Hoặc xem SDK output logs
tail -f build/logs/sdk_output/$(date +%Y-%m-%d).log
```

### Bước 4: Kiểm tra Output

**Cho Console Broker**:
- Xem console output để thấy JSON messages

**Cho Kafka Broker**:
```bash
# Consume từ Kafka topic
kafka-console-consumer --bootstrap-server 192.168.77.87:9092 \
  --topic videopipe_topic --from-beginning
```

**Cho MQTT Broker**:
```bash
# Subscribe đến MQTT topic
mosquitto_sub -h localhost \
  -p 1883 -t events
```

**Cho File Broker**:
```bash
# Xem file output
cat ./output/broker_output.xml
```

**Cho Socket Broker**:
- Tạo socket listener để nhận messages
- Hoặc sử dụng netcat: `nc -l 8080`

### Bước 5: Kiểm tra RTMP Stream

```bash
# Play RTMP stream
ffplay rtmp://server.com:1935/live/stream

# Hoặc sử dụng VLC
vlc rtmp://server.com:1935/live/stream
```

### Bước 6: Dừng Instance

```bash
# Stop instance
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/stop

# Delete instance
curl -X DELETE http://localhost:8080/v1/core/instance/{instanceId}
```

---

## Case Study: Face Detection với RTMP Streaming

### Pipeline

Pipeline bao gồm các node sau (theo thứ tự):
1. **File Source** - Đọc video từ file
2. **YuNet Face Detector** - Phát hiện khuôn mặt
3. **SFace Feature Encoder** - Mã hóa đặc trưng khuôn mặt
4. **Face OSD v2** - Vẽ overlay lên video
5. **RTMP Destination** - Stream video ra RTMP server

### Tạo Instance

```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d '{
    "name": "face_detection_demo_1",
    "group": "demo",
    "solution": "face_detection_rtmp",
    "persistent": true,
    "autoStart": true,
    "detectionSensitivity": "Low",
    "additionalParams": {
      "FILE_PATH": "/path/to/video/face.mp4",
      "RTMP_URL": "rtmp://server.com:1935/live/camera_demo_1",
      "MODEL_PATH": "/usr/share/cvedix/cvedix_data/models/face/face_detection_yunet_2022mar.onnx",
      "SFACE_MODEL_PATH": "/path/to/models/face_recognition_sface_2021dec.onnx",
      "RESIZE_RATIO": "1.0"
    }
  }'
```

### Các Tham Số

- `FILE_PATH`: Đường dẫn đến file video input (required)
- `RTMP_URL`: URL RTMP server để stream (required)
- `MODEL_PATH`: Đường dẫn đến model YuNet face detector (optional)
- `SFACE_MODEL_PATH`: Đường dẫn đến model SFace encoder (optional)
- `RESIZE_RATIO`: Tỷ lệ resize video (optional, default: `"0.5"`)

### Lưu Ý Quan Trọng

1. **RTMP URL**: RTMP node tự động thêm suffix `"_0"` vào stream key
2. **Model Paths**: Nếu không cung cấp, hệ thống sẽ tự động tìm model trong các thư mục mặc định
3. **Shape Mismatch Error**: Nếu gặp lỗi shape mismatch, re-encode video với resolution cố định:
   ```bash
   ffmpeg -i input.mp4 -vf "scale=320:240:force_original_aspect_ratio=decrease,pad=320:240:(ow-iw)/2:(oh-ih)/2" \
          -c:v libx264 -preset fast -crf 23 -c:a copy output_320x240.mp4
   ```
   Sau đó sử dụng `RESIZE_RATIO: "1.0"` trong `additionalParams`.

---

## Troubleshooting

### Lỗi: "Solution not found"
**Nguyên nhân**: Solution config chưa được đăng ký
**Giải pháp**: Tạo và đăng ký solution config trước khi tạo instance

### Lỗi: "Model file not found"
**Nguyên nhân**: Đường dẫn model không đúng
**Giải pháp**: 
- Kiểm tra đường dẫn model
- Sử dụng `MODEL_NAME` thay vì `MODEL_PATH` để tự động resolve
- Đặt model vào các thư mục được hỗ trợ:
  - `/usr/share/cvedix/cvedix_data/models/`
  - `./cvedix_data/models/`
  - Hoặc set `CVEDIX_DATA_ROOT` environment variable

### Lỗi: "Unknown node type"
**Nguyên nhân**: Node type không được hỗ trợ hoặc sai tên
**Giải pháp**: Kiểm tra danh sách node types được hỗ trợ trong `INFER_NODES_GUIDE.md`

### Lỗi: "Connection refused" (RTSP/Kafka/MQTT)
**Nguyên nhân**: Server không chạy hoặc firewall block
**Giải pháp**:
- Kiểm tra server đang chạy
- Kiểm tra firewall rules
- Kiểm tra network connectivity

### Lỗi: "Compilation flag not set"
**Nguyên nhân**: Node yêu cầu compile flag nhưng chưa được bật
**Giải pháp**: 
- Rebuild với flags tương ứng:
  - `CVEDIX_WITH_TRT=ON` cho TensorRT nodes
  - `CVEDIX_WITH_RKNN=ON` cho RKNN nodes
  - `CVEDIX_WITH_MQTT=ON` cho MQTT broker
  - `CVEDIX_WITH_KAFKA=ON` cho Kafka broker

---

## Checklist Kiểm tra

### Trước khi tạo instance:
- [ ] Solution config đã được đăng ký
- [ ] Model files đã có sẵn
- [ ] Input source đang hoạt động (RTSP stream, file, etc.)
- [ ] Output destination sẵn sàng (RTMP server, Kafka, MQTT, etc.)

### Sau khi tạo instance:
- [ ] Instance được tạo thành công (status 201)
- [ ] Instance ID được trả về
- [ ] Instance đang running (nếu autoStart=true)
- [ ] Logs không có lỗi
- [ ] Output được tạo (console, file, stream, etc.)

### Kiểm tra từng loại node:
- [ ] **Inference nodes**: Detections xuất hiện trong output
- [ ] **Source nodes**: Frames được đọc từ source
- [ ] **Broker nodes**: Messages được gửi đến broker
- [ ] **Destination nodes**: Stream/file được tạo

---

## Tài liệu Tham khảo

- `examples/instances/infer_nodes/INFER_NODES_GUIDE.md` - Hướng dẫn chi tiết các node types
- `docs/NODE_SUPPORT_STATUS.md` - Trạng thái hỗ trợ các nodes
- `docs/REQUIREMENT_CHECKLIST.md` - Checklist đáp ứng yêu cầu
- `examples/instances/infer_nodes/` - Tất cả example files

