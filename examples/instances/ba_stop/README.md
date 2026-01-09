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

### 📐 Cấu Hình Stop Zones

Có **2 cách** để cấu hình stop zones:

#### Cách 1: Sử dụng `StopZones` (Format Mới - Khuyến Nghị) ✅

Sử dụng `StopZones` trong `additionalParams` để định nghĩa nhiều zones với đầy đủ thông tin:

```json
{
  "additionalParams": {
    "StopZones": "[{\"id\":\"zone1\",\"name\":\"Entrance Stop Zone\",\"roi\":[{\"x\":20,\"y\":30},{\"x\":600,\"y\":40},{\"x\":600,\"y\":300},{\"x\":10,\"y\":300}],\"min_stop_seconds\":3,\"check_interval_frames\":20,\"check_min_hit_frames\":50,\"check_max_distance\":5}]"
  }
}
```

**Ưu điểm:**
- ✅ Hỗ trợ nhiều zones (multiple zones)
- ✅ Có thể quản lý qua API (`/v1/core/instance/{instanceId}/stops`)
- ✅ Hỗ trợ đầy đủ: name, roi, min_stop_seconds, check_interval_frames, check_min_hit_frames, check_max_distance
- ✅ Real-time update (restart instance để apply)

**Format chi tiết:**
- `id`: UUID của zone (tự động generate khi tạo qua API)
- `name`: Tên mô tả zone (optional)
- `roi`: Array các điểm polygon `[{"x": 20, "y": 30}, {"x": 600, "y": 40}, ...]` (tối thiểu 3 điểm)
- `min_stop_seconds`: Số giây tối thiểu để coi là dừng (mặc định: 3)
- `check_interval_frames`: Số frame giữa các lần kiểm tra (mặc định: 20)
- `check_min_hit_frames`: Số frame tối thiểu phát hiện trong zone (mặc định: 50)
- `check_max_distance`: Khoảng cách tối đa để coi là dừng (mặc định: 5)

**Ví dụ với nhiều zones:**
```json
{
  "StopZones": "[{\"id\":\"zone1\",\"name\":\"Channel 0 Stop Zone\",\"roi\":[{\"x\":20,\"y\":30},{\"x\":600,\"y\":40},{\"x\":600,\"y\":300},{\"x\":10,\"y\":300}],\"min_stop_seconds\":3},{\"id\":\"zone2\",\"name\":\"Channel 1 Stop Zone\",\"roi\":[{\"x\":20,\"y\":30},{\"x\":1000,\"y\":40},{\"x\":1000,\"y\":600},{\"x\":10,\"y\":600}],\"min_stop_seconds\":3}]"
}
```

#### Cách 2: Sử dụng Legacy Format (Format Cũ)

Format cũ chỉ hỗ trợ 1 zone và không thể quản lý qua API.

## 📝 Manual Testing Guide

### 1. Tạo Instance

```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @example_ba_stop_file_mqtt.json
```

### 2. Start Instance

```bash
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/start
```

### 3. Quản Lý Stop Zones Qua API

Sau khi tạo instance, bạn có thể quản lý stop zones qua API:

```bash
# Lấy tất cả stop zones
curl http://localhost:8080/v1/core/instance/{instanceId}/stops

# Lấy một stop zone cụ thể
curl http://localhost:8080/v1/core/instance/{instanceId}/stops/{stopId}

# Tạo stop zone mới
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/stops \
  -H "Content-Type: application/json" \
  -d '{
    "name": "New Stop Zone",
    "roi": [{"x": 20, "y": 30}, {"x": 600, "y": 40}, {"x": 600, "y": 300}, {"x": 10, "y": 300}],
    "min_stop_seconds": 3,
    "check_interval_frames": 20,
    "check_min_hit_frames": 50,
    "check_max_distance": 5
  }'

# Tạo nhiều stop zones cùng lúc
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/stops \
  -H "Content-Type: application/json" \
  -d '[
    {
      "name": "Zone 1",
      "roi": [{"x": 20, "y": 30}, {"x": 600, "y": 40}, {"x": 600, "y": 300}, {"x": 10, "y": 300}],
      "min_stop_seconds": 3
    },
    {
      "name": "Zone 2",
      "roi": [{"x": 20, "y": 30}, {"x": 1000, "y": 40}, {"x": 1000, "y": 600}, {"x": 10, "y": 600}],
      "min_stop_seconds": 3
    }
  ]'

# Cập nhật stop zone
curl -X PUT http://localhost:8080/v1/core/instance/{instanceId}/stops/{stopId} \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Updated Zone",
    "roi": [{"x": 50, "y": 50}, {"x": 650, "y": 60}, {"x": 650, "y": 320}, {"x": 40, "y": 320}],
    "min_stop_seconds": 5
  }'

# Xóa một stop zone
curl -X DELETE http://localhost:8080/v1/core/instance/{instanceId}/stops/{stopId}

# Xóa tất cả stop zones
curl -X DELETE http://localhost:8080/v1/core/instance/{instanceId}/stops

# Batch update nhiều zones
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/stops/batch \
  -H "Content-Type: application/json" \
  -d '[
    {"id": "zone1", "name": "Updated Zone 1", "roi": [...]},
    {"id": "zone2", "name": "Updated Zone 2", "roi": [...]}
  ]'
```

**Lưu ý:** 
- Khi thêm/sửa/xóa stop zones, instance sẽ tự động restart để áp dụng thay đổi
- Các thay đổi được lưu vào config và sẽ được áp dụng khi instance restart

### 4. Subscribe MQTT để nhận events

```bash
mosquitto_sub -h localhost -t ba_stop/events -v
```

### 5. Kiểm tra statistics

```bash
curl http://localhost:8080/v1/core/instance/{instanceId}/statistics
```

## 🔍 Troubleshooting
- Nếu detector báo lỗi `cv::dnn::readNet load network failed!`, kiểm tra rằng các file model đã được cài đặt đúng đường dẫn.
