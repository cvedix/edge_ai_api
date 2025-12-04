# Instance Configuration Fields - Chi tiết

Tài liệu này mô tả chi tiết tất cả các fields có thể sử dụng khi tạo instance qua API `POST /v1/core/instance`.

## Cấu trúc Request

### Fields Cơ bản

| Field | Type | Required | Default | Mô tả |
|-------|------|----------|---------|-------|
| `name` | string | ✅ Yes | - | Tên instance, pattern: `^[A-Za-z0-9 -_]+$` |
| `group` | string | ❌ No | "" | Nhóm instance, pattern: `^[A-Za-z0-9 -_]+$` |
| `solution` | string | ❌ No | "" | Solution ID đã được đăng ký |
| `persistent` | boolean | ❌ No | false | Lưu instance vào file để load lại khi restart |
| `autoStart` | boolean | ❌ No | false | Tự động khởi động pipeline khi tạo instance |

### Fields Detector (Nhận diện AI)

| Field | Type | Required | Default | Mô tả |
|-------|------|----------|---------|-------|
| `detectionSensitivity` | string | ❌ No | "Low" | Độ nhạy phát hiện: "Low", "Medium", "High", "Normal", "Slow" |
| `detectorMode` | string | ❌ No | "SmartDetection" | Chế độ detector: "SmartDetection", "FullRegionInference", "MosaicInference" |
| `detectorModelFile` | string | ❌ No | "" | Tên model file (ví dụ: "pva_det_full_frame_512") - hệ thống tự tìm trong thư mục models |
| `animalConfidenceThreshold` | double | ❌ No | 0.0 | Ngưỡng độ tin cậy cho động vật (0.0-1.0) |
| `personConfidenceThreshold` | double | ❌ No | 0.0 | Ngưỡng độ tin cậy cho người (0.0-1.0) |
| `vehicleConfidenceThreshold` | double | ❌ No | 0.0 | Ngưỡng độ tin cậy cho xe cộ (0.0-1.0) |
| `faceConfidenceThreshold` | double | ❌ No | 0.0 | Ngưỡng độ tin cậy cho khuôn mặt (0.0-1.0) |
| `licensePlateConfidenceThreshold` | double | ❌ No | 0.0 | Ngưỡng độ tin cậy cho biển số (0.0-1.0) |
| `confThreshold` | double | ❌ No | 0.0 | Ngưỡng độ tin cậy chung (0.0-1.0) |
| `detectorThermalModelFile` | string | ❌ No | "" | Tên model file cho camera nhiệt (ví dụ: "pva_det_mosaic_320") |

**Ví dụ Detector Config**:
```json
{
  "detectionSensitivity": "High",
  "detectorMode": "FullRegionInference",
  "detectorModelFile": "pva_det_full_frame_512",
  "animalConfidenceThreshold": 0.3,
  "personConfidenceThreshold": 0.3,
  "vehicleConfidenceThreshold": 0.3,
  "faceConfidenceThreshold": 0.1,
  "licensePlateConfidenceThreshold": 0.1,
  "confThreshold": 0.2
}
```

### Fields Performance (Hiệu năng)

| Field | Type | Required | Default | Mô tả |
|-------|------|----------|---------|-------|
| `performanceMode` | string | ❌ No | "Balanced" | Chế độ hiệu năng: "Balanced", "Performance", "Saved" |
| `frameRateLimit` | integer | ❌ No | 0 | Giới hạn tốc độ khung hình (FPS), 0 = không giới hạn |
| `recommendedFrameRate` | integer | ❌ No | 0 | Tốc độ khung hình được khuyến nghị (FPS) |
| `inputPixelLimit` | integer | ❌ No | 0 | Giới hạn số pixel đầu vào (ví dụ: 2000000 cho Full HD 1080p) |

**Ví dụ Performance Config**:
```json
{
  "performanceMode": "Balanced",
  "frameRateLimit": 15,
  "recommendedFrameRate": 5,
  "inputPixelLimit": 2000000
}
```

### Fields SolutionManager (Quản lý Solution)

| Field | Type | Required | Default | Mô tả |
|-------|------|----------|---------|-------|
| `metadataMode` | boolean | ❌ No | false | Gửi metadata về vật thể nhận diện |
| `statisticsMode` | boolean | ❌ No | false | Chạy thống kê |
| `diagnosticsMode` | boolean | ❌ No | false | Gửi dữ liệu chẩn đoán lỗi |
| `debugMode` | boolean | ❌ No | false | Bật chế độ debug |

**Ví dụ SolutionManager Config**:
```json
{
  "metadataMode": true,
  "statisticsMode": true,
  "diagnosticsMode": false,
  "debugMode": false
}
```

### Fields Input (Đầu vào)

| Field | Type | Required | Default | Mô tả |
|-------|------|----------|---------|-------|
| `inputOrientation` | integer | ❌ No | 0 | Hướng xoay input (0-3) |
| `inputPixelLimit` | integer | ❌ No | 0 | Giới hạn số pixel đầu vào |

### Fields Additional Parameters

| Field | Type | Required | Default | Mô tả |
|-------|------|----------|---------|-------|
| `additionalParams` | object | ❌ No | {} | Các tham số cho nodes |

**Các tham số phổ biến trong `additionalParams`**:

| Key | Type | Mô tả |
|-----|------|-------|
| `RTSP_URL` | string | URL RTSP stream (ví dụ: "rtsp://localhost:8554/stream") |
| `FILE_PATH` | string | Đường dẫn file video (ví dụ: "./cvedix_data/test_video/face.mp4") |
| `RTMP_URL` | string | URL RTMP destination (ví dụ: "rtmp://server.com:1935/live/stream") |
| `MODEL_PATH` | string | Đường dẫn đầy đủ đến model file |
| `MODEL_NAME` | string | Tên model (hệ thống tự tìm trong thư mục models) |
| `SFACE_MODEL_PATH` | string | Đường dẫn model SFace encoder |
| `LABELS_PATH` | string | Đường dẫn file labels |
| `KAFKA_SERVERS` | string | Kafka server address (ví dụ: "192.168.77.87:9092") |
| `KAFKA_TOPIC` | string | Kafka topic name |
| `MQTT_BROKER_URL` | string | MQTT broker URL |
| `MQTT_PORT` | string | MQTT broker port |
| `MQTT_TOPIC` | string | MQTT topic name |

## Ví dụ Request Đầy đủ

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

## Lưu ý

1. **Model File Resolution**: Khi sử dụng `detectorModelFile` hoặc `MODEL_NAME`, hệ thống sẽ tự động tìm model trong các thư mục:
   - `/usr/share/cvedix/cvedix_data/models/`
   - `./cvedix_data/models/`
   - Hoặc thư mục được chỉ định bởi `CVEDIX_DATA_ROOT` environment variable

2. **Confidence Thresholds**: Các giá trị từ 0.0 đến 1.0:
   - 0.0 = Không sử dụng threshold này
   - 0.1 = Rất thấp (không bỏ sót nhưng có thể báo sai)
   - 0.3 = Trung bình (cân bằng giữa độ chính xác và không bỏ sót)
   - 0.5+ = Cao (chỉ báo khi rất chắc chắn)

3. **Performance Mode**:
   - `Balanced`: Cân bằng giữa độ chính xác và tốc độ
   - `Performance`: Ưu tiên tốc độ
   - `Saved`: Ưu tiên tiết kiệm tài nguyên

4. **Frame Rate Limits**:
   - `frameRateLimit`: Giới hạn cứng, không vượt quá giá trị này
   - `recommendedFrameRate`: Khuyến nghị, hệ thống có thể điều chỉnh để tối ưu

## Mapping với Format Cũ

Các fields mới được map từ format cũ như sau:

| Format Cũ | Format Mới |
|-----------|------------|
| `Detector.model_file` | `detectorModelFile` |
| `Detector.animal_confidence_threshold` | `animalConfidenceThreshold` |
| `Detector.person_confidence_threshold` | `personConfidenceThreshold` |
| `Detector.vehicle_confidence_threshold` | `vehicleConfidenceThreshold` |
| `Detector.face_confidence_threshold` | `faceConfidenceThreshold` |
| `Detector.license_plate_confidence_threshold` | `licensePlateConfidenceThreshold` |
| `Detector.conf_threshold` | `confThreshold` |
| `Detector.current_preset` | `detectorMode` |
| `Detector.current_sensitivity_preset` | `detectionSensitivity` |
| `DetectorThermal.model_file` | `detectorThermalModelFile` |
| `PerformanceMode.current_preset` | `performanceMode` |
| `SolutionManager.frame_rate_limit` | `frameRateLimit` |
| `SolutionManager.recommended_frame_rate` | `recommendedFrameRate` |
| `SolutionManager.input_pixel_limit` | `inputPixelLimit` |
| `SolutionManager.send_metadata` | `metadataMode` |
| `SolutionManager.run_statistics` | `statisticsMode` |
| `SolutionManager.send_diagnostics` | `diagnosticsMode` |
| `SolutionManager.enable_debug` | `debugMode` |

