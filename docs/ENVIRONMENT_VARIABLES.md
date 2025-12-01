# Environment Variables Documentation

## Tổng Quan

Dự án Edge AI API sử dụng biến môi trường để cấu hình server và các thành phần. C++ sử dụng `std::getenv()` để đọc biến môi trường từ hệ thống.

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
| `API_HOST` | Địa chỉ host để bind server | `0.0.0.0` | `src/main.cpp` |
| `API_PORT` | Port của HTTP server | `8080` | `src/main.cpp` |
| `CLIENT_MAX_BODY_SIZE` | Kích thước body tối đa (bytes) | `1048576` (1MB) | `src/main.cpp` |
| `CLIENT_MAX_MEMORY_BODY_SIZE` | Kích thước memory body tối đa (bytes) | `1048576` (1MB) | `src/main.cpp` |
| `THREAD_NUM` | Số lượng worker threads (0 = auto, minimum 8 for AI) | `0` | `src/main.cpp` |
| `LOG_LEVEL` | Mức độ logging (TRACE/DEBUG/INFO/WARN/ERROR) | `INFO` | `src/main.cpp` |

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

#### CVEDIX SDK Configuration (Example)
| Biến | Mô tả | Mặc định | File sử dụng |
|------|-------|----------|--------------|
| `CVEDIX_DATA_ROOT` | Thư mục gốc cho CVEDIX data/models | `./cvedix_data/` | `main.cpp` |
| `CVEDIX_RTSP_URL` | URL nguồn RTSP cho video stream | (hardcoded) | `main.cpp` |
| `CVEDIX_RTMP_URL` | URL output RTMP cho streaming | (hardcoded) | `main.cpp` |
| `DISPLAY` | X11 Display (tự động detect) | (auto) | `main.cpp` |
| `WAYLAND_DISPLAY` | Wayland Display (tự động detect) | (auto) | `main.cpp` |

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

