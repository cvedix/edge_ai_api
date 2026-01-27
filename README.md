# Edge AI API

REST API server cho CVEDIX Edge AI SDK, cho phép điều khiển và giám sát các AI processing instances trên thiết bị biên.

![Edge AI Workflow](docs/image.png)

## 🚀 Quick Start

### Development Setup

```bash
# Full setup (dependencies + build)
./scripts/dev_setup.sh

# Chạy server
./scripts/load_env.sh
```

### Production Setup

```bash
# Full deployment (cần sudo)
sudo ./scripts/prod_setup.sh

# Hoặc sử dụng deploy script trực tiếp
sudo ./deploy/deploy.sh
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

### Build và Cài Đặt Debian Package

#### Build File .deb

```bash
# Build package
./packaging/scripts/build_deb.sh

# Với các tùy chọn
./packaging/scripts/build_deb.sh --clean          # Clean build trước khi build
./packaging/scripts/build_deb.sh --no-build       # Chỉ tạo package từ build có sẵn
./packaging/scripts/build_deb.sh --version 1.0.0  # Set version tùy chỉnh
./packaging/scripts/build_deb.sh --help           # Xem tất cả options
```

**Lưu ý:** Không cần `sudo` để build! Chỉ cần sudo khi **cài đặt** package.

**Yêu cầu build dependencies:**

Các package này cần được cài đặt **trước khi build** Debian package. Script `build_deb.sh` sẽ tự động kiểm tra và báo lỗi nếu thiếu dependencies. Cài đặt với:

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential cmake git \
    debhelper dpkg-dev fakeroot \
    libssl-dev zlib1g-dev \
    libjsoncpp-dev uuid-dev pkg-config \
    libopencv-dev \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    libmosquitto-dev
```

**Giải thích:**
- `build-essential`, `cmake`, `git`: Công cụ build cơ bản
- `debhelper`, `dpkg-dev`, `fakeroot`: Công cụ để tạo Debian package
- Các thư viện `lib*-dev`: Header files và libraries cần thiết để compile project

**Sau khi build:** File `.deb` sẽ được tạo ở project root với tên `edge-ai-api-{VERSION}-amd64.deb`

#### Cài Đặt và Chạy File .deb Đã Build

**⚠️ Quan trọng - Prerequisites:**

Trước khi cài đặt package, nếu bạn muốn cài OpenCV 4.10 tự động trong quá trình cài đặt, cần cài dependencies trước:

```bash
sudo apt-get update
sudo apt-get install -y unzip cmake make g++ wget
```

**Lý do:** Trong quá trình cài đặt package (`dpkg -i`), hệ thống không cho phép cài đặt thêm packages khác vì dpkg đang giữ lock. Nếu không cài dependencies trước, OpenCV sẽ được bỏ qua và bạn có thể cài sau.

**Các bước cài đặt:**

```bash
# 1. Cài dependencies cho OpenCV (nếu muốn cài OpenCV tự động)
sudo apt-get update
sudo apt-get install -y unzip cmake make g++ wget

# 2. Cài đặt package
sudo dpkg -i edge-ai-api-*.deb

# 3. Nếu có lỗi dependencies, fix với:
sudo apt-get install -f

# 4. Khởi động service
sudo systemctl start edge-ai-api
sudo systemctl enable edge-ai-api  # Tự động chạy khi khởi động

# 5. Kiểm tra service
sudo systemctl status edge-ai-api

# 6. Xem log
sudo journalctl -u edge-ai-api -f

# 7. Test API
curl http://localhost:8080/v1/core/health
```

**Nếu chưa cài OpenCV 4.10, cài sau:**

```bash
sudo apt-get update
sudo apt-get install -y unzip cmake make g++ wget
sudo /opt/edge_ai_api/scripts/build_opencv_safe.sh
sudo systemctl restart edge-ai-api
```

**Quản lý service:**
```bash
sudo systemctl start edge-ai-api      # Khởi động
sudo systemctl stop edge-ai-api       # Dừng
sudo systemctl restart edge-ai-api    # Khởi động lại
sudo systemctl status edge-ai-api     # Kiểm tra trạng thái
```

**Cấu trúc sau khi cài đặt:**
- **Executable**: `/usr/local/bin/edge_ai_api`
- **Libraries**: `/opt/edge_ai_api/lib/` (bundled - tự chứa)
- **Config**: `/opt/edge_ai_api/config/`
- **Data**: `/opt/edge_ai_api/` (instances, solutions, models, logs, etc.)
- **Service**: `/etc/systemd/system/edge-ai-api.service`

Xem chi tiết: [packaging/docs/BUILD_DEB.md](packaging/docs/BUILD_DEB.md)

### Test

```bash
curl http://localhost:8080/v1/core/health
curl http://localhost:8080/v1/core/version
```

---

## 🌐 Khởi Động Server

### Với File .env (Khuyến nghị)

```bash
# Tạo .env từ template
cp .env.example .env
nano .env  # Chỉnh sửa nếu cần

# Load và chạy server
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
curl http://localhost:8080/v1/core/instance

# Start/Stop
curl -X POST http://localhost:8080/v1/core/instance/{id}/start
curl -X POST http://localhost:8080/v1/core/instance/{id}/stop
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
curl http://localhost:8080/v1/core/log
curl "http://localhost:8080/v1/core/log/api?level=ERROR&tail=100"
```

---

## 🚀 Production Deployment

```bash
# Setup với systemd service
sudo ./scripts/prod_setup.sh

# Hoặc sử dụng deploy script
sudo ./deploy/deploy.sh

# Kiểm tra service
sudo systemctl status edge-ai-api
sudo journalctl -u edge-ai-api -f

# Quản lý
sudo systemctl restart edge-ai-api
sudo systemctl stop edge-ai-api
```

Xem chi tiết: [deploy/README.md](deploy/README.md)

---

## ⚠️ Troubleshooting

### Lỗi "Could NOT find Jsoncpp"

```bash
sudo apt-get install libjsoncpp-dev
```

### Lỗi CVEDIX SDK symlinks

```bash
# Chạy lại dev setup để fix symlinks
./scripts/dev_setup.sh --skip-deps --skip-build
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
| [docs/SCRIPTS.md](docs/SCRIPTS.md) | Scripts documentation (dev, prod, build) |
| [docs/ENVIRONMENT_VARIABLES.md](docs/ENVIRONMENT_VARIABLES.md) | Env vars |
| [docs/LOGGING.md](docs/LOGGING.md) | Logging guide |
| [docs/DEFAULT_SOLUTIONS_REFERENCE.md](docs/DEFAULT_SOLUTIONS_REFERENCE.md) | Default solutions |
| [deploy/README.md](deploy/README.md) | Production deployment guide |
| [packaging/docs/BUILD_DEB.md](packaging/docs/BUILD_DEB.md) | Build Debian package guide |

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
