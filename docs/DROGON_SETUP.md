# Drogon Framework Setup - Tự động với FetchContent

## 🎯 Tổng quan

Project đã được cấu hình để **tự động download và build Drogon Framework** khi build project. Không cần cài đặt thủ công!

## ✨ Cách hoạt động

Khi chạy `cmake ..`, CMake sẽ:
1. Tự động download Drogon từ GitHub (nếu chưa có)
2. Build Drogon như một dependency
3. Link Drogon vào project

## 🚀 Sử dụng (Mặc định)

### Build với FetchContent (Tự động)

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

**Lần đầu tiên:** Sẽ mất thời gian để download và build Drogon (~5-10 phút tùy máy)

**Các lần sau:** Chỉ build project của bạn, rất nhanh

### Drogon được lưu ở đâu?

Drogon được download và build trong thư mục `build/_deps/drogon-src/` và `build/_deps/drogon-build/`

## ⚙️ Tùy chọn cấu hình

### Chọn version Drogon

```bash
cmake .. -DDROGON_VERSION=v1.9.0
```

Các version có sẵn: https://github.com/drogonframework/drogon/releases

### Tắt FetchContent (Dùng Drogon đã cài sẵn)

Nếu bạn đã cài Drogon trên system:

```bash
cmake .. -DDROGON_USE_FETCHCONTENT=OFF
```

CMake sẽ tìm Drogon đã cài đặt thay vì download.

## 📋 Dependencies của Drogon

Drogon cần các dependencies sau. CMake sẽ tự động tìm hoặc build:

### Bắt buộc:
- **OpenSSL** - Cho HTTPS support
- **zlib** - Compression
- **jsoncpp** - JSON parsing (hoặc nlohmann_json)
- **libuuid** - UUID generation

### Tùy chọn:
- **PostgreSQL** - Database support (nếu dùng ORM)
- **MySQL** - Database support (nếu dùng ORM)
- **SQLite** - Database support (nếu dùng ORM)

### Cài đặt dependencies (Ubuntu/Debian)

```bash
sudo apt-get update
sudo apt-get install -y \
    libssl-dev \
    zlib1g-dev \
    libjsoncpp-dev \
    uuid-dev \
    cmake \
    git \
    build-essential
```

### Cài đặt dependencies (CentOS/RHEL)

```bash
sudo yum install -y \
    openssl-devel \
    zlib-devel \
    jsoncpp-devel \
    libuuid-devel \
    cmake \
    git \
    gcc-c++
```

## 🔍 Troubleshooting

### Lỗi: "Could not find OpenSSL"

```bash
# Ubuntu/Debian
sudo apt-get install libssl-dev

# CentOS/RHEL
sudo yum install openssl-devel
```

### Lỗi: "Could not find jsoncpp"

```bash
# Ubuntu/Debian
sudo apt-get install libjsoncpp-dev

# CentOS/RHEL
sudo yum install jsoncpp-devel
```

### Lỗi: "Git not found"

```bash
# Ubuntu/Debian
sudo apt-get install git

# CentOS/RHEL
sudo yum install git
```

### Lỗi: "CMake version too old"

Cần CMake 3.14+ cho FetchContent:

```bash
# Kiểm tra version
cmake --version

# Cài đặt CMake mới hơn
# Ubuntu/Debian
sudo apt-get install cmake

# Hoặc build từ source
wget https://github.com/Kitware/CMake/releases/download/v3.27.0/cmake-3.27.0.tar.gz
tar -xzf cmake-3.27.0.tar.gz
cd cmake-3.27.0
./bootstrap && make && sudo make install
```

### Build Drogon bị lỗi

1. Xóa cache và build lại:
```bash
rm -rf build/_deps
rm -rf build/CMakeCache.txt
cmake ..
make -j$(nproc)
```

2. Kiểm tra dependencies đã đủ chưa
3. Xem log chi tiết: `cmake .. --debug-output`

## 📊 So sánh FetchContent vs Manual Install

| Tính năng | FetchContent (Auto) | Manual Install |
|-----------|---------------------|----------------|
| Cài đặt | Tự động | Thủ công |
| Version control | Dễ dàng | Khó |
| Isolation | Tách biệt với system | Dùng chung |
| Build time | Lâu hơn lần đầu | Nhanh hơn |
| Dependencies | Tự động | Phải cài thủ công |
| Portability | Tốt | Phụ thuộc system |

## 🎯 Khuyến nghị

**Sử dụng FetchContent (mặc định)** vì:
- ✅ Không cần cài đặt thủ công
- ✅ Version được kiểm soát
- ✅ Dễ dàng cho CI/CD
- ✅ Tách biệt với system dependencies
- ✅ Dễ dàng switch version

**Chỉ dùng Manual Install khi:**
- Đã có Drogon cài sẵn trên system
- Cần dùng chung Drogon cho nhiều projects
- Có yêu cầu đặc biệt về cấu hình build

## 📝 Notes

- Drogon được build với các options tối ưu cho production
- ORM và examples được tắt để giảm build time
- FetchContent cache được lưu trong `build/_deps/`
- Có thể xóa `build/_deps/` để force rebuild Drogon

---

*Tài liệu này mô tả cách Drogon được tích hợp tự động vào project*

