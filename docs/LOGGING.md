# Logging Documentation

Tài liệu này mô tả các tính năng logging của Edge AI API Server, bao gồm cách sử dụng, cấu hình và phân tích hệ thống logging.

## 📋 Tổng Quan

Edge AI API Server cung cấp các tính năng logging chi tiết để giúp bạn theo dõi và debug hệ thống. Các tính năng logging có thể được bật/tắt thông qua command-line arguments khi khởi động server.

**✅ Kết Luận Quan Trọng:** Hệ thống logging đã được thiết kế với nhiều cơ chế bảo vệ để **ngăn chặn tràn bộ nhớ**:
- ✅ Log rotation (50MB/file, daily rotation)
- ✅ Automatic cleanup (30 ngày mặc định)
- ✅ Disk space monitoring (85% threshold)
- ✅ Size-based rolling

---

## 📝 Các Loại Logging

Hệ thống có **4 loại log** được phân loại và lưu vào các thư mục riêng:

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
[API] GET /v1/core/instance - Success: 5 instances (running: 2, stopped: 3) - 12ms
[API] POST /v1/core/instance/abc-123/start - Success - 234ms
[API] GET /v1/core/instance/xyz-789 - Success: face_detection (running: true, fps: 25.50) - 8ms
[API] POST /v1/core/instance - Success: Created instance abc-123 (Face Detection, solution: face_detection) - 156ms
```

**File location:** `logs/api/YYYY-MM-DD.log`

**Cách sử dụng:**
```bash
./build/bin/edge_ai_api --log-api
```

---

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

**File location:** `logs/instance/YYYY-MM-DD.log`

**Cách sử dụng:**
```bash
./build/bin/edge_ai_api --log-instance
```

---

### 3. SDK Output Logging (`--log-sdk-output` hoặc `--debug-sdk-output`)

Log output từ SDK khi instance gọi SDK và SDK trả về kết quả (detection results, metadata, etc.).

**Khi nào sử dụng:**
- Debug các vấn đề với SDK processing
- Theo dõi kết quả detection real-time
- Phân tích performance của SDK
- Troubleshooting SDK integration

**⚠️ Lưu ý:** SDK Output Logs có thể tạo nhiều log (hàng trăm MB mỗi ngày nếu bật với nhiều instances). Chỉ bật khi cần debug, không bật trong production.

**Thông tin được log:**
- Timestamp
- Instance ID và display name
- FPS (Frames Per Second)
- Solution ID
- Processing status
- Detection results

**Ví dụ log:**
```
[SDKOutput] [2025-12-04 14:30:25.123] Instance: Face Detection Camera 1 (abc-123) - FPS: 25.50, Solution: face_detection
[SDKOutput] [2025-12-04 14:30:35.456] Instance: Face Detection File Source (xyz-789) - FPS: 30.00, Solution: face_detection
[SDKOutput] Instance abc-123: Detection result - 3 faces detected
[SDKOutput] Instance abc-123: FPS: 25.50, Latency: 40ms
```

**File location:** `logs/sdk_output/YYYY-MM-DD.log`

**Cách sử dụng:**
```bash
./build/bin/edge_ai_api --log-sdk-output
```

---

### 4. General Logs (`logs/general/`)

**Luôn được ghi** (không cần flag)

**Nội dung:**
- Application startup/shutdown
- System errors
- General application events
- Logs không có prefix đặc biệt

**Ví dụ log:**
```
[INFO] Edge AI API Server starting...
[INFO] Server will listen on: 0.0.0.0:8080
[ERROR] Failed to start instance: abc-123
```

**File location:** `logs/general/YYYY-MM-DD.log`

---

## 🔄 Kết Hợp Nhiều Flags

Bạn có thể kết hợp nhiều logging flags cùng lúc:

```bash
# Log tất cả
./build/bin/edge_ai_api --log-api --log-instance --log-sdk-output

# Hoặc dùng --debug-* prefix
./build/bin/edge_ai_api --debug-api --debug-instance --debug-sdk-output

# Chỉ log API và instance execution
./build/bin/edge_ai_api --log-api --log-instance
```

> **Lưu ý về đường dẫn executable:**
>
> Khi build project với CMake, executable được đặt trong thư mục `build/bin/` thay vì trực tiếp trong `build/`. Đây là cấu hình mặc định của CMake để tổ chức các file output:
> - **Executables** → `build/bin/`
> - **Libraries** → `build/lib/`
> - **Object files** → `build/CMakeFiles/`
>
> Cấu trúc này giúp phân tách rõ ràng giữa các loại file build và giữ cho thư mục build gọn gàng hơn.

---

## 📁 Log Files và Cấu Trúc Thư Mục

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

---

## 📖 Xem Logs

Có 2 cách để xem logs:

### 1. Sử dụng Command Line (truyền thống)

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

### 2. Sử dụng REST API (khuyến nghị)

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

**Xem chi tiết:** [API_REFERENCE.md](API_REFERENCE.md) - Tài liệu đầy đủ về Logs API endpoints

---

## 🔧 Cấu Hình Logging

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

### Cấu Hình Bảo Vệ

| Tham Số | Giá Trị Mặc Định | Có Thể Cấu Hình | Mô Tả |
|---------|------------------|-----------------|-------|
| `max_file_size` | 50MB | ❌ Hardcoded | Kích thước tối đa mỗi file log |
| `LOG_RETENTION_DAYS` | 30 ngày | ✅ Env var | Số ngày giữ log (1-365) |
| `LOG_MAX_DISK_USAGE_PERCENT` | 85% | ✅ Env var | Ngưỡng disk usage để cleanup (50-95%) |
| `LOG_CLEANUP_INTERVAL_HOURS` | 24 giờ | ✅ Env var | Khoảng thời gian kiểm tra cleanup (1-168) |

---

## 🔍 Log Runtime

**Log Runtime** = Log được ghi trong quá trình **runtime** (khi ứng dụng đang chạy), khác với:
- **Compile-time logs**: Log được tạo khi build/compile
- **Static logs**: Log được định nghĩa tĩnh trong code

### Đặc Điểm Log Runtime:

1. **Dynamic**: Được ghi dựa trên events xảy ra khi ứng dụng chạy
2. **Real-time**: Phản ánh trạng thái hiện tại của hệ thống
3. **Categorized**: Được phân loại theo category (API, Instance, SDK, General)
4. **Rotated**: Tự động rotate theo ngày và kích thước file

### Các Loại Log Runtime:

| Loại | Khi Nào Ghi | Ví Dụ |
|------|-------------|-------|
| **API Runtime Logs** | Mỗi API request | `[API] GET /v1/core/health - 5ms` |
| **Instance Runtime Logs** | Khi instance start/stop | `[Instance] Starting instance: abc-123` |
| **SDK Runtime Logs** | Khi SDK xử lý frame | `[SDKOutput] Detection: 3 objects` |
| **General Runtime Logs** | Events hệ thống | `[INFO] Server started on port 8080` |

---

## 📊 Ước Tính Dung Lượng Log

### Giả Định:
- **API logs**: 100 requests/giờ, mỗi log ~200 bytes → ~20KB/giờ → ~480KB/ngày
- **Instance logs**: 10 events/giờ, mỗi log ~300 bytes → ~3KB/giờ → ~72KB/ngày
- **SDK logs**: 30 FPS × 3600s = 108,000 logs/giờ, mỗi log ~150 bytes → ~16MB/giờ → ~384MB/ngày
- **General logs**: 50 events/giờ, mỗi log ~250 bytes → ~12.5KB/giờ → ~300KB/ngày

### Tổng Ước Tính:
- **Một ngày**: ~385MB (chủ yếu từ SDK logs nếu bật)
- **30 ngày**: ~11.5GB (trước khi cleanup)
- **Sau cleanup**: Chỉ giữ 30 ngày gần nhất

### Với Rotation (50MB/file):
- **API**: ~10 files/ngày (nếu bật)
- **Instance**: ~2 files/ngày (nếu bật)
- **SDK**: ~8 files/ngày (nếu bật) ⚠️ **Cao nhất**
- **General**: ~6 files/ngày

---

## 💡 Best Practices

### Development

```bash
# Development với đầy đủ logging
./build/bin/edge_ai_api --log-api --log-instance --log-sdk-output

# Giữ logs lâu hơn
export LOG_RETENTION_DAYS=30
export LOG_MAX_DISK_USAGE_PERCENT=90
```

### Production

```bash
# Production - chỉ log API và instance execution
./build/bin/edge_ai_api --log-api --log-instance

# Hoặc không log gì cả nếu không cần thiết (chỉ general logs)
./build/bin/edge_ai_api

# Cấu hình cleanup tích cực
export LOG_RETENTION_DAYS=7        # Giữ 7 ngày
export LOG_MAX_DISK_USAGE_PERCENT=80  # Cleanup sớm hơn
export LOG_CLEANUP_INTERVAL_HOURS=12  # Kiểm tra mỗi 12 giờ
```

### Debugging

```bash
# Debug một vấn đề cụ thể - bật tất cả logging
./build/bin/edge_ai_api --log-api --log-instance --log-sdk-output

# Sau đó filter logs để tìm vấn đề
tail -f ./logs/general/$(date +%Y-%m-%d).log | grep -E "ERROR|WARNING|Exception"
```

---

## 📈 Monitoring Logs

### Kiểm Tra Dung Lượng Log:

```bash
# Xem tổng dung lượng logs
du -sh logs/

# Xem dung lượng từng category
du -sh logs/api/
du -sh logs/instance/
du -sh logs/sdk_output/
du -sh logs/general/

# Xem số lượng file log
find logs/ -name "*.log" | wc -l
```

### Kiểm Tra Disk Usage:

```bash
# Xem disk usage của thư mục logs
df -h logs/

# Hoặc sử dụng API
curl http://localhost:8080/v1/core/logs
```

---

## 🔧 Troubleshooting

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

---

## ⚠️ Lưu Ý Quan Trọng

1. **SDK Output Logs Có Thể Tạo Nhiều Log**
   - Nếu bật `--log-sdk-output` với nhiều instances chạy 30 FPS
   - Có thể tạo **hàng trăm MB log mỗi ngày**
   - **Khuyến nghị**: Chỉ bật khi cần debug, không bật trong production

2. **Disk Space Monitoring**
   - Hệ thống tự động cleanup khi disk > 85%
   - Nhưng nếu disk đầy quá nhanh, có thể vẫn bị tràn
   - **Khuyến nghị**: Monitor disk usage thường xuyên

3. **Cleanup Thread**
   - Chạy mỗi 24 giờ (có thể cấu hình)
   - Nếu cần cleanup ngay, có thể gọi `LogManager::performCleanup()` thủ công

---

## ✅ Kết Luận

1. **Logging KHÔNG làm tràn bộ nhớ** nhờ:
   - Rotation (50MB/file, daily)
   - Automatic cleanup (30 ngày)
   - Disk space monitoring (85% threshold)

2. **4 loại log đang được ghi**:
   - API logs (khi bật `--log-api`)
   - Instance logs (khi bật `--log-instance`)
   - SDK output logs (khi bật `--log-sdk-output`)
   - General logs (luôn bật)

3. **Log runtime** = Log được ghi khi ứng dụng đang chạy, phản ánh trạng thái real-time của hệ thống

4. **Khuyến nghị**:
   - Production: Chỉ bật general logs
   - Development: Có thể bật tất cả để debug
   - Monitor disk usage thường xuyên
   - Cấu hình cleanup tích cực nếu cần

---

## 📚 Xem Thêm

- [GETTING_STARTED.md](GETTING_STARTED.md) - Hướng dẫn khởi động server
- [ENVIRONMENT_VARIABLES.md](ENVIRONMENT_VARIABLES.md) - Cấu hình biến môi trường
- [DEVELOPMENT_GUIDE.md](DEVELOPMENT_GUIDE.md) - Hướng dẫn phát triển
- [API_REFERENCE.md](API_REFERENCE.md) - Tài liệu đầy đủ về Logs API endpoints
