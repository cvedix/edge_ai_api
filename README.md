# Edge AI API

REST API server cho CVEDIX Edge AI SDK, cho phép điều khiển và giám sát các AI processing instances trên thiết bị biên.

![Edge AI Workflow](docs/image.png)

## 🚀 Quick Start

### Setup Tự Động (Khuyến Nghị)

```bash
# Development
./setup.sh

# Production
sudo ./setup.sh --production
```

### Build Thủ Công

```bash
# 1. Cài dependencies
./scripts/install_dependencies.sh

# 2. Build
mkdir build && cd build
cmake ..
make -j$(nproc)

# 3. Chạy server
./bin/edge_ai_api
```

### Test

```bash
curl http://localhost:8080/v1/core/health
curl http://localhost:8080/v1/core/version
```

---

## 🌐 Khởi Động Server

### Với File .env (Khuyến nghị)

```bash
cp .env.example .env
nano .env  # Chỉnh sửa nếu cần
./scripts/load_env.sh
```

### Với Logging

```bash
./build/bin/edge_ai_api --log-api --log-instance --log-sdk-output
```

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `API_HOST` | 0.0.0.0 | Server host |
| `API_PORT` | 8080 | Server port |
| `THREAD_NUM` | 0 (auto) | Worker threads |
| `LOG_LEVEL` | INFO | Log level |

Xem đầy đủ: [docs/ENVIRONMENT_VARIABLES.md](docs/ENVIRONMENT_VARIABLES.md)

---

## 📡 API Endpoints

### Core APIs

```bash
curl http://localhost:8080/v1/core/health      # Health check
curl http://localhost:8080/v1/core/version     # Version info
curl http://localhost:8080/v1/core/watchdog    # Watchdog status
curl http://localhost:8080/v1/core/endpoints   # List endpoints
```

### Instance APIs

```bash
# Create instance
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d '{"name": "camera_1", "solution": "face_detection", "autoStart": true}'

# List instances
curl http://localhost:8080/v1/core/instances

# Start/Stop
curl -X POST http://localhost:8080/v1/core/instances/{id}/start
curl -X POST http://localhost:8080/v1/core/instances/{id}/stop
```

### Swagger UI

- **Swagger UI**: http://localhost:8080/swagger
- **OpenAPI Spec**: http://localhost:8080/openapi.yaml

Xem đầy đủ: [docs/API.md](docs/API.md)

---

## 🏗️ Kiến Trúc

```
[Client] → [REST API Server] → [Instance Manager] → [CVEDIX SDK]
                                      ↓
                              [Data Broker] → [Output]
```

**Thành phần:**
- **REST API Server**: Drogon Framework HTTP server
- **Instance Manager**: Quản lý vòng đời instances
- **CVEDIX SDK**: 43+ processing nodes (source, inference, tracker, broker, destination)
- **Data Broker**: Message routing và output publishing

---

## 📊 Logging & Monitoring

```bash
# Development - full logging
./build/bin/edge_ai_api --log-api --log-instance --log-sdk-output

# Production - minimal logging
./build/bin/edge_ai_api --log-api
```

**Logs API:**
```bash
curl http://localhost:8080/v1/core/logs
curl "http://localhost:8080/v1/core/logs/api?level=ERROR&tail=100"
```

---

## 🚀 Production Deployment

```bash
# Setup với systemd service
sudo ./setup.sh --production

# Kiểm tra service
sudo systemctl status edge-ai-api
sudo journalctl -u edge-ai-api -f

# Quản lý
sudo systemctl restart edge-ai-api
sudo systemctl stop edge-ai-api
```

---

## ⚠️ Troubleshooting

### Lỗi "Could NOT find Jsoncpp"

```bash
sudo apt-get install libjsoncpp-dev
```

### Lỗi CVEDIX SDK symlinks

```bash
sudo ./scripts/fix_all_symlinks.sh
```

### Build Drogon lâu

Lần đầu build mất ~5-10 phút để download Drogon. Các lần sau nhanh hơn.

---

## 📚 Tài Liệu

| File | Nội dung |
|------|----------|
| [docs/API.md](docs/API.md) | Full API reference |
| [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) | Development guide & Pre-commit |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | System architecture |
| [docs/ENVIRONMENT_VARIABLES.md](docs/ENVIRONMENT_VARIABLES.md) | Env vars |
| [docs/LOGGING.md](docs/LOGGING.md) | Logging guide |
| [docs/DEFAULT_SOLUTIONS_REFERENCE.md](docs/DEFAULT_SOLUTIONS_REFERENCE.md) | Default solutions |
| [deploy/README.md](deploy/README.md) | Deployment guide |

---

## 🔧 AI System Support

| Vendor | Device | SOC |
|--------|--------|-----|
| Qualcomm | DK2721 | QCS6490 |
| Intel | R360 | Core Ultra |
| NVIDIA | 030 | Jetson AGX Orin |
| NVIDIA | R7300 | Jetson Orin Nano |
| AMD | 2210 | Ryzen 8000 |
| Hailo | 1200/3300 | Hailo-8 |
| Rockchip | OPI5-Plus | RK3588 |

---

## 📝 License

Proprietary - CVEDIX
