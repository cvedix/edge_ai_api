# Các Bước Fix Record Output - Không Có File

## 🔍 Vấn Đề Hiện Tại

- ✅ RECORD_PATH đã được cấu hình: `/mnt/sb1/data`
- ✅ Instance đang chạy
- ❌ **FPS = 0.0** - Không có frame được xử lý
- ❌ Không có file trong thư mục

## 📋 Các Bước Fix

### Bước 1: Đảm Bảo Code Đã Được Rebuild

```bash
cd /home/cvedix/project/edge_ai_api
cd build
cmake ..
make -j$(nproc)

# Restart ứng dụng
pkill edge_ai_api
./bin/edge_ai_api
```

### Bước 2: Restart Instance Sau Khi Cấu Hình

**QUAN TRỌNG:** Instance phải được restart sau khi cấu hình để `file_des` node được thêm vào pipeline.

```bash
INSTANCE_ID="7ee356cd-109e-4a5a-b932-4130e2ea67f4"

# Stop instance
curl -X POST http://localhost:3546/v1/core/instance/$INSTANCE_ID/stop

# Start lại
curl -X POST http://localhost:3546/v1/core/instance/$INSTANCE_ID/start

# Kiểm tra status
curl -s http://localhost:3546/v1/core/instances | jq ".instances[] | select(.instanceId == \"$INSTANCE_ID\") | {running, fps}"
```

### Bước 3: Kiểm Tra Log - file_des Node Có Được Tạo Không

```bash
# Tìm log file
find /home/cvedix/project/edge_ai_api -name "*.log" -o -name "*.txt" | grep -E "log|txt" | head -5

# Kiểm tra log cho instance này
tail -500 /home/cvedix/project/edge_ai_api/log/2025-12-10.txt | grep -i "7ee356cd\|RECORD_PATH\|file_des_record\|Auto-adding\|Creating file destination" | tail -20
```

**Tìm các message:**
- `[PipelineBuilder] RECORD_PATH detected: /mnt/sb1/data` ✓
- `[PipelineBuilder] Auto-adding file_des node for recording...` ✓
- `[PipelineBuilder] ✓ Auto-added file_des node for recording to: /mnt/sb1/data` ✓

### Bước 4: Fix Vấn Đề FPS = 0

**Nguyên nhân:** Không có video input hoặc RTSP stream không hoạt động.

```bash
# Kiểm tra RTSP input
curl -s http://localhost:3546/v1/core/instance/$INSTANCE_ID/config | jq '.Input.uri'

# Test RTSP stream có hoạt động không
RTSP_URL=$(curl -s http://localhost:3546/v1/core/instance/$INSTANCE_ID/config | jq -r '.Input.uri' | grep -o 'rtsp://[^ ]*')
echo "RTSP URL: $RTSP_URL"

# Test với ffprobe (nếu có)
ffprobe -v error -show_entries stream=codec_name "$RTSP_URL" 2>&1 | head -5
```

**Nếu RTSP không hoạt động:**
- Kiểm tra RTSP server có đang chạy không
- Kiểm tra network connectivity
- Kiểm tra RTSP URL có đúng không

### Bước 5: Kiểm Tra Pipeline Connection

Nếu `file_des` node đã được tạo nhưng vẫn không có file:

```bash
# Kiểm tra log để xem pipeline có lỗi không
tail -200 /home/cvedix/project/edge_ai_api/log/2025-12-10.txt | grep -i "error\|exception\|failed" | tail -20

# Kiểm tra xem file_des node có được attach đúng không
# Log sẽ có message về việc attach nodes
```

## ✅ Checklist

Sau khi thực hiện các bước trên, kiểm tra:

- [ ] Code đã được rebuild (có tính năng auto-add file_des)
- [ ] Instance đã được restart sau khi cấu hình
- [ ] Log có message "RECORD_PATH detected"
- [ ] Log có message "Auto-added file_des node"
- [ ] FPS > 0 (có frame được xử lý)
- [ ] File xuất hiện trong `/mnt/sb1/data/`

## 🎯 Kết Quả Mong Đợi

Sau khi fix:
- FPS > 0 (ví dụ: FPS = 8.5)
- File MP4 xuất hiện trong `/mnt/sb1/data/`
- Tên file: `record_YYYYMMDD_HHMMSS.mp4`
- File tự động tạo mới mỗi 10 phút

