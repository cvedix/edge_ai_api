# Troubleshooting Record Output - Không Có File Được Tạo

## 🔍 Các Bước Kiểm Tra

### 1. Kiểm Tra Instance Status

```bash
# Lấy danh sách instances
curl -s http://localhost:3546/v1/core/instances

# Kiểm tra instance cụ thể
curl -s http://localhost:3546/v1/core/instance/{instanceId} | jq '.running, .loaded'
```

### 2. Kiểm Tra RECORD_PATH Đã Được Cấu Hình

```bash
# Kiểm tra output stream config
curl -s http://localhost:3546/v1/core/instance/{instanceId}/output/stream

# Kiểm tra trong instance config
curl -s http://localhost:3546/v1/core/instance/{instanceId}/config | jq '.AdditionalParams.RECORD_PATH'
```

**Kết quả mong đợi:**
```json
{
  "enabled": true,
  "path": "/mnt/sb1/data",
  "uri": "rtmp://localhost:1935/live/record_..."
}
```

### 3. Kiểm Tra Log - RECORD_PATH Có Được Phát Hiện Không

```bash
# Kiểm tra log systemd
journalctl -u edge-ai-api -n 200 | grep -i "RECORD_PATH\|file_des\|PipelineBuilder"

# Hoặc nếu chạy trực tiếp
tail -f /path/to/log | grep -i "RECORD_PATH\|file_des"
```

**Tìm các message sau:**
- `[PipelineBuilder] RECORD_PATH detected: /mnt/sb1/data`
- `[PipelineBuilder] Auto-adding file_des node for recording...`
- `[PipelineBuilder] ✓ Auto-added file_des node for recording to: /mnt/sb1/data`

### 4. Kiểm Tra Thư Mục

```bash
# Kiểm tra thư mục tồn tại và có quyền ghi
ls -ld /mnt/sb1/data
test -w /mnt/sb1/data && echo "Writable" || echo "NOT writable"

# Kiểm tra file
ls -lh /mnt/sb1/data/
```

### 5. Kiểm Tra Instance Đã Được Restart Sau Khi Cấu Hình

**Quan trọng:** Instance phải được **restart** sau khi cấu hình để `file_des` node được thêm vào pipeline.

```bash
# Restart instance
curl -X POST http://localhost:3546/v1/core/instance/{instanceId}/stop
curl -X POST http://localhost:3546/v1/core/instance/{instanceId}/start

# Kiểm tra lại
curl -s http://localhost:3546/v1/core/instance/{instanceId} | jq '.running'
```

## 🐛 Các Vấn Đề Thường Gặp

### Vấn đề 1: RECORD_PATH Không Được Phát Hiện

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
cd /home/cvedix/project/edge_ai_api
cd build
cmake ..
make -j$(nproc)

# 2. Restart ứng dụng
pkill edge_ai_api
./bin/edge_ai_api

# 3. Cấu hình lại và restart instance
curl -X POST http://localhost:3546/v1/core/instance/{instanceId}/output/stream \
  -H "Content-Type: application/json" \
  -d '{"enabled": true, "path": "/mnt/sb1/data"}'

curl -X POST http://localhost:3546/v1/core/instance/{instanceId}/stop
curl -X POST http://localhost:3546/v1/core/instance/{instanceId}/start
```

### Vấn đề 2: file_des Node Tạo Thất Bại

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

### Vấn đề 3: Instance Chạy Nhưng Không Có File

**Triệu chứng:**
- Instance đang chạy (`running: true`)
- `file_des` node đã được tạo (có trong log)
- Nhưng không có file trong thư mục

**Nguyên nhân:**
1. Không có video input
2. Pipeline không kết nối đúng
3. `file_des` node không nhận được frame

**Giải pháp:**
```bash
# 1. Kiểm tra input source
curl -s http://localhost:3546/v1/core/instance/{instanceId} | jq '.input'

# 2. Kiểm tra log để xem có frame được xử lý không
journalctl -u edge-ai-api -n 200 | grep -i "frame\|fps"

# 3. Kiểm tra pipeline có kết nối đúng không
# Xem log khi start instance - phải có message về việc attach nodes
```

### Vấn đề 4: Code Chưa Được Rebuild

**Triệu chứng:**
- Không có message "RECORD_PATH detected" trong log
- Code cũ không có tính năng auto-add `file_des` node

**Giải pháp:**
```bash
# Rebuild code
cd /home/cvedix/project/edge_ai_api
cd build
cmake ..
make -j$(nproc)

# Restart ứng dụng
pkill edge_ai_api
./bin/edge_ai_api

# Restart instance
curl -X POST http://localhost:3546/v1/core/instance/{instanceId}/stop
curl -X POST http://localhost:3546/v1/core/instance/{instanceId}/start
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
- [ ] Pipeline đang xử lý frame (có FPS trong log)

## 🔧 Script Debug

Sử dụng script debug tự động:

```bash
./scripts/debug_record_output.sh {instanceId}
```

Script sẽ kiểm tra tất cả các bước trên và đưa ra recommendations.

