# Debian Package Directory

Thư mục này chứa các file source cho Debian package (.deb).

## 📁 Cấu Trúc

```
debian/
├── changelog          # Package version và changelog
├── control            # Package metadata
├── rules              # Build rules
├── postinst           # Post-installation script
├── bundle_libs.sh     # Auto-generated library bundling script
└── README.md          # File này
```

## 📚 Documentation

Xem [packaging/docs/BUILD_DEB.md](../packaging/docs/BUILD_DEB.md) để biết chi tiết về cách build và cài đặt package.

## 🔧 Build Package

```bash
# Sử dụng build script (khuyến nghị)
./build_deb.sh

# Hoặc từ packaging directory
./packaging/scripts/build_deb.sh
```

## 📝 Lưu Ý

- Thư mục `debian/` phải ở **project root** (theo convention của Debian)
- File `bundle_libs.sh` được tự động tạo bởi `build_deb.sh`
- Không chỉnh sửa trực tiếp các file trong `debian/` trừ khi cần thiết
