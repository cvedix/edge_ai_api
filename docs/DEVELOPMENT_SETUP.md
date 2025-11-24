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
- [Drogon Setup](DROGON_SETUP.md)
- [Architecture](architecture.md)

