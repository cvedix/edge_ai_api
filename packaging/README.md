# 📦 Packaging Directory

Thư mục này chứa các scripts và tài liệu liên quan đến việc build Debian package (.deb).

## 📁 Cấu Trúc

```
packaging/
├── scripts/           # Build scripts
│   └── build_deb.sh   # Script chính để build .deb package
└── docs/              # Tài liệu hướng dẫn
    └── BUILD_DEB.md   # Hướng dẫn chi tiết
```

## 🚀 Quick Start

Xem [docs/BUILD_DEB.md](docs/BUILD_DEB.md) để biết chi tiết về cách build .deb package.

**Tóm tắt:**
```bash
# Build package
./build_deb.sh
# hoặc
./packaging/scripts/build_deb.sh

# Cài đặt
sudo dpkg -i edge-ai-api-*.deb
```
