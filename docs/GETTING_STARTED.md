# Hướng Dẫn Khởi Động và Sử Dụng

Tài liệu này hướng dẫn cách khởi động project và sử dụng các API endpoints.

## 🚀 Khởi Động Server

### Cách 1: Chạy Trực Tiếp (Development)

```bash
# Từ thư mục build
cd build
./edge_ai_api
```

**Với Logging (Khuyến nghị cho Development):**
```bash
# Bật tất cả logging
./edge_ai_api --log-api --log-instance --log-sdk-output

# Hoặc chỉ bật một số logging
./edge_ai_api --log-api --log-instance
```

**Xem các options:**
```bash
./edge_ai_api --help
```

Server sẽ khởi động và hiển thị:
```
========================================
Edge AI API Server
========================================
Starting REST API server...

Server will listen on: 0.0.0.0:8080
Available endpoints:
  GET /v1/core/health  - Health check
  GET /v1/core/version - Version information
  GET /swagger         - Swagger UI (all versions)
  GET /v1/swagger      - Swagger UI for API v1
  GET /v2/swagger      - Swagger UI for API v2
  GET /openapi.yaml    - OpenAPI spec (all versions)
  GET /v1/openapi.yaml - OpenAPI spec for v1
  GET /v2/openapi.yaml - OpenAPI spec for v2
```

**Nếu logging được bật, bạn sẽ thấy:**
```
API logging: ENABLED
Instance execution logging: ENABLED
SDK output logging: ENABLED
```

### Cách 2: Chạy với File .env (Khuyến nghị)

```bash
# Từ thư mục project root
# 1. Copy template nếu chưa có
cp .env.example .env

# 2. Chỉnh sửa .env với các giá trị của bạn
nano .env  # hoặc vim .env

# 3. Chạy server với script tự động load .env
./scripts/load_env.sh
```

Script sẽ tự động:
- Load các biến từ file `.env`
- Tìm executable ở đúng vị trí
- Chạy server với cấu hình đã load

**Ví dụ file `.env`:**
```bash
API_HOST=0.0.0.0
API_PORT=8082
WATCHDOG_CHECK_INTERVAL_MS=5000
LOG_LEVEL=INFO
```

### Cách 2b: Chạy với Environment Variables (Thủ công)

```bash
# Cấu hình host và port
export API_HOST=127.0.0.1
export API_PORT=9000

# Chạy server
cd build/bin
./edge_ai_api
```

### Cách 3: Chạy trong Background

```bash
cd build
./edge_ai_api > server.log 2>&1 &
echo $! > server.pid  # Lưu PID để dừng sau
```

Dừng server:
```bash
kill $(cat server.pid)
```

### Cách 4: Sử dụng nohup (Production-like)

```bash
cd build
nohup ./edge_ai_api > server.log 2>&1 &
echo $! > server.pid
```

## 🌐 Cấu Hình Server

### Thay Đổi Host và Port

**Cách 1: Sử dụng File .env (Khuyến nghị nhất)**

1. Tạo/cập nhật file `.env`:
```bash
cp .env.example .env
nano .env
```

2. Chỉnh sửa các giá trị:
```bash
API_HOST=0.0.0.0
API_PORT=8082
```

3. Chạy với script:
```bash
./scripts/load_env.sh
```

**Cách 2: Environment Variables (Thủ công)**
```bash
export API_HOST=0.0.0.0
export API_PORT=8080
cd build/bin
./edge_ai_api
```

**Cách 3: Export trực tiếp khi chạy**
```bash
API_PORT=8082 ./build/bin/edge_ai_api
```

### Các Biến Môi Trường Khác

Xem file `docs/ENVIRONMENT_VARIABLES.md` để biết đầy đủ các biến có thể cấu hình:

- `WATCHDOG_CHECK_INTERVAL_MS` - Khoảng thời gian kiểm tra watchdog (mặc định: 5000ms)
- `WATCHDOG_TIMEOUT_MS` - Timeout của watchdog (mặc định: 30000ms)
- `HEALTH_MONITOR_INTERVAL_MS` - Khoảng thời gian monitor health (mặc định: 1000ms)
- `CLIENT_MAX_BODY_SIZE` - Kích thước body tối đa (mặc định: 1MB)
- `THREAD_NUM` - Số lượng worker threads (0 = auto-detect)
- `LOG_LEVEL` - Mức độ logging (TRACE/DEBUG/INFO/WARN/ERROR)

### Cấu Hình Threads

Server tự động sử dụng số lượng CPU cores có sẵn (mặc định). Có thể override bằng biến `THREAD_NUM` trong `.env`:
```bash
THREAD_NUM=8  # Số thread cụ thể
THREAD_NUM=0  # Auto-detect (mặc định)
```

## 📡 API Endpoints

### 1. Health Check

**Endpoint:** `GET /v1/core/health`

**Mô tả:** Kiểm tra trạng thái sức khỏe của service

**Request:**
```bash
curl http://localhost:8080/v1/core/health
```

**Response (200 OK):**
```json
{
  "status": "healthy",
  "timestamp": "2024-01-01T00:00:00.000Z",
  "uptime": 3600,
  "service": "edge_ai_api",
  "version": "2025.0.1.1",
  "checks": {
    "uptime": true,
    "service": true
  }
}
```

**Response Codes:**
- `200 OK`: Service healthy
- `503 Service Unavailable`: Service unhealthy

### 2. Version Information

**Endpoint:** `GET /v1/core/version`

**Mô tả:** Lấy thông tin version của service

**Request:**
```bash
curl http://localhost:8080/v1/core/version
```

**Response (200 OK):**
```json
{
  "version": "2025.0.1.1",
  "build_time": "2024-01-01 00:00:00",
  "git_commit": "abc1234",
  "service": "edge_ai_api",
  "api_version": "v1"
}
```

### 3. Watchdog Status

**Endpoint:** `GET /v1/core/watchdog`

**Mô tả:** Lấy trạng thái watchdog và health monitor

**Request:**
```bash
curl http://localhost:8080/v1/core/watchdog
```

**Response (200 OK):**
```json
{
  "watchdog": {
    "enabled": true,
    "check_interval_ms": 5000,
    "timeout_ms": 30000,
    "last_check": "2024-01-01T00:00:00.000Z"
  },
  "health_monitor": {
    "enabled": true,
    "check_interval_ms": 1000,
    "last_heartbeat": "2024-01-01T00:00:00.000Z"
  }
}
```

### 4. Endpoints List

**Endpoint:** `GET /v1/core/endpoints`

**Mô tả:** Lấy danh sách tất cả endpoints có sẵn

**Request:**
```bash
curl http://localhost:8080/v1/core/endpoints
```

**Response (200 OK):**
```json
{
  "endpoints": [
    {
      "path": "/v1/core/health",
      "method": "GET",
      "description": "Health check endpoint"
    },
    {
      "path": "/v1/core/version",
      "method": "GET",
      "description": "Version information endpoint"
    }
  ],
  "total": 2
}
```

### 5. Swagger UI

**Endpoints:**
- `GET /swagger` - Swagger UI cho tất cả versions
- `GET /v1/swagger` - Swagger UI cho API v1
- `GET /v2/swagger` - Swagger UI cho API v2

**Sử dụng:**
Mở trình duyệt và truy cập:
```
http://localhost:8080/swagger
http://localhost:8080/v1/swagger
http://localhost:8080/v2/swagger
```

**Lưu ý:**
- Swagger UI tự động lấy server URL từ biến môi trường (`API_HOST` và `API_PORT`)
- Server URL trong OpenAPI spec được cập nhật động dựa trên request host để đảm bảo browser có thể truy cập
- Nếu `API_HOST=0.0.0.0`, Swagger UI sẽ tự động sử dụng `localhost` hoặc host từ request header
- CORS đã được cấu hình để cho phép cross-origin requests

**Ví dụ:**
- Nếu server chạy trên port 8082: `http://localhost:8082/v1/swagger`
- Swagger UI sẽ tự động sử dụng `http://localhost:8082` làm server URL để test API

**Tính năng Logging:**
- Server hỗ trợ các tính năng logging chi tiết để debug và monitor
- Xem chi tiết: [LOGGING.md](LOGGING.md)
- Các logging flags: `--log-api`, `--log-instance`, `--log-sdk-output`

### 6. OpenAPI Specification

**Endpoints:**
- `GET /openapi.yaml` - OpenAPI spec cho tất cả versions
- `GET /v1/openapi.yaml` - OpenAPI spec cho API v1
- `GET /v2/openapi.yaml` - OpenAPI spec cho API v2

**Request:**
```bash
curl http://localhost:8080/openapi.yaml
curl http://localhost:8080/v1/openapi.yaml
curl http://localhost:8080/v2/openapi.yaml
```

**Lưu ý:**
- Server URLs trong OpenAPI spec được cập nhật động từ biến môi trường
- URLs sẽ tự động thay đổi theo `API_HOST` và `API_PORT` được cấu hình

## 🧪 Testing APIs

### Sử dụng curl

```bash
# Health check
curl -X GET http://localhost:8080/v1/core/health

# Version
curl -X GET http://localhost:8080/v1/core/version

# Watchdog
curl -X GET http://localhost:8080/v1/core/watchdog

# Endpoints list
curl -X GET http://localhost:8080/v1/core/endpoints

# OpenAPI spec
curl -X GET http://localhost:8080/openapi.yaml

# Logs API
curl -X GET http://localhost:8080/v1/core/logs
curl -X GET http://localhost:8080/v1/core/logs/api
curl -X GET http://localhost:8080/v1/core/logs/api/2025-01-15
curl -X GET "http://localhost:8080/v1/core/logs/api?level=ERROR&tail=100"
```

### Sử dụng httpie (nếu có)

```bash
# Cài đặt httpie
pip install httpie

# Sử dụng
http GET localhost:8080/v1/core/health
http GET localhost:8080/v1/core/version
```

### Sử dụng Postman

1. Import OpenAPI spec:
   - Mở Postman
   - Import → Link
   - Nhập: `http://localhost:8080/openapi.yaml`
   - Postman sẽ tự động tạo collection với tất cả endpoints

2. Hoặc tạo request thủ công:
   - Method: GET
   - URL: `http://localhost:8080/v1/core/health`

### Sử dụng Swagger UI

1. Mở trình duyệt
2. Truy cập: `http://localhost:8080/swagger` hoặc `http://localhost:8080/v1/swagger`
3. Test các endpoints trực tiếp từ UI
4. Xem OpenAPI specification tại `/openapi.yaml`

**Tính năng Swagger UI:**
- Tự động cập nhật server URL từ biến môi trường
- Test API trực tiếp từ browser
- Xem tất cả endpoints và schemas
- Export OpenAPI specification
- Hỗ trợ CORS để test từ bất kỳ domain nào

**Lưu ý về Logging:**
- Khi sử dụng Swagger UI để test API, bạn có thể bật logging để theo dõi requests
- Chạy server với `--log-api` để xem tất cả API requests/responses trong logs
- Xem chi tiết: [LOGGING.md](LOGGING.md)

## 🔍 Monitoring và Logs

### Logging Features

Server hỗ trợ các tính năng logging chi tiết:

- **API Logging** (`--log-api`): Log tất cả API requests/responses
- **Instance Execution Logging** (`--log-instance`): Log instance lifecycle (start/stop)
- **SDK Output Logging** (`--log-sdk-output`): Log output từ SDK khi instance xử lý

**Xem chi tiết:** [LOGGING.md](LOGGING.md)

### Xem Logs

Có 2 cách để xem logs:

**1. Sử dụng Command Line:**
```bash
# Xem log real-time
tail -f ./logs/log.txt

# Filter theo loại log
tail -f ./logs/log.txt | grep "\[API\]"
tail -f ./logs/log.txt | grep "\[Instance\]"
tail -f ./logs/log.txt | grep "\[SDKOutput\]"
```

**Nếu chạy với output redirect:**
```bash
tail -f server.log
```

**2. Sử dụng REST API (khuyến nghị):**

Edge AI API Server cung cấp các endpoints để truy cập logs qua REST API:

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
```

**Xem chi tiết:** [LOGS_API.md](LOGS_API.md) - Tài liệu đầy đủ về Logs API endpoints

### Kiểm Tra Server Đang Chạy

```bash
# Kiểm tra process
ps aux | grep edge_ai_api

# Kiểm tra port
netstat -tuln | grep 8080
# hoặc
ss -tuln | grep 8080

# Test connectivity
curl http://localhost:8080/v1/core/health
```

### Graceful Shutdown

Server hỗ trợ graceful shutdown:
- Nhấn `Ctrl+C` để dừng server
- Server sẽ:
  1. Dừng nhận request mới
  2. Xử lý các request đang chạy
  3. Dừng watchdog và health monitor
  4. Cleanup resources
  5. Thoát

## 🐛 Troubleshooting

### Server không khởi động

**Lỗi: "Address already in use"**
```bash
# Port đã được sử dụng, tìm process
lsof -i :8080
# hoặc
netstat -tuln | grep 8080

# Dừng process hoặc đổi port
export API_PORT=8081
./edge_ai_api
```

**Lỗi: "Permission denied"**
```bash
# Kiểm tra quyền thực thi
chmod +x edge_ai_api

# Hoặc chạy với sudo (không khuyến nghị)
sudo ./edge_ai_api
```

### API không phản hồi

1. **Kiểm tra server đang chạy:**
```bash
ps aux | grep edge_ai_api
```

2. **Kiểm tra firewall:**
```bash
# Ubuntu/Debian
sudo ufw status
sudo ufw allow 8080

# CentOS/RHEL
sudo firewall-cmd --list-ports
sudo firewall-cmd --add-port=8080/tcp --permanent
sudo firewall-cmd --reload
```

3. **Kiểm tra kết nối:**
```bash
curl -v http://localhost:8080/v1/core/health
```

### Response chậm

- Kiểm tra tài nguyên hệ thống (CPU, memory)
- Kiểm tra logs để xem có lỗi không
- Kiểm tra network connectivity

## 📊 Performance

### Cấu Hình Tối Ưu

Server mặc định sử dụng:
- Threads: Số lượng CPU cores
- Max body size: 1MB
- Log level: Info

Có thể tùy chỉnh trong `src/main.cpp`:
```cpp
.setClientMaxBodySize(10 * 1024 * 1024)  // 10MB
.setLogLevel(trantor::Logger::kWarn)      // Chỉ log warnings
.setThreadNum(4)                          // 4 threads
```

## 🔐 Security Notes

### Development
- Server chạy trên `0.0.0.0:8080` (accessible từ mọi interface)
- Không có authentication/authorization
- CORS được enable cho tất cả origins

### Production
- Nên chạy sau reverse proxy (nginx, Apache)
- Thêm authentication/authorization
- Cấu hình CORS phù hợp
- Sử dụng HTTPS
- Giới hạn rate limiting
- Logging và monitoring

## 📚 Tài Liệu Liên Quan

- [Setup Môi Trường Phát Triển](DEVELOPMENT_SETUP.md)
- [Hướng Dẫn Phát Triển](DEVELOPMENT_GUIDE.md)
- [Architecture](architecture.md)
- [OpenAPI Specification](../openapi.yaml)

