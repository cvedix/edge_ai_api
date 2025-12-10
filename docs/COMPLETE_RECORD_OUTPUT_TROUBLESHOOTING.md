# Hướng Dẫn Đầy Đủ - Fix Record Output Không Có File

## 🔍 Tình Trạng Hiện Tại

- ✅ RECORD_PATH đã được cấu hình: `/mnt/sb1/data`
- ✅ Instance đang chạy (`running: true`)
- ❌ **FPS = 0.0** - Không có frame được xử lý
- ❌ Không có file trong `/mnt/sb1/data`

## 🎯 Nguyên Nhân Chính

**FPS = 0.0** nghĩa là:
- Không có video input hoạt động
- RTSP stream không accessible
- File video không đọc được
- Pipeline không nhận được frame

**Khi FPS = 0, `file_des` node không có gì để lưu → không có file được tạo.**

## 📋 Các Bước Fix

### Bước 1: Kiểm Tra và Fix Video Input

#### Nếu dùng RTSP Stream:

```bash
# Test RTSP stream có hoạt động không
RTSP_URL="rtsp://anhoidong.datacenter.cvedix.com:8554/live/camera_demo_1_0"
ffprobe -v error -show_entries stream=codec_name "$RTSP_URL" 2>&1 | head -5

# Hoặc dùng ffplay để xem
ffplay "$RTSP_URL"
```

**Nếu RTSP không hoạt động:**
- Kiểm tra RTSP server có đang chạy không
- Kiểm tra network connectivity
- Kiểm tra RTSP URL có đúng không
- Thử dùng file input thay vì RTSP

#### Nếu dùng File Input:

```bash
# Kiểm tra file có tồn tại không
FILE_PATH="/home/cvedix/project/cvedix_data/test_video/vehicle_count.mp4"
test -f "$FILE_PATH" && echo "File exists" || echo "File NOT found"

# Kiểm tra file có đọc được không
ffprobe -v error "$FILE_PATH" 2>&1 | head -5
```

### Bước 2: Đảm Bảo Code Đã Được Rebuild

```bash
cd /home/cvedix/project/edge_ai_api
cd build
cmake ..
make -j$(nproc)

# Restart ứng dụng
pkill edge_ai_api
./bin/edge_ai_api
```

### Bước 3: Restart Instance Sau Khi Cấu Hình

**QUAN TRỌNG:** Instance phải được restart sau khi cấu hình để `file_des` node được thêm vào pipeline.

```bash
INSTANCE_ID="82d819ba-9f4c-4fc5-b44c-28c23d9f6ca2"

# Restart instance
curl -X POST http://localhost:3546/v1/core/instances/$INSTANCE_ID/stop
sleep 2
curl -X POST http://localhost:3546/v1/core/instances/$INSTANCE_ID/start
sleep 3

# Hoặc dùng script
./scripts/restart_instance_for_record.sh $INSTANCE_ID
```

### Bước 4: Kiểm Tra Log - file_des Node Có Được Tạo Không

```bash
# Tìm log file
find /home/cvedix/project/edge_ai_api -name "*.log" -o -name "*.txt" | grep -E "log|txt" | head -5

# Kiểm tra log cho instance
tail -500 /home/cvedix/project/edge_ai_api/log/2025-12-10.txt | grep -i "82d819ba\|RECORD_PATH\|file_des_record\|Auto-adding\|Creating file destination" | tail -30
```

**Tìm các message quan trọng:**
- `[PipelineBuilder] RECORD_PATH detected: /mnt/sb1/data` ✓
- `[PipelineBuilder] Auto-adding file_des node for recording...` ✓
- `[PipelineBuilder] Creating file destination node:` ✓
- `[PipelineBuilder] ✓ Auto-added file_des node for recording to: /mnt/sb1/data` ✓

### Bước 5: Monitor FPS và Files

```bash
# Monitor FPS (phải > 0 để có file)
watch -n 1 'curl -s http://localhost:3546/v1/core/instances | jq ".instances[] | select(.instanceId == \"82d819ba-9f4c-4fc5-b44c-28c23d9f6ca2\") | {fps, running}"'

# Monitor files
watch -n 1 'ls -lht /mnt/sb1/data | head -10'
```

## ✅ Kết Quả Mong Đợi

Sau khi fix:
1. **FPS > 0** (ví dụ: 8.5, 15.0, 30.0)
2. **File MP4 xuất hiện** trong `/mnt/sb1/data/`
3. **Tên file:** `record_YYYYMMDD_HHMMSS.mp4`
4. **File tự động tạo mới** mỗi 10 phút

## 🐛 Troubleshooting

### Vấn đề: FPS = 0 Sau Khi Restart

**Nguyên nhân:**
- RTSP stream không hoạt động
- File video không tồn tại hoặc không đọc được
- Pipeline có lỗi

**Giải pháp:**
```bash
# 1. Kiểm tra input source
curl -s http://localhost:3546/v1/core/instance/{instanceId}/config | jq '.Input'

# 2. Test RTSP stream
ffprobe "rtsp://..." 2>&1

# 3. Test file
ffprobe "/path/to/file.mp4" 2>&1

# 4. Kiểm tra log để xem có lỗi không
tail -200 /home/cvedix/project/edge_ai_api/log/2025-12-10.txt | grep -i "error\|exception\|failed"
```

### Vấn đề: Log Không Có "RECORD_PATH detected"

**Nguyên nhân:**
- Code chưa được rebuild
- Instance chưa được restart sau khi cấu hình

**Giải pháp:**
```bash
# 1. Rebuild code
cd /home/cvedix/project/edge_ai_api/build
cmake ..
make -j$(nproc)

# 2. Restart ứng dụng
pkill edge_ai_api
./bin/edge_ai_api

# 3. Restart instance
curl -X POST http://localhost:3546/v1/core/instances/{instanceId}/stop
curl -X POST http://localhost:3546/v1/core/instances/{instanceId}/start
```

### Vấn đề: file_des Node Tạo Thất Bại

**Triệu chứng:**
- Log có "RECORD_PATH detected" nhưng không có "Auto-added file_des node"
- Log có "⚠ Failed to create file_des node"

**Giải pháp:**
```bash
# Fix quyền thư mục
sudo ./deploy/fix_external_data_permissions.sh --user cvedix --path /mnt/sb1/data

# Kiểm tra log chi tiết
tail -500 /home/cvedix/project/edge_ai_api/log/2025-12-10.txt | grep -i "file_des\|exception\|error" | tail -20
```

## 📊 Checklist

- [ ] Code đã được rebuild (có tính năng auto-add file_des)
- [ ] RECORD_PATH đã được cấu hình (`/mnt/sb1/data`)
- [ ] Instance đã được restart sau khi cấu hình
- [ ] Log có message "RECORD_PATH detected"
- [ ] Log có message "Auto-added file_des node"
- [ ] Video input hoạt động (RTSP stream accessible hoặc file tồn tại)
- [ ] FPS > 0 (có frame được xử lý)
- [ ] File MP4 xuất hiện trong `/mnt/sb1/data/`

## 🎯 Tóm Tắt

**Vấn đề chính:** FPS = 0 → không có frame → không có file

**Giải pháp:**
1. Fix video input (RTSP stream hoặc file)
2. Đảm bảo FPS > 0
3. File sẽ tự động xuất hiện khi có frame được xử lý

**Lưu ý:** `file_des` node chỉ lưu file khi có frame được xử lý. Nếu FPS = 0, sẽ không có file nào được tạo.


