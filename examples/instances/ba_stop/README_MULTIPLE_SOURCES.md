# Multiple Video Sources Support for ba_stop

## 📋 Tổng Quan

Tính năng này cho phép một instance ba_stop xử lý nhiều video sources cùng lúc (multi-channel processing), tương tự như sample `ba_stop_sample.cpp`.

## 🎯 Tính Năng

- ✅ Hỗ trợ nhiều video sources (2+ channels)
- ✅ Mỗi channel có thể có stop zones riêng
- ✅ Single detector/tracker xử lý tất cả channels
- ✅ BA stop node nhận regions cho từng channel

## 📐 Format JSON

### File Sources (FILE_PATHS)

#### Format 1: Simple Array (Tự động assign channel)

```json
{
  "additionalParams": {
    "input": {
      "FILE_PATHS": "[\"/path/to/video1.mp4\", \"/path/to/video2.mp4\"]"
    }
  }
}
```

- Channels sẽ được tự động assign: 0, 1, 2, ...
- Resize ratio mặc định: 0.4

#### Format 2: Advanced Array (Custom channel và resize_ratio)

```json
{
  "additionalParams": {
    "input": {
      "FILE_PATHS": "[{\"file_path\":\"/path/to/video1.mp4\",\"channel\":0,\"resize_ratio\":0.6},{\"file_path\":\"/path/to/video2.mp4\",\"channel\":1,\"resize_ratio\":0.6}]"
    }
  }
}
```

- Có thể chỉ định channel và resize_ratio cho từng video
- Channel có thể không liên tục (0, 2, 5, ...)

### RTSP Sources (RTSP_URLS)

#### Format 1: Simple Array (Tự động assign channel)

```json
{
  "additionalParams": {
    "input": {
      "RTSP_URLS": "[\"rtsp://camera1.example.com:8554/stream1\", \"rtsp://camera2.example.com:8554/stream2\"]"
    }
  }
}
```

- Channels sẽ được tự động assign: 0, 1, 2, ...
- Resize ratio mặc định: 0.6
- Decoder mặc định: avdec_h264
- Codec mặc định: h264

#### Format 2: Advanced Array (Custom parameters)

```json
{
  "additionalParams": {
    "input": {
      "RTSP_URLS": "[{\"rtsp_url\":\"rtsp://camera1.example.com:8554/stream1\",\"channel\":0,\"resize_ratio\":0.6,\"gst_decoder_name\":\"avdec_h264\",\"codec_type\":\"h264\"},{\"rtsp_url\":\"rtsp://camera2.example.com:8554/stream2\",\"channel\":1,\"resize_ratio\":0.5,\"codec_type\":\"h265\"}]"
    }
  }
}
```

- Có thể chỉ định:
  - `rtsp_url` hoặc `url`: RTSP stream URL
  - `channel`: Channel number (0, 1, 2, ...)
  - `resize_ratio`: Resize ratio (0.0 - 1.0)
  - `gst_decoder_name`: GStreamer decoder (e.g., "avdec_h264", "avdec_h265")
  - `codec_type`: Codec type ("h264", "h265", "auto")
  - `skip_interval`: Skip frames interval (0 = no skip)

## 🔧 Stop Zones cho Multiple Channels

Khi sử dụng multiple sources, cần định nghĩa stop zones cho từng channel:

```json
{
  "StopZones": "[{\"id\":\"zone1\",\"name\":\"Channel 0 Stop Zone\",\"roi\":[...],\"min_stop_seconds\":3,...},{\"id\":\"zone2\",\"name\":\"Channel 1 Stop Zone\",\"roi\":[...],\"min_stop_seconds\":3,...}]"
}
```

**Lưu ý:** 
- Mỗi stop zone sẽ được map với channel tương ứng dựa trên thứ tự trong array
- Hoặc có thể quản lý qua API `/v1/core/instance/{instanceId}/stops` sau khi tạo instance

## 📝 Example Files

### File Sources:
1. **example_ba_stop_multiple_sources.json** - Simple format với 2 videos
2. **example_ba_stop_multiple_sources_advanced.json** - Advanced format với custom channels

### RTSP Sources:
3. **example_ba_stop_multiple_rtsp.json** - Simple format với 2 RTSP streams

## 🚀 Cách Sử Dụng

### Bước 1: Tạo Instance

```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @example_ba_stop_multiple_sources.json
```

### Bước 2: Thêm Stop Zones (nếu chưa có trong JSON)

```bash
# Stop zone cho channel 0
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/stops \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Channel 0 Stop Zone",
    "roi": [
      {"x": 20, "y": 30},
      {"x": 600, "y": 40},
      {"x": 600, "y": 300},
      {"x": 10, "y": 300}
    ],
    "min_stop_seconds": 3,
    "check_interval_frames": 20,
    "check_min_hit_frames": 50,
    "check_max_distance": 5
  }'

# Stop zone cho channel 1
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/stops \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Channel 1 Stop Zone",
    "roi": [
      {"x": 20, "y": 30},
      {"x": 1000, "y": 40},
      {"x": 1000, "y": 600},
      {"x": 10, "y": 600}
    ],
    "min_stop_seconds": 3,
    "check_interval_frames": 20,
    "check_min_hit_frames": 50,
    "check_max_distance": 5
  }'
```

### Bước 3: Start Instance

```bash
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/start
```

## 🔍 Kiểm Tra

### Kiểm tra stop zones

```bash
curl http://localhost:8080/v1/core/instance/{instanceId}/stops
```

### Kiểm tra statistics

```bash
curl http://localhost:8080/v1/core/instance/{instanceId}/statistics
```

## ⚠️ Lưu Ý

1. **Multiple Sources Format**: 
   - `FILE_PATHS` (array) → Multiple file sources
   - `FILE_PATH` (string) → Single file source (backward compatible)
   - `RTSP_URLS` (array) → Multiple RTSP streams
   - `RTSP_URL` (string) → Single RTSP stream (backward compatible)

2. **Channel Mapping**: 
   - Channels được map với stop zones dựa trên thứ tự
   - Channel 0 → Stop zone đầu tiên
   - Channel 1 → Stop zone thứ hai
   - ...

3. **Performance**: 
   - Multiple sources sẽ tăng tải CPU/GPU
   - Đảm bảo hardware đủ mạnh để xử lý
   - RTSP streams cần bandwidth ổn định

4. **Resize Ratio**: 
   - File sources: Nên sử dụng resize_ratio < 1.0 (ví dụ: 0.4-0.6)
   - RTSP sources: Nên sử dụng resize_ratio 0.5-0.6 để giảm tải network
   - Ví dụ: 0.6 cho 2 channels, 0.5 cho 3+ channels

5. **RTSP Specific**: 
   - Đảm bảo RTSP streams có thể kết nối được
   - Có thể cần set `GST_RTSP_PROTOCOLS=tcp` nếu gặp vấn đề với UDP
   - Mỗi RTSP stream cần decoder phù hợp (h264/h265)

6. **Stop Zones Parameters**:
   - `min_stop_seconds`: Thời gian tối thiểu (giây) để coi là stop event
   - `check_interval_frames`: Số frame giữa các lần kiểm tra
   - `check_min_hit_frames`: Số frame liên tiếp object phải dừng
   - `check_max_distance`: Khoảng cách pixel tối đa để coi là "dừng"

## 📚 Tham Khảo

- Sample code: `sample/ba_stop_sample.cpp`
- Single source example: `example_ba_stop_rtmp.json`
- API documentation: `docs/API_document.md`
- Multiple sources for ba_jam: `../ba_jam/README_MULTIPLE_SOURCES.md`

