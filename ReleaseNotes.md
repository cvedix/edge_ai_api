 # Release Notes - Edge AI API

## 📦 Version Information

**Version:** 2025.0.1.3-Beta  
**Release Date:** 2025-01-XX  
**Build Type:** Release

---

## 🎯 Tổng Quan

**Edge AI API** là REST API server cho CVEDIX Edge AI SDK, cho phép điều khiển và giám sát các AI processing instances trên thiết bị biên thông qua giao diện RESTful API.

### Tính Năng Chính

- ✅ **RESTful API** - Quản lý instances qua HTTP API
- ✅ **Instance Management** - Tạo, cấu hình, khởi động, dừng AI processing instances
- ✅ **Solution Templates** - Quản lý và sử dụng các solution templates có sẵn
- ✅ **Face Recognition** - Hỗ trợ nhận diện khuôn mặt với database management
- ✅ **Real-time Monitoring** - WebSocket support cho monitoring real-time
- ✅ **Swagger UI** - Giao diện web để khám phá và test API
- ✅ **Systemd Integration** - Chạy như system service
- ✅ **Comprehensive Logging** - Logging và monitoring đầy đủ
- ✅ **Multi-Platform Support** - Hỗ trợ nhiều AI hardware platforms

---

## 🏗️ Kiến Trúc

![Architecture](asset/architecture.png)
```
[Client] → [REST API Server] → [Instance Manager] → [CVEDIX SDK]
                                      ↓
                              [Data Broker] → [Output]
                              
```

**Thành phần:**
- **REST API Server**: Drogon Framework HTTP server
- **Instance Manager**: Quản lý vòng đời instances (In-Process hoặc Subprocess mode)
- **CVEDIX SDK**: 43+ processing nodes (source, inference, tracker, broker, destination)
- **Data Broker**: Message routing và output publishing

---

## 📡 API Endpoints

### Core APIs

- `GET /v1/core/health` - Health check
- `GET /v1/core/version` - Version information
- `GET /v1/core/watchdog` - Watchdog status
- `GET /v1/core/endpoints` - List all endpoints with statistics

### Instance Management

- `POST /v1/core/instance` - Tạo instance mới
- `GET /v1/core/instance` - List tất cả instances
- `GET /v1/core/instance/{id}` - Chi tiết instance
- `PUT /v1/core/instance/{id}` - Update instance
- `DELETE /v1/core/instance/{id}` - Xóa instance
- `POST /v1/core/instance/{id}/start` - Khởi động instance
- `POST /v1/core/instance/{id}/stop` - Dừng instance
- `POST /v1/core/instance/{id}/restart` - Khởi động lại instance
- `GET /v1/core/instance/{id}/frame` - Lấy frame mới nhất
- `GET /v1/core/instance/{id}/statistics` - Thống kê instance

### Solution Management

- `GET /v1/core/solution` - List tất cả solutions
- `GET /v1/core/solution/{id}` - Chi tiết solution
- `POST /v1/core/solution` - Tạo solution mới
- `PUT /v1/core/solution/{id}` - Update solution
- `DELETE /v1/core/solution/{id}` - Xóa solution

### Face Recognition

- `POST /v1/recognition/face/database` - Tạo face database
- `GET /v1/recognition/face/database` - List face databases
- `POST /v1/recognition/face/database/{id}/person` - Thêm person
- `GET /v1/recognition/face/database/{id}/person` - List persons
- `POST /v1/recognition/face/database/{id}/person/{personId}/image` - Thêm ảnh

### System & Config

- `GET /v1/core/config` - Get configuration
- `POST /v1/core/config` - Update configuration
- `GET /v1/core/system/info` - System hardware information
- `GET /v1/core/system/status` - System status (CPU, RAM, etc.)
- `GET /v1/core/log` - View logs

### Swagger UI

- `GET /swagger` - Swagger UI interface
- `GET /openapi.yaml` - OpenAPI specification

Xem đầy đủ: [docs/API.md](docs/API.md)

---

## 🔧 AI System Support

Hỗ trợ nhiều AI hardware platforms:

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

## 🏗️ Hướng Dẫn Build Từ Source Code

### Yêu Cầu Hệ Thống

- **OS**: Ubuntu 20.04+ / Debian 10+
- **CMake**: 3.14+
- **Compiler**: GCC 9+ / Clang 10+

### Cài Đặt Dependencies

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential cmake git pkg-config \
    libssl-dev zlib1g-dev \
    libjsoncpp-dev uuid-dev \
    libeigen3-dev \
    libglib2.0-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libgstrtspserver-1.0-dev \
    libmosquitto-dev
```

### Build Project

#### Cách 1: Sử dụng Script Tự Động (Khuyến Nghị)

```bash
# Development setup (tự động cài dependencies và build)
./scripts/dev_setup.sh

# Chạy server
./scripts/load_env.sh
```

#### Cách 2: Build Thủ Công

```bash
# 1. Tạo thư mục build
mkdir build && cd build

# 2. Cấu hình với CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# 3. Build project
make -j$(nproc)

# 4. Chạy server
./bin/edge_ai_api
```

### Build với Tests

```bash
cd build
cmake .. -DBUILD_TESTS=ON
make -j$(nproc)
./bin/edge_ai_api_tests
```

### Kiểm Tra Build

```bash
# Test API
curl http://localhost:8080/v1/core/health
curl http://localhost:8080/v1/core/version

# Xem Swagger UI
# Mở browser: http://localhost:8080/swagger
```

---

## 📦 Hướng Dẫn Build và Cài Đặt Debian Package (.deb)

### Yêu Cầu Build Package

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential cmake git \
    debhelper dpkg-dev pkg-config \
    libssl-dev zlib1g-dev \
    libjsoncpp-dev uuid-dev \
    libeigen3-dev \
    libglib2.0-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libgstrtspserver-1.0-dev \
    libmosquitto-dev
```

### Build Debian Package

Có 3 cách để build package:

#### Cách 1: Dùng Wrapper Script (Khuyến Nghị)

```bash
# Từ project root
./build_deb.sh
```

#### Cách 2: Dùng Đường Dẫn Đầy Đủ

```bash
# Từ project root
./packaging/scripts/build_deb.sh
```

#### Cách 3: Từ Thư Mục Packaging

```bash
cd packaging/scripts
./build_deb.sh
```

**Script tự động thực hiện:**
- ✅ Kiểm tra dependencies
- ✅ Build project với CMake
- ✅ Bundle tất cả shared libraries
- ✅ Tạo file .deb package

> ⚠️ **Lưu ý**: Không cần `sudo` để build! Chỉ cần sudo khi **cài đặt** package sau này.

### Tùy Chọn Build

```bash
# Clean build (xóa build cũ trước)
./packaging/scripts/build_deb.sh --clean

# Chỉ tạo package từ build có sẵn
./packaging/scripts/build_deb.sh --no-build

# Set version tùy chỉnh
./packaging/scripts/build_deb.sh --version 1.0.0

# Xem tất cả options
./packaging/scripts/build_deb.sh --help
```

### Cài Đặt Package

Sau khi build, file `.deb` sẽ được tạo tại project root với tên:
```
edge-ai-api-2025.0.1.3-Beta-amd64.deb
```

**Cài đặt package:**

```bash
# Cài đặt
sudo dpkg -i edge-ai-api-2025.0.1.3-Beta-amd64.deb

# Nếu có lỗi dependencies, chạy:
sudo apt-get install -f

# Khởi động service
sudo systemctl start edge-ai-api

# Enable tự động chạy khi khởi động
sudo systemctl enable edge-ai-api
```

### Kiểm Tra Cài Đặt

```bash
# Kiểm tra service status
sudo systemctl status edge-ai-api

# Xem log
sudo journalctl -u edge-ai-api -f

# Test API
curl http://localhost:8080/v1/core/health
curl http://localhost:8080/v1/core/version
```

### Cấu Trúc Sau Khi Cài Đặt

Sau khi cài đặt package, các file sẽ được đặt tại:

- **Executable**: `/usr/local/bin/edge_ai_api`
- **Libraries**: `/opt/edge_ai_api/lib/` (bundled - tất cả trong một nơi)
- **Config**: `/opt/edge_ai_api/config/`
- **Data**: `/opt/edge_ai_api/` (instances, solutions, models, logs, etc.)
- **Service**: `/etc/systemd/system/edge-ai-api.service`

### Quản Lý Service

```bash
# Khởi động
sudo systemctl start edge-ai-api

# Dừng
sudo systemctl stop edge-ai-api

# Khởi động lại
sudo systemctl restart edge-ai-api

# Xem status
sudo systemctl status edge-ai-api

# Xem log
sudo journalctl -u edge-ai-api -n 100
```

### Gỡ Cài Đặt

```bash
# Gỡ package
sudo dpkg -r edge-ai-api

# Hoặc gỡ hoàn toàn (bao gồm config files)
sudo dpkg -P edge-ai-api
```

---

## 🚀 Production Deployment

### Sử dụng Production Setup Script

```bash
# Full deployment (cần sudo)
sudo ./scripts/prod_setup.sh

# Hoặc sử dụng deploy script trực tiếp
sudo ./deploy/deploy.sh
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

## 🔧 Tính Năng Package

✅ **Bundled Libraries**: Tất cả shared libraries được bundle vào package  
✅ **RPATH Configuration**: Executable tự động tìm libraries trong package  
✅ **Systemd Integration**: Tự động tạo và enable systemd service  
✅ **User Management**: Tự động tạo user `edgeai`  
✅ **Directory Structure**: Tự động tạo cấu trúc thư mục cần thiết  
✅ **ldconfig**: Tự động cấu hình ldconfig để tìm libraries  

---

## 📝 Tóm Tắt

| Bước | Lệnh | Cần Sudo? |
|------|------|-----------|
| **Cài dependencies** | `sudo apt-get install -y build-essential cmake ...` | ✅ **CÓ** |
| **Build từ source** | `./scripts/dev_setup.sh` | ❌ **KHÔNG** |
| **Build .deb** | `./build_deb.sh` | ❌ **KHÔNG** |
| **Cài đặt package** | `sudo dpkg -i *.deb` | ✅ **CÓ** |
| **Khởi động service** | `sudo systemctl start edge-ai-api` | ✅ **CÓ** |

---

## 🐛 Troubleshooting

### Lỗi Build: "Could NOT find Jsoncpp"

```bash
sudo apt-get install libjsoncpp-dev
```

### Lỗi Build: "dpkg-buildpackage: command not found"

```bash
sudo apt-get install -y dpkg-dev debhelper
```

### Lỗi: "Could not find required libraries"

Đảm bảo CVEDIX SDK đã được cài đặt tại `/opt/cvedix/lib` hoặc libraries đã được bundle vào package.

### Lỗi: "Service failed to start"

Kiểm tra log:
```bash
sudo journalctl -u edge-ai-api -n 50
```

Kiểm tra permissions:
```bash
sudo chown -R edgeai:edgeai /opt/edge_ai_api
```

### Libraries không được tìm thấy

Kiểm tra ldconfig:
```bash
sudo ldconfig -v | grep edge-ai-api
```

Nếu không có, chạy lại:
```bash
sudo ldconfig
```

### CVEDIX SDK symlinks

```bash
# Chạy lại dev setup để fix symlinks
./scripts/dev_setup.sh --skip-deps --skip-build
```

---

## ✨ Core Features

### 🧠 Core System
- Health check & version info
- System hardware information (CPU, RAM, Disk, OS, GPU)
- Runtime system status (CPU/RAM usage, load average, uptime)
- Watchdog & health monitor
- Prometheus metrics endpoint
- Endpoint statistics

### 🧾 Logging & Observability
- Quản lý log theo category: `api`, `instance`, `sdk_output`, `general`
- Filter theo level, time range, tail lines
- Truy xuất log theo ngày hoặc realtime

---

## 🤖 AI Processing
- Xử lý ảnh/frame đơn (base64)
- Priority-based queue & rate limiting
- Theo dõi AI runtime status & metrics
- Batch processing endpoint (chưa implement – trả về 501)

---

## ⚙️ Configuration Management
- Get / Update / Replace toàn bộ system configuration
- Update & delete config theo path (query & path parameter)
- Reset config về mặc định
- Persist configuration xuống file

---

## 📦 Instance Management
- Tạo, cập nhật, xóa instance AI
- Start / Stop / Restart instance
- Batch start / stop / restart song song
- Persistent instance (auto-load khi restart service)
- AutoStart / AutoRestart

### Runtime & Output
- Lấy runtime status, FPS, latency, statistics
- Truy xuất output (FILE / RTMP / RTSP)
- Lấy last processed frame (base64 JPEG)
- Cấu hình input source (RTSP / FILE / Manual)
- Stream & record output configuration

---

## 📐 Lines API (Behavior Analysis)
- CRUD crossing lines cho `ba_crossline`
- Realtime update, không cần restart instance
- Hỗ trợ direction, class filter, color RGBA

---

## 🧩 Solution Management
- Danh sách solution mặc định & custom
- CRUD custom solution
- Pipeline-based solution definition
- Sinh tự động schema & example body cho tạo instance

---

## 🗂️ Group Management
- Quản lý group instance
- Gán instance theo group
- Group mặc định & read-only protection

---

## 🧱 Node & Pipeline
- Node template discovery
- Pre-configured node pool
- CRUD node (source, detector, processor, destination, broker)
- Node availability & statistics
- Dynamic parameter schema cho UI

---

## 🎥 Media & Asset Management
### Video
- Upload / list / rename / delete video files

### Model
- Upload / list / rename / delete AI models

### Font
- Upload / list / rename fonts (OSD / rendering)

---

## 👤 Face Recognition
- Face recognition từ ảnh upload
- Face registration & subject management
- Search appearance (cosine similarity)
- Rename & merge subject
- Batch delete / delete all faces
- Hỗ trợ MySQL / PostgreSQL face database
- Fallback sang file-based database

---

## ⚠️ Known Limitations

### Chức Năng Chưa Hoàn Thiện
- AI batch processing endpoint trả về 501 (Not Implemented)

### Build Flags
Một số detector yêu cầu build flags tùy chọn:
- `CVEDIX_WITH_TRT` - TensorRT support
- `CVEDIX_WITH_RKNN` - Rockchip RKNN support  
- `CVEDIX_WITH_PADDLE` - PaddlePaddle support

### Dependencies
- Yêu cầu CVEDIX SDK được cài đặt tại `/opt/cvedix/lib` hoặc bundle vào package
- Một số tính năng yêu cầu GStreamer plugins đầy đủ

---

## 🔧 Breaking Changes
- Không có breaking changes trong version này (first stable release)

---

## 📌 Roadmap (Preview)
- AI batch processing
- Authentication & RBAC
- WebSocket / Event streaming
- Instance template & cloning
- Multi-tenant support

---

## 🧪 API Documentation & Testing

Toàn bộ danh sách API, request/response schema và ví dụ `curl` để **test trực tiếp API** được mô tả chi tiết trong:

- **Swagger UI**: http://localhost:8080/swagger (khi server đang chạy)
- **OpenAPI Spec**: http://localhost:8080/openapi.yaml
- **API Reference**: [docs/API.md](docs/API.md)

**Công cụ test:**
- Swagger UI - Giao diện web tương tác
- Postman Collection - [EDGE_AI_API.postman_collection.json](EDGE_AI_API.postman_collection.json)
- `curl` commands - Xem ví dụ trong [docs/API.md](docs/API.md)



---

## 📚 Tài Liệu Tham Khảo

- [README.md](README.md) - Tổng quan project
- [packaging/docs/BUILD_DEB.md](packaging/docs/BUILD_DEB.md) - Chi tiết build Debian package
- [docs/API.md](docs/API.md) - Full API reference
- [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) - Development guide
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) - System architecture
- [docs/DEFAULT_SOLUTIONS_REFERENCE.md](docs/DEFAULT_SOLUTIONS_REFERENCE.md) - Default solutions
- [deploy/README.md](deploy/README.md) - Production deployment guide

---

## 📞 Hỗ Trợ

Nếu gặp vấn đề, vui lòng:
1. Kiểm tra [Troubleshooting](#-troubleshooting) section
2. Xem log: `sudo journalctl -u edge-ai-api -n 100`
3. Liên hệ support team

