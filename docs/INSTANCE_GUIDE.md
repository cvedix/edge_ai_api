# Hướng Dẫn Tạo và Cập Nhật Instance

Tài liệu này hướng dẫn cách tạo và cập nhật AI instances trong Edge AI API Server.

## Mục Lục

1. [Tổng Quan](#tổng-quan)
2. [Tạo Instance](#tạo-instance)
3. [Cập Nhật Instance](#cập-nhật-instance)
4. [Các Loại Nodes](#các-loại-nodes)
5. [Ví Dụ Pipeline](#ví-dụ-pipeline)
6. [Troubleshooting](#troubleshooting)

---

## Tổng Quan

### Pipeline là gì?

**Pipeline** là một chuỗi các **nodes** được kết nối với nhau để xử lý dữ liệu video/ảnh:

```
[RTSP Source] → [Face Detector] → [Feature Encoder] → [JSON Broker] → [RTMP Destination]
```

**Các thành phần Pipeline:**
1. **Source Nodes** (Input): RTSP stream, File video, Image, RTMP, UDP, Application frames
2. **Inference Nodes** (Processing): Object detection, Face detection, Classification, Segmentation
3. **OSD Nodes** (Overlay): Face OSD, Object OSD, Text overlay
4. **Broker Nodes** (Output Messages): JSON (Console, MQTT, Kafka), XML (File, Socket)
5. **Destination Nodes** (Output Stream/File): RTMP stream, File video

### Instance là gì?

**Instance** là một phiên bản cụ thể của pipeline với các tham số cụ thể. Một solution config có thể được sử dụng để tạo nhiều instances khác nhau.

**Ví dụ:**
```
Solution: "face_detection_pipeline"
  ├─ Instance 1: Camera 1 → Face Detection → MQTT Broker
  ├─ Instance 2: Camera 2 → Face Detection → Kafka Broker  
  └─ Instance 3: Video File → Face Detection → XML File
```

### Workflow

```
1. Tạo Solution Config (nếu chưa có)
   ↓
2. Gọi API POST /v1/core/instance
   ↓
3. Validate parameters
   ↓
4. Build Pipeline → Tạo các nodes
   ↓
5. Create Instance → Lưu instance info
   ↓
6. Auto Start (nếu enabled) → Start pipeline
   ↓
7. Return Instance ID
```

---

## Tạo Instance

### Endpoint

```
POST /v1/core/instance
```

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
    "RTSP_URL": "rtsp://localhost:8554/stream",
    "MODEL_PATH": "/path/to/model"
  }
}
```

### Các Field Quan trọng

**Fields Cơ bản:**
- `name` (required): Tên instance, pattern: `^[A-Za-z0-9 -_]+$`
- `group` (optional): Nhóm instance
- `solution` (optional): Solution ID đã được đăng ký
- `persistent` (optional): Lưu instance vào file để load lại khi restart (default: false)
- `autoStart` (optional): Tự động khởi động pipeline khi tạo instance (default: false)

**Fields Detector:**
- `detectionSensitivity`: "Low", "Medium", "High", "Normal", "Slow" (default: "Low")
- `detectorMode`: "SmartDetection", "FullRegionInference", "MosaicInference" (default: "SmartDetection")
- `detectorModelFile`: Tên model file (ví dụ: "pva_det_full_frame_512")
- `animalConfidenceThreshold`, `personConfidenceThreshold`, `vehicleConfidenceThreshold`, etc. (0.0-1.0)

**Fields Performance:**
- `performanceMode`: "Balanced", "Performance", "Saved" (default: "Balanced")
- `frameRateLimit`: Giới hạn tốc độ khung hình (FPS)
- `inputPixelLimit`: Giới hạn số pixel đầu vào

**Fields Additional Parameters:**
- `additionalParams`: Các tham số cho nodes (MODEL_PATH, RTSP_URL, FILE_PATH, RTMP_URL, MQTT_BROKER_URL, etc.)

### Ví Dụ: Tạo Instance với RTSP Source và TensorRT Detector

```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d '{
    "name": "camera_1_face_detection",
    "solution": "face_detection",
    "autoStart": true,
    "detectionSensitivity": "Medium",
    "additionalParams": {
      "RTSP_URL": "rtsp://192.168.1.100:554/stream",
      "MODEL_PATH": "/opt/models/face_detection.trt",
      "RTMP_URL": "rtmp://localhost:1935/live/stream1"
    }
  }'
```

---

## Cập Nhật Instance

### Endpoint

```
PUT /v1/core/instances/{instanceId}
```

### Cách 1: Update Từng Field (CamelCase)

**Request Body:**
```json
{
  "name": "New Display Name",
  "autoStart": true,
  "frameRateLimit": 20,
  "metadataMode": true,
  "detectorMode": "SmartDetection",
  "detectionSensitivity": "High",
  "additionalParams": {
    "RTSP_URL": "rtsp://localhost:8554/stream",
    "MODEL_PATH": "/path/to/model"
  }
}
```

**Các Fields Hỗ Trợ:**
- `name`, `group`, `persistent`, `autoStart`, `autoRestart`
- `frameRateLimit`, `metadataMode`, `statisticsMode`, `diagnosticsMode`, `debugMode`
- `detectorMode`, `detectionSensitivity`, `movementSensitivity`
- `sensorModality`, `inputOrientation`, `inputPixelLimit`
- `additionalParams`

### Cách 2: Update Trực Tiếp Từ JSON Config (PascalCase)

Bạn có thể gửi **toàn bộ hoặc một phần** của JSON config theo format PascalCase:

```json
{
  "DisplayName": "face_detection_demo_1",
  "AutoStart": true,
  "Detector": {
    "current_preset": "SmartDetection",
    "current_sensitivity_preset": "Low",
    "model_file": "pva_det_full_frame_512",
    "person_confidence_threshold": 0.3
  },
  "Input": {
    "media_type": "IP Camera",
    "uri": "gstreamer:///urisourcebin uri=rtsp://localhost:8554/stream ! decodebin ! videoconvert ! video/x-raw, format=NV12 ! appsink drop=true name=cvdsink"
  },
  "Output": {
    "JSONExport": {
      "enabled": false
    },
    "handlers": {
      "rtsp:--0.0.0.0:8554-stream1": {
        "enabled": true,
        "uri": "rtsp://localhost:8554/stream"
      }
    }
  },
  "SolutionManager": {
    "frame_rate_limit": 15,
    "send_metadata": false,
    "run_statistics": false
  }
}
```

### Ví Dụ Update

**Update DisplayName và AutoStart:**
```bash
curl -X PUT http://localhost:8080/v1/core/instances/{instanceId} \
  -H "Content-Type: application/json" \
  -d '{
    "DisplayName": "Updated Camera Name",
    "AutoStart": false
  }'
```

**Update Detector Settings:**
```bash
curl -X PUT http://localhost:8080/v1/core/instances/{instanceId} \
  -H "Content-Type: application/json" \
  -d '{
    "Detector": {
      "current_preset": "SmartDetection",
      "current_sensitivity_preset": "High",
      "person_confidence_threshold": 0.5
    }
  }'
```

---

## Các Loại Nodes

### Source Nodes (Input) - 6 nodes ✅

- `rtsp_src`: Nhận video từ RTSP stream
- `file_src`: Đọc video từ file
- `app_src`: Nhận frames từ application code
- `image_src`: Đọc ảnh từ file hoặc UDP
- `rtmp_src`: Nhận video từ RTMP stream
- `udp_src`: Nhận video từ UDP stream

### Inference Nodes (Detector/Processing) - 23 nodes ✅

**TensorRT Nodes:**
- `trt_yolov8_detector`, `trt_yolov8_seg_detector`, `trt_yolov8_pose_detector`
- `trt_vehicle_detector`, `trt_vehicle_plate_detector`, `trt_vehicle_color_classifier`, etc.

**RKNN Nodes:**
- `rknn_yolov8_detector`, `rknn_face_detector`

**Other Inference Nodes:**
- `yunet_face_detector`, `sface_feature_encoder`, `yolo_detector`
- `mask_rcnn_detector`, `openpose_detector`, `classifier`, etc.

### Broker Nodes (Output Messages) - 12 nodes ✅

- `json_console_broker`, `json_enhanced_console_broker`
- `json_mqtt_broker`, `json_kafka_broker`
- `xml_file_broker`, `xml_socket_broker`
- `msg_broker`, `ba_socket_broker`, etc.

### Destination Nodes (Output Stream/File) - 2 nodes ✅

- `file_des`: Lưu video vào file
- `rtmp_des`: Gửi video qua RTMP stream

### Tổng Kết

| Loại Node | Số lượng | Trạng thái |
|-----------|----------|------------|
| **Source Nodes** | 6 | ✅ 100% |
| **Inference Nodes** | 23 | ✅ 100% |
| **Broker Nodes** | 12 | ✅ 100% |
| **Destination Nodes** | 2 | ✅ |
| **Tổng** | **43 nodes** | ✅ **Đầy đủ** |

---

## Ví Dụ Pipeline

### Pipeline 1: RTSP → Face Detection → MQTT

**Solution Config:**
```json
{
  "solutionId": "face_detection_mqtt",
  "pipeline": [
    {"nodeType": "rtsp_src", "nodeName": "rtsp_src_0"},
    {"nodeType": "yunet_face_detector", "nodeName": "face_detector_0"},
    {"nodeType": "json_mqtt_broker", "nodeName": "mqtt_broker_0"}
  ]
}
```

**Create Instance:**
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d '{
    "name": "camera_1",
    "solution": "face_detection_mqtt",
    "autoStart": true,
    "additionalParams": {
      "RTSP_URL": "rtsp://192.168.1.100:554/stream",
      "MQTT_BROKER_URL": "tcp://localhost:1883",
      "MQTT_TOPIC": "face_detection/events"
    }
  }'
```

### Pipeline 2: File → Object Detection → RTMP

**Solution Config:**
```json
{
  "solutionId": "object_detection_rtmp",
  "pipeline": [
    {"nodeType": "file_src", "nodeName": "file_src_0"},
    {"nodeType": "trt_yolov8_detector", "nodeName": "detector_0"},
    {"nodeType": "rtmp_des", "nodeName": "rtmp_des_0"}
  ]
}
```

**Create Instance:**
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d '{
    "name": "video_processing",
    "solution": "object_detection_rtmp",
    "autoStart": true,
    "additionalParams": {
      "FILE_PATH": "/path/to/video.mp4",
      "MODEL_PATH": "/opt/models/yolov8.trt",
      "RTMP_URL": "rtmp://localhost:1935/live/stream"
    }
  }'
```

---

## Troubleshooting

### Instance không start

**Nguyên nhân có thể:**
- Solution config không tồn tại hoặc không hợp lệ
- Parameters thiếu hoặc sai (RTSP_URL, MODEL_PATH, etc.)
- Model file không tồn tại
- Source không khả dụng (RTSP stream down, file không tồn tại)

**Giải pháp:**
- Kiểm tra solution config: `GET /v1/core/solutions/{solutionId}`
- Kiểm tra logs: `GET /v1/core/logs/instance?tail=100`
- Verify parameters trong `additionalParams`
- Kiểm tra model file path và source availability

### Instance start nhưng không xử lý frames

**Nguyên nhân có thể:**
- Source không có data (RTSP stream down, file empty)
- Pipeline configuration sai
- Model không tương thích với input format

**Giải pháp:**
- Kiểm tra source: Test RTSP URL hoặc verify file path
- Kiểm tra statistics: `GET /v1/core/instance/{instanceId}/statistics`
- Xem SDK output logs: `GET /v1/core/logs/sdk_output?tail=100`

### Update instance không có hiệu lực

**Nguyên nhân:**
- Instance đang chạy, cần restart để áp dụng thay đổi

**Giải pháp:**
```bash
# Stop instance
POST /v1/core/instances/{instanceId}/stop

# Update instance
PUT /v1/core/instances/{instanceId} ...

# Start instance
POST /v1/core/instances/{instanceId}/start
```

---

## Related Documentation

- [API_REFERENCE.md](API_REFERENCE.md) - Tài liệu tham khảo API đầy đủ
- [GETTING_STARTED.md](GETTING_STARTED.md) - Hướng dẫn khởi động server
- [DEVELOPMENT_SETUP.md](DEVELOPMENT_SETUP.md) - Hướng dẫn setup môi trường
- [Swagger UI](http://localhost:8080/swagger) - Interactive API documentation

