# Hướng dẫn Fix Lỗi CMake

Tài liệu này mô tả các lỗi CMake thường gặp và cách khắc phục khi build project edge_ai_api.

## Mục lục
1. [Lỗi thiếu header cvedix_yolov11_detector_node.h](#1-lỗi-thiếu-header-cvedix_yolov11_detector_nodeh)
2. [Lỗi thiếu libtinyexpr.so](#2-lỗi-thiếu-libtinyexprso)
3. [Lỗi thiếu libcvedix_instance_sdk.so](#3-lỗi-thiếu-libcvedix_instance_sdkso)
4. [Lỗi node types không được tìm thấy (RTSP/RTMP/Image source nodes)](#4-lỗi-node-types-không-được-tìm-thấy-rtsprtmpimage-source-nodes)

---

## 1. Lỗi thiếu header cvedix_yolov11_detector_node.h

### Triệu chứng
```
fatal error: cvedix/nodes/infers/cvedix_yolov11_detector_node.h: No such file or directory
   48 | #include <cvedix/nodes/infers/cvedix_yolov11_detector_node.h>
      |          ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
compilation terminated.
```

### Nguyên nhân
File header `cvedix_yolov11_detector_node.h` không tồn tại trong CVEDIX SDK. SDK chỉ cung cấp:
- `cvedix_yolo_detector_node.h` (YOLO generic)
- `cvedix_rknn_yolov11_detector_node.h` (YOLOv11 cho RKNN, chỉ khi có `CVEDIX_WITH_RKNN`)

### Giải pháp
Đã được fix trong code:
- File `src/core/pipeline_builder.cpp`: Comment include và cập nhật hàm `createYOLOv11DetectorNode()` để throw error rõ ràng
- Khi sử dụng `yolov11_detector`, sẽ nhận được thông báo lỗi hướng dẫn sử dụng `rknn_yolov11_detector` hoặc `yolo_detector` thay thế

### Kiểm tra
```bash
# Kiểm tra các file YOLO có sẵn trong SDK
ls -la /usr/include/cvedix/nodes/infers/ | grep -i yolo
ls -la /opt/cvedix/include/cvedix/nodes/infers/ | grep -i yolo
```

---

## 2. Lỗi thiếu libtinyexpr.so

### Triệu chứng
```
CMake Error at /usr/lib/cmake/cvedix/cvedix-targets.cmake:101 (message):
  The imported target "cvedix::tinyexpr" references the file
     "/usr/lib/libtinyexpr.so"
  but this file does not exist.
```

### Nguyên nhân
CVEDIX SDK được cài đặt ở `/opt/cvedix/` (non-standard location) nhưng CMake config tìm thư viện ở `/usr/lib/`. File thực tế nằm ở `/opt/cvedix/lib/libtinyexpr.so`.

### Giải pháp
Tạo symlink từ `/usr/lib/` đến file thực tế:

```bash
sudo ln -sf /opt/cvedix/lib/libtinyexpr.so /usr/lib/libtinyexpr.so
```

### Kiểm tra
```bash
# Kiểm tra file tồn tại
ls -la /opt/cvedix/lib/libtinyexpr.so

# Kiểm tra symlink đã được tạo
ls -la /usr/lib/libtinyexpr.so
# Kết quả mong đợi: lrwxrwxrwx ... /usr/lib/libtinyexpr.so -> /opt/cvedix/lib/libtinyexpr.so
```

---

## 3. Lỗi thiếu libcvedix_instance_sdk.so

### Triệu chứng
```
CMake Error at /usr/lib/cmake/cvedix/cvedix-targets.cmake:101 (message):
  The imported target "cvedix::cvedix_instance_sdk" references the file
     "/usr/lib/libcvedix_instance_sdk.so"
  but this file does not exist.
```

### Nguyên nhân
Tương tự lỗi trên, file thực tế nằm ở `/opt/cvedix/lib/libcvedix_instance_sdk.so` nhưng CMake tìm ở `/usr/lib/`.

### Giải pháp
Tạo symlink từ `/usr/lib/` đến file thực tế:

```bash
sudo ln -sf /opt/cvedix/lib/libcvedix_instance_sdk.so /usr/lib/libcvedix_instance_sdk.so
```

### Kiểm tra
```bash
# Kiểm tra file tồn tại
ls -la /opt/cvedix/lib/libcvedix_instance_sdk.so

# Kiểm tra symlink đã được tạo
ls -la /usr/lib/libcvedix_instance_sdk.so
# Kết quả mong đợi: lrwxrwxrwx ... /usr/lib/libcvedix_instance_sdk.so -> /opt/cvedix/lib/libcvedix_instance_sdk.so
```

---

## 4. Lỗi node types không được tìm thấy (RTSP/RTMP/Image source nodes)

### Triệu chứng
```
/home/cvedix/project/edge_ai_api/src/core/pipeline_builder.cpp:943:39: error: 'cvedix_rtsp_src_node' is not a member of 'cvedix_nodes'; did you mean 'cvedix_udp_src_node'?
  943 |         std::shared_ptr<cvedix_nodes::cvedix_rtsp_src_node> node;
      |                                       ^~~~~~~~~~~~~~~~~~~~

/home/cvedix/project/edge_ai_api/src/core/pipeline_builder.cpp:1766:52: error: 'cvedix_rtmp_des_node' is not a member of 'cvedix_nodes'; did you mean 'cvedix_file_des_node'?
 1766 |         auto node = std::make_shared<cvedix_nodes::cvedix_rtmp_des_node>(

/home/cvedix/project/edge_ai_api/src/core/pipeline_builder.cpp:3452:52: error: 'cvedix_image_src_node' is not a member of 'cvedix_nodes'; did you mean 'cvedix_file_src_node'?
 3452 |         auto node = std::make_shared<cvedix_nodes::cvedix_image_src_node>(

/home/cvedix/project/edge_ai_api/src/core/pipeline_builder.cpp:3515:52: error: 'cvedix_rtmp_src_node' is not a member of 'cvedix_nodes'; did you mean 'cvedix_udp_src_node'?
 3515 |         auto node = std::make_shared<cvedix_nodes::cvedix_rtmp_src_node>(
```

### Nguyên nhân
Các header files của CVEDIX SDK cho RTSP, RTMP, và Image source nodes được bọc trong điều kiện `#ifdef CVEDIX_WITH_GSTREAMER`. Nếu macro này không được định nghĩa trong quá trình biên dịch, các class này sẽ không được expose và compiler sẽ báo lỗi "not a member of namespace".

Các node types bị ảnh hưởng:
- `cvedix_rtsp_src_node` - RTSP source node
- `cvedix_rtmp_src_node` - RTMP source node  
- `cvedix_rtmp_des_node` - RTMP destination node
- `cvedix_image_src_node` - Image source node
- `cvedix_udp_src_node` - UDP source node (cũng cần GStreamer)

### Giải pháp
Đã được fix trong `CMakeLists.txt` và `tests/CMakeLists.txt`:

1. **Tự động phát hiện GStreamer** và định nghĩa macro `CVEDIX_WITH_GSTREAMER`:
   - Sử dụng `pkg-config` để kiểm tra GStreamer 1.0
   - Nếu không tìm thấy qua pkg-config, tìm thư viện trực tiếp
   - Nếu GStreamer được tìm thấy, tự động thêm `CVEDIX_WITH_GSTREAMER` vào compile definitions

2. **Áp dụng cho cả main target và test target**:
   - Main executable (`edge_ai_api`)
   - Test executable (`edge_ai_api_tests`)

### Code đã thêm vào CMakeLists.txt

```cmake
# Check if CVEDIX SDK was built with GStreamer support
# GStreamer is required for RTSP, RTMP, Image, and UDP source nodes
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(GSTREAMER QUIET gstreamer-1.0)
    if(GSTREAMER_FOUND)
        target_compile_definitions(edge_ai_api PRIVATE CVEDIX_WITH_GSTREAMER)
        message(STATUS "✓ GStreamer support enabled (CVEDIX_WITH_GSTREAMER)")
    else()
        message(WARNING "⚠ GStreamer not found. RTSP/RTMP/Image/UDP source nodes will not be available.")
        message(WARNING "  To install: sudo apt-get install libgstreamer1.0-dev")
    endif()
else()
    # If pkg-config not available, check for GStreamer libraries directly
    find_library(GSTREAMER_LIBRARY
        NAMES gstreamer-1.0
        PATHS
            /usr/lib
            /usr/lib/x86_64-linux-gnu
            /usr/local/lib
    )
    if(GSTREAMER_LIBRARY)
        target_compile_definitions(edge_ai_api PRIVATE CVEDIX_WITH_GSTREAMER)
        message(STATUS "✓ GStreamer support enabled (CVEDIX_WITH_GSTREAMER)")
    else()
        message(WARNING "⚠ GStreamer not found. RTSP/RTMP/Image/UDP source nodes will not be available.")
    endif()
endif()
```

### Kiểm tra

1. **Kiểm tra GStreamer đã được cài đặt:**
   ```bash
   pkg-config --exists gstreamer-1.0 && echo "GStreamer found" || echo "GStreamer not found"
   ```

2. **Kiểm tra macro đã được định nghĩa trong CMake:**
   ```bash
   cd build
   cmake .. 2>&1 | grep "GStreamer support"
   # Kết quả mong đợi: -- ✓ GStreamer support enabled (CVEDIX_WITH_GSTREAMER)
   ```

3. **Kiểm tra compile definitions:**
   ```bash
   cd build
   grep -r "CVEDIX_WITH_GSTREAMER" CMakeFiles/edge_ai_api.dir/compile_commands.json
   # Hoặc kiểm tra trong build log khi compile
   ```

### Cài đặt GStreamer (nếu chưa có)

Nếu GStreamer chưa được cài đặt:

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install libgstreamer1.0-dev gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly

# Kiểm tra sau khi cài đặt
pkg-config --modversion gstreamer-1.0
```

### Lưu ý

1. **GStreamer là bắt buộc** cho các node types sau:
   - RTSP source (`rtsp_src`)
   - RTMP source (`rtmp_src`)
   - RTMP destination (`rtmp_des`)
   - Image source (`image_src`)
   - UDP source (`udp_src`)

2. **Nếu không có GStreamer**, các node types này sẽ không khả dụng và bạn sẽ nhận được warning khi configure CMake.

3. **Test target cũng cần macro này** vì nó compile `pipeline_builder.cpp` chứa các node types này.

---

## Script tự động fix tất cả symlinks

Để tránh phải fix từng file một, bạn có thể chạy script sau để tạo tất cả symlinks cần thiết:

```bash
#!/bin/bash
# Script tạo symlinks cho CVEDIX SDK libraries

CVEDIX_LIB_DIR="/opt/cvedix/lib"
TARGET_LIB_DIR="/usr/lib"

# Danh sách các thư viện cần symlink
LIBS=(
    "libtinyexpr.so"
    "libcvedix_instance_sdk.so"
)

for lib in "${LIBS[@]}"; do
    SOURCE="${CVEDIX_LIB_DIR}/${lib}"
    TARGET="${TARGET_LIB_DIR}/${lib}"
    
    if [ -f "$SOURCE" ]; then
        if [ ! -e "$TARGET" ]; then
            echo "Creating symlink: $TARGET -> $SOURCE"
            sudo ln -sf "$SOURCE" "$TARGET"
        else
            echo "Symlink already exists: $TARGET"
        fi
    else
        echo "Warning: Source file not found: $SOURCE"
    fi
done

echo "Done! Verifying symlinks..."
ls -la /usr/lib/libtinyexpr.so /usr/lib/libcvedix_instance_sdk.so
```

Lưu script vào file `fix_cvedix_symlinks.sh`, chmod +x và chạy:
```bash
chmod +x fix_cvedix_symlinks.sh
./fix_cvedix_symlinks.sh
```

---

## Kiểm tra sau khi fix

Sau khi fix tất cả các lỗi, chạy lại CMake:

```bash
cd /home/cvedix/project/edge_ai_api/build
rm -rf CMakeCache.txt CMakeFiles/
cmake ..
```

Nếu thành công, bạn sẽ thấy:
```
-- Configuring done
-- Generating done
-- Build files have been written to: /home/cvedix/project/edge_ai_api/build
```

---

## Lưu ý

1. **Quyền sudo**: Các lệnh tạo symlink trong `/usr/lib/` cần quyền root
2. **Persistent**: Các symlinks sẽ tồn tại sau khi reboot, nhưng nếu CVEDIX SDK được cài đặt lại, có thể cần tạo lại
3. **Alternative**: Nếu không muốn tạo symlink trong `/usr/lib/`, có thể sửa CMake config của CVEDIX SDK, nhưng cách này phức tạp hơn và có thể bị ghi đè khi update SDK

---

## Troubleshooting

### Nếu vẫn gặp lỗi sau khi tạo symlink:

1. **Kiểm tra file tồn tại:**
   ```bash
   ls -la /opt/cvedix/lib/*.so
   ```

2. **Kiểm tra symlink đúng:**
   ```bash
   readlink -f /usr/lib/libtinyexpr.so
   # Phải trỏ đến: /opt/cvedix/lib/libtinyexpr.so
   ```

3. **Xóa cache CMake và build lại:**
   ```bash
   cd build
   rm -rf CMakeCache.txt CMakeFiles/
   cmake ..
   ```

4. **Kiểm tra CMake config:**
   ```bash
   cat /usr/lib/cmake/cvedix/cvedix-targets-debug.cmake | grep IMPORTED_LOCATION
   ```

---

## Tài liệu liên quan

- [CVEDIX SDK Documentation](https://cvedix.com/docs)
- [CMake Documentation](https://cmake.org/documentation/)

---

**Cập nhật lần cuối:** 2024-12-11

### Changelog

- **2024-12-11**: Thêm section 4 về lỗi CVEDIX_WITH_GSTREAMER cho RTSP/RTMP/Image source nodes

