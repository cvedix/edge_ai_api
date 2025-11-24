# Build & Run Guide - Edge AI REST API

Hướng dẫn xây dựng và chạy REST API server cho Edge AI.

## 📋 Yêu cầu hệ thống

- **OS**: Linux (Ubuntu 20.04+ recommended)
- **Compiler**: GCC 7+ hoặc Clang 8+ với C++17 support
- **CMake**: 3.10 hoặc cao hơn
- **Dependencies**:
  - Drogon HTTP Framework
  - JSON library (thường đi kèm với Drogon)

## 🔧 Cài đặt Dependencies

### 0. Cài đặt System Dependencies (Bắt buộc)

Drogon cần các dependencies sau. Chạy script tự động:

```bash
# Chạy script cài đặt dependencies
./scripts/install_dependencies.sh
```

**Hoặc cài đặt thủ công:**

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    libssl-dev \
    zlib1g-dev \
    libjsoncpp-dev \
    uuid-dev \
    pkg-config
```

**CentOS/RHEL:**
```bash
sudo yum install -y \
    gcc-c++ \
    cmake \
    git \
    openssl-devel \
    zlib-devel \
    jsoncpp-devel \
    libuuid-devel \
    pkgconfig
```

### 1. Cài đặt Drogon Framework

**✨ Tự động (Khuyến nghị - Mặc định):**

Project đã được cấu hình để tự động download và build Drogon khi build. **Không cần cài đặt thủ công!**

CMake sẽ tự động:
- Download Drogon từ GitHub
- Build Drogon như một dependency
- Link vào project

Chỉ cần chạy:
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

**Tùy chọn:** Nếu muốn sử dụng Drogon đã cài đặt sẵn trên system:
```bash
cmake .. -DDROGON_USE_FETCHCONTENT=OFF
```

### 2. Cài đặt thủ công (Nếu không dùng FetchContent)

#### Cách 1: Build từ source

```bash
# Clone Drogon repository
git clone https://github.com/drogonframework/drogon.git
cd drogon

# Create build directory
mkdir build && cd build

# Configure và build
cmake ..
make -j$(nproc)

# Install
sudo make install
```

Sau đó build project với:
```bash
cmake .. -DDROGON_USE_FETCHCONTENT=OFF
```

#### Cách 2: Sử dụng vcpkg

```bash
# Install vcpkg (nếu chưa có)
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh

# Install Drogon
./vcpkg install drogon

# Configure CMake với vcpkg
cmake .. -DCMAKE_TOOLCHAIN_FILE=[path to vcpkg]/scripts/buildsystems/vcpkg.cmake \
         -DDROGON_USE_FETCHCONTENT=OFF
```

#### Cách 3: Sử dụng package manager

```bash
# Ubuntu/Debian (nếu có package)
sudo apt-get install libdrogon-dev

# Build với system Drogon
cmake .. -DDROGON_USE_FETCHCONTENT=OFF
```

## 🏗️ Build Project

### 1. Clone repository (nếu chưa có)

```bash
cd /home/ubuntu/project/edge_ai_api
```

### 2. Tạo build directory

```bash
mkdir -p build
cd build
```

### 3. Configure với CMake

**Tự động download Drogon (Mặc định - Khuyến nghị):**
```bash
cmake ..
```

CMake sẽ tự động:
- Download Drogon từ GitHub (lần đầu tiên)
- Build Drogon
- Link vào project

**Sử dụng Drogon đã cài sẵn:**
```bash
cmake .. -DDROGON_USE_FETCHCONTENT=OFF
```

**Chọn version Drogon cụ thể:**
```bash
cmake .. -DDROGON_VERSION=v1.9.0
```

### 4. Build

```bash
make -j$(nproc)
```

**Lưu ý:** Lần đầu tiên build sẽ mất thời gian hơn vì cần build Drogon. Các lần sau sẽ nhanh hơn nhiều.

Hoặc nếu muốn build với verbose output:

```bash
make VERBOSE=1
```

### 5. Kiểm tra build thành công

Sau khi build, executable sẽ nằm tại:
```
build/edge_ai_api
```

## 🚀 Chạy Server

### Chạy cơ bản

```bash
cd build
./edge_ai_api
```

Server sẽ chạy trên `http://0.0.0.0:8080` (mặc định).

### Cấu hình qua Environment Variables

```bash
# Thay đổi port
export API_PORT=9090
./edge_ai_api

# Thay đổi host
export API_HOST=127.0.0.1
./edge_ai_api

# Hoặc kết hợp
export API_HOST=0.0.0.0
export API_PORT=8080
./edge_ai_api
```

### Chạy ở background

```bash
nohup ./edge_ai_api > api.log 2>&1 &
```

## 🧪 Test API

### 1. Test Health Endpoint

```bash
# Sử dụng curl
curl http://localhost:8080/v1/core/health

# Kết quả mong đợi:
# {
#   "status": "healthy",
#   "timestamp": "2024-01-01T00:00:00.000Z",
#   "uptime": 3600,
#   "service": "edge_ai_api",
#   "version": "1.0.0"
# }
```

### 2. Test Version Endpoint

```bash
curl http://localhost:8080/v1/core/version

# Kết quả mong đợi:
# {
#   "version": "1.0.0",
#   "build_time": "Jan 01 2024 00:00:00",
#   "git_commit": "unknown",
#   "api_version": "v1",
#   "service": "edge_ai_api"
# }
```

### 3. Sử dụng HTTPie (nếu có)

```bash
http GET http://localhost:8080/v1/core/health
http GET http://localhost:8080/v1/core/version
```

### 4. Sử dụng Postman

Import file `openapi.yaml` vào Postman để có sẵn các endpoints và test cases.

## 📁 Cấu trúc Project

```
edge_ai_api/
├── CMakeLists.txt          # CMake build configuration
├── BUILD.md                # File này - hướng dẫn build
├── README.md               # Tổng quan project
├── openapi.yaml            # OpenAPI specification
├── include/                # Header files
│   └── api/
│       ├── health_handler.h
│       └── version_handler.h
└── src/                    # Source files
    ├── main.cpp
    └── api/
        ├── health_handler.cpp
        └── version_handler.cpp
```

## 🔍 Troubleshooting

### Lỗi: "Could NOT find Jsoncpp"

**Nguyên nhân:** Thiếu development package của jsoncpp

**Giải pháp:**
```bash
# Ubuntu/Debian
sudo apt-get install libjsoncpp-dev

# CentOS/RHEL
sudo yum install jsoncpp-devel

# Sau đó build lại
cd build
rm -rf CMakeCache.txt CMakeFiles
cmake ..
make -j$(nproc)
```

**Hoặc chạy script tự động:**
```bash
./scripts/install_dependencies.sh
```

### Lỗi: "Could not find Drogon"

**Nếu dùng FetchContent (mặc định):**
- Đảm bảo có kết nối internet để download Drogon
- Kiểm tra Git đã được cài đặt: `git --version`
- Thử xóa build directory và build lại: `rm -rf build && mkdir build && cd build && cmake ..`

**Nếu dùng system Drogon:**
```bash
# Kiểm tra Drogon đã được cài đặt
pkg-config --modversion drogon

# Hoặc kiểm tra thư viện
ldconfig -p | grep drogon

# Nếu chưa có, cài đặt lại Drogon và chạy:
sudo ldconfig

# Hoặc chuyển sang dùng FetchContent:
cmake .. -DDROGON_USE_FETCHCONTENT=ON
```

### Lỗi: "undefined reference to..."

Đảm bảo đã link đúng thư viện trong CMakeLists.txt:
```cmake
target_link_libraries(edge_ai_api PRIVATE Drogon::Drogon)
```

### Lỗi: Port đã được sử dụng

```bash
# Kiểm tra port đang được sử dụng
sudo netstat -tulpn | grep 8080

# Hoặc sử dụng port khác
export API_PORT=9090
./edge_ai_api
```

### Lỗi: Permission denied

```bash
# Cấp quyền thực thi
chmod +x build/edge_ai_api

# Hoặc chạy với sudo (không khuyến nghị)
sudo ./edge_ai_api
```

## 📊 Performance Tuning

### Tăng số thread

Mặc định server sử dụng `std::thread::hardware_concurrency()` threads.
Có thể chỉnh sửa trong `src/main.cpp`:

```cpp
.setThreadNum(8)  // Số thread cụ thể
```

### Tăng body size limit

Trong `src/main.cpp`:

```cpp
.setClientMaxBodySize(10 * 1024 * 1024)  // 10MB
```

## 🐳 Docker (Optional)

Nếu muốn chạy trong Docker:

```dockerfile
# Dockerfile example
FROM ubuntu:20.04
# ... install dependencies ...
COPY build/edge_ai_api /usr/local/bin/
EXPOSE 8080
CMD ["edge_ai_api"]
```

## 📝 Notes

- Server mặc định chạy trên port 8080
- Tất cả endpoints có prefix `/v1/core/`
- API trả về JSON format
- Xem `openapi.yaml` để biết chi tiết về API specification

## 🔗 Tài liệu tham khảo

- [Drogon Framework Documentation](https://drogon.docsforge.com/)
- [OpenAPI Specification](https://swagger.io/specification/)
- [CMake Documentation](https://cmake.org/documentation/)

