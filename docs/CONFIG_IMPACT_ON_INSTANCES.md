# Tác Dụng của Config với Instance Management

## 📋 Tổng Quan

File `config.json` quản lý cấu hình hệ thống và ảnh hưởng trực tiếp đến việc tạo và quản lý instances trong Edge AI API.

## 🎯 Các Tác Dụng Chính

### 1. **max_running_instances** - Giới Hạn Số Instance

**Vị trí trong config:**
```json
{
  "system": {
    "max_running_instances": 0
  }
}
```

**Tác dụng:**
- **Giá trị 0**: Không giới hạn số instance (unlimited) - mặc định
- **Giá trị > 0**: Giới hạn số instance tối đa có thể tạo

**Cách hoạt động:**
1. Khi client gọi `POST /v1/core/instance` để tạo instance mới
2. Hệ thống kiểm tra:
   - Đếm số instance hiện tại: `instance_registry_->getInstanceCount()`
   - So sánh với limit: `max_running_instances`
3. Nếu vượt quá limit:
   - Trả về HTTP **429 (Too Many Requests)**
   - Thông báo: `"Maximum instance limit reached: {limit}. Current instances: {count}"`
   - **KHÔNG tạo instance mới**

**Ví dụ:**
```json
{
  "system": {
    "max_running_instances": 10
  }
}
```
- Cho phép tối đa 10 instances
- Instance thứ 11 sẽ bị từ chối với HTTP 429

**Code thực thi:**
```cpp
// src/api/create_instance_handler.cpp:96-111
// Check max running instances limit
auto& systemConfig = SystemConfig::getInstance();
int maxInstances = systemConfig.getMaxRunningInstances();
if (maxInstances > 0) {
    int currentCount = instance_registry_->getInstanceCount();
    if (currentCount >= maxInstances) {
        // Trả về lỗi 429
        callback(createErrorResponse(429, "Too Many Requests", ...));
        return;
    }
}
```

---

### 2. **auto_device_list** - Danh Sách Thiết Bị AI

**Vị trí trong config:**
```json
{
  "auto_device_list": [
    "hailo.auto",
    "blaize.auto",
    "tensorrt.1",
    "rknn.auto",
    ...
  ]
}
```

**Tác dụng (Dự kiến):**
- Danh sách các thiết bị AI có sẵn trong hệ thống
- Khi tạo instance, hệ thống có thể tự động chọn device từ danh sách này
- Ưu tiên theo thứ tự trong list

**Trạng thái hiện tại:**
- ✅ Đã có API để get/set: `SystemConfig::getAutoDeviceList()`
- ⚠️ Chưa tích hợp vào logic tạo instance (có thể mở rộng sau)

**Sử dụng trong tương lai:**
```cpp
// Có thể sử dụng khi tạo instance
auto devices = systemConfig.getAutoDeviceList();
// Chọn device phù hợp cho instance
```

---

### 3. **decoder_priority_list** - Ưu Tiên Decoder

**Vị trí trong config:**
```json
{
  "decoder_priority_list": [
    "blaize.auto",
    "rockchip",
    "nvidia.1",
    "intel.1",
    "software"
  ]
}
```

**Tác dụng (Dự kiến):**
- Xác định thứ tự ưu tiên khi chọn decoder cho video stream
- Instance sẽ sử dụng decoder theo thứ tự ưu tiên này
- Fallback sang decoder tiếp theo nếu decoder trước không khả dụng

**Trạng thái hiện tại:**
- ✅ Đã có API để get/set: `SystemConfig::getDecoderPriorityList()`
- ⚠️ Chưa tích hợp vào pipeline builder (có thể mở rộng sau)

---

### 4. **gstreamer** - Cấu Hình GStreamer Pipeline

**Vị trí trong config:**
```json
{
  "gstreamer": {
    "decode_pipelines": {
      "auto": {
        "pipeline": "decodebin ! videoconvert",
        "capabilities": ["H264", "HEVC", "VP9", ...]
      },
      "jetson": {
        "pipeline": "parsebin ! nvv4l2decoder ! nvvidconv",
        "capabilities": ["H264", "HEVC"]
      },
      ...
    },
    "plugin_rank": {
      "nvv4l2decoder": "257",
      ...
    }
  }
}
```

**Tác dụng (Dự kiến):**
- Định nghĩa GStreamer pipeline cho từng platform
- Instance sẽ sử dụng pipeline phù hợp với platform
- Plugin rank xác định thứ tự ưu tiên của GStreamer plugins

**Trạng thái hiện tại:**
- ✅ Đã có API để get/set: 
  - `SystemConfig::getGStreamerPipeline(platform)`
  - `SystemConfig::getGStreamerCapabilities(platform)`
  - `SystemConfig::getGStreamerPluginRank(pluginName)`
- ⚠️ Chưa tích hợp vào pipeline builder (có thể mở rộng sau)

**Sử dụng trong tương lai:**
```cpp
// Khi build pipeline cho instance
std::string platform = detectPlatform(); // auto, jetson, nvidia, etc.
std::string pipeline = systemConfig.getGStreamerPipeline(platform);
// Sử dụng pipeline này cho instance
```

---

### 5. **system.web_server** - Cấu Hình Web Server

**Vị trí trong config:**
```json
{
  "system": {
    "web_server": {
      "enabled": true,
      "ip_address": "0.0.0.0",
      "port": 3546,
      "name": "default",
      "cors": {
        "enabled": false
      }
    }
  }
}
```

**Tác dụng:**
- Cấu hình web server chung cho toàn hệ thống
- Ảnh hưởng đến tất cả API endpoints, không chỉ instance
- CORS settings ảnh hưởng đến việc gọi API từ browser

**Trạng thái hiện tại:**
- ✅ Đã có API để get/set: `SystemConfig::getWebServerConfig()`
- ⚠️ Chưa tích hợp vào Drogon server startup (có thể mở rộng sau)

---

### 6. **system.logging** - Cấu Hình Logging

**Vị trí trong config:**
```json
{
  "system": {
    "logging": {
      "log_file": "logs/api.log",
      "log_level": "debug",
      "max_log_file_size": 52428800,
      "max_log_files": 3
    }
  }
}
```

**Tác dụng:**
- Cấu hình logging cho toàn hệ thống
- Ảnh hưởng đến log của tất cả instances
- Log rotation dựa trên `max_log_file_size` và `max_log_files`

**Trạng thái hiện tại:**
- ✅ Đã có API để get/set: `SystemConfig::getLoggingConfig()`
- ⚠️ Chưa tích hợp vào logger (có thể mở rộng sau)

---

## 🔄 Luồng Hoạt Động Khi Tạo Instance

```
1. Client gọi POST /v1/core/instance
   ↓
2. CreateInstanceHandler::createInstance()
   ↓
3. Validate request (solution, parameters, ...)
   ↓
4. ✅ KIỂM TRA max_running_instances
   ├─ Lấy limit: SystemConfig::getMaxRunningInstances()
   ├─ Đếm hiện tại: InstanceRegistry::getInstanceCount()
   ├─ So sánh: currentCount >= maxInstances?
   │
   ├─ Nếu VƯỢT QUÁ → HTTP 429, DỪNG
   └─ Nếu OK → Tiếp tục
   ↓
5. InstanceRegistry::createInstance()
   ↓
6. Tạo instance thành công
   ↓
7. Trả về instance info cho client
```

---

## 📊 Bảng Tóm Tắt

| Config Section | Tác Dụng với Instance | Trạng Thái | Mức Độ Ảnh Hưởng |
|---------------|----------------------|------------|------------------|
| **max_running_instances** | ✅ **Giới hạn số instance** | **Đã tích hợp** | **CAO** - Quyết định có tạo được instance hay không |
| auto_device_list | Chọn thiết bị AI | Có API, chưa dùng | TRUNG - Ảnh hưởng performance |
| decoder_priority_list | Chọn decoder | Có API, chưa dùng | TRUNG - Ảnh hưởng decoding |
| gstreamer | Pipeline config | Có API, chưa dùng | TRUNG - Ảnh hưởng video processing |
| web_server | Server config | Có API, chưa dùng | THẤP - Ảnh hưởng API access |
| logging | Log config | Có API, chưa dùng | THẤP - Ảnh hưởng debugging |

---

## 🎯 Kết Luận

### Hiện Tại (Đã Implement):
- ✅ **max_running_instances**: **HOẠT ĐỘNG** - Kiểm tra và từ chối tạo instance nếu vượt quá limit

### Tương Lai (Có Thể Mở Rộng):
- ⚠️ Các config khác đã có API sẵn, có thể tích hợp vào:
  - Pipeline builder để chọn device/decoder
  - Instance creation để apply GStreamer config
  - Server startup để apply web_server config
  - Logger để apply logging config

### Cách Sử Dụng Ngay Bây Giờ:

1. **Giới hạn số instance:**
   ```bash
   # Update config
   PATCH /v1/core/config/system.max_running_instances
   Body: 10
   
   # Hoặc qua file
   # Sửa config.json: "max_running_instances": 10
   ```

2. **Kiểm tra limit:**
   ```bash
   # Get current config
   GET /v1/core/config/system.max_running_instances
   
   # Get full config
   GET /v1/core/config
   ```

3. **Test limit:**
   ```bash
   # Tạo instance thứ 11 (nếu limit = 10)
   POST /v1/core/instance
   # → HTTP 429: "Maximum instance limit reached: 10. Current instances: 10"
   ```

---

## 📝 Lưu Ý

- **max_running_instances = 0**: Không giới hạn (mặc định)
- Config được load khi server khởi động
- Có thể update config qua API mà không cần restart server
- Thay đổi config sẽ được lưu vào file `config.json`
- Config được cache trong memory, đọc nhanh khi kiểm tra limit

---

## 🔗 Tài Liệu Liên Quan

- [CONFIG_API_GUIDE.md](./CONFIG_API_GUIDE.md) - Hướng dẫn chi tiết về Config API và cách sử dụng
- [CREATE_INSTANCE_GUIDE.md](./CREATE_INSTANCE_GUIDE.md) - Hướng dẫn tạo instance
- [Swagger UI](http://localhost:8080/swagger) - Interactive API documentation

