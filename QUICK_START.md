# Quick Start Guide

## 🚀 Build nhanh (3 bước)

### Bước 1: Cài đặt dependencies

```bash
# Chạy script tự động (khuyến nghị)
./scripts/install_dependencies.sh

# Hoặc cài thủ công
sudo apt-get update
sudo apt-get install -y build-essential cmake git libssl-dev zlib1g-dev libjsoncpp-dev uuid-dev pkg-config
```

### Bước 2: Build project

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Bước 3: Chạy server

```bash
./edge_ai_api
```

Server sẽ chạy trên `http://0.0.0.0:8080`

## ✅ Test

```bash
# Health check
curl http://localhost:8080/v1/core/health

# Version
curl http://localhost:8080/v1/core/version
```

## ⚠️ Lỗi thường gặp

### "Could NOT find Jsoncpp"

```bash
sudo apt-get install libjsoncpp-dev
cd build
rm -rf CMakeCache.txt CMakeFiles
cmake ..
make -j$(nproc)
```

### "Could not find OpenSSL"

```bash
sudo apt-get install libssl-dev
```

### Build Drogon lâu

Lần đầu build sẽ mất ~5-10 phút để download và build Drogon. Các lần sau sẽ nhanh hơn nhiều.

## 📝 Notes

- Drogon sẽ tự động được download và build (không cần cài thủ công)
- Cần có kết nối internet lần đầu tiên
- CMake 3.14+ được yêu cầu

