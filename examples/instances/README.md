# Instance Management Examples

Thư mục này chứa các ví dụ và dữ liệu mẫu cho các thao tác quản lý instance (CRUD operations).

## Cấu trúc thư mục

```
examples/instances/
├── README.md                              # File này
├── demo_script.sh                         # Script demo tự động các thao tác CRUD
│
├── create_*.json                          # Các file JSON mẫu cho CREATE
│   ├── create_face_detection_basic.json
│   ├── create_face_detection_rtmp.json
│   ├── create_face_detection_file_source.json
│   ├── create_object_detection.json
│   ├── create_thermal_detection.json
│   └── create_minimal.json
│
└── update_*.json                           # Các file JSON mẫu cho UPDATE
    ├── update_change_name_group.json
    ├── update_change_settings.json
    ├── update_change_rtsp_url.json
    ├── update_change_persistent_autostart.json
    └── update_change_model_path.json
```

## API Endpoints

### Base URL
```
http://localhost:8848/v1/core
```

### Endpoints

| Method | Endpoint | Mô tả |
|--------|----------|-------|
| POST | `/instance` | Tạo instance mới |
| GET | `/instances` | Liệt kê tất cả instances |
| GET | `/instances/{instanceId}` | Lấy thông tin chi tiết instance |
| GET | `/instances/{instanceId}/output` | Lấy output/processing results real-time của instance |
| PUT | `/instances/{instanceId}` | Cập nhật instance |
| POST | `/instances/{instanceId}/start` | Khởi động instance |
| POST | `/instances/{instanceId}/stop` | Dừng instance |
| POST | `/instances/{instanceId}/restart` | Khởi động lại instance |
| DELETE | `/instances/{instanceId}` | Xóa instance |

## 1. CREATE - Tạo Instance Mới

### 1.1. Face Detection Cơ Bản
```bash
curl -X POST http://localhost:8848/v1/core/instance \
  -H 'Content-Type: application/json' \
  -d @examples/instances/create_face_detection_basic.json
```

**File:** `create_face_detection_basic.json`
- Tạo instance face detection với RTSP source
- Không tự động start
- Có metadata mode

### 1.2. Face Detection với RTMP Streaming
```bash
curl -X POST http://localhost:8848/v1/core/instance \
  -H 'Content-Type: application/json' \
  -d @examples/instances/create_face_detection_rtmp.json
```

**File:** `create_face_detection_rtmp.json`
- Tạo instance với RTMP streaming output
- Tự động start và auto-restart
- Sử dụng file source thay vì RTSP

### 1.3. Object Detection (YOLO)
```bash
curl -X POST http://localhost:8848/v1/core/instance \
  -H 'Content-Type: application/json' \
  -d @examples/instances/create_object_detection.json
```

**File:** `create_object_detection.json`
- Tạo instance object detection với YOLO model
- Cấu hình đầy đủ các tham số detection

### 1.4. Face Detection từ File
```bash
curl -X POST http://localhost:8848/v1/core/instance \
  -H 'Content-Type: application/json' \
  -d @examples/instances/create_face_detection_file_source.json
```

**File:** `create_face_detection_file_source.json`
- Sử dụng file video làm source
- Bật debug mode và statistics mode

### 1.5. Thermal Detection
```bash
curl -X POST http://localhost:8848/v1/core/instance \
  -H 'Content-Type: application/json' \
  -d @examples/instances/create_thermal_detection.json
```

**File:** `create_thermal_detection.json`
- Cấu hình cho camera nhiệt
- Sensor modality: Thermal

### 1.6. Minimal Instance
```bash
curl -X POST http://localhost:8848/v1/core/instance \
  -H 'Content-Type: application/json' \
  -d @examples/instances/create_minimal.json
```

**File:** `create_minimal.json`
- Cấu hình tối thiểu, chỉ có name và solution
- Tất cả các tham số khác dùng giá trị mặc định

## 2. READ - Đọc Thông Tin Instance

### 2.1. Lấy Output/Processing Results Real-time

**Endpoint mới:** `GET /v1/core/instances/{instanceId}/output`

Lấy thông tin output và processing results real-time của instance tại thời điểm request.

```bash
curl -X GET http://localhost:8848/v1/core/instances/{instanceId}/output
```

**Response ví dụ (cho instance không có RTMP output):**
```json
{
  "timestamp": "2025-01-15 14:30:25.123",
  "instanceId": "abc-123-def",
  "displayName": "face_detection_file_source",
  "solutionId": "face_detection",
  "solutionName": "Face Detection",
  "running": true,
  "loaded": true,
  "metrics": {
    "fps": 25.50,
    "frameRateLimit": 0
  },
  "input": {
    "type": "FILE",
    "path": "/path/to/video.mp4"
  },
  "output": {
    "type": "FILE",
    "files": {
      "exists": true,
      "directory": "./output/abc-123-def",
      "fileCount": 15,
      "totalSizeBytes": 15728640,
      "totalSize": "15 MB",
      "latestFile": "face_detection_20250115_143025.mp4",
      "latestFileTime": "2025-01-15 14:30:25",
      "recentFileCount": 3,
      "isActive": true
    }
  },
  "detection": {
    "sensitivity": "Low",
    "mode": "SmartDetection",
    "movementSensitivity": "Low",
    "sensorModality": "RGB"
  },
  "modes": {
    "statisticsMode": true,
    "metadataMode": false,
    "debugMode": true,
    "diagnosticsMode": false
  },
  "status": {
    "running": true,
    "processing": true,
    "message": "Instance is running and processing frames"
  }
}
```

**Response ví dụ (cho instance có RTMP output):**
```json
{
  "timestamp": "2025-01-15 14:30:25.123",
  "instanceId": "xyz-789",
  "displayName": "face_detection_rtmp_stream",
  "solutionId": "face_detection_rtmp",
  "solutionName": "Face Detection RTMP",
  "running": true,
  "loaded": true,
  "metrics": {
    "fps": 30.0,
    "frameRateLimit": 25
  },
  "input": {
    "type": "FILE",
    "path": "/path/to/video.mp4"
  },
  "output": {
    "type": "RTMP_STREAM",
    "rtmpUrl": "rtmp://localhost:1935/live/face_stream",
    "rtspUrl": "rtsp://localhost:8554/live/face_stream_0"
  },
  "detection": {
    "sensitivity": "High",
    "mode": "SmartDetection",
    "movementSensitivity": "Low",
    "sensorModality": "RGB"
  },
  "modes": {
    "statisticsMode": true,
    "metadataMode": true,
    "debugMode": false,
    "diagnosticsMode": false
  },
  "status": {
    "running": true,
    "processing": true,
    "message": "Instance is running and processing frames"
  }
}
```

**Các trường quan trọng:**

| Trường | Mô tả |
|--------|-------|
| `timestamp` | Thời điểm lấy thông tin (real-time) |
| `metrics.fps` | FPS hiện tại của instance |
| `output.type` | Loại output: `FILE` hoặc `RTMP_STREAM` |
| `output.files` | Thông tin file output (nếu type = FILE) |
| `output.files.isActive` | `true` nếu có file mới được tạo trong 1 phút qua |
| `status.processing` | `true` nếu instance đang xử lý frames (running && fps > 0) |

**Sử dụng:**
- Kiểm tra real-time xem instance có đang xử lý không
- Xem số lượng file output và kích thước
- Kiểm tra FPS hiện tại
- Xác định loại output (FILE hoặc RTMP)

### 2.2. Liệt kê tất cả instances
```bash
curl -X GET http://localhost:8848/v1/core/instances
```

**Response:**
```json
{
  "instances": [
    {
      "instanceId": "abc-123",
      "displayName": "face_detection_basic_1",
      "group": "face_detection",
      "solutionId": "face_detection",
      "solutionName": "Face Detection",
      "running": false,
      "loaded": true,
      "persistent": false,
      "fps": 0.0
    }
  ],
  "total": 1,
  "running": 0,
  "stopped": 1
}
```

### 2.2. Lấy thông tin chi tiết một instance
```bash
curl -X GET http://localhost:8848/v1/core/instances/{instanceId}
```

**Response:**
```json
{
  "instanceId": "abc-123",
  "displayName": "face_detection_basic_1",
  "group": "face_detection",
  "solutionId": "face_detection",
  "solutionName": "Face Detection",
  "persistent": false,
  "loaded": true,
  "running": false,
  "fps": 0.0,
  "version": "1.0.0",
  "frameRateLimit": 30,
  "metadataMode": true,
  "statisticsMode": false,
  "diagnosticsMode": false,
  "debugMode": false,
  "readOnly": false,
  "autoStart": false,
  "autoRestart": false,
  "systemInstance": false,
  "inputPixelLimit": 0,
  "inputOrientation": 0,
  "detectorMode": "SmartDetection",
  "detectionSensitivity": "Medium",
  "movementSensitivity": "Low",
  "sensorModality": "RGB",
  "originator": {
    "address": "192.168.1.100"
  }
}
```

## 3. UPDATE - Cập Nhật Instance

### 3.1. Cập nhật tên và group
```bash
curl -X PUT http://localhost:8848/v1/core/instances/{instanceId} \
  -H 'Content-Type: application/json' \
  -d @examples/instances/update_change_name_group.json
```

**File:** `update_change_name_group.json`
```json
{
  "name": "updated_instance_name",
  "group": "updated_group_name"
}
```

### 3.2. Cập nhật các settings
```bash
curl -X PUT http://localhost:8848/v1/core/instances/{instanceId} \
  -H 'Content-Type: application/json' \
  -d @examples/instances/update_change_settings.json
```

**File:** `update_change_settings.json`
- Thay đổi frameRateLimit, metadataMode, statisticsMode, debugMode
- Thay đổi detectionSensitivity và movementSensitivity

### 3.3. Cập nhật RTSP URL
```bash
curl -X PUT http://localhost:8848/v1/core/instances/{instanceId} \
  -H 'Content-Type: application/json' \
  -d @examples/instances/update_change_rtsp_url.json
```

**File:** `update_change_rtsp_url.json`
- Thay đổi RTSP_URL trong additionalParams

### 3.4. Cập nhật persistent và autoStart
```bash
curl -X PUT http://localhost:8848/v1/core/instances/{instanceId} \
  -H 'Content-Type: application/json' \
  -d @examples/instances/update_change_persistent_autostart.json
```

**File:** `update_change_persistent_autostart.json`
- Bật persistent, autoStart và autoRestart

### 3.5. Cập nhật model path
```bash
curl -X PUT http://localhost:8848/v1/core/instances/{instanceId} \
  -H 'Content-Type: application/json' \
  -d @examples/instances/update_change_model_path.json
```

**File:** `update_change_model_path.json`
- Thay đổi MODEL_PATH và SFACE_MODEL_PATH

## 4. START - Khởi Động Instance

```bash
curl -X POST http://localhost:8848/v1/core/instances/{instanceId}/start \
  -H 'Content-Type: application/json'
```

**Response:**
```json
{
  "instanceId": "abc-123",
  "displayName": "face_detection_basic_1",
  "running": true,
  "message": "Instance started successfully"
}
```

**Lưu ý:** 
- Instance phải được tạo trước khi có thể start
- Instance phải có pipeline hợp lệ
- Nếu instance đang chạy, sẽ tự động stop và start lại

## 5. STOP - Dừng Instance

```bash
curl -X POST http://localhost:8848/v1/core/instances/{instanceId}/stop \
  -H 'Content-Type: application/json'
```

**Response:**
```json
{
  "instanceId": "abc-123",
  "displayName": "face_detection_basic_1",
  "running": false,
  "message": "Instance stopped successfully"
}
```

**Lưu ý:**
- Instance phải đang chạy để có thể stop
- Sau khi stop, pipeline sẽ bị giải phóng

## 6. RESTART - Khởi Động Lại Instance

```bash
curl -X POST http://localhost:8848/v1/core/instances/{instanceId}/restart \
  -H 'Content-Type: application/json'
```

**Response:**
```json
{
  "instanceId": "abc-123",
  "displayName": "face_detection_basic_1",
  "running": true,
  "message": "Instance restarted successfully"
}
```

**Lưu ý:**
- Restart = Stop + Start
- Nếu instance đang chạy, sẽ stop trước rồi start lại
- Nếu instance đã dừng, chỉ cần start

## 7. DELETE - Xóa Instance

```bash
curl -X DELETE http://localhost:8848/v1/core/instances/{instanceId}
```

**Response:**
```json
{
  "success": true,
  "message": "Instance deleted successfully",
  "instanceId": "abc-123"
}
```

**Lưu ý:**
- Instance sẽ tự động stop trước khi xóa
- Instance persistent có thể được lưu lại tùy cấu hình
- Không thể xóa system instance (readOnly = true)

## Sử dụng Demo Script

Chạy script demo tự động để test tất cả các thao tác:

```bash
# Sử dụng URL mặc định (http://localhost:8848)
chmod +x examples/instances/demo_script.sh
./examples/instances/demo_script.sh

# Hoặc chỉ định URL khác
./examples/instances/demo_script.sh http://192.168.1.100:8848
```

Script sẽ:
1. Tạo một instance mới
2. Liệt kê tất cả instances
3. Lấy thông tin chi tiết instance
4. Cập nhật instance
5. Start instance
6. Stop instance
7. Restart instance
8. (Tùy chọn) Delete instance

## Các Solution ID có sẵn

- `face_detection` - Face Detection cơ bản
- `face_detection_rtmp` - Face Detection với RTMP streaming
- `object_detection` - Object Detection với YOLO

## Các tham số quan trọng

### Tham số bắt buộc
- `name` - Tên instance (pattern: `^[A-Za-z0-9 -_]+$`)

### Tham số tùy chọn
- `group` - Nhóm instance
- `solution` - Solution ID
- `persistent` - Lưu instance khi restart server (default: false)
- `autoStart` - Tự động start khi tạo (default: false)
- `autoRestart` - Tự động restart khi crash (default: false)
- `frameRateLimit` - Giới hạn FPS (0 = không giới hạn)
- `metadataMode` - Gửi metadata (default: false)
- `statisticsMode` - Gửi statistics (default: false)
- `diagnosticsMode` - Gửi diagnostics (default: false)
- `debugMode` - Bật debug mode (default: false)
- `detectionSensitivity` - Độ nhạy detection: "Low", "Medium", "High"
- `movementSensitivity` - Độ nhạy movement: "Low", "Medium", "High"
- `sensorModality` - Loại sensor: "RGB", "Thermal"
- `inputOrientation` - Hướng xoay input: 0-3
- `inputPixelLimit` - Giới hạn số pixel input

### Additional Parameters (trong additionalParams)
- `RTSP_URL` - URL RTSP stream cho source
- `FILE_PATH` - Đường dẫn file video cho source
- `RTMP_URL` - URL RTMP cho destination streaming
- `MODEL_PATH` - Đường dẫn model file
- `SFACE_MODEL_PATH` - Đường dẫn SFace model (cho face recognition)
- `SFACE_MODEL_NAME` - Tên SFace model (thay cho path)
- `MODEL_NAME` - Tên model (thay cho path)
- `RESIZE_RATIO` - Tỷ lệ resize (default: "1.0")

## Troubleshooting

### Lỗi "Instance registry not initialized"
- Đảm bảo server đã khởi động đúng cách
- Kiểm tra logs để xem lỗi khởi tạo

### Lỗi "Solution not found"
- Kiểm tra solution ID có đúng không
- Xem danh sách solutions có sẵn: `GET /v1/core/solutions`

### Lỗi "Failed to create instance"
- Kiểm tra các tham số bắt buộc (RTSP_URL, MODEL_PATH, etc.)
- Kiểm tra đường dẫn file có tồn tại không
- Kiểm tra quyền truy cập file

### Instance không start được
- Kiểm tra RTSP_URL có hợp lệ không
- Kiểm tra MODEL_PATH có tồn tại không
- Xem logs để biết lỗi chi tiết

## Ví dụ workflow hoàn chỉnh

```bash
# 1. Tạo instance
INSTANCE_ID=$(curl -X POST http://localhost:8848/v1/core/instance \
  -H 'Content-Type: application/json' \
  -d @examples/instances/create_face_detection_basic.json \
  | jq -r '.instanceId')

echo "Created instance: $INSTANCE_ID"

# 2. Kiểm tra trạng thái
curl -X GET http://localhost:8848/v1/core/instances/$INSTANCE_ID | jq '.'

# 3. Start instance
curl -X POST http://localhost:8848/v1/core/instances/$INSTANCE_ID/start

# 4. Đợi một chút rồi kiểm tra FPS
sleep 5
curl -X GET http://localhost:8848/v1/core/instances/$INSTANCE_ID | jq '.fps'

# 5. Stop instance
curl -X POST http://localhost:8848/v1/core/instances/$INSTANCE_ID/stop

# 6. Xóa instance
curl -X DELETE http://localhost:8848/v1/core/instances/$INSTANCE_ID
```

## 8. Kiểm tra kết quả xử lý - Làm sao biết instance đã xử lý thành công?

Sau khi start instance, bạn cần kiểm tra xem instance có đang xử lý thành công hay không. Có nhiều cách để kiểm tra:

### 8.1. Sử dụng script kiểm tra tự động

**Script kiểm tra một lần:**
```bash
chmod +x examples/instances/check_instance_status.sh
./examples/instances/check_instance_status.sh <INSTANCE_ID>
```

Script này sẽ kiểm tra:
- ✓ Instance có tồn tại không
- ✓ Trạng thái running/stopped
- ✓ FPS (frames per second) - nếu > 0 nghĩa là đang xử lý
- ✓ Output files có được tạo không
- ✓ RTMP/RTSP stream (nếu có)

**Script monitor liên tục:**
```bash
chmod +x examples/instances/monitor_instance.sh
./examples/instances/monitor_instance.sh <INSTANCE_ID> [BASE_URL] [INTERVAL]
```

Script này sẽ hiển thị trạng thái real-time:
```
[14:30:15] RUNNING | FPS: 25.5 ↑ | Files: 42 (+3) | Name: face_detection_demo
```

### 8.2. Kiểm tra thủ công qua API

#### 8.2.1. Kiểm tra trạng thái cơ bản

```bash
curl -X GET http://localhost:8848/v1/core/instances/{instanceId} | jq '.'
```

**Các trường quan trọng:**

| Trường | Ý nghĩa | Giá trị thành công |
|--------|---------|-------------------|
| `running` | Instance có đang chạy không | `true` |
| `loaded` | Instance đã được load chưa | `true` |
| `fps` | Frames per second | `> 0` (nếu đang xử lý) |

**Ví dụ response thành công:**
```json
{
  "instanceId": "abc-123",
  "running": true,
  "loaded": true,
  "fps": 25.5,  // ← Quan trọng: FPS > 0 nghĩa là đang xử lý
  "displayName": "face_detection_demo"
}
```

**Kiểm tra nhanh FPS:**
```bash
curl -s http://localhost:8848/v1/core/instances/{instanceId} | jq '.fps'
```

Nếu FPS > 0 → Instance đang xử lý thành công!

#### 8.2.2. Kiểm tra output files

Nếu instance có `file_des` node, output sẽ được lưu vào thư mục:

```bash
# Kiểm tra thư mục output
ls -lht ./output/{instanceId}/

# Hoặc từ build directory
ls -lht ./build/output/{instanceId}/

# Xem file mới nhất
ls -lht ./output/{instanceId}/ | head -5

# Monitor files real-time
watch -n 1 'ls -lht ./output/{instanceId}/ | head -10'
```

**Dấu hiệu thành công:**
- ✓ Có file mới được tạo liên tục
- ✓ File có timestamp gần đây
- ✓ File có kích thước hợp lý (> 0 bytes)

#### 8.2.3. Kiểm tra RTMP stream

Nếu instance có RTMP output:

```bash
# Lấy RTMP URL từ instance info
RTMP_URL=$(curl -s http://localhost:8848/v1/core/instances/{instanceId} | jq -r '.rtmpUrl')

# Test stream bằng ffplay
ffplay $RTMP_URL

# Hoặc VLC
vlc $RTMP_URL
```

**Dấu hiệu thành công:**
- ✓ Stream có thể kết nối được
- ✓ Có video hiển thị
- ✓ Video có detection overlay (nếu có)

#### 8.2.4. Kiểm tra RTSP stream

Nếu instance có RTSP output:

```bash
# Lấy RTSP URL từ instance info
RTSP_URL=$(curl -s http://localhost:8848/v1/core/instances/{instanceId} | jq -r '.rtspUrl')

# Test stream
ffplay $RTSP_URL
```

### 8.3. Kiểm tra logs

**Xem logs của server:**
```bash
# Nếu chạy trực tiếp
tail -f /var/log/edge_ai_api.log

# Nếu chạy như service
sudo journalctl -u edge-ai-api -f

# Filter theo instance ID
tail -f /var/log/edge_ai_api.log | grep {instanceId}

# Filter theo processing logs (cho instances không có RTMP output)
tail -f /var/log/edge_ai_api.log | grep InstanceProcessingLog
```

**Các log message quan trọng:**

✅ **Thành công:**
- `Pipeline started successfully`
- `RTSP node start() completed`
- `Instance started successfully`
- `FPS: XX.X` (trong logs định kỳ)

❌ **Lỗi:**
- `Failed to connect to RTSP`
- `Model loading failed`
- `Pipeline error`
- `Exception in`

#### 8.3.1. Automatic Processing Result Logging

**Tính năng tự động log kết quả xử lý:**

Hệ thống tự động log kết quả xử lý real-time cho các instances **không có RTMP output**. Điều này giúp bạn theo dõi quá trình xử lý mà không cần kiểm tra output files hoặc stream.

**Khi nào logging được kích hoạt:**
- Instance không có `RTMP_URL` trong `additionalParams`
- Instance không có RTMP destination node trong pipeline
- Instance đang chạy (running = true)

**Nội dung log bao gồm:**
- Timestamp
- Instance ID và tên
- Solution ID và tên
- Trạng thái (RUNNING)
- FPS hiện tại
- Input source (FILE hoặc RTSP URL)
- Output type (File-based hoặc RTMP)
- Detection settings (sensitivity, mode)
- Processing modes (statistics, metadata, debug)
- Frame rate limit (nếu có)

**Tần suất log:**
- Log ban đầu: Sau 2 giây khi instance start
- Log định kỳ: Mỗi 10 giây một lần

**Ví dụ log:**
```
[InstanceProcessingLog] ========================================
[InstanceProcessingLog] [2025-01-15 14:30:25.123] Instance: face_detection_file_source (abc-123-def)
[InstanceProcessingLog] Solution: Face Detection (face_detection)
[InstanceProcessingLog] Status: RUNNING
[InstanceProcessingLog] FPS: 25.50
[InstanceProcessingLog] Input Source: FILE - /path/to/video.mp4
[InstanceProcessingLog] Output: File-based (no RTMP stream)
[InstanceProcessingLog] Output Directory: ./output/abc-123-def
[InstanceProcessingLog] Detection Sensitivity: Low
[InstanceProcessingLog] Statistics Mode: ENABLED
[InstanceProcessingLog] ========================================
```

**Lưu ý:**
- Logging chỉ áp dụng cho instances không có RTMP output
- Nếu instance có RTMP output, bạn có thể xem kết quả trực tiếp qua stream
- Logging tự động dừng khi instance bị stop hoặc delete

### 8.4. Checklist kiểm tra thành công

Sử dụng checklist này để đảm bảo instance đang hoạt động tốt:

```bash
#!/bin/bash
INSTANCE_ID="your-instance-id"
API_BASE="http://localhost:8848/v1/core"

echo "=== Checklist kiểm tra Instance ==="
echo ""

# 1. Instance tồn tại
echo -n "1. Instance tồn tại: "
if curl -s "${API_BASE}/instances/${INSTANCE_ID}" | jq -e '.instanceId' > /dev/null; then
    echo "✓"
else
    echo "✗"
    exit 1
fi

# 2. Instance đang chạy
echo -n "2. Instance đang chạy: "
RUNNING=$(curl -s "${API_BASE}/instances/${INSTANCE_ID}" | jq -r '.running')
if [ "$RUNNING" = "true" ]; then
    echo "✓"
else
    echo "✗ (Đang dừng)"
fi

# 3. FPS > 0
echo -n "3. FPS > 0 (đang xử lý): "
FPS=$(curl -s "${API_BASE}/instances/${INSTANCE_ID}" | jq -r '.fps')
if (( $(echo "$FPS > 0" | bc -l) )); then
    echo "✓ (FPS: $FPS)"
else
    echo "✗ (FPS: $FPS)"
fi

# 4. Có output files
echo -n "4. Có output files: "
if [ -d "./output/${INSTANCE_ID}" ]; then
    FILE_COUNT=$(find "./output/${INSTANCE_ID}" -type f | wc -l)
    if [ $FILE_COUNT -gt 0 ]; then
        echo "✓ ($FILE_COUNT files)"
    else
        echo "⚠ (Thư mục tồn tại nhưng chưa có file)"
    fi
else
    echo "ℹ (Không có file_des node)"
fi

echo ""
echo "=== Kết quả ==="
```

### 8.5. Các vấn đề thường gặp và cách xử lý

#### Vấn đề: Instance running nhưng FPS = 0

**Nguyên nhân có thể:**
- Input source không hợp lệ (RTSP_URL không kết nối được, FILE_PATH không tồn tại)
- Đang trong quá trình khởi động (đợi thêm 10-30 giây)
- Model chưa load xong

**Cách xử lý:**
```bash
# 1. Kiểm tra input source
curl -s http://localhost:8848/v1/core/instances/{instanceId} | jq '.additionalParams.RTSP_URL'

# 2. Test RTSP connection
ffprobe rtsp://your-rtsp-url

# 3. Xem logs để tìm lỗi
tail -f /var/log/edge_ai_api.log | grep -i error

# 4. Restart instance
curl -X POST http://localhost:8848/v1/core/instances/{instanceId}/restart
```

#### Vấn đề: Không có output files

**Nguyên nhân có thể:**
- Instance không có `file_des` node (chỉ có RTMP stream)
- Thư mục output không có quyền ghi
- Pipeline chưa start thành công

**Cách xử lý:**
```bash
# 1. Kiểm tra instance có file_des node không
curl -s http://localhost:8848/v1/core/instances/{instanceId} | jq '.solutionId'

# 2. Kiểm tra quyền thư mục
ls -ld ./output/{instanceId}/

# 3. Tạo thư mục thủ công nếu cần
mkdir -p ./output/{instanceId}
chmod 755 ./output/{instanceId}
```

#### Vấn đề: RTMP stream không hoạt động

**Cách xử lý:**
```bash
# 1. Kiểm tra RTMP URL
RTMP_URL=$(curl -s http://localhost:8848/v1/core/instances/{instanceId} | jq -r '.rtmpUrl')
echo "RTMP URL: $RTMP_URL"

# 2. Test RTMP server connection
ffmpeg -re -i test.mp4 -c copy -f flv $RTMP_URL

# 3. Kiểm tra RTMP server logs
```

### 8.6. Ví dụ workflow kiểm tra hoàn chỉnh

```bash
#!/bin/bash
# Workflow kiểm tra instance từ đầu đến cuối

INSTANCE_ID="your-instance-id"
API_BASE="http://localhost:8848/v1/core"

echo "=== Bước 1: Tạo instance ==="
INSTANCE_ID=$(curl -X POST ${API_BASE}/instance \
  -H 'Content-Type: application/json' \
  -d @examples/instances/create_face_detection_basic.json \
  | jq -r '.instanceId')
echo "Created: $INSTANCE_ID"

echo ""
echo "=== Bước 2: Start instance ==="
curl -X POST ${API_BASE}/instances/${INSTANCE_ID}/start

echo ""
echo "=== Bước 3: Đợi khởi động (10 giây) ==="
sleep 10

echo ""
echo "=== Bước 4: Kiểm tra trạng thái ==="
./examples/instances/check_instance_status.sh $INSTANCE_ID

echo ""
echo "=== Bước 5: Monitor real-time (30 giây) ==="
timeout 30 ./examples/instances/monitor_instance.sh $INSTANCE_ID

echo ""
echo "=== Bước 6: Kiểm tra output files ==="
ls -lht ./output/${INSTANCE_ID}/ | head -10

echo ""
echo "=== Hoàn thành! ==="
```

### 8.7. Tóm tắt các dấu hiệu thành công

✅ **Instance đang xử lý thành công khi:**

1. **API Status:**
   - `running = true`
   - `loaded = true`
   - `fps > 0` (quan trọng nhất!)

2. **Output Files:**
   - Có file mới được tạo liên tục trong `./output/{instanceId}/`
   - File có timestamp gần đây
   - File có kích thước hợp lý

3. **Streaming:**
   - RTMP/RTSP stream có thể kết nối được
   - Video hiển thị với detection overlay

4. **Logs:**
   - Không có error messages
   - Có log "Pipeline started successfully"
   - Có log FPS định kỳ

**Nếu tất cả các điều kiện trên đều đúng → Instance đang xử lý thành công!** 🎉

