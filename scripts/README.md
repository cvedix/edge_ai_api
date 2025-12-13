# Scripts - Troubleshooting & Utilities

Thư mục này chứa các script hỗ trợ để xử lý các vấn đề khi cài đặt và vận hành Edge AI API.

## ⚠️ Lưu Ý

**Các script trong thư mục này KHÔNG được chạy tự động trong quá trình setup.**  
Chỉ chạy các script này khi bạn gặp vấn đề cụ thể và cần khắc phục.

## 📋 Danh Sách Scripts

### 🔧 Scripts Khắc Phục Vấn Đề

#### `fix_all_symlinks.sh`
**Khi nào cần chạy:**
- Khi CMake báo lỗi không tìm thấy libraries (libtinyexpr.so, libcvedix_instance_sdk.so)
- Khi CMake báo lỗi không tìm thấy cereal hoặc cpp-base64 headers
- Khi build fail với lỗi "cannot find -lcvedix_instance_sdk" hoặc tương tự

**Cách chạy:**
```bash
sudo ./scripts/fix_all_symlinks.sh
```

**Script này sẽ tự động fix:**
- CVEDIX SDK library symlinks
- Cereal library symlink
- cpp-base64 library symlink
- OpenCV 4.10 symlinks (nếu cần)

---

#### `install_opencv_4.10.sh`
**Khi nào cần chạy:**
- Khi CVEDIX SDK yêu cầu OpenCV 4.10 nhưng hệ thống chỉ có OpenCV 4.6.0
- Khi build fail với lỗi liên quan đến OpenCV version
- Khi `fix_all_symlinks.sh` không đủ để giải quyết vấn đề OpenCV

**Cách chạy:**
```bash
./scripts/install_opencv_4.10.sh
```

**Lưu ý:** Script này sẽ build OpenCV từ source, có thể mất 30-60 phút.

---

### 🛠️ Scripts Tiện Ích

#### `load_env.sh`
**Khi nào cần chạy:**
- Khi muốn chạy server với environment variables từ file `.env`
- Script này được gọi tự động trong `setup.sh` (development mode)

**Cách chạy:**
```bash
./scripts/load_env.sh                    # Load .env và chạy server
./scripts/load_env.sh /path/to/.env      # Dùng .env file tùy chỉnh
./scripts/load_env.sh --load-only        # Chỉ load env, không chạy server
```

---

#### `run_tests.sh`
**Khi nào cần chạy:**
- Khi muốn chạy test suite
- Khi kiểm tra xem build có hoạt động đúng không

**Cách chạy:**
```bash
./scripts/run_tests.sh           # Chạy tests
./scripts/run_tests.sh build     # Build và chạy tests
```

---

#### `record_output_helper.sh`
**Khi nào cần chạy:**
- Khi gặp vấn đề với record output (không lưu file, lỗi encoding, etc.)
- Khi cần debug vấn đề record output
- Khi cần restart instance để fix record output

**Cách chạy:**
```bash
./scripts/record_output_helper.sh <instanceId> check     # Kiểm tra trạng thái
./scripts/record_output_helper.sh <instanceId> debug    # Debug vấn đề
./scripts/record_output_helper.sh <instanceId> restart  # Restart instance
```

---

#### `rtsp_helper.sh`
**Khi nào cần chạy:**
- Khi RTSP stream không hoạt động
- Khi cần debug RTSP connection
- Khi cần test RTSP stream

**Cách chạy:**
```bash
./scripts/rtsp_helper.sh <instanceId> <rtsp_url> check     # Kiểm tra trạng thái
./scripts/rtsp_helper.sh <instanceId> <rtsp_url> debug     # Debug pipeline
./scripts/rtsp_helper.sh <instanceId> <rtsp_url> diagnose  # Chẩn đoán connection
./scripts/rtsp_helper.sh <instanceId> <rtsp_url> test      # Test stream
```

---

#### `generate_default_solution_template.sh`
**Khi nào cần chạy:**
- Khi muốn tạo template code cho solution mới
- Khi phát triển solution mới

**Cách chạy:**
```bash
./scripts/generate_default_solution_template.sh
```

---

#### `restore_default_solutions.sh`
**Khi nào cần chạy:**
- Khi muốn reset solutions.json về trạng thái mặc định (rỗng)
- Khi default solutions bị lỗi và cần reset

**Cách chạy:**
```bash
./scripts/restore_default_solutions.sh
```

---

#### `install_dependencies.sh`
**Khi nào cần chạy:**
- Khi muốn cài đặt dependencies hệ thống một cách độc lập
- Khi `setup.sh` không cài được dependencies

**Cách chạy:**
```bash
./scripts/install_dependencies.sh
```

**Lưu ý:** Script này là optional. CMake sẽ tự động check dependencies.

---

## 🔄 Quy Trình Xử Lý Vấn Đề

### 1. Build Fail với Lỗi Libraries
```bash
# Bước 1: Chạy fix symlinks
sudo ./scripts/fix_all_symlinks.sh

# Bước 2: Thử build lại
cd build && cmake .. && make -j$(nproc)
```

### 2. Build Fail với Lỗi OpenCV
```bash
# Bước 1: Thử fix symlinks trước
sudo ./scripts/fix_all_symlinks.sh

# Bước 2: Nếu vẫn lỗi, cài OpenCV 4.10
./scripts/install_opencv_4.10.sh

# Bước 3: Build lại
cd build && cmake .. && make -j$(nproc)
```

### 3. CMake Configuration Fail
```bash
# Xem thông báo lỗi từ CMake
# CMake sẽ tự động đề xuất script cần chạy
# Thường là: sudo ./scripts/fix_all_symlinks.sh
```

---

## 📚 Tài Liệu Tham Khảo

- `docs/SCRIPTS_ANALYSIS.md` - Phân tích chi tiết về các scripts
- `docs/TROUBLESHOOTING.md` - Hướng dẫn troubleshooting
- `docs/CMAKE_ISSUES_ANALYSIS.md` - Phân tích các vấn đề CMake

---

## ⚡ Quick Reference

| Vấn Đề | Script Cần Chạy |
|--------|----------------|
| Không tìm thấy CVEDIX libraries | `sudo ./scripts/fix_all_symlinks.sh` |
| Không tìm thấy cereal/cpp-base64 | `sudo ./scripts/fix_all_symlinks.sh` |
| OpenCV version không đúng | `sudo ./scripts/fix_all_symlinks.sh` hoặc `./scripts/install_opencv_4.10.sh` |
| Record output không hoạt động | `./scripts/record_output_helper.sh <id> debug` |
| RTSP stream không hoạt động | `./scripts/rtsp_helper.sh <id> <url> diagnose` |
| Chạy server với .env | `./scripts/load_env.sh` |
| Chạy tests | `./scripts/run_tests.sh` |

