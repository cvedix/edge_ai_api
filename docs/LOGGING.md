# Logging Documentation

Tài liệu này mô tả các tính năng logging của Edge AI API Server.

## Tổng Quan

Edge AI API Server cung cấp các tính năng logging chi tiết để giúp bạn theo dõi và debug hệ thống. Các tính năng logging có thể được bật/tắt thông qua command-line arguments khi khởi động server.

## Các Loại Logging

### 1. API Logging (`--log-api` hoặc `--debug-api`)

Log tất cả các request và response của REST API.

**Khi nào sử dụng:**
- Debug các vấn đề với API requests
- Theo dõi performance của API endpoints
- Phân tích usage patterns
- Troubleshooting API errors

**Thông tin được log:**
- HTTP method và path
- Request source (IP address)
- Response status
- Response time (milliseconds)
- Instance ID (nếu có)
- Error messages (nếu có)

**Ví dụ log:**
```
[API] GET /v1/core/instances - Success: 5 instances (running: 2, stopped: 3) - 12ms
[API] POST /v1/core/instances/abc-123/start - Success - 234ms
[API] GET /v1/core/instances/xyz-789 - Success: face_detection (running: true, fps: 25.50) - 8ms
[API] POST /v1/core/instance - Success: Created instance abc-123 (Face Detection, solution: face_detection) - 156ms
```

**Cách sử dụng:**
```bash
./build/bin/edge_ai_api --log-api
```

### 2. Instance Execution Logging (`--log-instance` hoặc `--debug-instance`)

Log các sự kiện liên quan đến vòng đời của instance (start, stop, status changes).

**Khi nào sử dụng:**
- Debug các vấn đề khi start/stop instance
- Theo dõi trạng thái instance
- Phân tích lifecycle của instances
- Troubleshooting instance management

**Thông tin được log:**
- Instance ID và display name
- Solution ID và name
- Action (start/stop)
- Status (running/stopped)
- Timestamp

**Ví dụ log:**
```
[Instance] Starting instance: abc-123 (Face Detection Camera 1, solution: face_detection)
[Instance] Instance started successfully: abc-123 (Face Detection Camera 1, solution: face_detection, running: true)
[Instance] Stopping instance: xyz-789 (Face Detection File Source, solution: face_detection, was running: true)
[Instance] Instance stopped successfully: xyz-789 (Face Detection File Source, solution: face_detection)
```

**Cách sử dụng:**
```bash
./build/bin/edge_ai_api --log-instance
```

### 3. SDK Output Logging (`--log-sdk-output` hoặc `--debug-sdk-output`)

Log output từ SDK khi instance gọi SDK và SDK trả về kết quả (detection results, metadata, etc.).

**Khi nào sử dụng:**
- Debug các vấn đề với SDK processing
- Theo dõi kết quả detection real-time
- Phân tích performance của SDK
- Troubleshooting SDK integration

**Thông tin được log:**
- Timestamp
- Instance ID và display name
- FPS (Frames Per Second)
- Solution ID
- Processing status

**Ví dụ log:**
```
[SDKOutput] [2025-12-04 14:30:25.123] Instance: Face Detection Camera 1 (abc-123) - FPS: 25.50, Solution: face_detection
[SDKOutput] [2025-12-04 14:30:35.456] Instance: Face Detection File Source (xyz-789) - FPS: 30.00, Solution: face_detection
```

**Cách sử dụng:**
```bash
./build/bin/edge_ai_api --log-sdk-output
```

> **Lưu ý về đường dẫn executable:**
> 
> Khi build project với CMake, executable được đặt trong thư mục `build/bin/` thay vì trực tiếp trong `build/`. Đây là cấu hình mặc định của CMake để tổ chức các file output:
> - **Executables** → `build/bin/`
> - **Libraries** → `build/lib/`
> - **Object files** → `build/CMakeFiles/`
> 
> Cấu trúc này giúp phân tách rõ ràng giữa các loại file build và giữ cho thư mục build gọn gàng hơn. Nếu bạn muốn thay đổi vị trí này, có thể cấu hình trong `CMakeLists.txt` bằng cách set `CMAKE_RUNTIME_OUTPUT_DIRECTORY`.

## Kết Hợp Nhiều Flags

Bạn có thể kết hợp nhiều logging flags cùng lúc:

```bash
# Log tất cả
./build/bin/edge_ai_api --log-api --log-instance --log-sdk-output

# Hoặc dùng --debug-* prefix
./build/bin/edge_ai_api --debug-api --debug-instance --debug-sdk-output

# Chỉ log API và instance execution
./build/bin/edge_ai_api --log-api --log-instance
```

## Log Files và Cấu Trúc Thư Mục

Hệ thống logging tự động phân loại logs vào các thư mục riêng biệt:

### Cấu Trúc Thư Mục

```
logs/
├── api/              # API request/response logs (khi --log-api được bật)
│   ├── 2025-12-04.log
│   ├── 2025-12-05.log
│   └── ...
├── instance/         # Instance execution logs (khi --log-instance được bật)
│   ├── 2025-12-04.log
│   ├── 2025-12-05.log
│   └── ...
├── sdk_output/       # SDK output logs (khi --log-sdk-output được bật)
│   ├── 2025-12-04.log
│   ├── 2025-12-05.log
│   └── ...
└── general/          # General application logs (luôn có)
    ├── 2025-12-04.log
    ├── 2025-12-05.log
    └── ...
```

### Đặc Điểm

- **Phân loại tự động**: Logs được tự động phân loại vào đúng thư mục dựa trên prefix
- **Daily rotation**: Mỗi ngày tạo file log mới với format `YYYY-MM-DD.log`
- **Size-based rolling**: Mỗi file log có kích thước tối đa 50MB, tự động roll khi đạt giới hạn
- **Monthly cleanup**: Tự động xóa logs cũ hơn 30 ngày (có thể cấu hình)
- **Disk space monitoring**: Tự động cleanup khi dung lượng đĩa > 85% (có thể cấu hình)

**Xem logs:**

Có 2 cách để xem logs:

**1. Sử dụng Command Line (truyền thống):**
```bash
# Xem log real-time theo category
tail -f ./logs/api/2025-12-04.log
tail -f ./logs/instance/2025-12-04.log
tail -f ./logs/sdk_output/2025-12-04.log
tail -f ./logs/general/2025-12-04.log

# Xem log của ngày hiện tại
tail -f ./logs/api/$(date +%Y-%m-%d).log

# Filter theo loại log trong general log
tail -f ./logs/general/2025-12-04.log | grep "\[API\]"
tail -f ./logs/general/2025-12-04.log | grep "\[Instance\]"
tail -f ./logs/general/2025-12-04.log | grep "\[SDKOutput\]"

# Filter theo instance ID
tail -f ./logs/api/2025-12-04.log | grep "abc-123"
```

**2. Sử dụng REST API (khuyến nghị):**

Edge AI API Server cung cấp các endpoints để truy cập logs qua REST API với nhiều tính năng filtering và querying:

```bash
# List tất cả log files theo category
curl -X GET http://localhost:8080/v1/core/logs

# Get logs từ category api
curl -X GET http://localhost:8080/v1/core/logs/api

# Get logs từ category instance cho một ngày cụ thể
curl -X GET http://localhost:8080/v1/core/logs/instance/2025-01-15

# Filter theo log level (chỉ ERROR logs)
curl -X GET "http://localhost:8080/v1/core/logs/api?level=ERROR"

# Get 100 dòng cuối cùng (tail)
curl -X GET "http://localhost:8080/v1/core/logs/api?tail=100"

# Filter theo time range
curl -X GET "http://localhost:8080/v1/core/logs/api?from=2025-01-15T10:00:00.000Z&to=2025-01-15T11:00:00.000Z"

# Kết hợp nhiều filters
curl -X GET "http://localhost:8080/v1/core/logs/api?level=ERROR&tail=50"
```

**Xem chi tiết:** [LOGS_API.md](LOGS_API.md) - Tài liệu đầy đủ về Logs API endpoints

## Cấu Hình Logging

### Log Level

Logging level có thể được cấu hình qua biến môi trường `LOG_LEVEL`:

```bash
export LOG_LEVEL=DEBUG  # TRACE, DEBUG, INFO, WARN, ERROR
./build/bin/edge_ai_api --log-api
```

**Các mức log:**
- `TRACE`: Tất cả logs (rất chi tiết)
- `DEBUG`: Debug information
- `INFO`: Thông tin chung (mặc định)
- `WARN`: Cảnh báo
- `ERROR`: Lỗi

### Log Directory

Thay đổi thư mục lưu log:

```bash
export LOG_DIR=/var/log/edge_ai_api
./build/bin/edge_ai_api --log-api
```

### Log Retention và Cleanup

**Retention Policy:**
- **Mặc định**: Giữ logs trong 30 ngày
- **Cấu hình**: `export LOG_RETENTION_DAYS=60` (giữ logs trong 60 ngày)

**Disk Space Management:**
- **Threshold mặc định**: 85% disk usage
- **Cấu hình**: `export LOG_MAX_DISK_USAGE_PERCENT=90` (cleanup khi > 90%)
- **Khi disk sắp đầy**: Tự động xóa logs cũ hơn 7 ngày để giải phóng dung lượng

**Cleanup Interval:**
- **Mặc định**: Kiểm tra và cleanup mỗi 24 giờ
- **Cấu hình**: `export LOG_CLEANUP_INTERVAL_HOURS=12` (kiểm tra mỗi 12 giờ)

**Ví dụ cấu hình đầy đủ:**
```bash
export LOG_DIR=/var/log/edge_ai_api
export LOG_RETENTION_DAYS=60
export LOG_MAX_DISK_USAGE_PERCENT=90
export LOG_CLEANUP_INTERVAL_HOURS=24
./build/bin/edge_ai_api --log-api --log-instance --log-sdk-output
```

## Best Practices

### Development

```bash
# Development với đầy đủ logging
./build/bin/edge_ai_api --log-api --log-instance --log-sdk-output
```

### Production

```bash
# Production - chỉ log API và instance execution
./build/bin/edge_ai_api --log-api --log-instance

# Hoặc không log gì cả nếu không cần thiết
./build/bin/edge_ai_api
```

### Debugging

```bash
# Debug một vấn đề cụ thể - bật tất cả logging
./build/bin/edge_ai_api --log-api --log-instance --log-sdk-output

# Sau đó filter logs để tìm vấn đề
tail -f ./logs/log.txt | grep -E "ERROR|WARNING|Exception"
```

## Troubleshooting

### Logs không xuất hiện

1. Kiểm tra logging flags đã được bật chưa:
   ```bash
   ./build/bin/edge_ai_api --help
   ```

2. Kiểm tra log directory có tồn tại và có quyền ghi:
   ```bash
   ls -la ./logs
   ```

3. Kiểm tra LOG_LEVEL có quá cao không:
   ```bash
   export LOG_LEVEL=DEBUG
   ```

### Logs quá nhiều

1. Chỉ bật logging cần thiết:
   ```bash
   # Chỉ log API
   ./build/bin/edge_ai_api --log-api
   ```

2. Tăng LOG_LEVEL để giảm số lượng logs:
   ```bash
   export LOG_LEVEL=WARN  # Chỉ log warnings và errors
   ```

### Performance Impact

Logging có thể ảnh hưởng đến performance, đặc biệt là:
- `--log-api`: Có thể làm chậm API responses một chút (thường < 1ms)
- `--log-sdk-output`: Có thể ảnh hưởng đến performance của SDK processing

**Khuyến nghị:**
- Development: Bật tất cả logging
- Production: Chỉ bật logging cần thiết
- Debugging: Bật tất cả logging tạm thời

## Xem Thêm

- [GETTING_STARTED.md](GETTING_STARTED.md) - Hướng dẫn khởi động server
- [ENVIRONMENT_VARIABLES.md](ENVIRONMENT_VARIABLES.md) - Cấu hình biến môi trường
- [DEVELOPMENT_GUIDE.md](DEVELOPMENT_GUIDE.md) - Hướng dẫn phát triển

