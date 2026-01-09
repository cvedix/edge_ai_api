# Scripts Test RTMP Stream Output

## 📋 Tổng Quan

Có 2 scripts để test instance với RTMP stream output:

1. **`test_jam_rtmp.sh`** - Test với BA Jam Detection (phát hiện kẹt xe)
2. **`test_crossline_rtmp.sh`** - Test với BA Crossline Detection (đếm đối tượng qua đường line)

## 🚀 Script 1: test_jam_rtmp.sh

### Mục đích
Test instance với **BA Jam Detection** để phát hiện tình trạng kẹt xe và stream output ra RTMP.

### Cách sử dụng
```bash
# Sử dụng mặc định (ba_jam_default)
./test_jam_rtmp.sh

# Hoặc chỉ định BASE_URL và RTMP_URL
./test_jam_rtmp.sh http://localhost:8080 rtmp://localhost:1935/live/jam_test

# Hoặc chỉ định cả solution ID
./test_jam_rtmp.sh http://localhost:8080 rtmp://localhost:1935/live/jam_test ba_jam_default
```

### Lưu ý
⚠️ **Node type `ba_jam` có thể chưa được build trong SDK**. Nếu gặp lỗi "Unknown node type: ba_jam", hãy:
- Kiểm tra xem SDK có hỗ trợ ba_jam node không
- Hoặc sử dụng script `test_crossline_rtmp.sh` thay thế

### Workflow
1. Tạo instance với solution `ba_jam_default`
2. Thêm jam zones qua API `/v1/core/instance/{id}/jams`
3. Cấu hình RTMP stream output
4. Khởi động instance
5. Kiểm tra kết quả qua RTMP stream

## 🚀 Script 2: test_crossline_rtmp.sh

### Mục đích
Test instance với **BA Crossline Detection** để đếm đối tượng đi qua đường line và stream output ra RTMP.

### Cách sử dụng
```bash
# Sử dụng mặc định
./test_crossline_rtmp.sh

# Hoặc chỉ định BASE_URL và RTMP_URL
./test_crossline_rtmp.sh http://localhost:8080 rtmp://localhost:1935/live/crossline_test
```

### Workflow
1. Tạo instance với solution `ba_crossline_default`
2. Thêm crossing lines qua API `/v1/core/instance/{id}/lines`
3. Cấu hình RTMP stream output
4. Khởi động instance
5. Kiểm tra kết quả qua RTMP stream

### Ưu điểm
✅ **Đã được test và hoạt động tốt** - Node type `ba_crossline` có sẵn trong SDK

## 🔄 So Sánh

| Tính năng | test_jam_rtmp.sh | test_crossline_rtmp.sh |
|-----------|------------------|------------------------|
| **Node type** | `ba_jam` | `ba_crossline` |
| **Mục đích** | Phát hiện kẹt xe | Đếm đối tượng qua line |
| **API quản lý zones** | `/jams` | `/lines` |
| **Trạng thái SDK** | ⚠️ Có thể chưa build | ✅ Đã có sẵn |
| **Khuyến nghị** | Thử nếu SDK hỗ trợ | ✅ Sử dụng nếu jam không hoạt động |

## 📝 Yêu Cầu Tiên Quyết

Cả hai script đều yêu cầu:

1. **API Server đang chạy** trên port 8080 (hoặc port bạn chỉ định)
2. **RTMP Server** (ví dụ: MediaMTX) đang chạy và lắng nghe trên RTMP URL
3. **Input Source** hợp lệ:
   - RTSP stream: `rtsp://host:port/path`
   - Hoặc file video: đường dẫn đến file video

## 🔧 Kiểm Tra Trước Khi Chạy

### 1. Kiểm tra API Server
```bash
curl http://localhost:8080/v1/core/instance/status/summary
```

### 2. Kiểm tra RTMP Server
```bash
netstat -tlnp | grep 1935
```

### 3. Kiểm tra Solutions có sẵn
```bash
curl http://localhost:8080/v1/core/solution | jq '.'
```

## 🐛 Troubleshooting

### Lỗi: "Unknown node type: ba_jam"
**Giải pháp:** Sử dụng script `test_crossline_rtmp.sh` thay thế

### Lỗi: "Cannot connect to API server"
**Giải pháp:** 
- Kiểm tra API server có đang chạy không
- Kiểm tra port (8080 hoặc 8848)
- Kiểm tra firewall

### Lỗi: "RTMP stream không hoạt động"
**Giải pháp:**
- Kiểm tra RTMP server có đang chạy không
- Kiểm tra RTMP URL có đúng không
- Test RTMP server với ffmpeg: `ffmpeg -re -i test.mp4 -c copy -f flv rtmp://localhost:1935/live/test`

## 📚 Tài Liệu Tham Khảo

- [README_JAM_RTMP.md](./README_JAM_RTMP.md) - Hướng dẫn chi tiết về jam detection
- [HUONG_DAN_JAM_RTMP.md](./HUONG_DAN_JAM_RTMP.md) - Hướng dẫn nhanh bằng tiếng Việt
- [BA Crossline README](../../ba_crossline/README.md) - Hướng dẫn về crossline detection
