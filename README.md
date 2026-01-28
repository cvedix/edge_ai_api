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

Có 2 loại package có thể build:

**1. ALL-IN-ONE Package (Khuyến nghị - Ưu tiên):**
```bash
# Build ALL-IN-ONE package (tự chứa TẤT CẢ dependencies)
./packaging/scripts/build_deb_all_in_one.sh --sdk-deb <path-to-sdk.deb>

# Ví dụ:
./packaging/scripts/build_deb_all_in_one.sh \
    --sdk-deb ../cvedix-ai-runtime-2025.0.1.3-x86_64.deb

# Với các tùy chọn
./packaging/scripts/build_deb_all_in_one.sh --sdk-deb <path> --clean
./packaging/scripts/build_deb_all_in_one.sh --sdk-deb <path> --no-build
```

**2. Package thông thường:**
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

Các package này cần được cài đặt **trước khi build** Debian package. Script sẽ tự động kiểm tra và báo lỗi nếu thiếu dependencies:

```bash
sudo apt update
sudo apt install -y \
    build-essential cmake git pkg-config \
    debhelper dpkg-dev fakeroot \
    libssl-dev zlib1g-dev \
    libjsoncpp-dev uuid-dev \
    libopencv-dev \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    libmosquitto-dev \
    gstreamer1.0-libav \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-ugly \
    libfreetype6-dev libharfbuzz-dev \
    libjpeg-dev libpng-dev libtiff-dev \
    libavcodec-dev libavformat-dev libswscale-dev \
    libgtk-3-dev
```

**Giải thích:**
- `build-essential`, `cmake`, `git`, `pkg-config`: Công cụ build cơ bản
- `debhelper`, `dpkg-dev`, `fakeroot`: Công cụ để tạo Debian package
- Các thư viện `lib*-dev`: Header files và libraries cần thiết để compile project
  - `libfreetype6-dev`, `libharfbuzz-dev`: Font rendering libraries
  - `libjpeg-dev`, `libpng-dev`, `libtiff-dev`: Image format libraries
  - `libavcodec-dev`, `libavformat-dev`, `libswscale-dev`: FFmpeg libraries cho video processing
  - `libgtk-3-dev`: GTK+ development libraries
- GStreamer plugins: Cần thiết cho ALL-IN-ONE package để bundle plugins

**Sau khi build:**
- Package thông thường: `edge-ai-api-{VERSION}-amd64.deb`
- ALL-IN-ONE package: `edge-ai-api-all-in-one-{VERSION}-amd64.deb`

#### Cài Đặt và Chạy File .deb Đã Build

**⚠️ Khuyến nghị: Sử dụng ALL-IN-ONE package** - Tự chứa tất cả dependencies, không cần cài thêm packages.

**⚠️ QUAN TRỌNG - Đọc Trước Khi Cài Đặt:**

Trước khi cài đặt package `.deb`, bạn **BẮT BUỘC** phải:
1. ✅ Cập nhật package list (`sudo apt-get update`)
2. ✅ Cài đặt system libraries cơ bản (libc6, libstdc++6, libgcc-s1, adduser, systemd)
3. ✅ Cài đặt dependencies cho OpenCV (nếu package chưa bundle OpenCV)
4. ✅ (Tùy chọn) Cài đặt GStreamer plugins

**Lý do:** Trong quá trình cài đặt package (`dpkg -i`), hệ thống không cho phép cài đặt thêm packages khác vì dpkg đang giữ lock. Nếu không chuẩn bị dependencies trước, quá trình cài đặt sẽ thất bại hoặc OpenCV không thể cài đặt tự động.

Xem chi tiết phần Prerequisites bên dưới để biết các lệnh cài đặt cụ thể.

##### Cài Đặt ALL-IN-ONE Package (Khuyến nghị - Ưu tiên)

**⚠️ BẮT BUỘC - Prerequisites Trước Khi Cài Đặt Package:**

**QUAN TRỌNG:** Để cài đặt package thành công, bạn **BẮT BUỘC** phải chuẩn bị và cài đặt các dependencies sau **TRƯỚC KHI** chạy `dpkg -i`. Nếu không chuẩn bị đầy đủ, quá trình cài đặt sẽ thất bại hoặc gặp lỗi.

**Bước 1: Cập Nhật Package List**
```bash
sudo apt-get update
```

**Bước 2: Cài Đặt System Libraries Cơ Bản (BẮT BUỘC)**
```bash
sudo apt-get install -y \
    libc6 \
    libstdc++6 \
    libgcc-s1 \
    adduser \
    systemd
```

**Bước 3: Cài Đặt Dependencies Cho OpenCV (Nếu Package Chưa Bundle OpenCV)**

Nếu package chưa bundle OpenCV 4.10, bạn cần cài đặt các dependencies để OpenCV có thể được cài đặt tự động trong quá trình cài package:

```bash
sudo apt-get install -y \
    unzip \
    cmake \
    make \
    g++ \
    wget \
    build-essential \
    pkg-config \
    libfreetype6-dev \
    libharfbuzz-dev \
    libjpeg-dev \
    libpng-dev \
    libtiff-dev \
    libavcodec-dev \
    libavformat-dev \
    libswscale-dev \
    libgtk-3-dev \
    gfortran \
    openexr \
    libatlas-base-dev \
    python3-dev \
    python3-numpy
```

**Bước 4: (Tùy chọn) Cài Đặt GStreamer Plugins Trước**

Để đảm bảo GStreamer plugins hoạt động tốt, bạn có thể cài đặt trước:

```bash
sudo apt-get install -y \
    gstreamer1.0-libav \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-ugly \
    gstreamer1.0-tools
```

**Lưu ý quan trọng:**
- ⚠️ **KHÔNG** bỏ qua các bước trên! Cài đặt các dependencies **TRƯỚC KHI** chạy `dpkg -i`.
- Nếu thiếu dependencies, quá trình cài đặt sẽ thất bại hoặc OpenCV không thể cài đặt tự động.
- Trong quá trình cài đặt package (`dpkg -i`), hệ thống không cho phép cài đặt thêm packages khác vì dpkg đang giữ lock.

**Các bước cài đặt package:**

```bash
# Bước 1: Cài đặt package
sudo dpkg -i edge-ai-api-all-in-one-*.deb

# Trong quá trình cài đặt, nếu thiếu OpenCV 4.10, hệ thống sẽ hiển thị:
# ==========================================
# OpenCV 4.10 Installation Required
# ==========================================
# OpenCV 4.10 with freetype support is required for edge_ai_api.
# The installation process will take approximately 30-60 minutes.
#
# Checking disk space...
#   ✓ Sufficient disk space available (27 GB)
# Checking network connectivity...
#   ✓ Network connectivity OK
# Choose an option:
#   1) Install OpenCV 4.10 automatically (recommended)
#   2) Skip installation and install manually later
#
# Chọn option 1 để cài đặt tự động (mất khoảng 30-60 phút)

# Bước 2: Nếu có lỗi dependencies (hiếm khi xảy ra với ALL-IN-ONE)
sudo apt-get install -f

# Bước 3: Nếu OpenCV cài đặt bị lỗi hoặc bị gián đoạn, chạy lại script cài đặt:
sudo /opt/edge_ai_api/scripts/build_opencv_safe.sh

# Bước 4: Khởi động service
sudo systemctl start edge-ai-api
sudo systemctl enable edge-ai-api  # Tự động chạy khi khởi động

# Bước 5: Kiểm tra service
sudo systemctl status edge-ai-api

# Bước 6: Xem log
sudo journalctl -u edge-ai-api -f

# Bước 7: Test API
curl http://localhost:8080/v1/core/health
```

**Lưu ý về OpenCV:**
- ⚠️ **OpenCV 4.10 là BẮT BUỘC** để package hoạt động đúng. Package sẽ không hoạt động với các phiên bản OpenCV khác.
- Nếu package đã bundle OpenCV 4.10, quá trình cài đặt sẽ không yêu cầu cài thêm.
- Nếu thiếu OpenCV 4.10, quá trình cài đặt sẽ tự động phát hiện và cho phép cài đặt tự động (yêu cầu dependencies đã được cài ở Bước 3).
- Nếu cài đặt OpenCV bị lỗi hoặc bị gián đoạn, chạy lại: `sudo /opt/edge_ai_api/scripts/build_opencv_safe.sh`

**Verify Installation:**

```bash
# Kiểm tra package status
dpkg -l | grep edge-ai-api

# Kiểm tra libraries
ls -la /opt/edge_ai_api/lib/

# Kiểm tra GStreamer plugins
ls -la /opt/edge_ai_api/lib/gstreamer-1.0/

# Kiểm tra default fonts
ls -la /opt/edge_ai_api/fonts/

# Kiểm tra default models
ls -la /opt/edge_ai_api/models/

# Kiểm tra CVEDIX SDK
ls -la /opt/cvedix/lib/

# Test executable
/usr/local/bin/edge_ai_api --help

# Kiểm tra service
sudo systemctl status edge-ai-api

# Test API
curl http://localhost:8080/v1/core/health
```

##### Cài Đặt Package Thông Thường

**⚠️ Quan trọng - Prerequisites:**

Trước khi cài đặt package thông thường, cần cài dependencies trước:

```bash
sudo apt-get update
sudo apt-get install -y unzip cmake make g++ wget
```

**Lý do:** Trong quá trình cài đặt package (`dpkg -i`), hệ thống không cho phép cài đặt thêm packages khác vì dpkg đang giữ lock.

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

##### Cài Đặt GStreamer Plugins (Nếu Cần)

ALL-IN-ONE package đã bundle GStreamer libraries và plugins, nhưng trên một số hệ thống production có thể thiếu một số plugins cần thiết cho việc xử lý video:

- **isomp4** (qtdemux): Để đọc file MP4
- **h264parse**: Để parse H.264 video stream
- **avdec_h264**: Để decode H.264 video
- **filesrc**: Để đọc file video
- **videoconvert**: Để convert video format
- **x264enc**: Để encode H.264 (cho RTMP output)
- **flvmux**: Để mux FLV (cho RTMP)
- **rtmpsink**: Để output RTMP stream

**Kiểm tra plugins hiện tại:**

```bash
# Kiểm tra plugins trong bundled directory
export GST_PLUGIN_PATH=/opt/edge_ai_api/lib/gstreamer-1.0
gst-inspect-1.0 isomp4
gst-inspect-1.0 h264parse
gst-inspect-1.0 avdec_h264
gst-inspect-1.0 filesrc
gst-inspect-1.0 videoconvert
```

Nếu các lệnh trên trả về "No such element", plugins chưa được cài đặt.

**Cài đặt GStreamer plugins:**

```bash
# Cài đặt tất cả plugins cần thiết
sudo apt-get update
sudo apt-get install -y \
    gstreamer1.0-libav \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-ugly

# Verify installation
export GST_PLUGIN_PATH=/opt/edge_ai_api/lib/gstreamer-1.0
gst-inspect-1.0 isomp4 | head -5
gst-inspect-1.0 h264parse | head -5
gst-inspect-1.0 avdec_h264 | head -5

# Restart service để áp dụng thay đổi
sudo systemctl restart edge-ai-api

# Kiểm tra logs để xác nhận không còn lỗi thiếu plugins
sudo journalctl -u edge-ai-api -n 50 | grep -i "plugin\|gstreamer"
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
- **GStreamer plugins**: `/opt/edge_ai_api/lib/gstreamer-1.0/`
- **Config**: `/opt/edge_ai_api/config/`
- **Data**: `/opt/edge_ai_api/` (instances, solutions, models, logs, etc.)
- **Fonts**: `/opt/edge_ai_api/fonts/` (default fonts)
- **Models**: `/opt/edge_ai_api/models/` (default models)
- **Service**: `/etc/systemd/system/edge-ai-api.service`

##### Khắc Phục Lỗi Thường Gặp Khi Cài Đặt

**⚠️ Lưu ý:** Hầu hết các lỗi này có thể tránh được nếu bạn đã chuẩn bị đầy đủ dependencies ở phần Prerequisites trên.

**Lỗi: "dpkg: dependency problems prevent configuration"**

Lỗi này xảy ra khi thiếu system libraries cơ bản. **Giải pháp:**

```bash
# Bước 1: Cài đặt các system libraries cơ bản còn thiếu
sudo apt-get update
sudo apt-get install -y \
    libc6 \
    libstdc++6 \
    libgcc-s1 \
    adduser \
    systemd

# Bước 2: Fix dependencies
sudo apt-get install -f

# Bước 3: Thử cài lại package
sudo dpkg -i edge-ai-api-all-in-one-*.deb
```

**Lỗi: "dpkg: error processing package"**

```bash
# Bước 1: Xem chi tiết lỗi
sudo dpkg --configure -a

# Bước 2: Nếu package bị broken, remove và cài lại
sudo dpkg --remove --force-remove-reinstreq edge-ai-api
sudo dpkg -i edge-ai-api-all-in-one-*.deb

# Bước 3: Fix dependencies
sudo apt-get install -f
```

**Lỗi: "E: Sub-process /usr/bin/dpkg returned an error code"**

```bash
# Bước 1: Unlock dpkg nếu bị lock
sudo rm /var/lib/dpkg/lock-frontend
sudo rm /var/lib/dpkg/lock
sudo rm /var/cache/apt/archives/lock

# Bước 2: Reconfigure dpkg
sudo dpkg --configure -a

# Bước 3: Cài lại package
sudo dpkg -i edge-ai-api-all-in-one-*.deb
```

**Lỗi: "Package is in a very bad inconsistent state"**

```bash
# Bước 1: Remove package hoàn toàn
sudo dpkg --remove --force-remove-reinstreq edge-ai-api
sudo apt-get purge edge-ai-api

# Bước 2: Clean up
sudo apt-get autoremove
sudo apt-get autoclean

# Bước 3: Cài lại từ đầu (nhớ chuẩn bị dependencies trước!)
sudo dpkg -i edge-ai-api-all-in-one-*.deb
sudo apt-get install -f
```

**Lỗi: "Missing required plugins" khi start instance**

```bash
# Cài đặt GStreamer plugins (xem phần trên)
# Kiểm tra GST_PLUGIN_PATH trong service file
cat /etc/systemd/system/edge-ai-api.service | grep GST_PLUGIN_PATH

# Đảm bảo GST_PLUGIN_PATH trỏ đến bundled directory
cat /opt/edge_ai_api/config/.env | grep GST_PLUGIN_PATH

# Restart service
sudo systemctl restart edge-ai-api
```

**Lỗi: "GStreamer: pipeline have not been created"**

```bash
# Update GStreamer registry
export GST_PLUGIN_PATH=/opt/edge_ai_api/lib/gstreamer-1.0
gst-inspect-1.0 > /dev/null 2>&1

# Kiểm tra plugins
gst-inspect-1.0 isomp4

# Restart service
sudo systemctl restart edge-ai-api
```

**Lỗi: Service failed to start**

```bash
# Kiểm tra log
sudo journalctl -u edge-ai-api -n 100

# Kiểm tra permissions
sudo chown -R edgeai:edgeai /opt/edge_ai_api
sudo chmod -R 755 /opt/edge_ai_api

# Kiểm tra executable
ls -la /usr/local/bin/edge_ai_api
file /usr/local/bin/edge_ai_api

# Kiểm tra libraries
ldd /usr/local/bin/edge_ai_api | grep "not found"
```

**Lỗi: "Package is in a very bad inconsistent state"**

```bash
# Remove package hoàn toàn
sudo dpkg --remove --force-remove-reinstreq edge-ai-api
sudo apt-get purge edge-ai-api

# Clean up
sudo apt-get autoremove
sudo apt-get autoclean

# Cài lại từ đầu
sudo dpkg -i edge-ai-api-*.deb
sudo apt-get install -f
```

**Kiểm tra toàn bộ cài đặt:**

```bash
# Script validation (nếu có)
sudo /opt/edge_ai_api/scripts/validate_installation.sh --verbose
```

Xem chi tiết:
- [packaging/docs/BUILD_DEB.md](packaging/docs/BUILD_DEB.md) - Build package thông thường
- [packaging/docs/BUILD_ALL_IN_ONE.md](packaging/docs/BUILD_ALL_IN_ONE.md) - Build ALL-IN-ONE package

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
