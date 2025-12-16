# API Reference - Tài Liệu Tham Khảo API

Tài liệu này tổng hợp tất cả các API endpoints của Edge AI API Server, bao gồm Frame API, Statistics API, Logs API, Hardware Info API và Config API.

## Mục Lục

1. [Frame API](#frame-api)
2. [Statistics API](#statistics-api)
3. [Logs API](#logs-api)
4. [Hardware Info API](#hardware-info-api)
5. [Config API](#config-api)

---

## Frame API

### GET /v1/core/instances/{instanceId}/frame

Lấy khung hình cuối cùng đã được xử lý từ instance đang chạy.

**Tính năng:**
- Frame được encode thành JPEG base64 format
- Frame được cache tự động khi pipeline xử lý
- Thread-safe và hiệu suất cao

**Lưu ý:** Frame capture chỉ hoạt động nếu pipeline có `app_des_node`.

**Request:**
```bash
curl http://localhost:8080/v1/core/instances/{instanceId}/frame
```

**Response (200 OK):**
```json
{
  "frame": "/9j/4AAQSkZJRgABAQEAYABgAAD...",
  "running": true
}
```

**Use Cases:**
- Live preview trong web dashboard
- Thumbnail generation
- Monitoring và debugging

**Troubleshooting:**
- Frame trống: Kiểm tra pipeline có `app_des_node`, instance đang chạy, source có data
- Frame không update: Kiểm tra source có frames mới

---

## Statistics API

### GET /v1/core/instance/{instanceId}/statistics

Lấy thống kê thời gian thực của instance đang chạy.

**Thống kê bao gồm:**
- Số lượng frames đã xử lý
- Tốc độ xử lý (FPS)
- Độ trễ (latency)
- Độ phân giải
- Thời gian bắt đầu

**Request:**
```bash
curl http://localhost:8080/v1/core/instance/{instanceId}/statistics
```

**Response (200 OK):**
```json
{
  "frames_processed": 1250,
  "source_framerate": 30.0,
  "current_framerate": 25.5,
  "latency": 200.0,
  "start_time": 1764900520,
  "resolution": "1280x720",
  "format": "BGR",
  "source_resolution": "1920x1080"
}
```

**Use Cases:**
- Monitoring instance performance
- Alerting khi có vấn đề (high latency, dropped frames)
- Dashboard visualization
- Performance analysis

**Best Practices:**
- Polling frequency: 1-2 giây cho monitoring
- Luôn kiểm tra status code trước khi xử lý response
- Đặt timeout hợp lý cho requests

---

## Logs API

### GET /v1/core/logs

List tất cả log files theo category.

**Categories:**
- `api` - API request/response logs
- `instance` - Instance execution logs
- `sdk_output` - SDK output logs
- `general` - General application logs

**Request:**
```bash
curl http://localhost:8080/v1/core/logs
```

**Response:**
```json
{
  "categories": {
    "api": [{"date": "2025-01-15", "size": 1048576, "path": "/path/to/log"}],
    "instance": [...],
    "sdk_output": [],
    "general": [...]
  }
}
```

### GET /v1/core/logs/{category}

Get logs của một category với filtering.

**Query Parameters:**
- `level` (optional): Filter theo log level (INFO, WARNING, ERROR, FATAL, DEBUG, VERBOSE)
- `from` (optional): Filter từ timestamp (ISO 8601 format)
- `to` (optional): Filter đến timestamp (ISO 8601 format)
- `tail` (optional): Lấy N dòng cuối cùng

**Request:**
```bash
# Get ERROR logs
curl "http://localhost:8080/v1/core/logs/api?level=ERROR"

# Get logs trong khoảng thời gian
curl "http://localhost:8080/v1/core/logs/api?from=2025-01-15T10:00:00.000Z&to=2025-01-15T11:00:00.000Z"

# Get 100 dòng cuối cùng
curl "http://localhost:8080/v1/core/logs/api?tail=100"
```

### GET /v1/core/logs/{category}/{date}

Get logs của category và date cụ thể với filtering.

**Request:**
```bash
curl "http://localhost:8080/v1/core/logs/instance/2025-01-15?level=ERROR&tail=50"
```

**Best Practices:**
- Sử dụng date cụ thể để tăng tốc độ
- Kết hợp filters (`level` và `tail`) để giảm lượng dữ liệu
- Sử dụng `tail` với số lượng nhỏ (50-100) cho real-time monitoring

---

## Hardware Info API

### GET /v1/core/system/info

Lấy thông tin phần cứng tĩnh của hệ thống (CPU, GPU, RAM, Disk, Mainboard, OS, Battery).

**Request:**
```bash
curl http://localhost:8080/v1/core/system/info
```

**Response:**
```json
{
  "cpu": {
    "vendor": "GenuineIntel",
    "model": "Intel(R) Core(TM) i7-10700K CPU @ 3.80GHz",
    "physical_cores": 8,
    "logical_cores": 16,
    "max_frequency": 3792,
    "cache_size": 16777216
  },
  "ram": {
    "size_mib": 65437,
    "free_mib": 54405,
    "available_mib": 54405
  },
  "gpu": [...],
  "disk": [...],
  "mainboard": {...},
  "os": {...},
  "battery": []
}
```

### GET /v1/core/system/status

Lấy trạng thái hiện tại của hệ thống (CPU usage, RAM usage, load average, uptime).

**Request:**
```bash
curl http://localhost:8080/v1/core/system/status
```

**Response:**
```json
{
  "cpu": {
    "usage_percent": 25.5,
    "current_frequency_mhz": 3792,
    "temperature_celsius": 45.5
  },
  "ram": {
    "total_mib": 65437,
    "used_mib": 11032,
    "usage_percent": 16.85
  },
  "load_average": {
    "1min": 0.75,
    "5min": 0.82,
    "15min": 0.88
  },
  "uptime_seconds": 86400
}
```

**Lưu ý:**
- Thông tin phần cứng có thể cache (tĩnh)
- Thông tin trạng thái nên gọi real-time (tối đa 1 lần/giây)
- Một số giá trị có thể là -1 hoặc null nếu hệ thống không hỗ trợ

---

## Config API

### GET /v1/core/config

Lấy toàn bộ cấu hình hệ thống.

**Request:**
```bash
curl http://localhost:8080/v1/core/config
```

### GET /v1/core/config?path={path}

Lấy một phần cấu hình theo đường dẫn.

**Request:**
```bash
# Lấy max_running_instances
curl 'http://localhost:8080/v1/core/config?path=system/max_running_instances'

# Lấy web server config
curl 'http://localhost:8080/v1/core/config?path=system/web_server'
```

### POST /v1/core/config

Cập nhật cấu hình (merge với config hiện tại).

**Request:**
```bash
curl -X POST http://localhost:8080/v1/core/config \
  -H "Content-Type: application/json" \
  -d '{
    "system": {
      "max_running_instances": 10
    }
  }'
```

### PUT /v1/core/config

Thay thế toàn bộ cấu hình (⚠️ CẨN THẬN: sẽ xóa các field không được gửi).

**Request:**
```bash
curl -X PUT http://localhost:8080/v1/core/config \
  -H "Content-Type: application/json" \
  -d '{...}'
```

### PATCH /v1/core/config?path={path}

Cập nhật một phần cấu hình tại đường dẫn cụ thể.

**Request:**
```bash
curl -X PATCH 'http://localhost:8080/v1/core/config?path=system/max_running_instances' \
  -H "Content-Type: application/json" \
  -d '10'
```

### DELETE /v1/core/config?path={path}

Xóa một phần cấu hình.

**Request:**
```bash
curl -X DELETE 'http://localhost:8080/v1/core/config?path=gstreamer/decode_pipelines/custom'
```

### POST /v1/core/config/reset

Reset toàn bộ cấu hình về giá trị mặc định (⚠️ CẢNH BÁO).

**Request:**
```bash
curl -X POST http://localhost:8080/v1/core/config/reset
```

### Cấu Trúc Config.json

**Các section chính:**
- `auto_device_list`: Danh sách thiết bị AI có sẵn
- `decoder_priority_list`: Thứ tự ưu tiên decoder
- `gstreamer`: Cấu hình GStreamer pipeline và plugin rank
- `system.web_server`: Cấu hình web server
- `system.logging`: Cấu hình logging
- `system.max_running_instances`: ⭐ Giới hạn số instance tối đa (đã tích hợp)
- `system.modelforge_permissive`: Modelforge permissive mode

**Quan trọng:**
- `max_running_instances = 0`: Không giới hạn (mặc định)
- `max_running_instances > 0`: Giới hạn số instance tối đa
- Khi vượt quá limit → HTTP 429 (Too Many Requests)

**Best Practices:**
- Sử dụng `POST` (merge) thay vì `PUT` (replace) trong hầu hết trường hợp
- Backup config trước khi thay đổi lớn
- Kiểm tra config sau khi update
- Sử dụng path cụ thể cho update nhỏ

---

## Vị Trí File Config

Hệ thống tự động tìm và tạo file `config.json` theo thứ tự ưu tiên:

1. Biến môi trường `CONFIG_FILE` (ưu tiên cao nhất)
2. Thư mục hiện tại: `./config.json`
3. Production path: `/opt/edge_ai_api/config/config.json`
4. System path: `/etc/edge_ai_api/config.json`
5. User config directory: `~/.config/edge_ai_api/config.json`
6. Last resort: `./config.json` (thư mục hiện tại)

---

## Error Handling

### 400 Bad Request
- Request body không phải JSON hợp lệ
- Thiếu required fields
- Category hoặc date format sai

### 404 Not Found
- Instance không tồn tại
- Log file không tồn tại cho date được chỉ định
- Configuration section không tồn tại

### 429 Too Many Requests
- Vượt quá `max_running_instances` limit

### 500 Internal Server Error
- Lỗi server khi xử lý request
- Lỗi khi lưu config vào file
- Lỗi khi đọc log files

---

## Swagger UI

Bạn có thể test tất cả API endpoints trực tiếp từ Swagger UI:

- **Swagger UI**: `http://localhost:8080/swagger`
- **API v1 Swagger**: `http://localhost:8080/v1/swagger`
- **OpenAPI Spec**: `http://localhost:8080/openapi.yaml`

---

## Related Documentation

- [INSTANCE_GUIDE.md](INSTANCE_GUIDE.md) - Hướng dẫn tạo và cập nhật instance
- [GETTING_STARTED.md](GETTING_STARTED.md) - Hướng dẫn khởi động server
- [DEVELOPMENT_SETUP.md](DEVELOPMENT_SETUP.md) - Hướng dẫn setup môi trường
- [OpenAPI Specification](../openapi.yaml) - Full API specification

