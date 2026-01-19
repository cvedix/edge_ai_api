# Multiple Video Sources Support for ba_jam

## 📋 Tổng Quan

Tính năng này cho phép một instance ba_jam xử lý nhiều video sources cùng lúc (multi-channel processing), tương tự như sample `ba_jam_sample.cpp`.

## 🎯 Tính Năng

- ✅ Hỗ trợ nhiều video sources (2+ channels)
- ✅ Mỗi channel có thể có jam zones riêng
- ✅ Single detector/tracker xử lý tất cả channels
- ✅ BA jam node nhận regions cho từng channel

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
      "FILE_PATHS": "[{\"file_path\":\"/path/to/video1.mp4\",\"channel\":0,\"resize_ratio\":0.5},{\"file_path\":\"/path/to/video2.mp4\",\"channel\":1,\"resize_ratio\":0.6}]"
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
      "RTSP_URLS": "[{\"rtsp_url\":\"rtsp://camera1.example.com:8554/stream1\",\"channel\":0,\"resize_ratio\":0.6,\"gst_decoder_name\":\"avdec_h264\",\"codec_type\":\"h264\"},{\"rtsp_url\":\"rtsp://camera2.example.com:8554/stream2\",\"channel\":1,\"resize_ratio\":0.5,\"gst_decoder_name\":\"avdec_h265\",\"codec_type\":\"h265\"}]"
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

## 🔧 Jam Zones cho Multiple Channels

Khi sử dụng multiple sources, cần định nghĩa jam zones cho từng channel:

```json
{
  "JamZones": "[{\"id\":\"zone1\",\"name\":\"Channel 0 Jam Zone\",\"roi\":[...],...},{\"id\":\"zone2\",\"name\":\"Channel 1 Jam Zone\",\"roi\":[...],...}]"
}
```

**Lưu ý:** 
- Mỗi jam zone sẽ được map với channel tương ứng dựa trên thứ tự trong array
- Hoặc có thể quản lý qua API `/v1/core/instance/{instanceId}/jams` sau khi tạo instance

## 📝 Example Files

### File Sources:
1. **example_ba_jam_multiple_sources.json** - Simple format với 2 videos
2. **example_ba_jam_multiple_sources_advanced.json** - Advanced format với custom channels
3. **example_ba_jam_multiple_sources_rtmp.json** - Multiple sources với RTMP output

### RTSP Sources:
4. **example_ba_jam_multiple_rtsp.json** - Simple format với 2 RTSP streams
5. **example_ba_jam_multiple_rtsp_advanced.json** - Advanced format với custom RTSP parameters

## 🚀 Cách Sử Dụng

### Bước 1: Tạo Instance

```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @example_ba_jam_multiple_sources.json
```

### Bước 2: Thêm Jam Zones (nếu chưa có trong JSON)

```bash
# Jam zone cho channel 0
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/jams \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Channel 0 Jam Zone",
    "roi": [
      {"x": 20, "y": 360},
      {"x": 400, "y": 250},
      {"x": 535, "y": 250},
      {"x": 555, "y": 560},
      {"x": 30, "y": 550}
    ],
    "check_interval_frames": 20,
    "check_min_hit_frames": 50,
    "check_max_distance": 8,
    "check_min_stops": 8,
    "check_notify_interval": 10
  }'

# Jam zone cho channel 1
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/jams \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Channel 1 Jam Zone",
    "roi": [
      {"x": 12, "y": 353},
      {"x": 472, "y": 108},
      {"x": 581, "y": 100},
      {"x": 732, "y": 399}
    ],
    "check_interval_frames": 20,
    "check_min_hit_frames": 50,
    "check_max_distance": 8,
    "check_min_stops": 8,
    "check_notify_interval": 10
  }'
```

### Bước 3: Start Instance

```bash
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/start
```

## 🔍 Kiểm Tra

### Kiểm tra jam zones

```bash
curl http://localhost:8080/v1/core/instance/{instanceId}/jams
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
   - Channels được map với jam zones dựa trên thứ tự
   - Channel 0 → Jam zone đầu tiên
   - Channel 1 → Jam zone thứ hai
   - ...

3. **Performance**: 
   - Multiple sources sẽ tăng tải CPU/GPU
   - Đảm bảo hardware đủ mạnh để xử lý
   - RTSP streams cần bandwidth ổn định

4. **Resize Ratio**: 
   - File sources: Nên sử dụng resize_ratio < 1.0 (ví dụ: 0.4-0.5)
   - RTSP sources: Nên sử dụng resize_ratio 0.5-0.6 để giảm tải network
   - Ví dụ: 0.5 cho 2 channels, 0.4 cho 3+ channels

5. **RTSP Specific**: 
   - Đảm bảo RTSP streams có thể kết nối được
   - Có thể cần set `GST_RTSP_PROTOCOLS=tcp` nếu gặp vấn đề với UDP
   - Mỗi RTSP stream cần decoder phù hợp (h264/h265)

## 📚 Tham Khảo

- Sample code: `sample/ba_jam_sample.cpp`
- Single source example: `example_ba_jam_rtmp.json`
- API documentation: `docs/API_document.md`

