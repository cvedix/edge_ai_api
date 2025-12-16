# Hướng Dẫn Build File .deb - Đơn Giản

## Chỉ Cần Chạy Một Lệnh (Không Cần Sudo!)

```bash
./packaging/scripts/build_deb.sh
```

**Xong!** File `.deb` sẽ được tạo: `edge-ai-api-2025.0.1.3-Beta-amd64.deb`

> ⚠️ **Lưu ý**: Không cần `sudo` để build! Chỉ cần sudo khi **cài đặt** package sau này.

## Cài Đặt Package (Cần Sudo)

**Sau khi có file .deb**, mới cần sudo để cài đặt:

```bash
sudo dpkg -i edge-ai-api-2025.0.1.3-Beta-amd64.deb
sudo apt-get install -f  # Nếu có lỗi dependencies
sudo systemctl start edge-ai-api
```

## Tóm Tắt

| Bước | Lệnh | Cần Sudo? |
|------|------|-----------|
| **Build .deb** | `./packaging/scripts/build_deb.sh` | ❌ **KHÔNG** |
| **Cài đặt** | `sudo dpkg -i *.deb` | ✅ **CÓ** |

## Tùy Chọn

```bash
# Clean và build lại từ đầu
./packaging/scripts/build_deb.sh --clean

# Chỉ tạo package từ build có sẵn (skip build)
./packaging/scripts/build_deb.sh --no-build

# Set version tùy chỉnh
./packaging/scripts/build_deb.sh --version 1.0.0

# Xem help
./packaging/scripts/build_deb.sh --help
```

## Yêu Cầu

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

## Script Làm Gì?

1. ✅ Kiểm tra dependencies
2. ✅ Build project với CMake
3. ✅ Bundle tất cả libraries
4. ✅ Tạo file .deb package
5. ✅ Đặt tên file đúng format

Tất cả trong một lần chạy!

