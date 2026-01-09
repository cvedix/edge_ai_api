# Behavior Analysis Jam Instance - Hướng Dẫn Test

## 📋 Tổng Quan

Instance này thực hiện phát hiện "jam" (kẹt xe/dừng xe) trong các vùng định nghĩa sẵn (Jam zones). Thường dùng để phát hiện phương tiện dừng quá lâu trong khu vực ra/vào hoặc chờ phía trước.

## 🎯 Tính Năng

- ✅ Phát hiện phương tiện dừng (jam) trong vùng định nghĩa
- ✅ Tracking phương tiện với SORT tracker
- ✅ MQTT event publishing khi phát hiện jam
- ✅ RTMP streaming ouput (tùy chọn)
- ✅ OSD hiển thị trạng thái jam/tracking

## 📁 Cấu Trúc Files

```
ba_jam/
├── README.md
├── example_ba_jam_rtmp.json
├── example_ba_jam_file_mqtt.json
├── test_rtsp_source_rtmp_mqtt.json
└── report_body_example.json
```

## 🔧 Solution Config

### Solution ID: `ba_jam`

**Pipeline (ví dụ):**
```
File/RTSP Source → YOLO Detector → SORT Tracker → BA Jam → MQTT Broker → OSD → [Screen | RTMP]
```

**Tham số quan trọng:**
- `WEIGHTS_PATH`, `CONFIG_PATH`, `LABELS_PATH`: YOLO model paths
- `RTMP_URL`: RTMP streaming URL (nếu có)
- `JamZones`: JSON string định nghĩa các zone để phát hiện jam (ví dụ bên dưới)

### 📐 Ví dụ `JamZones` (AdditionalParams)

```json
{
  "additionalParams": {
    "JamZones": "[{\"id\":\"zone1\",\"name\":\"Front Lane\",\"coordinates\":[{\"x\":100,\"y\":300},{\"x\":700,\"y\":300},{\"x\":700,\"y\":400},{\"x\":100,\"y\":400}],\"min_frames_stopped\": 30}]"
  }
}
```

**Ghi chú:**
- `coordinates`: Array các điểm để vẽ polygon (ít nhất 3 điểm)
- `min_frames_stopped`: số frame liên tiếp để coi là jam (ví dụ: 30 frames)

## 📝 Manual Testing Guide

1. Tạo instance (ví dụ file source + MQTT)
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @example_ba_jam_file_mqtt.json
```
2. Start instance
```bash
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/start
```
3. Subscribe MQTT để nhận events
```bash
mosquitto_sub -h localhost -t ba_jam/events -v
```
4. Kiểm tra statistics
```bash
curl http://localhost:8080/v1/core/instance/{instanceId}/statistics
```

## 🔍 Troubleshooting
- Nếu detector báo lỗi `cv::dnn::readNet load network failed!`, hãy đảm bảo các model ONNX/Yolo đã được đặt đúng đường dẫn và có mặt trong máy (`/usr/share/cvedix/cvedix_data/models` hoặc `build/bin/models`).

