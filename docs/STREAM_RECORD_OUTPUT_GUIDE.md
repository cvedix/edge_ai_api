# Hướng Dẫn Sử Dụng Stream/Record Output API

## 📋 Tổng Quan

API này cho phép bạn cấu hình **stream output** (phát video trực tiếp) hoặc **record output** (lưu video vào file) cho một instance.

## 🎯 Hai Chế Độ Hoạt Động

### 1. **Record Output Mode** (Lưu Video vào File)
- **Mục đích**: Lưu video đang chạy vào file MP4 trên local disk
- **Sử dụng khi**: Bạn muốn lưu lại video để xem sau, phân tích, hoặc backup
- **Cách dùng**: Gửi `path` trong request body

### 2. **Stream Output Mode** (Phát Video Trực Tiếp)
- **Mục đích**: Phát video trực tiếp qua RTMP/RTSP/HLS
- **Sử dụng khi**: Bạn muốn stream video đến MediaMTX, YouTube Live, hoặc các dịch vụ streaming khác
- **Cách dùng**: Gửi `uri` trong request body

## 🔧 API Endpoints

### POST /v1/core/instance/{instanceId}/output/stream

**Cấu hình stream/record output cho instance**

#### Request Body - Record Output Mode

```json
{
  "enabled": true,
  "path": "/mnt/sb1/data"
}
```

**Response:**
- **204 No Content** - Thành công (không có body)
- **400 Bad Request** - Lỗi validation (path không hợp lệ, không có quyền ghi, etc.)
- **404 Not Found** - Instance không tồn tại
- **500 Internal Server Error** - Lỗi server

#### Request Body - Stream Output Mode

```json
{
  "enabled": true,
  "uri": "rtmp://localhost:1935/live/stream"
}
```

#### Request Body - Disable Output

```json
{
  "enabled": false
}
```

### GET /v1/core/instance/{instanceId}/output/stream

**Lấy cấu hình stream/record output hiện tại**

#### Response - Record Output Enabled

```json
{
  "enabled": true,
  "uri": "rtmp://localhost:1935/live/record_a4d54476-475e-4790-a3c4-805e5c41fd9b",
  "path": "/mnt/sb1/data"
}
```

#### Response - Stream Output Enabled

```json
{
  "enabled": true,
  "uri": "rtmp://localhost:1935/live/stream",
  "path": ""
}
```

#### Response - Disabled

```json
{
  "enabled": false,
  "uri": "",
  "path": ""
}
```

## ✅ Cách Kiểm Tra Thành Công

### 1. Kiểm Tra Response Code

**POST Request:**
- ✅ **204 No Content** = Thành công
- ❌ **400/404/500** = Có lỗi (xem message trong response body)

```bash
# Kiểm tra response code
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/output/stream \
  -H "Content-Type: application/json" \
  -d '{"enabled": true, "path": "/mnt/sb1/data"}' \
  -w "\nHTTP Status: %{http_code}\n"
```

### 2. Kiểm Tra Bằng GET Endpoint

Sau khi POST thành công, dùng GET để xác nhận cấu hình:

```bash
# Lấy cấu hình hiện tại
curl -X GET http://localhost:8080/v1/core/instance/{instanceId}/output/stream

# Response sẽ trả về:
# {
#   "enabled": true,
#   "uri": "rtmp://localhost:1935/live/record_...",
#   "path": "/mnt/sb1/data"
# }
```

### 3. Kiểm Tra File Đã Được Tạo

Sau khi instance chạy và có video input, kiểm tra xem file đã được tạo chưa:

```bash
# Kiểm tra file trong thư mục
ls -lh /mnt/sb1/data/

# Hoặc xem file mới nhất
ls -lt /mnt/sb1/data/ | head -5
```

## 📝 Ví Dụ Sử Dụng

### Ví Dụ 1: Bật Record Output

```bash
# 1. Cấu hình record output
curl -X POST http://localhost:8080/v1/core/instance/a4d54476-475e-4790-a3c4-805e5c41fd9b/output/stream \
  -H "Content-Type: application/json" \
  -d '{
    "enabled": true,
    "path": "/mnt/sb1/data"
  }'

# 2. Kiểm tra cấu hình
curl -X GET http://localhost:8080/v1/core/instance/a4d54476-475e-4790-a3c4-805e5c41fd9b/output/stream

# 3. Đảm bảo instance đang chạy
curl -X POST http://localhost:8080/v1/core/instance/a4d54476-475e-4790-a3c4-805e5c41fd9b/start

# 4. Kiểm tra file đã được tạo (sau một lúc)
ls -lh /mnt/sb1/data/
```

### Ví Dụ 2: Bật Stream Output

```bash
# Cấu hình stream output
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/output/stream \
  -H "Content-Type: application/json" \
  -d '{
    "enabled": true,
    "uri": "rtmp://localhost:1935/live/stream"
  }'
```

### Ví Dụ 3: Tắt Output

```bash
# Tắt stream/record output
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/output/stream \
  -H "Content-Type: application/json" \
  -d '{
    "enabled": false
  }'
```

## 🎬 Ý Nghĩa của Record Output Mode

### Cách Hoạt Động

1. **Khi bạn POST với `path`**:
   - API sẽ lưu `RECORD_PATH` vào cấu hình instance (`AdditionalParams`)
   - Khi instance được start/restart, pipeline builder sẽ tự động phát hiện `RECORD_PATH`
   - Pipeline builder tự động thêm `file_des` node vào cuối pipeline
   - `file_des` node sẽ lưu video trực tiếp vào thư mục chỉ định

2. **Video được lưu**:
   - Format: **MP4** (tự động bởi CVEDIX SDK)
   - Location: Thư mục bạn chỉ định trong `path` (ví dụ: `/mnt/sb1/data`)
   - Tên file: Tự động tạo với prefix "record" và timestamp
   - Max duration: 10 phút mỗi file (tự động tạo file mới)
   - OSD: Có overlay (bounding boxes, labels, etc.)

3. **Khi instance chạy**:
   - Video từ input source (RTSP, file, etc.) sẽ được xử lý
   - Sau khi xử lý, video được gửi đến `file_des` node
   - `file_des` node tự động lưu video thành file MP4 vào thư mục chỉ định
   - **Không cần service bên ngoài** (MediaMTX/ffmpeg) - CVEDIX SDK tự xử lý

### Lưu Ý Quan Trọng

- ✅ **Instance phải được restart** sau khi cấu hình để `file_des` node được thêm vào pipeline
- ✅ **Instance phải đang chạy** để video được ghi
- ✅ **Thư mục phải có quyền ghi** (đã fix ở bước trước)
- ✅ **Không cần service bên ngoài** - CVEDIX SDK tự động lưu file trực tiếp
- ⚠️ **File chỉ được tạo khi có video input** và instance đang chạy
- ⚠️ **Cần rebuild code** sau khi cập nhật để có tính năng auto-add `file_des` node

## 🔍 Troubleshooting

### Vấn đề: POST thành công nhưng không thấy file

**Nguyên nhân có thể:**
1. Instance chưa được start
2. Không có video input
3. Service ghi (MediaMTX/ffmpeg) chưa được cấu hình

**Giải pháp:**
```bash
# 1. Kiểm tra instance đang chạy
curl -X GET http://localhost:8080/v1/core/instance/{instanceId}

# 2. Start instance nếu chưa chạy
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/start

# 3. Kiểm tra log để xem có lỗi không
tail -f /opt/edge_ai_api/logs/*.log
```

### Vấn đề: Lỗi "Path does not have write permissions"

**Giải pháp:**
```bash
# Fix quyền thư mục
sudo ./deploy/fix_external_data_permissions.sh --user cvedix --path /mnt/sb1/data
```

### Vấn đề: Response 204 nhưng GET không thấy field "path"

**Nguyên nhân:**
- Code đã được cập nhật nhưng chưa rebuild
- Ứng dụng đang chạy với code cũ

**Giải pháp:**
```bash
# 1. Dừng ứng dụng
ps aux | grep edge_ai_api | grep -v grep
kill <PID>

# 2. Rebuild
cd /home/cvedix/project/edge_ai_api
cd build
cmake ..
make -j$(nproc)

# 3. Restart
./build/bin/edge_ai_api

# 4. Test lại
curl -X GET http://localhost:8080/v1/core/instance/{instanceId}/output/stream
```

### Vấn đề: Response 204 nhưng GET không thấy cấu hình

**Nguyên nhân:**
- Instance có thể đã bị xóa hoặc reset

**Giải pháp:**
```bash
# Kiểm tra instance có tồn tại không
curl -X GET http://localhost:8080/v1/core/instance/{instanceId}

# Nếu không tồn tại, tạo lại instance
```

### Vấn đề: Không có file trong thư mục sau khi cấu hình

**Nguyên nhân:**
1. Instance chưa được restart sau khi cấu hình (cần restart để `file_des` node được thêm vào)
2. Instance chưa được start
3. Không có video input

**Giải pháp:**
```bash
# 1. Cấu hình record output
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/output/stream \
  -H "Content-Type: application/json" \
  -d '{"enabled": true, "path": "/mnt/sb1/data"}'

# 2. Restart instance để áp dụng thay đổi (quan trọng!)
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/stop
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/start

# 3. Kiểm tra instance đang chạy
curl -X GET http://localhost:8080/v1/core/instance/{instanceId} | jq '.running'

# 4. Kiểm tra file đã được tạo (sau một lúc)
ls -lh /mnt/sb1/data/
```

## 🎥 Cách Hoạt Động (Tự Động)

**Không cần cài đặt service bên ngoài!** CVEDIX SDK tự động xử lý việc lưu file.

Khi bạn cấu hình `path`:
1. API lưu `RECORD_PATH` vào instance config
2. Khi instance được start/restart, pipeline builder tự động:
   - Phát hiện `RECORD_PATH` trong `AdditionalParams`
   - Tạo `file_des` node với các tham số:
     - `save_dir`: Thư mục bạn chỉ định
     - `name_prefix`: "record"
     - `max_duration`: 10 phút mỗi file
     - `osd`: true (có overlay)
3. `file_des` node tự động lưu video thành file MP4

**Lưu ý:** Instance cần được **restart** sau khi cấu hình để `file_des` node được thêm vào pipeline.

## 🔍 Troubleshooting Chi Tiết

### Vấn đề: FPS = 0.0 - Không có frame được xử lý

**Nguyên nhân:**
- Không có video input hoạt động
- RTSP stream không accessible
- File video không đọc được
- Pipeline không nhận được frame

**Khi FPS = 0, `file_des` node không có gì để lưu → không có file được tạo.**

**Giải pháp:**

#### 1. Kiểm Tra và Fix Video Input

**Nếu dùng RTSP Stream:**
```bash
# Test RTSP stream có hoạt động không
RTSP_URL="rtsp://localhost:8554/live/camera_demo_1_0"
ffprobe -v error -show_entries stream=codec_name "$RTSP_URL" 2>&1 | head -5

# Hoặc dùng ffplay để xem
ffplay "$RTSP_URL"
```

**Nếu RTSP không hoạt động:**
- Kiểm tra RTSP server có đang chạy không
- Kiểm tra network connectivity
- Kiểm tra RTSP URL có đúng không
- Thử dùng file input thay vì RTSP

**Nếu dùng File Input:**
```bash
# Kiểm tra file có tồn tại không
FILE_PATH="/home/cvedix/project/cvedix_data/test_video/vehicle_count.mp4"
test -f "$FILE_PATH" && echo "File exists" || echo "File NOT found"

# Kiểm tra file có đọc được không
ffprobe -v error "$FILE_PATH" 2>&1 | head -5
```

#### 2. Đảm Bảo Code Đã Được Rebuild

```bash
cd /home/cvedix/project/edge_ai_api
cd build
cmake ..
make -j$(nproc)

# Restart ứng dụng
pkill edge_ai_api
./bin/edge_ai_api
```

#### 3. Kiểm Tra Log - file_des Node Có Được Tạo Không

```bash
# Kiểm tra log systemd
journalctl -u edge-ai-api -n 200 | grep -i "RECORD_PATH\|file_des\|PipelineBuilder"

# Hoặc nếu chạy trực tiếp
tail -f /path/to/log | grep -i "RECORD_PATH\|file_des"
```

**Tìm các message quan trọng:**
- `[PipelineBuilder] RECORD_PATH detected: /mnt/sb1/data` ✓
- `[PipelineBuilder] Auto-adding file_des node for recording...` ✓
- `[PipelineBuilder] ✓ Auto-added file_des node for recording to: /mnt/sb1/data` ✓

#### 4. Monitor FPS và Files

```bash
# Monitor FPS (phải > 0 để có file)
watch -n 1 'curl -s http://localhost:8080/v1/core/instances | jq ".instances[] | select(.instanceId == \"{instanceId}\") | {fps, running}"'

# Monitor files
watch -n 1 'ls -lht /mnt/sb1/data | head -10'
```

### Vấn đề: RECORD_PATH Không Được Phát Hiện

**Triệu chứng:**
- Log không có message "RECORD_PATH detected"
- `file_des` node không được tạo

**Nguyên nhân:**
1. Code chưa được rebuild
2. `RECORD_PATH` không được lưu vào `AdditionalParams`
3. Instance chưa được restart sau khi cấu hình

**Giải pháp:**
```bash
# 1. Rebuild code
cd /home/cvedix/project/edge_ai_api/build
cmake ..
make -j$(nproc)

# 2. Restart ứng dụng
pkill edge_ai_api
./bin/edge_ai_api

# 3. Cấu hình lại và restart instance
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/output/stream \
  -H "Content-Type: application/json" \
  -d '{"enabled": true, "path": "/mnt/sb1/data"}'

curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/stop
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/start
```

### Vấn đề: file_des Node Tạo Thất Bại

**Triệu chứng:**
- Log có "RECORD_PATH detected" nhưng không có "Auto-added file_des node"
- Log có "⚠ Failed to create file_des node"

**Nguyên nhân:**
1. Thư mục không có quyền ghi
2. Thư mục không tồn tại
3. Lỗi khi tạo `cvedix_file_des_node`

**Giải pháp:**
```bash
# Fix quyền thư mục
sudo ./deploy/fix_external_data_permissions.sh --user cvedix --path /mnt/sb1/data

# Kiểm tra log chi tiết
journalctl -u edge-ai-api -n 200 | grep -i "file_des\|exception\|error"
```

### Vấn đề: Instance Chạy Nhưng Không Có File

**Triệu chứng:**
- Instance đang chạy (`running: true`)
- `file_des` node đã được tạo (có trong log)
- Nhưng không có file trong thư mục

**Nguyên nhân:**
1. Không có video input
2. Pipeline không kết nối đúng
3. `file_des` node không nhận được frame
4. FPS = 0 (không có frame được xử lý)

**Giải pháp:**
```bash
# 1. Kiểm tra input source
curl -s http://localhost:8080/v1/core/instance/{instanceId} | jq '.input'

# 2. Kiểm tra FPS
curl -s http://localhost:8080/v1/core/instance/{instanceId} | jq '.fps'

# 3. Kiểm tra log để xem có frame được xử lý không
journalctl -u edge-ai-api -n 200 | grep -i "frame\|fps"
```

## ✅ Checklist Debug

- [ ] Instance đang chạy (`running: true`)
- [ ] RECORD_PATH đã được cấu hình (có trong output/stream response)
- [ ] RECORD_PATH có trong AdditionalParams (có trong config)
- [ ] Thư mục tồn tại và có quyền ghi
- [ ] Code đã được rebuild (có tính năng auto-add file_des)
- [ ] Instance đã được restart sau khi cấu hình
- [ ] Log có message "RECORD_PATH detected"
- [ ] Log có message "Auto-added file_des node"
- [ ] Instance có video input (RTSP, file, etc.)
- [ ] Pipeline đang xử lý frame (FPS > 0)
- [ ] File MP4 xuất hiện trong thư mục

## 🛠️ Helper Scripts

Sử dụng script helper để debug và quản lý record output:

```bash
# Quick status check
./scripts/record_output_helper.sh <instanceId> check

# Detailed debugging
./scripts/record_output_helper.sh <instanceId> debug

# Restart instance for record
./scripts/record_output_helper.sh <instanceId> restart
```

## 📚 Tài Liệu Tham Khảo

- [OpenAPI Specification](../openapi.yaml) - Chi tiết API endpoints
- [Directory Creation Guide](./DIRECTORY_CREATION_GUIDE.md) - Hướng dẫn tạo thư mục
- [MediaMTX Documentation](https://github.com/bluenviron/mediamtx) - Stream server

