# Environment Variables Documentation

## Tổng Quan

Dự án Edge AI API sử dụng biến môi trường để cấu hình server và các thành phần. C++ sử dụng `std::getenv()` để đọc biến môi trường từ hệ thống.

> **📖 Xem thêm:**
> - [Unified Configuration Approach](CONFIG_UNIFIED_APPROACH.md) - **Cách tiếp cận thống nhất** giữa config.json và env vars
> - [Development Setup](DEVELOPMENT_SETUP.md) - Hướng dẫn chi tiết về cách xử lý tạo thư mục tự động với fallback

## ⚡ Unified Configuration

**config.json có ưu tiên cao hơn biến môi trường** - Xem chi tiết tại [CONFIG_UNIFIED_APPROACH.md](CONFIG_UNIFIED_APPROACH.md)

Ví dụ:
- `config.json` có `port: 8080` → Server chạy trên port 8080 (ưu tiên)
- Set `API_PORT=9000` nhưng config.json có port → Vẫn dùng port 8080 từ config.json
- Nếu `config.json` không có port → Mới dùng `API_PORT=9000` (fallback)

## Cách Sử Dụng

### Cách 1: Export Trực Tiếp (Đơn giản nhất)

```bash
export API_HOST=0.0.0.0
export API_PORT=8080
./build/edge_ai_api
```

### Cách 2: Sử Dụng File .env với Script

1. Copy `.env.example` thành `.env`:
```bash
cp .env.example .env
```

2. Chỉnh sửa `.env` với các giá trị của bạn

3. Chạy server với script:
```bash
./scripts/load_env.sh
```

Hoặc load thủ công:
```bash
set -a
source .env
set +a
./build/edge_ai_api
```

### Cách 3: Sử Dụng systemd Service

File `deploy/edge-ai-api.service` đã cấu hình sẵn:
```ini
Environment="API_HOST=0.0.0.0"
Environment="API_PORT=8080"
```

## Biến Môi Trường Hiện Tại

### ✅ Đã Implement (Production Ready)

#### Server Configuration
| Biến | Mô tả | Mặc định | File sử dụng |
|------|-------|----------|--------------|
| `CONFIG_FILE` | Đường dẫn đến file config.json | Tự động tìm: `./config.json` → `/opt/edge_ai_api/config/config.json` → `/etc/edge_ai_api/config.json` | `src/main.cpp` |
| `API_HOST` | Địa chỉ host để bind server | Override từ `config.json["system"]["web_server"]["ip_address"]` | `src/config/system_config.cpp` |
| `API_PORT` | Port của HTTP server | Override từ `config.json["system"]["web_server"]["port"]` | `src/config/system_config.cpp` |
| `CLIENT_MAX_BODY_SIZE` | Kích thước body tối đa (bytes) | `1048576` (1MB) | `src/main.cpp` |
| `CLIENT_MAX_MEMORY_BODY_SIZE` | Kích thước memory body tối đa (bytes) | `1048576` (1MB) | `src/main.cpp` |
| `THREAD_NUM` | Số lượng worker threads (0 = auto, minimum 8 for AI) | `0` | `src/main.cpp` |
| `LOG_LEVEL` | Mức độ logging (TRACE/DEBUG/INFO/WARN/ERROR) | Override từ `config.json["system"]["logging"]["log_level"]` | `src/config/system_config.cpp` |
| `MAX_RUNNING_INSTANCES` | Số lượng instances tối đa (0 = unlimited) | Override từ `config.json["system"]["max_running_instances"]` | `src/config/system_config.cpp` |

#### Configuration File
| Biến | Mô tả | Mặc định | File sử dụng |
|------|-------|----------|--------------|
| `CONFIG_FILE` | Đường dẫn tuyệt đối đến file config.json | Tự động tìm theo thứ tự:<br/>1. `./config.json` (thư mục hiện tại)<br/>2. `/opt/edge_ai_api/config/config.json`<br/>3. `/etc/edge_ai_api/config.json`<br/>4. Tạo mới `./config.json` | `src/main.cpp` |

**Ví dụ sử dụng CONFIG_FILE:**
```bash
# Sử dụng đường dẫn tùy chỉnh
export CONFIG_FILE="/opt/edge_ai_api/config/config.json"
./build/edge_ai_api

# Hoặc trong systemd service
Environment="CONFIG_FILE=/opt/edge_ai_api/config/config.json"
```

**Lưu ý:** Nếu file không tồn tại, hệ thống sẽ tự động tạo file config mặc định tại đường dẫn đó.

#### Logging Configuration
| Biến | Mô tả | Mặc định | File sử dụng |
|------|-------|----------|--------------|
| `LOG_DIR` | Thư mục lưu log files | Override thư mục của `config.json["system"]["logging"]["log_file"]` | `src/config/system_config.cpp` |
| `LOG_RETENTION_DAYS` | Số ngày giữ logs (tự động xóa sau thời gian này) | `30` | `src/core/log_manager.cpp` |
| `LOG_MAX_DISK_USAGE_PERCENT` | Ngưỡng dung lượng đĩa để trigger cleanup (%) | `85` | `src/core/log_manager.cpp` |
| `LOG_CLEANUP_INTERVAL_HOURS` | Khoảng thời gian kiểm tra và cleanup (giờ) | `24` | `src/core/log_manager.cpp` |

#### Performance Optimization Settings
| Biến | Mô tả | Mặc định | File sử dụng |
|------|-------|----------|--------------|
| `KEEPALIVE_REQUESTS` | Số requests giữ connection alive | `100` | `src/main.cpp` |
| `KEEPALIVE_TIMEOUT` | Timeout cho keep-alive (seconds) | `60` | `src/main.cpp` |
| `ENABLE_REUSE_PORT` | Enable port reuse cho load distribution | `true` | `src/main.cpp` |

**Lưu ý về Swagger UI:**
- Swagger UI tự động sử dụng `API_HOST` và `API_PORT` để cấu hình server URL
- Nếu `API_HOST=0.0.0.0`, Swagger UI sẽ tự động thay thế bằng `localhost` hoặc host từ request header để đảm bảo browser có thể truy cập
- Server URLs trong OpenAPI spec được cập nhật động khi serve, không cần restart server khi thay đổi port

#### Watchdog Configuration
| Biến | Mô tả | Mặc định | File sử dụng |
|------|-------|----------|--------------|
| `WATCHDOG_CHECK_INTERVAL_MS` | Khoảng thời gian kiểm tra watchdog (ms) | `5000` | `src/main.cpp` |
| `WATCHDOG_TIMEOUT_MS` | Timeout của watchdog (ms) | `30000` | `src/main.cpp` |

#### Health Monitor Configuration
| Biến | Mô tả | Mặc định | File sử dụng |
|------|-------|----------|--------------|
| `HEALTH_MONITOR_INTERVAL_MS` | Khoảng thời gian monitor health (ms) | `1000` | `src/main.cpp` |

#### Data Storage Configuration
| Biến | Mô tả | Mặc định | File sử dụng |
|------|-------|----------|--------------|
| `SOLUTIONS_DIR` | Thư mục lưu trữ custom solutions | `./solutions` | `src/main.cpp` |
| `INSTANCES_DIR` | Thư mục lưu trữ instance configurations | `/opt/edge_ai_api/instances` | `src/main.cpp` |
| `MODELS_DIR` | Thư mục lưu trữ model files | `./models` | `src/main.cpp` |

**Lưu ý về Storage Directories:**
- **Default**: `/opt/edge_ai_api/instances` (tự động tạo nếu chưa tồn tại)
- **Development**: Có thể override bằng biến môi trường `INSTANCES_DIR=./instances` để lưu ở project root
- **Production**: Khuyến nghị sử dụng mặc định `/opt/edge_ai_api/instances` hoặc `/var/lib/edge_ai_api/instances`
- **⚠️ Không nên lưu trong `build/` directory** - Dữ liệu có thể bị mất khi clean build
- Xem chi tiết: [Development Setup](DEVELOPMENT_SETUP.md) - Hướng dẫn tạo thư mục tự động với fallback

#### CVEDIX SDK Configuration (Example)
| Biến | Mô tả | Mặc định | File sử dụng |
|------|-------|----------|--------------|
| `CVEDIX_DATA_ROOT` | Thư mục gốc cho CVEDIX data/models | `./cvedix_data/` | `main.cpp` |
| `CVEDIX_RTSP_URL` | URL nguồn RTSP cho video stream | (hardcoded) | `main.cpp` |
| `CVEDIX_RTMP_URL` | URL output RTMP cho streaming | (hardcoded) | `main.cpp` |
| `DISPLAY` | X11 Display (tự động detect) | (auto) | `main.cpp` |
| `WAYLAND_DISPLAY` | Wayland Display (tự động detect) | (auto) | `main.cpp` |

#### RTSP Transport Protocol Configuration
| Biến | Mô tả | Mặc định | File sử dụng |
|------|-------|----------|--------------|
| `GST_RTSP_PROTOCOLS` | GStreamer RTSP transport protocol (`tcp` hoặc `udp`) | `tcp` | `src/core/pipeline_builder.cpp` |
| `RTSP_TRANSPORT` | Alternative name cho `GST_RTSP_PROTOCOLS` (`tcp` hoặc `udp`) | (auto-set to `tcp`) | `src/core/pipeline_builder.cpp` |

#### Subprocess Worker Configuration
| Biến | Mô tả | Mặc định | File sử dụng |
|------|-------|----------|--------------|
| `EDGE_AI_EXECUTION_MODE` | Execution mode: `in-process` hoặc `subprocess` | `in-process` | `src/main.cpp` |
| `EDGE_AI_WORKER_PATH` | Đường dẫn đến worker executable | `edge_ai_worker` | `src/worker/worker_supervisor.cpp` |
| `EDGE_AI_SOCKET_DIR` | Thư mục chứa Unix socket files cho IPC | `/opt/edge_ai_api/run` | `src/worker/unix_socket.cpp` |

**Lưu ý về Socket Directory:**
- **Default**: `/opt/edge_ai_api/run` (tự động tạo nếu chưa tồn tại)
- **Fallback**: Nếu không thể tạo `/opt/edge_ai_api/run` (permission denied), sẽ tự động fallback về `/tmp`
- **Production**: Khuyến nghị sử dụng `/opt/edge_ai_api/run` hoặc `/var/run/edge_ai` (nếu có quyền)
- **Development**: Có thể override bằng `EDGE_AI_SOCKET_DIR=/tmp` để test
- Socket files sẽ có format: `{EDGE_AI_SOCKET_DIR}/edge_ai_worker_{instance_id}.sock`

**Lưu ý về RTSP Transport:**
- **Mặc định sử dụng TCP**: Để tránh vấn đề firewall chặn UDP, hệ thống mặc định sử dụng TCP
- **UDP nhanh hơn nhưng dễ bị firewall block**: Chỉ dùng UDP khi trong cùng network và firewall cho phép
- **Cách set**: `export GST_RTSP_PROTOCOLS=tcp` hoặc `export RTSP_TRANSPORT=tcp`
- Xem thêm: [RTSP Troubleshooting Guide](./RTSP_TROUBLESHOOTING.md)

### 📝 Có Thể Implement (Future)

Các biến sau có thể được thêm vào trong tương lai:

- `AI_REQUEST_TIMEOUT_MS` - Timeout cho AI processing requests (hiện tại: 30000ms hardcoded trong `src/api/ai_handler.cpp`)
- `ENDPOINT_MAX_RESPONSE_TIME_MS` - Threshold cho healthy endpoint (hiện tại: 1000ms hardcoded)
- `ENDPOINT_MAX_ERROR_RATE` - Threshold error rate cho healthy endpoint (hiện tại: 0.1 hardcoded)
- `RATE_LIMITER_CLEANUP_INTERVAL_SEC` - Cleanup interval cho rate limiter (hiện tại: 300s constexpr)
- `AI_CACHE_CLEANUP_INTERVAL_SEC` - Cleanup interval cho AI cache (hiện tại: 60s constexpr)

Xem `docs/HARDCODE_AUDIT.md` để biết chi tiết.

## Ví Dụ Cấu Hình

### Development
```bash
export API_HOST=127.0.0.1
export API_PORT=8080
```

### Production
```bash
export API_HOST=0.0.0.0
export API_PORT=80
export SOLUTIONS_DIR=/var/lib/edge_ai_api/solutions
export INSTANCES_DIR=/var/lib/edge_ai_api/instances
export MODELS_DIR=/var/lib/edge_ai_api/models
export LOG_DIR=/var/log/edge_ai_api
```

### Custom Port
```bash
export API_PORT=9000
```

## Lưu Ý

1. **File .env không được commit vào git** - Đã được thêm vào `.gitignore`
2. **File .env.example được commit** - Dùng làm template
3. **C++ không có built-in .env parser** - Phải export biến trước khi chạy
4. **systemd service** - Sử dụng `Environment=` directive trong service file

## Tương Lai

Có thể thêm một thư viện C++ nhẹ để parse `.env` file tự động, ví dụ:
- [cpp-dotenv](https://github.com/adeharo9/cpp-dotenv)
- Hoặc tự implement một parser đơn giản

Hiện tại, cách tiếp cận hiện tại (export + std::getenv) là đủ cho hầu hết use cases.
