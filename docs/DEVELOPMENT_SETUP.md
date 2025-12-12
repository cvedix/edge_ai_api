# Hướng Dẫn Setup Môi Trường Phát Triển

Tài liệu này hướng dẫn cách thiết lập môi trường phát triển cho Edge AI API project từ đầu.

## 📋 Yêu Cầu Hệ Thống

### Hệ Điều Hành
- **Ubuntu 20.04+** hoặc **Debian 10+** (khuyến nghị)
- **CentOS 8+** hoặc **RHEL 8+** (có thể sử dụng)

### Dependencies Bắt Buộc

#### 1. Build Tools
```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config

# CentOS/RHEL
sudo yum install -y \
    gcc-c++ \
    cmake \
    git \
    pkgconfig
```

#### 2. Dependencies cho Drogon Framework
```bash
# Ubuntu/Debian
sudo apt-get install -y \
    libssl-dev \
    zlib1g-dev \
    libjsoncpp-dev \
    uuid-dev

# CentOS/RHEL
sudo yum install -y \
    openssl-devel \
    zlib-devel \
    jsoncpp-devel \
    libuuid-devel
```

#### 3. Kiểm Tra Version CMake
```bash
cmake --version
# Cần CMake 3.14 trở lên
```

Nếu version thấp hơn, cài đặt CMake mới:
```bash
# Ubuntu/Debian
sudo apt-get install cmake

# Hoặc build từ source
wget https://github.com/Kitware/CMake/releases/download/v3.27.0/cmake-3.27.0.tar.gz
tar -xzf cmake-3.27.0.tar.gz
cd cmake-3.27.0
./bootstrap && make && sudo make install
```

## 🚀 Cài Đặt Tự Động (Khuyến Nghị)

Project có script tự động cài đặt dependencies:

```bash
# Chạy script cài đặt
./scripts/install_dependencies.sh
```

Script này sẽ:
- Kiểm tra và cài đặt các dependencies cần thiết
- Xác minh version CMake
- Cài đặt các thư viện cần thiết cho Drogon

## 🔧 Cài Đặt Thủ Công

Nếu không muốn dùng script, có thể cài đặt thủ công theo các bước trên.

## 📦 Clone Project

```bash
# Clone repository
git clone https://github.com/cvedix/edge_ai_api.git
cd edge_ai_api

# Hoặc nếu đã có project, đảm bảo đang ở thư mục root
cd /path/to/edge_ai_api
```

## 🏗️ Build Project

### Bước 1: Tạo thư mục build
```bash
mkdir build
cd build
```

### Bước 2: Cấu hình với CMake
```bash
cmake ..
```

**Lưu ý:** Lần đầu tiên chạy `cmake ..` sẽ:
- Tự động download Drogon Framework từ GitHub (nếu chưa có)
- Tự động download jsoncpp (nếu chưa cài trên system)
- Build các dependencies này
- Mất khoảng 5-10 phút tùy máy và kết nối internet

### Drogon Framework Setup

Project đã được cấu hình để **tự động download và build Drogon Framework** khi build project. Không cần cài đặt thủ công!

#### Cách hoạt động

Khi chạy `cmake ..`, CMake sẽ:
1. Tự động download Drogon từ GitHub (nếu chưa có)
2. Build Drogon như một dependency
3. Link Drogon vào project

**Lần đầu tiên:** Sẽ mất thời gian để download và build Drogon (~5-10 phút tùy máy)

**Các lần sau:** Chỉ build project của bạn, rất nhanh

#### Drogon được lưu ở đâu?

Drogon được download và build trong thư mục `build/_deps/drogon-src/` và `build/_deps/drogon-build/`

#### Tùy chọn cấu hình

**Chọn version Drogon:**
```bash
cmake .. -DDROGON_VERSION=v1.9.0
```

**Tắt FetchContent (Dùng Drogon đã cài sẵn):**
```bash
cmake .. -DDROGON_USE_FETCHCONTENT=OFF
```

#### Dependencies của Drogon

Drogon cần các dependencies sau. CMake sẽ tự động tìm hoặc build:

**Bắt buộc:**
- **OpenSSL** - Cho HTTPS support
- **zlib** - Compression
- **jsoncpp** - JSON parsing (hoặc nlohmann_json)
- **libuuid** - UUID generation

**Tùy chọn:**
- **PostgreSQL** - Database support (nếu dùng ORM)
- **MySQL** - Database support (nếu dùng ORM)
- **SQLite** - Database support (nếu dùng ORM)

### Bước 3: Build project
```bash
make -j$(nproc)
```

Sử dụng `-j$(nproc)` để build song song với số lượng CPU cores có sẵn, giúp build nhanh hơn.

### Bước 4: Kiểm tra build thành công
```bash
# Executable sẽ được tạo tại
ls -lh edge_ai_api

# Hoặc
./edge_ai_api --help  # (nếu có)
```

## 🧪 Build với Unit Tests

Để build kèm unit tests:

```bash
cd build
cmake .. -DBUILD_TESTS=ON
make -j$(nproc)
```

Tests sẽ được build và executable nằm tại `build/bin/edge_ai_api_tests`.

## 🔍 Kiểm Tra Cài Đặt

### Test Dependencies
```bash
# Kiểm tra CMake
cmake --version

# Kiểm tra OpenSSL
openssl version

# Kiểm tra jsoncpp
pkg-config --modversion jsoncpp

# Kiểm tra git
git --version
```

### Test Build
```bash
cd build
./edge_ai_api
```

Server sẽ khởi động và hiển thị thông tin endpoints. Nhấn `Ctrl+C` để dừng.

## 🛠️ Cấu Hình Môi Trường Phát Triển

### Environment Variables

Project hỗ trợ cấu hình qua biến môi trường. Có 2 cách:

**Cách 1: Sử dụng File .env (Khuyến nghị)**

```bash
# 1. Copy template
cp .env.example .env

# 2. Chỉnh sửa .env
nano .env

# 3. Chạy server với script tự động load
./scripts/load_env.sh
```

**Cách 2: Export thủ công**

```bash
# Cấu hình host và port
export API_HOST=0.0.0.0
export API_PORT=8080

# Chạy server
cd build/bin
./edge_ai_api
```

**Các biến môi trường hỗ trợ:**

Xem `docs/ENVIRONMENT_VARIABLES.md` để biết đầy đủ. Các biến chính:
- `API_HOST` - Host address
- `API_PORT` - Port number
- `WATCHDOG_CHECK_INTERVAL_MS` - Watchdog interval
- `HEALTH_MONITOR_INTERVAL_MS` - Health monitor interval
- `CLIENT_MAX_BODY_SIZE` - Max request body size
- `THREAD_NUM` - Worker threads (0 = auto)
- `LOG_LEVEL` - Log level (TRACE/DEBUG/INFO/WARN/ERROR)

### IDE Setup (Optional)

#### Visual Studio Code
1. Cài extension: **C/C++**, **CMake Tools**
2. Mở project folder
3. CMake Tools sẽ tự động detect CMakeLists.txt
4. Chọn build configuration và build

#### CLion
1. Mở project folder
2. CLion sẽ tự động detect CMakeLists.txt
3. Configure CMake nếu cần
4. Build và run từ IDE

## 📝 Cấu Trúc Project

Sau khi build, cấu trúc project:

```
edge_ai_api/
├── build/                    # Thư mục build (tạo sau khi cmake)
│   ├── edge_ai_api          # Executable chính
│   ├── bin/
│   │   └── edge_ai_api_tests # Test executable (nếu build tests)
│   └── _deps/               # Dependencies tự động download
│       ├── drogon-src/      # Drogon source code
│       └── jsoncpp-src/     # jsoncpp source code (nếu auto-download)
├── src/                     # Source code
├── include/                 # Header files
├── tests/                   # Unit tests
├── docs/                    # Documentation
├── scripts/                 # Utility scripts
├── CMakeLists.txt          # CMake configuration
└── openapi.yaml            # OpenAPI specification
```

## ⚠️ Troubleshooting

### Lỗi: "Could NOT find OpenSSL"
```bash
sudo apt-get install libssl-dev
# Sau đó xóa build và build lại
rm -rf build
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Lỗi: "Could NOT find Jsoncpp"
```bash
sudo apt-get install libjsoncpp-dev
# Hoặc để CMake tự động download (mặc định)
# Xóa build và build lại
rm -rf build
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Lỗi: "CMake version too old"
Cài đặt CMake mới hơn (xem phần Kiểm Tra Version CMake ở trên).

### Lỗi: "Git not found"
```bash
sudo apt-get install git
```

### Build Drogon bị lỗi
1. Xóa cache và build lại:
```bash
rm -rf build/_deps
rm -rf build/CMakeCache.txt
cd build
cmake ..
make -j$(nproc)
```

2. Kiểm tra kết nối internet (cần để download Drogon lần đầu)

3. Kiểm tra log chi tiết:
```bash
cmake .. --debug-output
```

### Build chậm
- Lần đầu build sẽ chậm vì phải download và build Drogon (~5-10 phút)
- Các lần build sau sẽ nhanh hơn nhiều
- Sử dụng `-j$(nproc)` để build song song

### Lỗi CMake với CVEDIX SDK

#### Lỗi thiếu header cvedix_yolov11_detector_node.h
```
fatal error: cvedix/nodes/infers/cvedix_yolov11_detector_node.h: No such file or directory
```

**Nguyên nhân:** File header `cvedix_yolov11_detector_node.h` không tồn tại trong CVEDIX SDK. SDK chỉ cung cấp:
- `cvedix_yolo_detector_node.h` (YOLO generic)
- `cvedix_rknn_yolov11_detector_node.h` (YOLOv11 cho RKNN, chỉ khi có `CVEDIX_WITH_RKNN`)

**Giải pháp:** Đã được fix trong code. Khi sử dụng `yolov11_detector`, sẽ nhận được thông báo lỗi hướng dẫn sử dụng `rknn_yolov11_detector` hoặc `yolo_detector` thay thế.

#### Lỗi thiếu libtinyexpr.so hoặc libcvedix_instance_sdk.so
```
CMake Error: The imported target "cvedix::tinyexpr" references the file
   "/usr/lib/libtinyexpr.so"
but this file does not exist.
```

**Nguyên nhân:** CVEDIX SDK được cài đặt ở `/opt/cvedix/` (non-standard location) nhưng CMake config tìm thư viện ở `/usr/lib/`. File thực tế nằm ở `/opt/cvedix/lib/`.

**Giải pháp:** Tạo symlink từ `/usr/lib/` đến file thực tế:

```bash
sudo ln -sf /opt/cvedix/lib/libtinyexpr.so /usr/lib/libtinyexpr.so
sudo ln -sf /opt/cvedix/lib/libcvedix_instance_sdk.so /usr/lib/libcvedix_instance_sdk.so
```

**Kiểm tra:**
```bash
ls -la /usr/lib/libtinyexpr.so
ls -la /usr/lib/libcvedix_instance_sdk.so
# Kết quả mong đợi: lrwxrwxrwx ... -> /opt/cvedix/lib/...
```

#### Lỗi node types không được tìm thấy (RTSP/RTMP/Image source nodes)
```
error: 'cvedix_rtsp_src_node' is not a member of 'cvedix_nodes'
error: 'cvedix_rtmp_des_node' is not a member of 'cvedix_nodes'
error: 'cvedix_image_src_node' is not a member of 'cvedix_nodes'
```

**Nguyên nhân:** Các header files của CVEDIX SDK cho RTSP, RTMP, và Image source nodes được bọc trong điều kiện `#ifdef CVEDIX_WITH_GSTREAMER`. Nếu macro này không được định nghĩa trong quá trình biên dịch, các class này sẽ không được expose.

**Giải pháp:** Đã được fix trong `CMakeLists.txt`. CMake sẽ tự động phát hiện GStreamer và định nghĩa macro `CVEDIX_WITH_GSTREAMER`:

1. **Kiểm tra GStreamer đã được cài đặt:**
   ```bash
   pkg-config --exists gstreamer-1.0 && echo "GStreamer found" || echo "GStreamer not found"
   ```

2. **Cài đặt GStreamer (nếu chưa có):**
   ```bash
   # Ubuntu/Debian
   sudo apt-get install libgstreamer1.0-dev gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly
   ```

3. **Kiểm tra macro đã được định nghĩa trong CMake:**
   ```bash
   cd build
   cmake .. 2>&1 | grep "GStreamer support"
   # Kết quả mong đợi: -- ✓ GStreamer support enabled (CVEDIX_WITH_GSTREAMER)
   ```

**Lưu ý:** GStreamer là bắt buộc cho các node types sau:
- RTSP source (`rtsp_src`)
- RTMP source (`rtmp_src`)
- RTMP destination (`rtmp_des`)
- Image source (`image_src`)
- UDP source (`udp_src`)

#### Script tự động fix tất cả symlinks

Để tránh phải fix từng file một, bạn có thể chạy script sau để tạo tất cả symlinks cần thiết:

```bash
#!/bin/bash
# Script tạo symlinks cho CVEDIX SDK libraries

CVEDIX_LIB_DIR="/opt/cvedix/lib"
TARGET_LIB_DIR="/usr/lib"

# Danh sách các thư viện cần symlink
LIBS=(
    "libtinyexpr.so"
    "libcvedix_instance_sdk.so"
)

for lib in "${LIBS[@]}"; do
    SOURCE="${CVEDIX_LIB_DIR}/${lib}"
    TARGET="${TARGET_LIB_DIR}/${lib}"
    
    if [ -f "$SOURCE" ]; then
        if [ ! -e "$TARGET" ]; then
            echo "Creating symlink: $TARGET -> $SOURCE"
            sudo ln -sf "$SOURCE" "$TARGET"
        else
            echo "Symlink already exists: $TARGET"
        fi
    else
        echo "Warning: Source file not found: $SOURCE"
    fi
done

echo "Done! Verifying symlinks..."
ls -la /usr/lib/libtinyexpr.so /usr/lib/libcvedix_instance_sdk.so
```

Lưu script vào file `scripts/fix_cvedix_symlinks.sh`, chmod +x và chạy:
```bash
chmod +x scripts/fix_cvedix_symlinks.sh
./scripts/fix_cvedix_symlinks.sh
```

## 📊 Performance Tuning

### Tăng số thread

Mặc định server sử dụng `std::thread::hardware_concurrency()` threads.
Có thể cấu hình qua biến môi trường `THREAD_NUM` trong file `.env`:

```bash
THREAD_NUM=8  # Số thread cụ thể
THREAD_NUM=0  # Auto-detect (mặc định)
```

### Tăng body size limit

Cấu hình qua biến môi trường `CLIENT_MAX_BODY_SIZE` trong file `.env`:

```bash
CLIENT_MAX_BODY_SIZE=10485760  # 10MB (mặc định: 1MB)
```

Xem `docs/ENVIRONMENT_VARIABLES.md` để biết thêm các biến cấu hình.

## 🐳 Docker (Optional)

Nếu muốn chạy trong Docker, có thể tạo Dockerfile:

```dockerfile
# Dockerfile example
FROM ubuntu:20.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libssl-dev \
    zlib1g-dev \
    libjsoncpp-dev \
    uuid-dev \
    pkg-config

# Copy project
WORKDIR /app
COPY . .

# Build
RUN mkdir build && cd build && \
    cmake .. && \
    make -j$(nproc)

# Expose port
EXPOSE 8080

# Run
CMD ["./build/bin/edge_ai_api"]
```

Build và run:
```bash
docker build -t edge-ai-api .
docker run -p 8080:8080 edge-ai-api
```

## ✅ Xác Minh Setup Thành Công

Sau khi setup xong, chạy các lệnh sau để xác minh:

```bash
# 1. Build project
cd build
cmake ..
make -j$(nproc)

# 2. Chạy server (trong terminal khác hoặc background)
./edge_ai_api

# 3. Test API
curl http://localhost:8080/v1/core/health
curl http://localhost:8080/v1/core/version

# 4. Nếu build tests, chạy tests
./bin/edge_ai_api_tests
```

Nếu tất cả các bước trên thành công, môi trường phát triển đã sẵn sàng!

## 📚 Tài Liệu Liên Quan

- [Hướng Dẫn Khởi Động và Sử Dụng](GETTING_STARTED.md)
- [Hướng Dẫn Phát Triển](DEVELOPMENT_GUIDE.md)
- [Architecture](architecture.md)

