# Face Detection Instance - Hướng Dẫn Test

## 📋 Tổng Quan

Instance này thực hiện phát hiện khuôn mặt sử dụng mô hình YuNet, có thể kết hợp với face recognition (SFace), tracking (SORT), và streaming (RTMP).

## 🎯 Tính Năng

- ✅ Phát hiện khuôn mặt với YuNet detector
- ✅ Face recognition với SFace encoder (tùy chọn)
- ✅ Face tracking với SORT tracker (tùy chọn)
- ✅ RTMP streaming output (tùy chọn)
- ✅ Screen display output
- ✅ MQTT event publishing (tùy chọn)

## 📁 Cấu Trúc Files

```
face_detection/
├── README.md                    # File này
├── solution.json                # Solution config (nếu cần tạo custom)
├── test_file_source.json        # Test với file source
├── test_rtsp_source.json        # Test với RTSP source
├── test_rtmp_output.json        # Test với RTMP output
├── test_mqtt_events.json        # Test với MQTT events
└── report_body_example.json     # Ví dụ report body từ MQTT
```

## 🔧 Solution Config

### Solution ID: `face_detection`

**Mặc định có sẵn** trong hệ thống, không cần tạo solution config.

**Pipeline:**
```
RTSP Source → YuNet Detector → SFace Encoder → Face OSD → Screen Display
```

### Solution ID: `face_detection_file`

**Mặc định có sẵn** trong hệ thống.

**Pipeline:**
```
File Source → YuNet Detector → SFace Encoder → Face OSD → Screen Display
```

### Solution ID: `face_detection_rtmp`

**Mặc định có sẵn** trong hệ thống.

**Pipeline:**
```
File Source → YuNet Detector → SFace Encoder → Face OSD → Split → [Screen | RTMP]
```

## 📝 Manual Testing Guide

### 1. Test Cơ Bản với File Source

**Bước 1:** Tạo instance
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @face_detection/test_file_source.json
```

**Bước 2:** Kiểm tra status
```bash
curl http://localhost:8080/v1/core/instance/{instanceId}
```

**Bước 3:** Start instance
```bash
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/start
```

**Bước 4:** Kiểm tra statistics
```bash
curl http://localhost:8080/v1/core/instance/{instanceId}/statistics
```

**Bước 5:** Stop instance
```bash
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/stop
```

### 2. Test với RTSP Source

**Yêu cầu:**
- RTSP camera hoặc RTSP stream server
- RTSP URL hợp lệ

**Các bước tương tự như trên, sử dụng file:**
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @face_detection/test_rtsp_source.json
```

### 3. Test với RTMP Output

**Yêu cầu:**
- RTMP server (nginx-rtmp hoặc tương tự)
- RTMP URL hợp lệ

**Các bước:**
```bash
# Tạo instance với RTMP output
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @face_detection/test_rtmp_output.json

# Kiểm tra RTMP stream
ffplay rtmp://your-server:1935/live/stream_key
```

### 4. Test với MQTT Events

**Yêu cầu:**
- MQTT broker (mosquitto)
- MQTT client để subscribe

**Các bước:**
```bash
# Tạo instance với MQTT
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @face_detection/test_mqtt_events.json

# Subscribe MQTT topic
mosquitto_sub -h localhost -t face_detection/events -v
```

## 📊 Kiểm Tra Kết Quả

### 1. Kiểm Tra Screen Display

- Mở cửa sổ hiển thị video
- Kiểm tra bounding boxes quanh khuôn mặt
- Kiểm tra track IDs và confidence scores

### 2. Kiểm Tra Statistics

```bash
curl http://localhost:8080/v1/core/instance/{instanceId}/statistics
```

**Expected output:**
```json
{
  "frames_processed": 1250,
  "source_framerate": 30.0,
  "current_framerate": 25.5,
  "latency": 200.0,
  "resolution": "1280x720",
  "format": "BGR"
}
```

### 3. Kiểm Tra MQTT Events

**Event structure:**
- Xem `report_body_example.json` để biết cấu trúc chi tiết

**Các event types:**
- `face_detected`: Khi phát hiện khuôn mặt mới
- `face_tracked`: Khi tracking khuôn mặt
- `face_recognized`: Khi nhận diện khuôn mặt (nếu có SFace)

## 🔍 Troubleshooting

### Lỗi: Model không tìm thấy
```bash
# Kiểm tra model path
ls -la /path/to/models/face/face_detection_yunet_2022mar.onnx

# Cập nhật MODEL_PATH trong JSON config
```

### Lỗi: RTSP connection failed
- Kiểm tra RTSP URL có đúng không
- Kiểm tra network connectivity
- Kiểm tra RTSP server đang chạy

### Lỗi: RTMP connection failed
```bash
# Test RTMP server
ffmpeg -re -i test.mp4 -c copy -f flv rtmp://server:1935/live/test

# Kiểm tra firewall
sudo ufw allow 1935/tcp
```

### Lỗi: MQTT connection failed
```bash
# Kiểm tra MQTT broker
sudo systemctl status mosquitto

# Test connection
mosquitto_sub -h localhost -t test -v
```

## 📚 Tài Liệu Tham Khảo

- Sample code: `sample/face_tracking_sample.cpp`
- Sample code: `sample/1-1-1_sample.cpp`
- Sample code: `sample/simple_rtmp_mqtt_sample.cpp`
- Testing guide: `sample/YUNET_TESTING_GUIDE.md`
