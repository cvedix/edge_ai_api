# Quick Start Guide

> **Lưu ý:** Đây là hướng dẫn nhanh. Để biết chi tiết, xem:
> - [docs/DEVELOPMENT_SETUP.md](docs/DEVELOPMENT_SETUP.md) - Hướng dẫn setup và build chi tiết
> - [docs/GETTING_STARTED.md](docs/GETTING_STARTED.md) - Hướng dẫn sử dụng chi tiết

## 🚀 Build nhanh (2 cách)

### Cách 1: Sử dụng setup.sh (Khuyến Nghị - Tự Động Tất Cả)

```bash
# Development setup (tự động cài dependencies, build, và chạy server)
./setup.sh

# Production setup (cần sudo)
sudo ./setup.sh --production
```

Script này sẽ tự động làm tất cả: cài dependencies, build project, và khởi động server.

**Xem chi tiết:** [docs/DEVELOPMENT_SETUP.md](docs/DEVELOPMENT_SETUP.md)

### Cách 2: Build thủ công (3 bước)

#### Bước 1: Cài đặt dependencies

```bash
# Chạy script tự động (khuyến nghị)
./scripts/install_dependencies.sh

# Hoặc cài thủ công
sudo apt-get update
sudo apt-get install -y build-essential cmake git libssl-dev zlib1g-dev libjsoncpp-dev uuid-dev pkg-config
```

#### Bước 2: Build project

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

#### Bước 3: Chạy server

**Cách 1: Sử dụng file .env (Khuyến nghị)**

```bash
# Từ thư mục project root (không phải build/)
cd ..
cp .env.example .env
# Chỉnh sửa .env nếu cần (ví dụ: API_PORT=8082)
./scripts/load_env.sh
```

**Cách 2: Chạy trực tiếp với default**

```bash
cd build
./bin/edge_ai_api
```

Server sẽ chạy trên `http://0.0.0.0:8080` (mặc định) hoặc port đã cấu hình trong `.env`

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

## 📚 Tài Liệu Chi Tiết

- **[docs/DEVELOPMENT_SETUP.md](docs/DEVELOPMENT_SETUP.md)** - Hướng dẫn setup và build đầy đủ với troubleshooting
- **[docs/GETTING_STARTED.md](docs/GETTING_STARTED.md)** - Hướng dẫn sử dụng API và cấu hình
- **[docs/ENVIRONMENT_VARIABLES.md](docs/ENVIRONMENT_VARIABLES.md)** - Danh sách đầy đủ biến môi trường

