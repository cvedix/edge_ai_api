# Phân Tích và Tối Ưu Scripts

Tài liệu này phân tích tất cả các scripts trong project và đề xuất những file nào cần giữ lại, file nào có thể xóa hoặc tích hợp.

## 📊 Tổng Quan Scripts

### Scripts Chính (Root)
- ✅ **`setup.sh`** - Entry point chính, cần thiết
  - Development: `./setup.sh`
  - Production: `sudo ./setup.sh --production`
  - Đã tích hợp: install dependencies, build, deploy

### Scripts Production (deploy/)
- ✅ **`deploy/build.sh`** - Production deployment script đầy đủ, cần thiết
- ✅ **`deploy/install_directories.sh`** - Helper script, giữ lại (có thể dùng độc lập)
- ✅ **`deploy/set_full_permissions.sh`** - Helper script, giữ lại (có thể dùng độc lập)

### Scripts Development (scripts/)
- ✅ **`scripts/load_env.sh`** - Load environment variables, được sử dụng nhiều, cần thiết
- ✅ **`scripts/run_tests.sh`** - Test script, cần thiết
- ⚠️ **`scripts/install_dependencies.sh`** - Đã tích hợp vào `setup.sh`, có thể giữ lại như helper script độc lập
- ✅ **`scripts/fix_all_symlinks.sh`** - **MỚI**: Script tổng hợp fix tất cả symlinks (khuyến nghị sử dụng)
- ⚠️ **`scripts/fix_cvedix_symlinks.sh`** - Đã tích hợp vào `fix_all_symlinks.sh`, giữ lại như helper script
- ⚠️ **`scripts/fix_cereal_symlink.sh`** - Đã tích hợp vào `fix_all_symlinks.sh`, giữ lại như helper script
- ⚠️ **`scripts/fix_cpp_base64_symlink.sh`** - Đã tích hợp vào `fix_all_symlinks.sh`, giữ lại như helper script
- ✅ **`scripts/generate_default_solution_template.sh`** - Utility script, giữ lại
- ✅ **`scripts/restore_default_solutions.sh`** - Utility script, giữ lại
- ✅ **`scripts/check_rtsp_instance.sh`** - Debug script, giữ lại
- ✅ **`scripts/debug_rtsp_pipeline.sh`** - Debug script, giữ lại
- ✅ **`scripts/diagnose_rtsp.sh`** - Debug script, giữ lại
- ✅ **`scripts/test_rtsp_connection.sh`** - Test script, giữ lại

### Scripts Samples
- ✅ **`samples/build.sh`** - Build script cho samples, cần thiết

### Scripts Examples
- ✅ **`examples/instances/scripts/*.sh`** - Example scripts, giữ lại

## 🎯 Đề Xuất Tối Ưu

### 1. Scripts Đã Được Tích Hợp (Giữ Lại Như Helper Scripts)

Các script sau đã được tích hợp vào script chính nhưng vẫn giữ lại để:
- Có thể sử dụng độc lập khi cần
- Dễ debug và troubleshoot
- Linh hoạt hơn cho các use case đặc biệt

**Không cần xóa:**
- `scripts/install_dependencies.sh` - Có thể dùng độc lập
- `scripts/fix_cvedix_symlinks.sh` - Có thể fix riêng libraries
- `scripts/fix_cereal_symlink.sh` - Có thể fix riêng cereal
- `scripts/fix_cpp_base64_symlink.sh` - Có thể fix riêng base64
- `deploy/install_directories.sh` - Có thể dùng độc lập
- `deploy/set_full_permissions.sh` - Có thể dùng độc lập

### 2. Script Tổng Hợp Mới

**✅ `scripts/fix_all_symlinks.sh`** - Script mới, khuyến nghị sử dụng
- Tích hợp tất cả logic fix symlinks
- Fix libraries, cereal, và cpp-base64 trong một lần chạy
- Dễ sử dụng và maintain

### 3. Cấu Trúc Scripts Đề Xuất

```
edge_ai_api/
├── setup.sh                          # Entry point chính
├── deploy/
│   ├── build.sh                      # Production deployment
│   ├── install_directories.sh        # Helper: install directories
│   └── set_full_permissions.sh      # Helper: set permissions
└── scripts/
    ├── load_env.sh                   # Load environment
    ├── run_tests.sh                  # Run tests
    ├── fix_all_symlinks.sh          # ⭐ Fix all symlinks (khuyến nghị)
    ├── fix_cvedix_symlinks.sh       # Helper: fix libraries only
    ├── fix_cereal_symlink.sh         # Helper: fix cereal only
    ├── fix_cpp_base64_symlink.sh    # Helper: fix base64 only
    ├── install_dependencies.sh      # Helper: install deps only
    ├── generate_default_solution_template.sh
    ├── restore_default_solutions.sh
    ├── check_rtsp_instance.sh
    ├── debug_rtsp_pipeline.sh
    ├── diagnose_rtsp.sh
    └── test_rtsp_connection.sh
```

## 📝 Hướng Dẫn Sử Dụng

### Setup Từ Đầu (Khuyến Nghị)

```bash
# Development
./setup.sh

# Production
sudo ./setup.sh --production
```

### Fix Symlinks (Khi Gặp Lỗi CMake)

```bash
# Khuyến nghị: Fix tất cả symlinks
sudo ./scripts/fix_all_symlinks.sh

# Hoặc fix riêng từng phần nếu cần
sudo ./scripts/fix_cvedix_symlinks.sh
sudo ./scripts/fix_cereal_symlink.sh
sudo ./scripts/fix_cpp_base64_symlink.sh
```

### Install Dependencies (Nếu Cần Dùng Độc Lập)

```bash
./scripts/install_dependencies.sh
```

### Setup Directories (Nếu Cần Dùng Độc Lập)

```bash
sudo ./deploy/install_directories.sh
sudo ./deploy/set_full_permissions.sh  # Nếu cần quyền 777
```

## ✅ Kết Luận

**Không cần xóa script nào** - Tất cả scripts đều có mục đích sử dụng:
- Scripts chính (`setup.sh`, `deploy/build.sh`) - Entry points
- Helper scripts - Có thể dùng độc lập khi cần
- Debug/test scripts - Cần thiết cho development và troubleshooting
- Utility scripts - Hữu ích cho các tác vụ đặc biệt

**Cải thiện đã thực hiện:**
- ✅ Tạo `scripts/fix_all_symlinks.sh` - Script tổng hợp fix symlinks
- ✅ Tích hợp logic vào `setup.sh` và `deploy/build.sh`
- ✅ Giữ lại các helper scripts để linh hoạt

**Khuyến nghị:**
- Sử dụng `setup.sh` cho setup từ đầu
- Sử dụng `scripts/fix_all_symlinks.sh` khi gặp lỗi symlinks
- Giữ lại tất cả helper scripts để có thể sử dụng độc lập khi cần

