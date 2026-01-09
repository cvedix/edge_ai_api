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

### 📐 Cấu Hình Jam Zones

Có **2 cách** để cấu hình jam zones:

#### Cách 1: Sử dụng `JamZones` (Format Mới - Khuyến Nghị) ✅

Sử dụng `JamZones` trong `additionalParams` để định nghĩa nhiều zones với đầy đủ thông tin:

```json
{
  "additionalParams": {
    "JamZones": "[{\"id\":\"zone1\",\"name\":\"Front Lane\",\"roi\":[{\"x\":100,\"y\":300},{\"x\":700,\"y\":300},{\"x\":700,\"y\":400},{\"x\":100,\"y\":400}],\"checkMinStops\":30,\"checkMaxDistance\":5}]"
  }
}
```

**Ưu điểm:**
- ✅ Hỗ trợ nhiều zones (multiple zones)
- ✅ Có thể quản lý qua API (`/v1/core/instance/{instanceId}/jams`)
- ✅ Hỗ trợ đầy đủ: name, roi, checkMinStops, checkMaxDistance, checkIntervalFrames, checkNotifyInterval
- ✅ Real-time update (restart instance để apply)

**Format chi tiết:**
- `id`: UUID của zone (tự động generate khi tạo qua API)
- `name`: Tên mô tả zone (optional)
- `roi`: Array các điểm polygon `[{"x": 100, "y": 300}, {"x": 700, "y": 300}, ...]` (tối thiểu 3 điểm)
- `checkMinStops`: Số frame tối thiểu để coi là jam (mặc định: 30)
- `checkMaxDistance`: Khoảng cách tối đa để coi là dừng (mặc định: 5)
- `checkIntervalFrames`: Số frame giữa các lần kiểm tra (mặc định: 10)
- `checkNotifyInterval`: Số frame giữa các lần gửi notification (mặc định: 0 - gửi mỗi lần phát hiện)

**Ví dụ với nhiều zones:**
```json
{
  "JamZones": "[{\"id\":\"zone1\",\"name\":\"Entrance Zone\",\"roi\":[{\"x\":100,\"y\":300},{\"x\":700,\"y\":300},{\"x\":700,\"y\":400},{\"x\":100,\"y\":400}],\"checkMinStops\":30},{\"id\":\"zone2\",\"name\":\"Exit Zone\",\"roi\":[{\"x\":200,\"y\":500},{\"x\":800,\"y\":500},{\"x\":800,\"y\":600},{\"x\":200,\"y\":600}],\"checkMinStops\":20}]"
}
```

#### Cách 2: Sử dụng Legacy Format (Format Cũ)

Format cũ chỉ hỗ trợ 1 zone và không thể quản lý qua API.

## 📝 Manual Testing Guide

### 1. Tạo Instance

```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @example_ba_jam_file_mqtt.json
```

### 2. Start Instance

```bash
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/start
```

### 3. Quản Lý Jam Zones Qua API

Sau khi tạo instance, bạn có thể quản lý jam zones qua API:

```bash
# Lấy tất cả jam zones
curl http://localhost:8080/v1/core/instance/{instanceId}/jams

# Lấy một jam zone cụ thể
curl http://localhost:8080/v1/core/instance/{instanceId}/jams/{jamId}

# Tạo jam zone mới
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/jams \
  -H "Content-Type: application/json" \
  -d '{
    "name": "New Jam Zone",
    "roi": [{"x": 100, "y": 300}, {"x": 700, "y": 300}, {"x": 700, "y": 400}, {"x": 100, "y": 400}],
    "checkMinStops": 30,
    "checkMaxDistance": 5,
    "checkIntervalFrames": 10
  }'

# Tạo nhiều jam zones cùng lúc
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/jams \
  -H "Content-Type: application/json" \
  -d '[
    {
      "name": "Zone 1",
      "roi": [{"x": 100, "y": 300}, {"x": 700, "y": 300}, {"x": 700, "y": 400}, {"x": 100, "y": 400}],
      "checkMinStops": 30
    },
    {
      "name": "Zone 2",
      "roi": [{"x": 200, "y": 500}, {"x": 800, "y": 500}, {"x": 800, "y": 600}, {"x": 200, "y": 600}],
      "checkMinStops": 20
    }
  ]'

# Cập nhật jam zone
curl -X PUT http://localhost:8080/v1/core/instance/{instanceId}/jams/{jamId} \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Updated Zone",
    "roi": [{"x": 150, "y": 350}, {"x": 750, "y": 350}, {"x": 750, "y": 450}, {"x": 150, "y": 450}],
    "checkMinStops": 25
  }'

# Xóa một jam zone
curl -X DELETE http://localhost:8080/v1/core/instance/{instanceId}/jams/{jamId}

# Xóa tất cả jam zones
curl -X DELETE http://localhost:8080/v1/core/instance/{instanceId}/jams

# Batch update nhiều zones
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/jams/batch \
  -H "Content-Type: application/json" \
  -d '[
    {"id": "zone1", "name": "Updated Zone 1", "roi": [...]},
    {"id": "zone2", "name": "Updated Zone 2", "roi": [...]}
  ]'
```

**Lưu ý:** 
- Khi thêm/sửa/xóa jam zones, instance sẽ tự động restart để áp dụng thay đổi
- Các thay đổi được lưu vào config và sẽ được áp dụng khi instance restart

### 4. Subscribe MQTT để nhận events

```bash
mosquitto_sub -h localhost -t ba_jam/events -v
```

### 5. Kiểm tra statistics

```bash
curl http://localhost:8080/v1/core/instance/{instanceId}/statistics
```

## 🔍 Troubleshooting
- Nếu detector báo lỗi `cv::dnn::readNet load network failed!`, hãy đảm bảo các model ONNX/Yolo đã được đặt đúng đường dẫn và có mặt trong máy (`/usr/share/cvedix/cvedix_data/models` hoặc `build/bin/models`).

