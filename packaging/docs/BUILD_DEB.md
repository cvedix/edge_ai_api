# Hướng Dẫn Build và Cài Đặt Debian Package

File này hướng dẫn cách build file `.deb` tự chứa tất cả dependencies và cách cài đặt.

## 📦 Packaging Directory

Thư mục `packaging/` chứa các scripts và tài liệu liên quan đến việc build Debian package (.deb).

**Cấu trúc:**
```
packaging/
├── scripts/           # Build scripts
│   └── build_deb.sh   # Script chính để build .deb package
└── docs/              # Tài liệu hướng dẫn
    └── BUILD_DEB.md   # File này
```

## 🚀 Quick Start - Chỉ Cần Một Lệnh!

Có 3 cách để build:

**Option 1: Dùng Wrapper (Khuyến Nghị)**
```bash
# Từ project root
./build_deb.sh
```

**Option 2: Dùng Đường Dẫn Đầy Đủ**
```bash
# Từ project root
./packaging/scripts/build_deb.sh
```

**Option 3: Từ Thư Mục Packaging**
```bash
cd packaging/scripts
./build_deb.sh
```

**Sau khi build:**
```bash
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

## 📝 Lưu Ý

1. **Bundled Libraries**: Package bundle tất cả shared libraries cần thiết vào `/opt/edge_ai_api/lib`. Điều này đảm bảo ứng dụng hoạt động ngay cả khi hệ thống thiếu một số dependencies.

2. **RPATH**: Executable được cấu hình với RPATH để tìm libraries trong `/opt/edge_ai_api/lib` trước khi tìm trong system paths.

3. **CVEDIX SDK**: Nếu CVEDIX SDK được cài đặt tại `/opt/cvedix/lib`, các libraries sẽ được tự động bundle vào package.

4. **System Dependencies**: Một số system dependencies vẫn cần được cài đặt (như libssl3, libc6, etc.) nhưng chúng thường đã có sẵn trên hệ thống Debian/Ubuntu.

5. **File .deb được tạo sẽ nằm ở project root**

6. **Thư mục `debian/` phải ở project root** (theo convention của Debian)

7. **Không cần sudo để build** - chỉ cần sudo khi cài đặt package

## 🔗 Liên Quan

- `debian/` - Debian package source files (phải ở root)
- `deploy/` - Production deployment scripts
- `scripts/` - Development scripts
- `docs/` - General documentation
