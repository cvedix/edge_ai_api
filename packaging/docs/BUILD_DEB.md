# Hướng Dẫn Build File .deb Package

File này hướng dẫn cách build file `.deb` tự chứa tất cả dependencies để người dùng chỉ cần tải và cài đặt.

## 🚀 Quick Start - Chỉ Cần Một Lệnh!

```bash
# Build file .deb (tất cả trong một lần chạy)
./packaging/scripts/build_deb.sh

# File sẽ được tạo: edge-ai-api-2025.0.1.3-Beta-amd64.deb

# Cài đặt
sudo dpkg -i edge-ai-api-2025.0.1.3-Beta-amd64.deb

# Khởi động service
sudo systemctl start edge-ai-api
```

**Script `packaging/scripts/build_deb.sh` tự động làm tất cả:**
- ✅ Kiểm tra dependencies
- ✅ Build project
- ✅ Bundle libraries
- ✅ Tạo file .deb

> ⚠️ **Lưu ý**: Không cần `sudo` để build! Chỉ cần sudo khi **cài đặt** package sau này.

## 📋 Yêu Cầu Build

Script sẽ tự động kiểm tra và báo lỗi nếu thiếu. Cài đặt với:

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential cmake git \
    debhelper dpkg-dev \
    libssl-dev zlib1g-dev \
    libjsoncpp-dev uuid-dev pkg-config \
    libopencv-dev \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    libmosquitto-dev
```

## 🔧 Build Package

```bash
# Build với script tự động (khuyến nghị - tất cả trong một)
./packaging/scripts/build_deb.sh

# Hoặc với các tùy chọn
./packaging/scripts/build_deb.sh --clean          # Clean build trước
./packaging/scripts/build_deb.sh --no-build       # Chỉ tạo package từ build có sẵn
./packaging/scripts/build_deb.sh --version 1.0.0  # Set version tùy chỉnh
./packaging/scripts/build_deb.sh --help           # Xem tất cả options
```

## 💾 Cài Đặt Package

**Sau khi có file .deb**, mới cần sudo để cài đặt:

```bash
# Cài đặt
sudo dpkg -i edge-ai-api-2025.0.1.3-Beta-amd64.deb

# Nếu có lỗi dependencies
sudo apt-get install -f

# Khởi động service
sudo systemctl start edge-ai-api
sudo systemctl enable edge-ai-api  # Tự động chạy khi khởi động
```

## ✅ Kiểm Tra

```bash
# Kiểm tra service
sudo systemctl status edge-ai-api

# Xem log
sudo journalctl -u edge-ai-api -f

# Test API
curl http://localhost:8080/v1/core/health
```

## 📦 Cấu Trúc Package

Sau khi cài đặt:

- **Executable**: `/usr/local/bin/edge_ai_api`
- **Libraries**: `/opt/edge_ai_api/lib/` (bundled - tất cả trong một nơi)
- **Config**: `/opt/edge_ai_api/config/`
- **Data**: `/opt/edge_ai_api/` (instances, solutions, models, logs, etc.)
- **Service**: `/etc/systemd/system/edge-ai-api.service`

## ✨ Tính Năng

✅ **Bundled Libraries**: Tất cả shared libraries được bundle vào package  
✅ **RPATH Configuration**: Executable tự động tìm libraries trong package  
✅ **Systemd Integration**: Tự động tạo và enable systemd service  
✅ **User Management**: Tự động tạo user `edgeai`  
✅ **Directory Structure**: Tự động tạo cấu trúc thư mục cần thiết  
✅ **ldconfig**: Tự động cấu hình ldconfig để tìm libraries  

## 📝 Tóm Tắt

| Bước | Lệnh | Cần Sudo? |
|------|------|-----------|
| **Build .deb** | `./packaging/scripts/build_deb.sh` | ❌ **KHÔNG** |
| **Cài đặt package** | `sudo dpkg -i *.deb` | ✅ **CÓ** |
| **Khởi động service** | `sudo systemctl start edge-ai-api` | ✅ **CÓ** |

## 🛠️ Script Làm Gì?

1. ✅ Kiểm tra dependencies
2. ✅ Build project với CMake
3. ✅ Bundle tất cả libraries
4. ✅ Tạo file .deb package
5. ✅ Đặt tên file đúng format

Tất cả trong một lần chạy!

## 🐛 Troubleshooting

Xem chi tiết trong [debian/README.md](../debian/README.md)
