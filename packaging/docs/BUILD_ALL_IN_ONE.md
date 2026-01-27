# Build All-In-One Debian Package

## 📦 Tổng Quan

Script `build_deb_all_in_one.sh` tạo một package **ALL-IN-ONE** - tự chứa **TẤT CẢ** dependencies:

- ✅ CVEDIX SDK runtime (bundled)
- ✅ OpenCV libraries (bundled)
- ✅ GStreamer libraries và plugins (bundled)
- ✅ FFmpeg libraries (bundled)
- ✅ Default fonts và models từ `cvedix_data/` (bundled)
- ✅ Tất cả libraries khác (bundled)

Package này **chỉ cần system libraries cơ bản** (libc6, libstdc++6, libgcc-s1) và có thể cài đặt trên bất kỳ Ubuntu/Debian nào mà **không cần cài dependencies**.

## 🚀 Sử Dụng

### Quick Start

```bash
# Từ project root
./packaging/scripts/build_deb_all_in_one.sh --sdk-deb <path-to-sdk.deb>

# Hoặc từ thư mục scripts
cd packaging/scripts
./build_deb_all_in_one.sh --sdk-deb <path-to-sdk.deb>
```

### Options

```bash
--sdk-deb PATH    Path to SDK .deb file (required)
--clean           Clean build directory trước khi build
--no-build        Skip build (chỉ tạo package từ build có sẵn)
--version VER     Set version (default: auto-detect)
--help            Hiển thị help
```

### Ví Dụ

```bash
# Build với SDK
./packaging/scripts/build_deb_all_in_one.sh \
    --sdk-deb ../cvedix-ai-runtime-2025.0.1.3-x86_64.deb

# Clean build
./packaging/scripts/build_deb_all_in_one.sh \
    --sdk-deb ../cvedix-ai-runtime-2025.0.1.3-x86_64.deb \
    --clean

# Chỉ tạo package từ build có sẵn
./packaging/scripts/build_deb_all_in_one.sh \
    --sdk-deb ../cvedix-ai-runtime-2025.0.1.3-x86_64.deb \
    --no-build
```

## 📋 Khác Biệt Với build_deb_with_sdk.sh

| Tính năng | build_deb_with_sdk.sh | build_deb_all_in_one.sh |
|-----------|------------------------|--------------------------|
| CVEDIX SDK | ✅ Bundled | ✅ Bundled |
| OpenCV | ❌ Cần cài từ system | ✅ Bundled |
| GStreamer | ❌ Cần cài từ system | ✅ Bundled |
| GStreamer plugins | ❌ Cần cài từ system | ✅ Bundled |
| Default fonts (cvedix_data/font) | ❌ Không có | ✅ Bundled |
| Default models (cvedix_data/models) | ❌ Không có | ✅ Bundled |
| FFmpeg | ❌ Cần cài từ system | ✅ Bundled |
| Dependencies | Nhiều (50+ packages) | Chỉ system libraries cơ bản |
| Package size | Nhỏ hơn | Lớn hơn (do bundle nhiều) |
| Installation | Cần `apt-get install -f` | Chỉ cần `dpkg -i` |

## 📦 Cài Đặt Package

Sau khi build, file `.deb` sẽ được tạo tại project root:

```
edge-ai-api-all-in-one-2026.0.1.22-amd64.deb
```

### Cài Đặt

```bash
# Chỉ cần một lệnh duy nhất!
sudo dpkg -i edge-ai-api-all-in-one-2026.0.1.22-amd64.deb

# Không cần cài dependencies!
# Package tự chứa tất cả mọi thứ
```

### Verify Installation

```bash
# Kiểm tra package status
dpkg -l | grep edge-ai-api

# Kiểm tra libraries
ls -la /opt/edge_ai_api/lib/

# Kiểm tra GStreamer plugins
ls -la /opt/edge_ai_api/lib/gstreamer-1.0/

# Kiểm tra default fonts
ls -la /opt/edge_ai_api/fonts/

# Kiểm tra default models
ls -la /opt/edge_ai_api/models/

# Test executable
/usr/local/bin/edge_ai_api --help
```

## 🔧 Cách Hoạt Động

### 1. Bundle Libraries

Script tự động bundle tất cả libraries từ:

- **Build directory**: Libraries được build cùng project
- **CVEDIX SDK**: Từ extracted SDK .deb
- **System paths**: OpenCV, GStreamer, FFmpeg từ `/usr/lib`, `/usr/local/lib`
- **ldd output**: Tất cả dependencies từ executable

### 2. Bundle GStreamer Plugins

Script tự động tìm và bundle GStreamer plugins từ:

- `/usr/lib/x86_64-linux-gnu/gstreamer-1.0`
- `/usr/local/lib/gstreamer-1.0`
- `/usr/lib/gstreamer-1.0`

Plugins được copy vào `/opt/edge_ai_api/lib/gstreamer-1.0/`

### 3. Bundle Default Data (Fonts and Models)

Nếu project có `cvedix_data/font` và `cvedix_data/models`, chúng sẽ được tự động bundle vào package:

- **Fonts**: `cvedix_data/font/*` → `/opt/edge_ai_api/fonts/`
- **Models**: `cvedix_data/models/*` → `/opt/edge_ai_api/models/`

Đây là default data cho users trên máy mới, không cần upload thủ công.

### 4. Minimal Dependencies

File `debian/control` chỉ yêu cầu:

```
Depends: libc6, libstdc++6, libgcc-s1, adduser, systemd
```

Tất cả libraries khác đều được bundle trong package.

### 5. RPATH Configuration

Executables được cấu hình với RPATH:

```
/opt/edge_ai_api/lib:/opt/cvedix/lib
```

Đảm bảo tìm libraries từ bundled directory trước.

## 📊 Package Size

Package ALL-IN-ONE sẽ lớn hơn do bundle nhiều libraries:

- **build_deb_with_sdk.sh**: ~50-100 MB
- **build_deb_all_in_one.sh**: ~200-500 MB (tùy thuộc vào libraries được bundle)

## ⚠️ Lưu Ý

1. **Package size**: Package sẽ lớn hơn do bundle nhiều libraries
2. **Build time**: Build có thể lâu hơn do phải bundle nhiều libraries
3. **Disk space**: Cần đủ disk space để bundle libraries và data (ít nhất 5GB free)
4. **GStreamer plugins**: Plugins được bundle từ system, đảm bảo system có đầy đủ plugins
5. **OpenCV version**: OpenCV được bundle từ system, đảm bảo system có OpenCV 4.10+
6. **Default data**: Nếu `cvedix_data/font` và `cvedix_data/models` tồn tại trong project, chúng sẽ được tự động bundle vào package và cài đặt vào `/opt/edge_ai_api/fonts/` và `/opt/edge_ai_api/models/` làm default data

## 🔍 Troubleshooting

### Package quá lớn

Nếu package quá lớn, có thể:
- Kiểm tra xem có bundle duplicate libraries không
- Xem xét không bundle một số libraries không cần thiết

### Missing libraries

Nếu thiếu libraries sau khi cài đặt:
- Kiểm tra bundle_libs.sh có chạy đúng không
- Kiểm tra ldd output của executable
- Verify libraries trong `/opt/edge_ai_api/lib/`

### GStreamer plugins không hoạt động

Nếu GStreamer plugins không hoạt động:
- Kiểm tra `GST_PLUGIN_PATH` trong `.env` file
- Verify plugins trong `/opt/edge_ai_api/lib/gstreamer-1.0/`
- Check logs: `sudo journalctl -u edge-ai-api -n 50`

## 📚 Tài Liệu Liên Quan

- `BUILD_DEB.md` - Hướng dẫn build package thông thường
- `build_deb_with_sdk.sh` - Script build với SDK bundled (không all-in-one)
- `debian/control` - Package dependencies configuration
- `debian/bundle_libs.sh` - Script bundle libraries



