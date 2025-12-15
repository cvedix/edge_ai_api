# Quick Build .deb - Chỉ Cần Một Lệnh!

## ✅ Không Cần Sudo Để Build

Chỉ cần chạy:

```bash
./packaging/scripts/build_deb.sh
```

**Xong!** File `.deb` sẽ được tạo: `edge-ai-api-2025.0.1.3-Beta-amd64.deb`

## 📋 Yêu Cầu Trước Khi Build

Script sẽ tự động kiểm tra và báo lỗi nếu thiếu. Cài đặt với:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake git debhelper dpkg-dev \
    libssl-dev zlib1g-dev libjsoncpp-dev uuid-dev pkg-config \
    libopencv-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    libmosquitto-dev
```

## 🚀 Build

```bash
# Build với default settings
./packaging/scripts/build_deb.sh

# Clean và build lại từ đầu
./packaging/scripts/build_deb.sh --clean

# Chỉ tạo package từ build có sẵn (skip build)
./packaging/scripts/build_deb.sh --no-build

# Set version tùy chỉnh
./packaging/scripts/build_deb.sh --version 1.0.0
```

## 📦 Kết Quả

Sau khi build thành công, bạn sẽ có file:
- `edge-ai-api-2025.0.1.3-Beta-amd64.deb`

## 💾 Cài Đặt Package (Cần Sudo)

**Sau khi có file .deb**, mới cần sudo để cài đặt:

```bash
# Cài đặt package
sudo dpkg -i edge-ai-api-2025.0.1.3-Beta-amd64.deb

# Nếu có lỗi dependencies
sudo apt-get install -f

# Khởi động service
sudo systemctl start edge-ai-api
```

## 📝 Tóm Tắt

| Bước | Lệnh | Cần Sudo? |
|------|------|-----------|
| **Build .deb** | `./packaging/scripts/build_deb.sh` | ❌ **KHÔNG** |
| **Cài đặt package** | `sudo dpkg -i *.deb` | ✅ **CÓ** |
| **Khởi động service** | `sudo systemctl start edge-ai-api` | ✅ **CÓ** |

## ⚠️ Lưu Ý

- **Build**: Không cần sudo, chỉ cần quyền ghi vào thư mục project
- **Cài đặt**: Cần sudo vì cài vào hệ thống (`/opt`, `/usr/local/bin`, etc.)
- Script tự động làm tất cả: build project, bundle libraries, tạo package

