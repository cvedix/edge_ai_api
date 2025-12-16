# 📦 Packaging Directory

Thư mục này chứa các scripts và tài liệu liên quan đến việc build Debian package (.deb).

## 📁 Cấu Trúc

```
packaging/
├── scripts/           # Build scripts
│   └── build_deb.sh   # Script chính để build .deb package
├── docs/              # Tài liệu hướng dẫn
│   ├── BUILD_DEB.md           # Hướng dẫn build chi tiết
│   ├── BUILD_DEB_SIMPLE.md    # Hướng dẫn build đơn giản
│   └── QUICK_BUILD_DEB.md     # Hướng dẫn build nhanh
└── README.md          # File này
```

## 🚀 Quick Start

### Build Debian Package

```bash
# Từ project root
./packaging/scripts/build_deb.sh

# Hoặc từ thư mục packaging/scripts
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

Xem các file trong `packaging/docs/` để biết thêm chi tiết:
- **BUILD_DEB.md** - Hướng dẫn đầy đủ với tất cả các bước
- **BUILD_DEB_SIMPLE.md** - Hướng dẫn đơn giản, nhanh
- **QUICK_BUILD_DEB.md** - Quick reference

## 📝 Lưu Ý

- File `.deb` được tạo sẽ nằm ở **project root**
- Thư mục `debian/` phải ở **project root** (theo convention của Debian)
- Script sẽ tự động bundle tất cả dependencies vào package

## 🔗 Liên Quan

- `debian/` - Debian package source files (phải ở root)
- `deploy/` - Production deployment scripts
- `scripts/` - Development scripts

