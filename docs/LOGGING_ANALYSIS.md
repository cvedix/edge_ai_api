# Phân Tích Hệ Thống Logging

## 📋 Tổng Quan

Báo cáo này phân tích hệ thống logging của Edge AI API để kiểm tra:
1. Việc ghi log có làm tràn bộ nhớ không
2. Các loại log đang được ghi
3. Log runtime là gì

---

## ✅ Kết Luận: Logging KHÔNG Làm Tràn Bộ Nhớ

Hệ thống logging đã được thiết kế với nhiều cơ chế bảo vệ để **ngăn chặn tràn bộ nhớ**:

### 1. **Log Rotation (Xoay vòng log)**
- ✅ **Max file size**: 50MB mỗi file
- ✅ **Daily rotation**: Tự động tạo file mới mỗi ngày (format: `YYYY-MM-DD.log`)
- ✅ **RollingFileAppender**: Sử dụng Plog's RollingFileAppender để tự động rotate khi file đạt 50MB

### 2. **Automatic Cleanup (Tự động dọn dẹp)**
- ✅ **Retention period**: Tự động xóa log cũ hơn **30 ngày** (có thể cấu hình)
- ✅ **Cleanup thread**: Chạy background thread kiểm tra và cleanup mỗi **24 giờ**
- ✅ **Disk space monitoring**: Tự động cleanup khi disk usage > **85%**

### 3. **Disk Space Protection**
- ✅ **Threshold**: Khi disk usage >= 85%, tự động xóa log cũ hơn **7 ngày**
- ✅ **Monitoring**: Kiểm tra disk usage trước và sau cleanup
- ✅ **Warning**: Cảnh báo nếu disk vẫn còn đầy sau cleanup

### 4. **Cấu Hình Bảo Vệ**

| Tham Số | Giá Trị Mặc Định | Có Thể Cấu Hình | Mô Tả |
|---------|------------------|-----------------|-------|
| `max_file_size` | 50MB | ❌ Hardcoded | Kích thước tối đa mỗi file log |
| `LOG_RETENTION_DAYS` | 30 ngày | ✅ Env var | Số ngày giữ log (1-365) |
| `LOG_MAX_DISK_USAGE_PERCENT` | 85% | ✅ Env var | Ngưỡng disk usage để cleanup (50-95%) |
| `LOG_CLEANUP_INTERVAL_HOURS` | 24 giờ | ✅ Env var | Khoảng thời gian kiểm tra cleanup (1-168) |

---

## 📝 Các Loại Log Đang Được Ghi

Hệ thống có **4 loại log** được phân loại và lưu vào các thư mục riêng:

### 1. **API Logs** (`logs/api/`)
- **Khi nào**: Khi bật flag `--log-api` hoặc `--debug-api`
- **Nội dung**: 
  - Tất cả API requests/responses
  - HTTP method, path, status code
  - Response time
  - Instance ID (nếu có)
- **File**: `logs/api/YYYY-MM-DD.log`
- **Ví dụ**:
  ```
  [API] GET /v1/core/instances - Success: 5 instances - 12ms
  [API] POST /v1/core/instances/abc-123/start - Success - 234ms
  ```

### 2. **Instance Logs** (`logs/instance/`)
- **Khi nào**: Khi bật flag `--log-instance` hoặc `--debug-instance`
- **Nội dung**:
  - Instance lifecycle events (start/stop)
  - Instance status changes
  - Instance configuration changes
- **File**: `logs/instance/YYYY-MM-DD.log`
- **Ví dụ**:
  ```
  [Instance] Starting instance: abc-123 (Face Detection Camera 1)
  [Instance] Instance started successfully: abc-123 (running: true)
  ```

### 3. **SDK Output Logs** (`logs/sdk_output/`)
- **Khi nào**: Khi bật flag `--log-sdk-output` hoặc `--debug-sdk-output`
- **Nội dung**:
  - Output từ CVEDIX SDK khi instance xử lý
  - Detection results
  - Metadata từ pipeline
- **File**: `logs/sdk_output/YYYY-MM-DD.log`
- **Ví dụ**:
  ```
  [SDKOutput] Instance abc-123: Detection result - 3 faces detected
  [SDKOutput] Instance abc-123: FPS: 25.50, Latency: 40ms
  ```

### 4. **General Logs** (`logs/general/`)
- **Khi nào**: **Luôn được ghi** (không cần flag)
- **Nội dung**:
  - Application startup/shutdown
  - System errors
  - General application events
  - Logs không có prefix đặc biệt
- **File**: `logs/general/YYYY-MM-DD.log`
- **Ví dụ**:
  ```
  [INFO] Edge AI API Server starting...
  [INFO] Server will listen on: 0.0.0.0:8080
  [ERROR] Failed to start instance: abc-123
  ```

---

## 🔍 Log Runtime Là Gì?

**Log Runtime** = Log được ghi trong quá trình **runtime** (khi ứng dụng đang chạy), khác với:
- **Compile-time logs**: Log được tạo khi build/compile
- **Static logs**: Log được định nghĩa tĩnh trong code

### Đặc Điểm Log Runtime:

1. **Dynamic**: Được ghi dựa trên events xảy ra khi ứng dụng chạy
2. **Real-time**: Phản ánh trạng thái hiện tại của hệ thống
3. **Categorized**: Được phân loại theo category (API, Instance, SDK, General)
4. **Rotated**: Tự động rotate theo ngày và kích thước file

### Các Loại Log Runtime:

| Loại | Khi Nào Ghi | Ví Dụ |
|------|-------------|-------|
| **API Runtime Logs** | Mỗi API request | `[API] GET /v1/core/health - 5ms` |
| **Instance Runtime Logs** | Khi instance start/stop | `[Instance] Starting instance: abc-123` |
| **SDK Runtime Logs** | Khi SDK xử lý frame | `[SDKOutput] Detection: 3 objects` |
| **General Runtime Logs** | Events hệ thống | `[INFO] Server started on port 8080` |

---

## 📊 Ước Tính Dung Lượng Log

### Giả Định:
- **API logs**: 100 requests/giờ, mỗi log ~200 bytes → ~20KB/giờ → ~480KB/ngày
- **Instance logs**: 10 events/giờ, mỗi log ~300 bytes → ~3KB/giờ → ~72KB/ngày
- **SDK logs**: 30 FPS × 3600s = 108,000 logs/giờ, mỗi log ~150 bytes → ~16MB/giờ → ~384MB/ngày
- **General logs**: 50 events/giờ, mỗi log ~250 bytes → ~12.5KB/giờ → ~300KB/ngày

### Tổng Ước Tính:
- **Một ngày**: ~385MB (chủ yếu từ SDK logs nếu bật)
- **30 ngày**: ~11.5GB (trước khi cleanup)
- **Sau cleanup**: Chỉ giữ 30 ngày gần nhất

### Với Rotation (50MB/file):
- **API**: ~10 files/ngày (nếu bật)
- **Instance**: ~2 files/ngày (nếu bật)
- **SDK**: ~8 files/ngày (nếu bật) ⚠️ **Cao nhất**
- **General**: ~6 files/ngày

---

## ⚠️ Lưu Ý Quan Trọng

### 1. **SDK Output Logs Có Thể Tạo Nhiều Log**
- Nếu bật `--log-sdk-output` với nhiều instances chạy 30 FPS
- Có thể tạo **hàng trăm MB log mỗi ngày**
- **Khuyến nghị**: Chỉ bật khi cần debug, không bật trong production

### 2. **Disk Space Monitoring**
- Hệ thống tự động cleanup khi disk > 85%
- Nhưng nếu disk đầy quá nhanh, có thể vẫn bị tràn
- **Khuyến nghị**: Monitor disk usage thường xuyên

### 3. **Cleanup Thread**
- Chạy mỗi 24 giờ (có thể cấu hình)
- Nếu cần cleanup ngay, có thể gọi `LogManager::performCleanup()` thủ công

---

## 🔧 Cấu Hình Tối Ưu

### Production (Khuyến nghị):
```bash
# Chỉ bật general logs (mặc định)
# Không bật --log-api, --log-instance, --log-sdk-output

# Cấu hình cleanup tích cực
export LOG_RETENTION_DAYS=7        # Giữ 7 ngày
export LOG_MAX_DISK_USAGE_PERCENT=80  # Cleanup sớm hơn
export LOG_CLEANUP_INTERVAL_HOURS=12  # Kiểm tra mỗi 12 giờ
```

### Development (Debug):
```bash
# Bật tất cả logs để debug
./edge_ai_api --log-api --log-instance --log-sdk-output

# Giữ logs lâu hơn
export LOG_RETENTION_DAYS=30
export LOG_MAX_DISK_USAGE_PERCENT=90
```

---

## 📈 Monitoring Logs

### Kiểm Tra Dung Lượng Log:
```bash
# Xem tổng dung lượng logs
du -sh logs/

# Xem dung lượng từng category
du -sh logs/api/
du -sh logs/instance/
du -sh logs/sdk_output/
du -sh logs/general/

# Xem số lượng file log
find logs/ -name "*.log" | wc -l
```

### Kiểm Tra Disk Usage:
```bash
# Xem disk usage của thư mục logs
df -h logs/

# Hoặc sử dụng API
curl http://localhost:8080/v1/core/logs
```

---

## ✅ Kết Luận

1. **Logging KHÔNG làm tràn bộ nhớ** nhờ:
   - Rotation (50MB/file, daily)
   - Automatic cleanup (30 ngày)
   - Disk space monitoring (85% threshold)

2. **4 loại log đang được ghi**:
   - API logs (khi bật `--log-api`)
   - Instance logs (khi bật `--log-instance`)
   - SDK output logs (khi bật `--log-sdk-output`)
   - General logs (luôn bật)

3. **Log runtime** = Log được ghi khi ứng dụng đang chạy, phản ánh trạng thái real-time của hệ thống

4. **Khuyến nghị**:
   - Production: Chỉ bật general logs
   - Development: Có thể bật tất cả để debug
   - Monitor disk usage thường xuyên
   - Cấu hình cleanup tích cực nếu cần

---

**Last Updated**: 2025  
**Status**: ✅ Hệ thống logging an toàn, không làm tràn bộ nhớ

