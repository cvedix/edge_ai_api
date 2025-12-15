# Hướng Dẫn Build và Cài Đặt Debian Package

File này hướng dẫn cách build file `.deb` tự chứa tất cả dependencies và cách cài đặt.

## Yêu Cầu

Để build file `.deb`, bạn cần các công cụ sau:

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    debhelper \
    dpkg-dev \
    libssl-dev \
    zlib1g-dev \
    libjsoncpp-dev \
    uuid-dev \
    pkg-config \
    libopencv-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libmosquitto-dev
```

## Build File .deb

### Cách 1: Sử dụng script tự động (Khuyến Nghị)

```bash
# Build package với tất cả dependencies
./scripts/build_deb.sh

# Clean build trước khi build
./scripts/build_deb.sh --clean

# Chỉ tạo package từ build có sẵn (skip build)
./scripts/build_deb.sh --no-build

# Set version tùy chỉnh
./scripts/build_deb.sh --version 2025.0.1.4-Beta
```

Script sẽ tự động:
1. Kiểm tra dependencies
2. Build project với CMake
3. Bundle tất cả shared libraries
4. Tạo file `.deb` package
5. Đặt tên file: `edge-ai-api-2025.0.1.3-Beta-amd64.deb`

### Cách 2: Sử dụng dpkg-buildpackage trực tiếp

```bash
# Build package
dpkg-buildpackage -b -us -uc

# File .deb sẽ được tạo ở thư mục cha
```

## Cài Đặt Package

### Cài đặt từ file .deb

```bash
# Cài đặt package
sudo dpkg -i edge-ai-api-2025.0.1.3-Beta-amd64.deb

# Nếu có lỗi dependencies, fix với:
sudo apt-get install -f
```

### Sau khi cài đặt

Package sẽ tự động:
- ✅ Tạo user `edgeai` và group `edgeai`
- ✅ Cài đặt executable vào `/usr/local/bin/edge_ai_api`
- ✅ Bundle tất cả shared libraries vào `/opt/edge_ai_api/lib` (tất cả trong một nơi)
- ✅ Cài đặt systemd service
- ✅ Tạo cấu trúc thư mục tại `/opt/edge_ai_api`
- ✅ Cấu hình ldconfig để tìm libraries

### Khởi động Service

```bash
# Khởi động service
sudo systemctl start edge-ai-api

# Kiểm tra trạng thái
sudo systemctl status edge-ai-api

# Xem log
sudo journalctl -u edge-ai-api -f

# Enable tự động chạy khi khởi động
sudo systemctl enable edge-ai-api
```

### Test API

```bash
# Health check
curl http://localhost:8080/v1/core/health

# Version
curl http://localhost:8080/v1/core/version

# Swagger UI
# Mở browser: http://localhost:8080/swagger
```

## Cấu Hình

File cấu hình mặc định tại `/opt/edge_ai_api/config/config.json`

Tạo file `.env` để cấu hình biến môi trường:

```bash
sudo nano /opt/edge_ai_api/config/.env
```

Ví dụ:
```bash
API_HOST=0.0.0.0
API_PORT=8080
LOG_LEVEL=INFO
```

Sau đó restart service:
```bash
sudo systemctl restart edge-ai-api
```

## Gỡ Cài Đặt

```bash
# Gỡ cài đặt package
sudo dpkg -r edge-ai-api

# Hoặc gỡ hoàn toàn (bao gồm config files)
sudo dpkg -P edge-ai-api
```

## Cấu Trúc Package

Sau khi cài đặt, package sẽ tạo cấu trúc sau:

```
/usr/local/bin/edge_ai_api          # Executable
/opt/edge_ai_api/                    # Application directory
├── lib/                             # Bundled libraries (tất cả trong một nơi)
├── config/
│   ├── config.json                  # Main config
│   └── .env                         # Environment variables
├── instances/                       # Instance configurations
├── solutions/                       # Solution templates
├── groups/                          # Group configurations
├── models/                          # Uploaded models
├── logs/                            # Application logs
├── data/                            # Application data
└── uploads/                         # Uploaded files
/etc/systemd/system/edge-ai-api.service  # Systemd service
```

## Troubleshooting

### Lỗi: "dpkg-buildpackage: command not found"

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

## Lưu Ý

1. **Bundled Libraries**: Package bundle tất cả shared libraries cần thiết vào `/opt/edge_ai_api/lib`. Điều này đảm bảo ứng dụng hoạt động ngay cả khi hệ thống thiếu một số dependencies. Tất cả mọi thứ được lưu trong `/opt/edge_ai_api` để dễ quản lý.

2. **RPATH**: Executable được cấu hình với RPATH để tìm libraries trong `/opt/edge_ai_api/lib` trước khi tìm trong system paths.

3. **CVEDIX SDK**: Nếu CVEDIX SDK được cài đặt tại `/opt/cvedix/lib`, các libraries sẽ được tự động bundle vào package.

4. **System Dependencies**: Một số system dependencies vẫn cần được cài đặt (như libssl3, libc6, etc.) nhưng chúng thường đã có sẵn trên hệ thống Debian/Ubuntu.

## Version

Package version được định nghĩa trong:
- `debian/changelog` - Version và changelog
- `CMakeLists.txt` - Project version

Để thay đổi version, chỉnh sửa `debian/changelog` và chạy lại build script.

