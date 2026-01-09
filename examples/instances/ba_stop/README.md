# Behavior Analysis Stop Line Instance - Hướng Dẫn Test

## 📋 Tổng Quan

Instance này thực hiện phát hiện dừng (stop) tại các stop-line được định nghĩa trước (ví dụ: khu vực chờ, vạch dừng). Dùng để cảnh báo phương tiện dừng sai quy định hoặc chặn luồng giao thông.

## 🎯 Tính Năng

- ✅ Phát hiện phương tiện dừng tại stop-line
- ✅ Tracking phương tiện với SORT tracker
- ✅ MQTT event publishing khi phát hiện stop
- ✅ RTMP streaming ouput (tùy chọn)

## 📁 Cấu Trúc Files

```
ba_stop/
├── README.md
├── example_ba_stop_rtmp.json
├── example_ba_stop_file_mqtt.json
├── test_rtsp_source_rtmp_mqtt.json
└── report_body_example.json
```

## 🔧 Solution Config

### Solution ID: `ba_stop`

**Pipeline (ví dụ):**
```
File/RTSP Source → YOLO Detector → SORT Tracker → BA Stop → MQTT Broker → OSD → [Screen | RTMP]
```

**Tham số quan trọng:**
- `WEIGHTS_PATH`, `CONFIG_PATH`, `LABELS_PATH`: YOLO model paths
- `RTMP_URL`: RTMP streaming URL (nếu có)
- `StopLines` hoặc legacy `STOP_LINE_START_X/Y` `/ STOP_LINE_END_X/Y` để định nghĩa vị trí vạch dừng

### 📐 Ví dụ `StopLines` (AdditionalParams)

```json
{
  "additionalParams": {
    "StopLines": "[{\"id\":\"stop1\",\"name\":\"Entrance Stop\",\"coordinates\":[{\"x\":200,\"y\":350},{\"x\":900,\"y\":350}],\"min_frames_stopped\":20}]"
  }
}
```

## 📝 Manual Testing Guide

- Tạo instance, start, subscribe MQTT và kiểm tra `statistics` như hướng dẫn ở `ba_jam`.

## 🔍 Troubleshooting
- Nếu detector báo lỗi `cv::dnn::readNet load network failed!`, kiểm tra rằng các file model đã được cài đặt đúng đường dẫn.
