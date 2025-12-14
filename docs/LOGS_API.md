# Logs API Documentation

Tài liệu này hướng dẫn sử dụng các API endpoints để truy cập và quản lý logs của Edge AI API Server.

## Tổng Quan

Edge AI API Server cung cấp 3 endpoints để truy cập logs:

1. **GET /v1/core/logs** - List tất cả log files theo category
2. **GET /v1/core/logs/{category}** - Get logs của một category với filtering
3. **GET /v1/core/logs/{category}/{date}** - Get logs của category và date cụ thể với filtering

Logs được tổ chức theo 4 categories:
- **api** - API request/response logs
- **instance** - Instance execution logs (start/stop/status)
- **sdk_output** - SDK output logs khi instances xử lý
- **general** - General application logs

## Endpoints

### 1. List All Log Files

**Endpoint:** `GET /v1/core/logs`

**Mô tả:** Trả về danh sách tất cả log files được tổ chức theo category.

**Response:**
```json
{
  "categories": {
    "api": [
      {
        "date": "2025-01-15",
        "size": 1048576,
        "path": "/var/lib/edge_ai_api/logs/api/2025-01-15.log"
      }
    ],
    "instance": [
      {
        "date": "2025-01-15",
        "size": 2048576,
        "path": "/var/lib/edge_ai_api/logs/instance/2025-01-15.log"
      }
    ],
    "sdk_output": [],
    "general": [
      {
        "date": "2025-01-15",
        "size": 512000,
        "path": "/var/lib/edge_ai_api/logs/general/2025-01-15.log"
      }
    ]
  }
}
```

**Ví dụ sử dụng:**
```bash
# Sử dụng curl
curl -X GET http://localhost:8080/v1/core/logs

# Sử dụng httpie
http GET localhost:8080/v1/core/logs
```

### 2. Get Logs by Category

**Endpoint:** `GET /v1/core/logs/{category}`

**Mô tả:** Trả về logs từ một category cụ thể với các tùy chọn filtering.

**Path Parameters:**
- `category` (required): Category name - `api`, `instance`, `sdk_output`, hoặc `general`

**Query Parameters:**
- `level` (optional): Filter theo log level - `INFO`, `WARNING`, `ERROR`, `FATAL`, `DEBUG`, `VERBOSE` (case-insensitive)
- `from` (optional): Filter logs từ timestamp này (ISO 8601 format: `YYYY-MM-DDTHH:MM:SS.mmmZ`)
- `to` (optional): Filter logs đến timestamp này (ISO 8601 format: `YYYY-MM-DDTHH:MM:SS.mmmZ`)
- `tail` (optional): Lấy N dòng cuối cùng từ file log mới nhất (integer)

**Response:**
```json
{
  "category": "api",
  "total_lines": 1000,
  "filtered_lines": 50,
  "logs": [
    {
      "timestamp": "2025-01-15T10:30:45.123Z",
      "level": "INFO",
      "message": "[API] GET /v1/core/instances - Success: 5 instances (running: 2, stopped: 3) - 12ms"
    },
    {
      "timestamp": "2025-01-15T10:30:46.456Z",
      "level": "ERROR",
      "message": "[API] POST /v1/core/instances/abc-123/start - Error: Instance not found"
    }
  ]
}
```

**Ví dụ sử dụng:**
```bash
# Get tất cả logs từ category api
curl -X GET http://localhost:8080/v1/core/logs/api

# Get chỉ ERROR logs từ category api
curl -X GET "http://localhost:8080/v1/core/logs/api?level=ERROR"

# Get logs từ một khoảng thời gian
curl -X GET "http://localhost:8080/v1/core/logs/api?from=2025-01-15T10:00:00.000Z&to=2025-01-15T11:00:00.000Z"

# Get 100 dòng cuối cùng từ category api
curl -X GET "http://localhost:8080/v1/core/logs/api?tail=100"

# Kết hợp nhiều filters
curl -X GET "http://localhost:8080/v1/core/logs/api?level=ERROR&from=2025-01-15T10:00:00.000Z&tail=50"
```

### 3. Get Logs by Category and Date

**Endpoint:** `GET /v1/core/logs/{category}/{date}`

**Mô tả:** Trả về logs từ một category và date cụ thể với các tùy chọn filtering.

**Path Parameters:**
- `category` (required): Category name - `api`, `instance`, `sdk_output`, hoặc `general`
- `date` (required): Date trong format `YYYY-MM-DD` (ví dụ: `2025-01-15`)

**Query Parameters:**
- `level` (optional): Filter theo log level - `INFO`, `WARNING`, `ERROR`, `FATAL`, `DEBUG`, `VERBOSE` (case-insensitive)
- `from` (optional): Filter logs từ timestamp này (ISO 8601 format)
- `to` (optional): Filter logs đến timestamp này (ISO 8601 format)
- `tail` (optional): Lấy N dòng cuối cùng từ file log (integer)

**Response:**
```json
{
  "category": "instance",
  "date": "2025-01-15",
  "total_lines": 500,
  "filtered_lines": 25,
  "logs": [
    {
      "timestamp": "2025-01-15T14:30:25.123Z",
      "level": "INFO",
      "message": "[Instance] Starting instance: abc-123 (Face Detection Camera 1, solution: face_detection)"
    },
    {
      "timestamp": "2025-01-15T14:30:26.456Z",
      "level": "INFO",
      "message": "[Instance] Instance started successfully: abc-123 (Face Detection Camera 1, solution: face_detection, running: true)"
    }
  ]
}
```

**Ví dụ sử dụng:**
```bash
# Get tất cả logs từ category instance cho ngày 2025-01-15
curl -X GET http://localhost:8080/v1/core/logs/instance/2025-01-15

# Get chỉ WARNING và ERROR logs
curl -X GET "http://localhost:8080/v1/core/logs/instance/2025-01-15?level=WARNING"

# Get logs trong một khoảng thời gian cụ thể
curl -X GET "http://localhost:8080/v1/core/logs/api/2025-01-15?from=2025-01-15T10:00:00.000Z&to=2025-01-15T11:00:00.000Z"

# Get 50 dòng cuối cùng
curl -X GET "http://localhost:8080/v1/core/logs/general/2025-01-15?tail=50"

# Kết hợp nhiều filters
curl -X GET "http://localhost:8080/v1/core/logs/sdk_output/2025-01-15?level=INFO&tail=100"
```

## Log Format

Mỗi log entry có format:
```
YYYY-MM-DD HH:MM:SS.mmm [LEVEL] message
```

**Ví dụ:**
```
2025-01-15 10:30:45.123 [INFO] [API] GET /v1/core/instances - Success: 5 instances (running: 2, stopped: 3) - 12ms
2025-01-15 10:30:46.456 [ERROR] [API] POST /v1/core/instances/abc-123/start - Error: Instance not found
2025-01-15 14:30:25.123 [INFO] [Instance] Starting instance: abc-123 (Face Detection Camera 1, solution: face_detection)
```

Khi trả về qua API, mỗi log entry được parse thành JSON:
```json
{
  "timestamp": "2025-01-15T10:30:45.123Z",
  "level": "INFO",
  "message": "[API] GET /v1/core/instances - Success: 5 instances (running: 2, stopped: 3) - 12ms"
}
```

## Filtering

### Filter by Level

Filter logs theo log level. Case-insensitive.

**Các log levels:**
- `INFO` - Thông tin thông thường
- `WARNING` hoặc `WARN` - Cảnh báo
- `ERROR` - Lỗi
- `FATAL` - Lỗi nghiêm trọng
- `DEBUG` - Debug information
- `VERBOSE` - Thông tin chi tiết

**Ví dụ:**
```bash
# Chỉ lấy ERROR logs
curl -X GET "http://localhost:8080/v1/core/logs/api?level=ERROR"

# Chỉ lấy WARNING và ERROR (cần gọi 2 lần hoặc filter ở client)
curl -X GET "http://localhost:8080/v1/core/logs/api?level=WARNING"
curl -X GET "http://localhost:8080/v1/core/logs/api?level=ERROR"
```

### Filter by Time Range

Filter logs trong một khoảng thời gian sử dụng timestamps ISO 8601.

**Format:** `YYYY-MM-DDTHH:MM:SS.mmmZ`

**Ví dụ:**
```bash
# Logs từ 10:00 AM đến 11:00 AM ngày 2025-01-15
curl -X GET "http://localhost:8080/v1/core/logs/api?from=2025-01-15T10:00:00.000Z&to=2025-01-15T11:00:00.000Z"

# Logs từ 00:00:00 đến 23:59:59 của một ngày
curl -X GET "http://localhost:8080/v1/core/logs/api/2025-01-15?from=2025-01-15T00:00:00.000Z&to=2025-01-15T23:59:59.999Z"
```

### Tail Logs

Lấy N dòng cuối cùng từ log file. Hữu ích khi muốn xem logs mới nhất.

**Ví dụ:**
```bash
# Lấy 100 dòng cuối cùng
curl -X GET "http://localhost:8080/v1/core/logs/api?tail=100"

# Lấy 50 dòng cuối cùng từ một ngày cụ thể
curl -X GET "http://localhost:8080/v1/core/logs/instance/2025-01-15?tail=50"
```

**Lưu ý:** 
- Khi sử dụng `tail` với endpoint `/v1/core/logs/{category}`, chỉ lấy từ file log mới nhất
- Khi sử dụng `tail` với endpoint `/v1/core/logs/{category}/{date}`, lấy từ file log của ngày đó

### Kết Hợp Filters

Bạn có thể kết hợp nhiều filters cùng lúc:

```bash
# ERROR logs trong khoảng thời gian, lấy 50 dòng cuối
curl -X GET "http://localhost:8080/v1/core/logs/api?level=ERROR&from=2025-01-15T10:00:00.000Z&to=2025-01-15T11:00:00.000Z&tail=50"

# INFO logs từ một ngày cụ thể, lấy 100 dòng cuối
curl -X GET "http://localhost:8080/v1/core/logs/instance/2025-01-15?level=INFO&tail=100"
```

## Error Handling

### 400 Bad Request

Khi category không hợp lệ hoặc date format sai:

```json
{
  "error": "Bad request",
  "message": "Invalid category: invalid_category"
}
```

```json
{
  "error": "Bad request",
  "message": "Invalid date format. Expected YYYY-MM-DD"
}
```

### 404 Not Found

Khi log file không tồn tại cho date được chỉ định:

```json
{
  "error": "Not found",
  "message": "Log file not found for date: 2025-01-15"
}
```

### 500 Internal Server Error

Khi có lỗi server khi đọc log files:

```json
{
  "error": "Internal server error",
  "message": "Error details here"
}
```

## Use Cases

### 1. Monitor API Requests

Xem tất cả API requests trong ngày:

```bash
curl -X GET "http://localhost:8080/v1/core/logs/api/2025-01-15"
```

Xem chỉ các API requests bị lỗi:

```bash
curl -X GET "http://localhost:8080/v1/core/logs/api/2025-01-15?level=ERROR"
```

### 2. Debug Instance Issues

Xem logs khi instance start/stop:

```bash
curl -X GET "http://localhost:8080/v1/core/logs/instance?tail=100"
```

Xem logs của một ngày cụ thể:

```bash
curl -X GET "http://localhost:8080/v1/core/logs/instance/2025-01-15"
```

### 3. Monitor SDK Performance

Xem SDK output logs real-time (100 dòng cuối):

```bash
curl -X GET "http://localhost:8080/v1/core/logs/sdk_output?tail=100"
```

### 4. Analyze Errors

Tìm tất cả ERROR logs trong một khoảng thời gian:

```bash
curl -X GET "http://localhost:8080/v1/core/logs/general?level=ERROR&from=2025-01-15T00:00:00.000Z&to=2025-01-15T23:59:59.999Z"
```

### 5. Real-time Monitoring

Xem logs mới nhất từ tất cả categories:

```bash
# API logs
curl -X GET "http://localhost:8080/v1/core/logs/api?tail=50"

# Instance logs
curl -X GET "http://localhost:8080/v1/core/logs/instance?tail=50"

# SDK output logs
curl -X GET "http://localhost:8080/v1/core/logs/sdk_output?tail=50"

# General logs
curl -X GET "http://localhost:8080/v1/core/logs/general?tail=50"
```

## Swagger UI

Bạn có thể test các endpoints trực tiếp từ Swagger UI:

1. Mở trình duyệt và truy cập: `http://localhost:8080/swagger` hoặc `http://localhost:8080/v1/swagger`
2. Tìm section "Logs"
3. Test các endpoints với các parameters khác nhau

## Performance Considerations

- **Large Files:** Khi log files lớn, việc đọc toàn bộ file có thể mất thời gian. Sử dụng `tail` parameter để giới hạn số dòng trả về.
- **Time Range Filtering:** Filter theo time range sẽ parse tất cả logs, có thể chậm với files lớn. Nên kết hợp với `tail` để giảm số lượng logs cần parse.
- **Multiple Categories:** Endpoint `/v1/core/logs/{category}` đọc từ tất cả files trong category. Với nhiều files, response có thể lớn. Nên sử dụng endpoint với date cụ thể hoặc dùng `tail`.

## Best Practices

1. **Sử dụng date cụ thể:** Khi biết ngày cần xem, sử dụng endpoint với date để tăng tốc độ.
2. **Kết hợp filters:** Sử dụng `level` và `tail` cùng lúc để giảm lượng dữ liệu trả về.
3. **Monitor real-time:** Sử dụng `tail` với số lượng nhỏ (50-100) để xem logs mới nhất.
4. **Error analysis:** Filter theo `level=ERROR` để tập trung vào các vấn đề.
5. **Time-based queries:** Sử dụng time range để phân tích logs trong một khoảng thời gian cụ thể.

## Related Documentation

- [LOGGING.md](LOGGING.md) - Tài liệu về logging features và cách bật/tắt logging
- [GETTING_STARTED.md](GETTING_STARTED.md) - Hướng dẫn khởi động server và sử dụng API
- [ENVIRONMENT_VARIABLES.md](ENVIRONMENT_VARIABLES.md) - Cấu hình logging directories và settings

