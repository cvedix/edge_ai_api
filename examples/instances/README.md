# Instance Testing Guide - Tổng Hợp

## 📋 Tổng Quan

Thư mục này chứa toàn bộ tài liệu và examples để test các instances trong Edge AI API.

## 📁 Cấu Trúc Thư Mục

```
examples/instances/
├── README.md                    # File này
├── face_detection/              # Face Detection instances
│   ├── README.md
│   ├── test_file_source.json
│   ├── test_rtsp_source.json
│   ├── test_rtmp_output.json
│   ├── test_mqtt_events.json
│   └── report_body_example.json
├── ba_crossline/                # Behavior Analysis Crossline instances
│   ├── README.md
│   ├── test_file_source_mqtt.json
│   ├── test_rtsp_source_rtmp_mqtt.json
│   ├── test_rtsp_source_mqtt_only.json
│   ├── test_rtsp_source_rtmp_only.json
│   ├── test_rtmp_output_only.json
│   └── report_body_example.json
├── ba_jam/                      # Behavior Analysis Jam instances
│   ├── README.md
│   ├── example_ba_jam_rtmp.json
│   ├── example_ba_jam_file_mqtt.json
│   ├── test_rtsp_source_rtmp_mqtt.json
│   └── report_body_example.json
├── ba_stop/                     # Behavior Analysis Stop Line instances
│   ├── README.md
│   ├── example_ba_stop_rtmp.json
│   ├── example_ba_stop_file_mqtt.json
│   ├── test_rtsp_source_rtmp_mqtt.json
│   └── report_body_example.json
├── mask_rcnn/                   # MaskRCNN instances
│   ├── README.md
│   ├── test_file_source.json
│   ├── test_rtmp_output.json
│   └── report_body_example.json
├── rtmp_mqtt/                   # RTMP/MQTT integration guide
│   └── README.md
├── create/                      # Create examples (legacy)
├── update/                      # Update examples (legacy)
├── scripts/                     # Utility scripts
└── tests/                       # Test files
```

## 🎯 Các Loại Instance

### 1. Face Detection (`face_detection/`)

**Solutions:**
- `face_detection`: RTSP source + face detection
- `face_detection_file`: File source + face detection
- `face_detection_rtmp`: File source + RTMP output

**Tính năng:**
- Phát hiện khuôn mặt với YuNet
- Face recognition với SFace (tùy chọn)
- Face tracking với SORT (tùy chọn)
- RTMP streaming (tùy chọn)
- MQTT events (tùy chọn)

**Xem:** [face_detection/README.md](./face_detection/README.md)

### 2. Behavior Analysis Crossline (`ba_crossline/`)

**Solutions:**
- `ba_crossline_with_mqtt`: BA crossline với MQTT events

**Tính năng:**
- Phát hiện phương tiện với YOLO
- Tracking với SORT
- Đếm phương tiện đi qua line
- RTMP streaming (tùy chọn)
- MQTT events khi có phương tiện đi qua

**Xem:** [ba_crossline/README.md](./ba_crossline/README.md)

### 3. Behavior Analysis Jam (`ba_jam/`)

**Solutions:**
- `ba_jam`: Phát hiện "jam" (vehicle stopped) trong các zone định nghĩa

**Tính năng:**
- Phát hiện phương tiện dừng lâu trong zone
- Tracking với SORT
- MQTT events khi phát hiện jam
- RTMP streaming (tùy chọn)

**Xem:** [ba_jam/README.md](./ba_jam/README.md)

### 4. Behavior Analysis Stop Line (`ba_stop/`)

**Solutions:**
- `ba_stop`: Phát hiện dừng (stop) tại các stop-line định nghĩa

**Tính năng:**
- Phát hiện phương tiện dừng tại stop-line
- Tracking với SORT
- MQTT events khi phát hiện stop
- RTMP streaming (tùy chọn)

**Xem:** [ba_stop/README.md](./ba_stop/README.md)

### 5. MaskRCNN (`mask_rcnn/`)

**Solutions:**
- `mask_rcnn_detection`: File source + instance segmentation
- `mask_rcnn_rtmp`: File source + RTMP output

**Tính năng:**
- Instance segmentation với MaskRCNN
- Phát hiện 80 COCO classes
- Tạo mask cho từng đối tượng
- RTMP streaming (tùy chọn)

**Xem:** [mask_rcnn/README.md](./mask_rcnn/README.md)

## 📝 Quick Start Guide

### 1. Chọn Instance Type

Xem các thư mục con để chọn instance phù hợp:
- `face_detection/`: Phát hiện khuôn mặt
- `ba_crossline/`: Đếm phương tiện đi qua line
- `mask_rcnn/`: Instance segmentation

### 2. Chọn Test File

Mỗi thư mục có các file test JSON:
- `test_file_source.json`: Test với file video
- `test_rtsp_source.json`: Test với RTSP stream
- `test_rtmp_output.json`: Test với RTMP output
- `test_mqtt_events.json`: Test với MQTT events

### 3. Tạo Instance

```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @face_detection/test_file_source.json
```

### 4. Start Instance

```bash
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/start
```

### 5. Kiểm Tra Kết Quả

```bash
# Kiểm tra status
curl http://localhost:8080/v1/core/instance/{instanceId}

# Kiểm tra statistics
curl http://localhost:8080/v1/core/instance/{instanceId}/statistics

# Subscribe MQTT (nếu có)
mosquitto_sub -h localhost -t events -v
```

## 🔧 Common Parameters

### File Source
```json
{
  "FILE_PATH": "/path/to/video.mp4",
  "RESIZE_RATIO": "1.0"
}
```

### RTSP Source
```json
{
  "RTSP_SRC_URL": "rtsp://server:8554/stream",
  "RESIZE_RATIO": "1.0",
  "GST_DECODER_NAME": "avdec_h264",
  "SKIP_INTERVAL": "0",
  "CODEC_TYPE": "h264"
}
```

### RTMP Output
```json
{
  "RTMP_URL": "rtmp://server:1935/live/stream_key",
  "ENABLE_SCREEN_DES": "false"
}
```

### MQTT Events
```json
{
  "MQTT_BROKER_URL": "localhost",
  "MQTT_PORT": "1883",
  "MQTT_TOPIC": "events",
  "MQTT_USERNAME": "",
  "MQTT_PASSWORD": "",
  "MQTT_RATE_LIMIT_MS": "1000",
  "BROKE_FOR": "FACE"  // hoặc "NORMAL"
}
```

## 📊 Report Body Structure

Mỗi instance có file `report_body_example.json` mô tả cấu trúc report body từ MQTT events.

**Common fields:**
- `events[]`: Array of events
- `frame_id`: Frame number
- `frame_time`: Timestamp in seconds
- `system_date`: ISO date string
- `system_timestamp`: Unix timestamp in milliseconds

**Event fields:**
- `id`: UUID
- `instance_id`: Instance name
- `type`: Event type (face_detected, crossline_enter, object_detected, etc.)
- `label`: Human-readable label
- `best_thumbnail`: Cropped image with position
- `extra`: Additional data (bbox, class, track_id, etc.)
- `tracks[]`: Array of tracked objects

## 🔍 Troubleshooting

### Lỗi: Instance không start

```bash
# Kiểm tra logs
tail -f /opt/edge_ai_api/logs/edge_ai_api.log

# Kiểm tra status
curl http://localhost:8080/v1/core/instance/{instanceId}
```

### Lỗi: Model không tìm thấy

- Kiểm tra đường dẫn model trong JSON config
- Đảm bảo model files tồn tại
- Kiểm tra permissions

### Lỗi: RTSP/RTMP connection failed

- Kiểm tra network connectivity
- Kiểm tra server đang chạy
- Kiểm tra firewall rules
- Test với ffmpeg/ffplay

### Lỗi: MQTT connection failed

- Kiểm tra MQTT broker đang chạy
- Kiểm tra credentials
- Test với mosquitto_sub

## 📚 Tài Liệu Tham Khảo

- API Documentation: `docs/INSTANCE_GUIDE.md`
- Solutions Reference: `docs/DEFAULT_SOLUTIONS_REFERENCE.md`
- Sample Code: `sample/README.md`
- Testing Guides:
  - `sample/YUNET_TESTING_GUIDE.md`
  - `sample/MASKRCNN_TESTING_GUIDE.md`
  - `sample/SELECTED_SAMPLES_RTMP_MQTT.md`

## 🗂️ Legacy Files

Các thư mục sau được giữ lại cho tương thích ngược:
- `create/`: Create examples (có thể sử dụng)
- `update/`: Update examples (có thể sử dụng)
- `scripts/`: Utility scripts
- `tests/`: Test files

Các file JSON ở root (`example_*.json`) là các examples cũ. Chúng đã được tổ chức lại vào các thư mục con tương ứng.

**Khuyến nghị:** Sử dụng files trong các thư mục con (`face_detection/`, `ba_crossline/`, `mask_rcnn/`) thay vì các file ở root.

Xem [LEGACY_FILES.md](./LEGACY_FILES.md) để biết mapping chi tiết.
