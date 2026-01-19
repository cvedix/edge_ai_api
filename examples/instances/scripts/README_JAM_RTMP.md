# Hướng Dẫn Test Jam Node với RTMP Stream Output

## 📋 Tổng Quan

Tài liệu này hướng dẫn cách test một instance xử lý với jam node và stream output kết quả ra một luồng RTMP để kiểm tra kết quả xử lý.

## 🎯 Workflow

1. **Tạo instance** với solution `ba_jam_default` hoặc `ba_jam`
2. **Thêm jam zones** vào instance để định nghĩa các vùng phát hiện kẹt xe
3. **Cấu hình RTMP stream output** để stream kết quả xử lý ra RTMP server
4. **Khởi động instance** để bắt đầu xử lý
5. **Kiểm tra kết quả** qua RTMP stream và API endpoints

## 🚀 Sử Dụng Script Test

### Cách 1: Sử dụng script tự động

```bash
# Sử dụng với URL mặc định (http://localhost:8080)
./test_jam_rtmp.sh

# Hoặc chỉ định BASE_URL và RTMP_URL (ví dụ port 8848)
./test_jam_rtmp.sh http://localhost:8848 rtmp://localhost:1935/live/jam_test
```

### Cách 2: Thực hiện thủ công từng bước

#### Bước 1: Tạo instance với ba_jam solution

```bash
curl -X POST http://localhost:8848/v1/core/instance \
  -H 'Content-Type: application/json' \
  -d '{
    "name": "jam_test_instance",
    "group": "jam_detection",
    "solution": "ba_jam_default",
    "persistent": false,
    "autoStart": false,
    "frameRateLimit": 30,
    "metadataMode": true,
    "additionalParams": {
      "RTSP_URL": "rtsp://localhost:8554/live/stream1"
    }
  }'
```

Lưu `instanceId` từ response để sử dụng cho các bước tiếp theo.

#### Bước 2: Thêm jam zones

```bash
INSTANCE_ID="your-instance-id-here"

curl -X POST http://localhost:8848/v1/core/instance/${INSTANCE_ID}/jams \
  -H 'Content-Type: application/json' \
  -d '{
    "name": "Downtown Jam Zone",
    "roi": [
      {"x": 0, "y": 100},
      {"x": 1920, "y": 100},
      {"x": 1920, "y": 400},
      {"x": 0, "y": 400}
    ],
    "enabled": true,
    "check_interval_frames": 20,
    "check_min_hit_frames": 50,
    "check_max_distance": 8,
    "check_min_stops": 8,
    "check_notify_interval": 10
  }'
```

**Tham số jam zone:**
- `roi`: Mảng các điểm định nghĩa vùng phát hiện (tối thiểu 3 điểm)
- `check_interval_frames`: Số frame giữa các lần kiểm tra (mặc định: 20)
- `check_min_hit_frames`: Số frame tối thiểu để xác nhận jam (mặc định: 50)
- `check_max_distance`: Khoảng cách tối đa giữa các object để coi là jam (mặc định: 8)
- `check_min_stops`: Số lượng object dừng tối thiểu để phát hiện jam (mặc định: 8)
- `check_notify_interval`: Khoảng thời gian giữa các thông báo jam (mặc định: 10)

#### Bước 3: Cấu hình RTMP stream output

```bash
curl -X POST http://localhost:8848/v1/core/instance/${INSTANCE_ID}/output/stream \
  -H 'Content-Type: application/json' \
  -d '{
    "enabled": true,
    "uri": "rtmp://localhost:1935/live/jam_test"
  }'
```

**Lưu ý:** 
- URI phải bắt đầu với `rtmp://`, `rtsp://`, hoặc `hls://`
- Đảm bảo RTMP server (ví dụ: MediaMTX) đang chạy và lắng nghe trên URL này
- Instance sẽ tự động restart để áp dụng cấu hình stream output

#### Bước 4: Khởi động instance

```bash
curl -X POST http://localhost:8848/v1/core/instance/${INSTANCE_ID}/start \
  -H 'Content-Type: application/json'
```

#### Bước 5: Kiểm tra kết quả

**Xem RTMP stream:**
```bash
# Sử dụng ffplay
ffplay rtmp://localhost:1935/live/jam_test

# Hoặc với VLC
# File > Open Network Stream > rtmp://localhost:1935/live/jam_test
```

**Kiểm tra output/processing results:**
```bash
curl -X GET http://localhost:8848/v1/core/instance/${INSTANCE_ID}/output | jq '.'
```

**Kiểm tra statistics:**
```bash
curl -X GET http://localhost:8848/v1/core/instance/${INSTANCE_ID}/statistics | jq '.'
```

**Kiểm tra jam zones:**
```bash
curl -X GET http://localhost:8848/v1/core/instance/${INSTANCE_ID}/jams | jq '.'
```

**Kiểm tra trạng thái instance:**
```bash
curl -X GET http://localhost:8848/v1/core/instance/${INSTANCE_ID} | jq '.'
```

## 🔧 Cấu Hình RTMP Server

### MediaMTX Setup

1. **Cài đặt MediaMTX:**
```bash
# Download và cài đặt MediaMTX
wget https://github.com/bluenviron/mediamtx/releases/latest/download/mediamtx_v1.0.0_linux_amd64.tar.gz
tar -xzf mediamtx_v1.0.0_linux_amd64.tar.gz
sudo mv mediamtx /usr/local/bin/
```

2. **Khởi động MediaMTX:**
```bash
# Chạy MediaMTX (mặc định lắng nghe trên port 1935 cho RTMP)
mediamtx
```

3. **Kiểm tra MediaMTX đang chạy:**
```bash
netstat -tlnp | grep 1935
```

### Test RTMP Stream

```bash
# Publish test stream
ffmpeg -re -i test.mp4 -c copy -f flv rtmp://localhost:1935/live/test

# Play stream
ffplay rtmp://localhost:1935/live/test
```

## 📝 API Endpoints Sử Dụng

### Instance Management
- `POST /v1/core/instance` - Tạo instance mới
- `GET /v1/core/instance/{instanceId}` - Lấy thông tin instance
- `POST /v1/core/instance/{instanceId}/start` - Khởi động instance
- `POST /v1/core/instance/{instanceId}/stop` - Dừng instance
- `DELETE /v1/core/instance/{instanceId}` - Xóa instance

### Jam Zones Management
- `POST /v1/core/instance/{instanceId}/jams` - Thêm jam zone(s)
- `GET /v1/core/instance/{instanceId}/jams` - Lấy danh sách jam zones
- `PUT /v1/core/instance/{instanceId}/jams/{jamId}` - Cập nhật jam zone
- `DELETE /v1/core/instance/{instanceId}/jams/{jamId}` - Xóa jam zone
- `DELETE /v1/core/instance/{instanceId}/jams` - Xóa tất cả jam zones

### Stream Output
- `POST /v1/core/instance/{instanceId}/output/stream` - Cấu hình stream output
- `GET /v1/core/instance/{instanceId}/output/stream` - Lấy cấu hình stream output

### Monitoring
- `GET /v1/core/instance/{instanceId}/output` - Lấy output/processing results
- `GET /v1/core/instance/{instanceId}/statistics` - Lấy statistics

## ⚠️ Lưu Ý

1. **Input Source:** Đảm bảo `RTSP_URL` hoặc `FILE_PATH` trong `additionalParams` hợp lệ và có thể truy cập được.

2. **RTMP Server:** Đảm bảo RTMP server (MediaMTX) đang chạy trước khi cấu hình stream output.

3. **Auto Restart:** Instance sẽ tự động restart khi:
   - Thêm/sửa/xóa jam zones
   - Thay đổi cấu hình stream output
   - Thay đổi các tham số trong `additionalParams`

4. **Jam Zone ROI:** 
   - ROI phải có tối thiểu 3 điểm
   - Các điểm phải tạo thành một polygon hợp lệ
   - Tọa độ tính theo pixel của frame

5. **Performance:** 
   - Việc khởi động instance có thể mất vài giây để build pipeline
   - Kiểm tra trạng thái `running` sau khi start để đảm bảo instance đã sẵn sàng

## 🐛 Troubleshooting

### Instance không khởi động được
- Kiểm tra input source (RTSP_URL/FILE_PATH) có hợp lệ không
- Kiểm tra logs của API server
- Kiểm tra solution `ba_jam_default` có tồn tại không: `GET /v1/core/solution`

### RTMP stream không hoạt động
- Kiểm tra RTMP server có đang chạy không: `netstat -tlnp | grep 1935`
- Kiểm tra firewall có chặn port 1935 không
- Test RTMP server với ffmpeg: `ffmpeg -re -i test.mp4 -c copy -f flv rtmp://localhost:1935/live/test`

### Jam zones không được áp dụng
- Kiểm tra jam zones đã được thêm chưa: `GET /v1/core/instance/{instanceId}/jams`
- Kiểm tra instance có đang chạy không: `GET /v1/core/instance/{instanceId}`
- Instance sẽ tự động restart khi thêm jam zones, đợi vài giây rồi kiểm tra lại

## 📚 Tài Liệu Tham Khảo

- [API Documentation](../../../../docs/API_document.md)
- [RTMP/MQTT Integration Guide](../../rtmp_mqtt/README.md)
- [Solution Registry](../../../../src/solutions/solution_registry.cpp)
