# 📦 Packaging Directory

Thư mục này chứa các scripts và tài liệu liên quan đến việc build Debian package (.deb).

## 📁 Cấu Trúc

```
packaging/
├── scripts/           # Build scripts
│   └── build_deb.sh   # Script chính để build .deb package
├── docs/              # Tài liệu hướng dẫn
│   └── BUILD_DEB.md   # Hướng dẫn build .deb package
└── README.md          # File này
```

## 🚀 Quick Start

### Build Debian Package

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

### Options

```bash
./packaging/scripts/build_deb.sh --help

# Clean build
./packaging/scripts/build_deb.sh --clean

# Skip build (chỉ tạo package từ build có sẵn)
./packaging/scripts/build_deb.sh --no-build

# Set version
./packaging/scripts/build_deb.sh --version 1.0.0
```

## 📚 Documentation

Xem [BUILD_DEB.md](docs/BUILD_DEB.md) để biết chi tiết về cách build .deb package.

## 📝 Lưu Ý

- File `.deb` được tạo sẽ nằm ở **project root**
- Thư mục `debian/` phải ở **project root** (theo convention của Debian)
- Script sẽ tự động bundle tất cả dependencies vào package
- **Không cần sudo để build** - chỉ cần sudo khi cài đặt package

## ✅ Tính Năng

- ✅ **Bundled Libraries**: Tất cả shared libraries được bundle vào package
- ✅ **RPATH Configuration**: Executable tự động tìm libraries trong package
- ✅ **Systemd Integration**: Tự động tạo và enable systemd service
- ✅ **User Management**: Tự động tạo user `edgeai`
- ✅ **Directory Structure**: Tự động tạo cấu trúc thư mục cần thiết
- ✅ **Backward Compatible**: Wrapper script giữ nguyên cách dùng cũ

## 🔗 Liên Quan

- `debian/` - Debian package source files (phải ở root)
- `deploy/` - Production deployment scripts
- `scripts/` - Development scripts
- `docs/` - General documentation
