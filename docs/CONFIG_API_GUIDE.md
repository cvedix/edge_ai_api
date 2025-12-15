# Hướng Dẫn Sử Dụng Config API

## 📋 Tổng Quan

Config API cho phép bạn quản lý cấu hình hệ thống Edge AI API thông qua REST API. Cấu hình này được lưu trong file `config.json` và ảnh hưởng trực tiếp đến hoạt động của hệ thống, đặc biệt là việc tạo và quản lý instances.

## 📍 Vị Trí File Config

Hệ thống tự động tìm và tạo file `config.json` theo thứ tự ưu tiên sau (với fallback tự động):

### Thứ Tự Ưu Tiên:

1. **Biến môi trường `CONFIG_FILE`** (ưu tiên cao nhất)
   ```bash
   export CONFIG_FILE="/opt/edge_ai_api/config/config.json"
   ```

2. **Thư mục hiện tại**: `./config.json`
   - Nếu file đã tồn tại → sử dụng ngay
   - Nếu không tồn tại → thử tạo thư mục và file

3. **Production path**: `/opt/edge_ai_api/config/config.json`
   - Tự động tạo thư mục nếu có quyền
   - Nếu không có quyền → fallback sang tầng tiếp theo

4. **System path**: `/etc/edge_ai_api/config.json`
   - Tự động tạo thư mục nếu có quyền
   - Nếu không có quyền → fallback sang tầng tiếp theo

5. **User config directory**: `~/.config/edge_ai_api/config.json`
   - Fallback khi không có quyền tạo production/system paths
   - Tuân thủ XDG Base Directory Specification

6. **Last resort**: `./config.json` (thư mục hiện tại)
   - Luôn có quyền ghi
   - Đảm bảo hệ thống luôn chạy được

### Ví Dụ:

```bash
# Production: Set biến môi trường
export CONFIG_FILE="/opt/edge_ai_api/config/config.json"
./build/edge_ai_api

# Hoặc để hệ thống tự động tìm (sẽ thử /opt trước)
./build/edge_ai_api
# Log sẽ hiển thị: "[EnvConfig] ✓ Created directory and will use: /opt/edge_ai_api/config/config.json"

# Development: File sẽ tự động tạo ở thư mục hiện tại nếu không có quyền
./build/edge_ai_api
# Log sẽ hiển thị: "[EnvConfig] ✓ Using last resort: ./config.json (current directory)"
```

**Lưu ý:** Hệ thống sẽ tự động tạo thư mục cha nếu cần thiết. Bạn chỉ cần đảm bảo có quyền ghi vào thư mục đó (hoặc để hệ thống tự động fallback).

## 🎯 Các Endpoint Config API

Tất cả các endpoint config nằm trong tag **"Config"** trong Swagger UI.

### 1. GET /v1/core/config

Lấy toàn bộ cấu hình hệ thống.

**Request:**
```bash
curl -X GET http://localhost:8080/v1/core/config
```

**Response (200 OK):**
```json
{
  "auto_device_list": [
    "hailo.auto",
    "blaize.auto",
    "tensorrt.1"
  ],
  "decoder_priority_list": [
    "blaize.auto",
    "rockchip",
    "software"
  ],
  "gstreamer": {
    "decode_pipelines": {
      "auto": {
        "pipeline": "decodebin ! videoconvert",
        "capabilities": ["H264", "HEVC", "VP9"]
      }
    },
    "plugin_rank": {
      "nvv4l2decoder": "257"
    }
  },
  "system": {
    "web_server": {
      "enabled": true,
      "ip_address": "0.0.0.0",
      "port": 3546,
      "name": "default",
      "cors": {
        "enabled": false
      }
    },
    "logging": {
      "log_file": "logs/api.log",
      "log_level": "debug",
      "max_log_file_size": 52428800,
      "max_log_files": 3
    },
    "max_running_instances": 0,
    "modelforge_permissive": false
  }
}
```

---

### 2. GET /v1/core/config?path={path}

Lấy một phần cấu hình theo đường dẫn.

**Query Parameters:**
- `path`: Đường dẫn đến section cần lấy (sử dụng `/` để phân cách)

**Ví dụ:**

```bash
# Lấy cấu hình web server
curl -X GET 'http://localhost:8080/v1/core/config?path=system/web_server'

# Lấy max_running_instances
curl -X GET 'http://localhost:8080/v1/core/config?path=system/max_running_instances'

# Lấy GStreamer pipeline cho platform "auto"
curl -X GET 'http://localhost:8080/v1/core/config?path=gstreamer/decode_pipelines/auto'
```

**✅ Lưu ý:**

- **Khuyến nghị**: Sử dụng **query parameter** `path` với dấu `/` hoặc dấu `.`
- **Hỗ trợ**: Path parameter với dấu `.` (ví dụ: `system.max_running_instances`)
- **Không hỗ trợ**: Path parameter với dấu `/` ngay cả khi URL encode (`system%2Fmax_running_instances`) vì Drogon tự động decode trước khi routing

**Ví dụ đúng:**
```bash
# ✅ Query parameter với dấu /
curl -X GET 'http://localhost:8080/v1/core/config?path=system/max_running_instances'

# ✅ Path parameter với dấu .
curl -X GET 'http://localhost:8080/v1/core/config/system.max_running_instances'

# ❌ Path parameter với URL encode %2F (KHÔNG hoạt động)
curl -X GET 'http://localhost:8080/v1/core/config/system%2Fmax_running_instances'
```

**Response (200 OK):**
```json
{
  "enabled": true,
  "ip_address": "0.0.0.0",
  "port": 3546
}
```

**Response (404 Not Found):**
```json
{
  "error": "Not found",
  "message": "Configuration section not found: system/invalid_path"
}
```

---

### 3. POST /v1/core/config

Tạo hoặc cập nhật cấu hình (merge với config hiện tại).

**Request:**
```bash
curl -X POST http://localhost:8080/v1/core/config \
  -H "Content-Type: application/json" \
  -d '{
    "system": {
      "max_running_instances": 10
    }
  }'
```

**Response (200 OK):**
```json
{
  "message": "Configuration updated successfully",
  "config": {
    // Full updated configuration
  }
}
```

**Lưu ý:** 
- Chỉ các field được gửi sẽ được cập nhật
- Các field khác giữ nguyên giá trị cũ
- Config được lưu vào file `config.json` sau khi update

---

### 4. PUT /v1/core/config

Thay thế toàn bộ cấu hình.

**Request:**
```bash
curl -X PUT http://localhost:8080/v1/core/config \
  -H "Content-Type: application/json" \
  -d '{
    "auto_device_list": ["hailo.auto"],
    "system": {
      "max_running_instances": 5
    }
  }'
```

**Response (200 OK):**
```json
{
  "message": "Configuration replaced successfully",
  "config": {
    // New complete configuration
  }
}
```

**Lưu ý:**
- ⚠️ **CẨN THẬN**: Endpoint này sẽ thay thế toàn bộ config, các field không được gửi sẽ bị xóa
- Nên sử dụng `POST` (merge) thay vì `PUT` (replace) trong hầu hết trường hợp

---

### 5. PATCH /v1/core/config?path={path}

Cập nhật một phần cấu hình tại đường dẫn cụ thể.

**Query Parameters:**
- `path`: Đường dẫn đến section cần cập nhật

**Request:**
```bash
# Cập nhật max_running_instances
curl -X PATCH 'http://localhost:8080/v1/core/config?path=system/max_running_instances' \
  -H "Content-Type: application/json" \
  -d '10'

# Cập nhật web server port
curl -X PATCH 'http://localhost:8080/v1/core/config?path=system/web_server' \
  -H "Content-Type: application/json" \
  -d '{
    "port": 8080
  }'
```

**Response (200 OK):**
```json
{
  "message": "Configuration section updated successfully",
  "path": "system/max_running_instances",
  "value": 10
}
```

---

### 6. DELETE /v1/core/config?path={path}

Xóa một phần cấu hình.

**Query Parameters:**
- `path`: Đường dẫn đến section cần xóa

**Request:**
```bash
# Xóa một section (ví dụ: xóa custom gstreamer pipeline)
curl -X DELETE 'http://localhost:8080/v1/core/config?path=gstreamer/decode_pipelines/custom'
```

**Response (200 OK):**
```json
{
  "message": "Configuration section deleted successfully",
  "path": "gstreamer/decode_pipelines/custom"
}
```

**Response (404 Not Found):**
```json
{
  "error": "Not found",
  "message": "Configuration section not found: gstreamer/decode_pipelines/custom"
}
```

---

### 7. POST /v1/core/config/reset

Reset toàn bộ cấu hình về giá trị mặc định (default).

**⚠️ CẢNH BÁO:** Endpoint này sẽ thay thế toàn bộ config bằng giá trị mặc định. Nên backup config trước khi reset.

**Request:**
```bash
curl -X POST http://localhost:8080/v1/core/config/reset
```

**Response (200 OK):**
```json
{
  "message": "Configuration reset to defaults successfully",
  "config": {
    // Toàn bộ config mặc định
    "auto_device_list": ["hailo.auto", "blaize.auto", ...],
    "system": {
      "web_server": {
        "enabled": true,
        "ip_address": "0.0.0.0",
        "port": 3546
      },
      "max_running_instances": 0
    },
    ...
  }
}
```

**Lưu ý:**
- ⚠️ **CẨN THẬN**: Endpoint này sẽ xóa toàn bộ config hiện tại và thay thế bằng giá trị mặc định
- Nên backup config trước khi reset: `curl -X GET http://localhost:8080/v1/core/config > config_backup.json`
- Config sẽ được lưu vào file sau khi reset thành công

---

## 📁 Cấu Trúc Config.json

### Tổng Quan

File `config.json` có cấu trúc như sau:

```json
{
  "auto_device_list": [...],
  "decoder_priority_list": [...],
  "gstreamer": {
    "decode_pipelines": {...},
    "plugin_rank": {...}
  },
  "system": {
    "web_server": {...},
    "logging": {...},
    "max_running_instances": 0,
    "modelforge_permissive": false
  }
}
```

### Chi Tiết Các Trường

#### 1. auto_device_list

Danh sách các thiết bị AI có sẵn trong hệ thống.

```json
{
  "auto_device_list": [
    "hailo.auto",
    "blaize.auto",
    "tensorrt.1",
    "rknn.auto",
    "tensorrt.2",
    "cavalry",
    "openvino.VPU",
    "openvino.GPU",
    "openvino.CPU",
    "snpe.dsp",
    "snpe.aip",
    "mnn.auto",
    "armnn.GpuAcc",
    "armnn.CpuAcc",
    "armnn.CpuRef",
    "memx.memx",
    "memx.cpu"
  ]
}
```

**Tác dụng:**
- Danh sách các thiết bị AI mà hệ thống có thể sử dụng
- Thứ tự trong list có thể ảnh hưởng đến việc chọn device (theo thứ tự ưu tiên)
- Hiện tại: Có API, chưa tích hợp vào logic tạo instance

---

#### 2. decoder_priority_list

Danh sách decoder theo thứ tự ưu tiên.

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

**Tác dụng:**
- Xác định thứ tự ưu tiên khi chọn decoder cho video stream
- Instance sẽ thử decoder theo thứ tự này
- Fallback sang decoder tiếp theo nếu decoder trước không khả dụng
- Hiện tại: Có API, chưa tích hợp vào pipeline builder

---

#### 3. gstreamer

Cấu hình GStreamer pipeline và plugin rank.

```json
{
  "gstreamer": {
    "decode_pipelines": {
      "auto": {
        "pipeline": "decodebin ! videoconvert",
        "capabilities": ["H264", "HEVC", "VP9", "VC1", "AV1", "MJPEG"]
      },
      "jetson": {
        "pipeline": "parsebin ! nvv4l2decoder ! nvvidconv",
        "capabilities": ["H264", "HEVC"]
      },
      "nvidia": {
        "pipeline": "decodebin ! nvvideoconvert ! videoconvert",
        "capabilities": ["H264", "HEVC", "VP9", "AV1", "MJPEG"]
      },
      "msdk": {
        "pipeline": "decodebin ! msdkvpp ! videoconvert",
        "capabilities": ["H264", "HEVC", "VP9", "VC1"]
      },
      "vaapi": {
        "pipeline": "decodebin ! vaapipostproc ! videoconvert",
        "capabilities": ["H264", "HEVC", "VP9", "AV1"]
      }
    },
    "plugin_rank": {
      "nvv4l2decoder": "257",
      "nvjpegdec": "257",
      "nvjpegenc": "257",
      "nvvidconv": "257",
      "msdkvpp": "257",
      "vaapipostproc": "257",
      "vpldec": "257",
      "qsv": "300",
      "qsvh265dec": "300",
      "qsvh264dec": "300",
      "qsvh265enc": "300",
      "qsvh264enc": "300",
      "amfh264dec": "300",
      "amfh265dec": "300",
      "amfhvp9dec": "300",
      "amfhav1dec": "300",
      "nvh264dec": "257",
      "nvh265dec": "257",
      "nvh264enc": "257",
      "nvh265enc": "257",
      "nvvp9dec": "257",
      "nvvp9enc": "257",
      "nvmpeg4videodec": "257",
      "nvmpeg2videodec": "257",
      "nvmpegvideodec": "257",
      "mpph264enc": "256",
      "mpph265enc": "256",
      "mppvp8enc": "256",
      "mppjpegenc": "256",
      "mppvideodec": "256",
      "mppjpegdec": "256"
    }
  }
}
```

**Tác dụng:**
- Định nghĩa GStreamer pipeline cho từng platform (auto, jetson, nvidia, msdk, vaapi)
- Plugin rank xác định thứ tự ưu tiên của GStreamer plugins
- Instance sẽ sử dụng pipeline phù hợp với platform của hệ thống
- Hiện tại: Có API, chưa tích hợp vào pipeline builder

---

#### 4. system.web_server

Cấu hình web server.

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

**Các trường:**
- `enabled`: Bật/tắt web server
- `ip_address`: Địa chỉ IP để bind (0.0.0.0 = tất cả interfaces)
- `port`: Port để lắng nghe
- `name`: Tên server
- `cors.enabled`: Bật/tắt CORS (Cross-Origin Resource Sharing)

**Tác dụng:**
- Ảnh hưởng đến tất cả API endpoints
- CORS settings ảnh hưởng đến việc gọi API từ browser
- Hiện tại: Có API, chưa tích hợp vào Drogon server startup

---

#### 5. system.logging

Cấu hình logging.

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

**Các trường:**
- `log_file`: Đường dẫn file log
- `log_level`: Mức độ log (debug, info, warning, error)
- `max_log_file_size`: Kích thước tối đa file log (bytes) - mặc định 50MB
- `max_log_files`: Số lượng file log tối đa (rotation)

**Tác dụng:**
- Cấu hình logging cho toàn hệ thống
- Ảnh hưởng đến log của tất cả instances
- Log rotation dựa trên `max_log_file_size` và `max_log_files`
- Hiện tại: Có API, chưa tích hợp vào logger

---

#### 6. system.max_running_instances ⭐

**QUAN TRỌNG**: Giới hạn số instance tối đa có thể tạo.

```json
{
  "system": {
    "max_running_instances": 0
  }
}
```

**Giá trị:**
- `0`: Không giới hạn (unlimited) - **Mặc định**
- `> 0`: Giới hạn số instance tối đa

**Tác dụng:**
- ✅ **ĐÃ TÍCH HỢP**: Kiểm tra khi tạo instance mới
- Nếu số instance hiện tại >= limit → Từ chối tạo instance mới (HTTP 429)
- Quyết định có tạo được instance hay không

**Cách hoạt động:**
1. Client gọi `POST /v1/core/instance` để tạo instance
2. Hệ thống kiểm tra:
   - Đếm số instance hiện tại: `InstanceRegistry::getInstanceCount()`
   - So sánh với limit: `SystemConfig::getMaxRunningInstances()`
3. Nếu vượt quá limit:
   - Trả về HTTP **429 (Too Many Requests)**
   - Message: `"Maximum instance limit reached: {limit}. Current instances: {count}"`
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
auto& systemConfig = SystemConfig::getInstance();
int maxInstances = systemConfig.getMaxRunningInstances();
if (maxInstances > 0) {
    int currentCount = instance_registry_->getInstanceCount();
    if (currentCount >= maxInstances) {
        callback(createErrorResponse(429, "Too Many Requests", 
            "Maximum instance limit reached: " + std::to_string(maxInstances) + 
            ". Current instances: " + std::to_string(currentCount)));
        return;
    }
}
```

---

#### 7. system.modelforge_permissive

Flag cho phép modelforge permissive mode.

```json
{
  "system": {
    "modelforge_permissive": false
  }
}
```

**Tác dụng:**
- Chưa được sử dụng trong code hiện tại
- Có thể được sử dụng trong tương lai để kiểm soát modelforge behavior

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

## 📊 Bảng Tóm Tắt Config và Tác Động

| Config Section | Tác Dụng với Instance | Trạng Thái | Mức Độ Ảnh Hưởng |
|---------------|----------------------|------------|------------------|
| **max_running_instances** | ✅ **Giới hạn số instance** | **Đã tích hợp** | **CAO** - Quyết định có tạo được instance hay không |
| auto_device_list | Chọn thiết bị AI | Có API, chưa dùng | TRUNG - Ảnh hưởng performance |
| decoder_priority_list | Chọn decoder | Có API, chưa dùng | TRUNG - Ảnh hưởng decoding |
| gstreamer | Pipeline config | Có API, chưa dùng | TRUNG - Ảnh hưởng video processing |
| web_server | Server config | Có API, chưa dùng | THẤP - Ảnh hưởng API access |
| logging | Log config | Có API, chưa dùng | THẤP - Ảnh hưởng debugging |
| modelforge_permissive | Modelforge mode | Có API, chưa dùng | THẤP - Chưa rõ tác dụng |

---

## 💡 Ví Dụ Sử Dụng

### Ví Dụ 1: Giới Hạn Số Instance

**Tình huống:** Bạn muốn giới hạn hệ thống chỉ cho phép tối đa 5 instances.

**Bước 1: Kiểm tra config hiện tại**
```bash
curl -X GET http://localhost:8080/v1/core/config/system/max_running_instances
```

**Bước 2: Cập nhật limit**
```bash
curl -X PATCH 'http://localhost:8080/v1/core/config?path=system/max_running_instances' \
  -H "Content-Type: application/json" \
  -d '5'
```

**Bước 3: Verify**
```bash
curl -X GET 'http://localhost:8080/v1/core/config?path=system/max_running_instances'
# Response: 5
```

**Bước 4: Test**
```bash
# Tạo 5 instances thành công
for i in {1..5}; do
  curl -X POST http://localhost:8080/v1/core/instance \
    -H "Content-Type: application/json" \
    -d '{"solution": "face_detection"}'
done

# Tạo instance thứ 6 → HTTP 429
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d '{"solution": "face_detection"}'
# Response: HTTP 429
# {
#   "error": "Too Many Requests",
#   "message": "Maximum instance limit reached: 5. Current instances: 5"
# }
```

---

### Ví Dụ 2: Cập Nhật Web Server Port

**Tình huống:** Bạn muốn thay đổi port của web server từ 3546 sang 8080.

**Cách 1: Update toàn bộ web_server config**
```bash
curl -X PATCH 'http://localhost:8080/v1/core/config?path=system/web_server' \
  -H "Content-Type: application/json" \
  -d '{
    "enabled": true,
    "ip_address": "0.0.0.0",
    "port": 8080,
    "name": "default",
    "cors": {
      "enabled": false
    }
  }'
```

**Cách 2: Chỉ update port (merge)**
```bash
curl -X POST http://localhost:8080/v1/core/config \
  -H "Content-Type: application/json" \
  -d '{
    "system": {
      "web_server": {
        "port": 8080
      }
    }
  }'
```

**Lưu ý:** Thay đổi port sẽ chỉ có hiệu lực sau khi restart server.

---

### Ví Dụ 3: Thêm Custom GStreamer Pipeline

**Tình huống:** Bạn muốn thêm một custom GStreamer pipeline cho platform "custom".

```bash
curl -X POST http://localhost:8080/v1/core/config \
  -H "Content-Type: application/json" \
  -d '{
    "gstreamer": {
      "decode_pipelines": {
        "custom": {
          "pipeline": "uridecodebin ! videoconvert ! video/x-raw,format=NV12",
          "capabilities": ["H264", "HEVC"]
        }
      }
    }
  }'
```

**Verify:**
```bash
curl -X GET 'http://localhost:8080/v1/core/config?path=gstreamer/decode_pipelines/custom'
```

---

### Ví Dụ 4: Backup và Restore Config

**Backup config:**
```bash
curl -X GET http://localhost:8080/v1/core/config > config_backup.json
```

**Restore config:**
```bash
curl -X PUT http://localhost:8080/v1/core/config \
  -H "Content-Type: application/json" \
  -d @config_backup.json
```

**Lưu ý:** ⚠️ Sử dụng `PUT` sẽ thay thế toàn bộ config. Đảm bảo file backup đầy đủ.

---

## 🎯 Best Practices

### 1. Sử dụng POST (merge) thay vì PUT (replace)

**✅ Tốt:**
```bash
# Chỉ update field cần thiết
curl -X POST http://localhost:8080/v1/core/config \
  -H "Content-Type: application/json" \
  -d '{
    "system": {
      "max_running_instances": 10
    }
  }'
```

**❌ Tránh:**
```bash
# PUT sẽ xóa tất cả các field khác
curl -X PUT http://localhost:8080/v1/core/config \
  -H "Content-Type: application/json" \
  -d '{
    "system": {
      "max_running_instances": 10
    }
  }'
# ⚠️ Các field khác (gstreamer, auto_device_list, ...) sẽ bị xóa!
```

---

### 2. Backup config trước khi thay đổi lớn

```bash
# Backup
curl -X GET http://localhost:8080/v1/core/config > config_backup_$(date +%Y%m%d_%H%M%S).json

# Thực hiện thay đổi
curl -X POST http://localhost:8080/v1/core/config ...

# Nếu có vấn đề, restore
curl -X PUT http://localhost:8080/v1/core/config \
  -H "Content-Type: application/json" \
  -d @config_backup_*.json
```

---

### 3. Kiểm tra config sau khi update

```bash
# Update config
curl -X PATCH http://localhost:8080/v1/core/config/system/max_running_instances \
  -H "Content-Type: application/json" \
  -d '10'

# Verify
curl -X GET http://localhost:8080/v1/core/config/system/max_running_instances
# Expected: 10
```

---

### 4. Sử dụng path cụ thể cho update nhỏ

**✅ Tốt:**
```bash
# Chỉ update một field
curl -X PATCH http://localhost:8080/v1/core/config/system/max_running_instances \
  -H "Content-Type: application/json" \
  -d '10'
```

**⚠️ Có thể:**
```bash
# Update nhiều field cùng lúc
curl -X POST http://localhost:8080/v1/core/config \
  -H "Content-Type: application/json" \
  -d '{
    "system": {
      "max_running_instances": 10,
      "web_server": {
        "port": 8080
      }
    }
  }'
```

---

## 📝 Lưu Ý Quan Trọng

1. **max_running_instances = 0**: Không giới hạn (mặc định)
2. **Config được load khi server khởi động**: Thay đổi config file trực tiếp cần restart server
3. **Update qua API không cần restart**: Config được cập nhật ngay lập tức trong memory
4. **Config được lưu vào file**: Thay đổi qua API sẽ được lưu vào `config.json`
5. **Config được cache trong memory**: Đọc nhanh khi kiểm tra limit
6. **Thread-safe**: SystemConfig sử dụng mutex để đảm bảo thread-safety
7. **Validation**: Config được validate trước khi lưu (phải là JSON object hợp lệ)

---

## 🔗 Tài Liệu Liên Quan

- [CREATE_INSTANCE_GUIDE.md](./CREATE_INSTANCE_GUIDE.md) - Hướng dẫn tạo instance
- [UPDATE_INSTANCE_GUIDE.md](./UPDATE_INSTANCE_GUIDE.md) - Hướng dẫn cập nhật instance
- [Swagger UI](http://localhost:8080/swagger) - Interactive API documentation

---

## 🐛 Xử Lý Lỗi

### Lỗi 400 - Bad Request

**Nguyên nhân:**
- Request body không phải JSON hợp lệ
- Thiếu required fields

**Ví dụ:**
```json
{
  "error": "Invalid request",
  "message": "Request body must be valid JSON"
}
```

**Giải pháp:**
- Kiểm tra JSON syntax
- Đảm bảo Content-Type header là `application/json`

---

### Lỗi 404 - Not Found

**Nguyên nhân:**
- Path không tồn tại trong config

**Ví dụ:**
```json
{
  "error": "Not found",
  "message": "Configuration section not found: system/invalid_path"
}
```

**Giải pháp:**
- Kiểm tra path có đúng không
- Sử dụng `GET /v1/core/config` để xem cấu trúc config hiện tại

---

### Lỗi 500 - Internal Server Error

**Nguyên nhân:**
- Lỗi khi lưu config vào file
- Lỗi validation

**Ví dụ:**
```json
{
  "error": "Internal server error",
  "message": "Failed to update configuration"
}
```

**Giải pháp:**
- Kiểm tra quyền ghi file `config.json`
- Kiểm tra disk space
- Xem server logs để biết chi tiết

---

## 📞 Hỗ Trợ

Nếu gặp vấn đề, vui lòng:
1. Kiểm tra Swagger UI tại `/swagger` để xem API documentation
2. Xem server logs để biết chi tiết lỗi
3. Kiểm tra file `config.json` có hợp lệ không
4. Tham khảo [CREATE_INSTANCE_GUIDE.md](./CREATE_INSTANCE_GUIDE.md) để hiểu rõ hơn về cách tạo instance và tác động của config

