# RTSP Decoder Troubleshooting Guide

## Vấn đề: GStreamer CRITICAL errors với RTSP stream

### Lỗi gặp phải:
```
GStreamer-CRITICAL **: gst_caps_get_structure: assertion 'GST_IS_CAPS (caps)' failed
GStreamer-CRITICAL **: gst_sample_get_caps: assertion 'GST_IS_SAMPLE (sample)' failed
retrieveVideoFrame GStreamer: gst_sample_get_caps() returns NULL
```

### Nguyên nhân:
- Decoder không tương thích với stream format
- Caps negotiation giữa decoder và appsink thất bại
- SDK không lấy được sample từ appsink

### Giải pháp đã thử:
1. ✅ Đổi từ `avdec_h264` → `openh264dec` (vẫn lỗi)
2. ⏳ Cần thử các decoder khác

### Các decoder có thể thử:

#### 1. Kiểm tra decoder có sẵn:
```bash
gst-inspect-1.0 | grep -E "h264.*dec|dec.*h264"
```

#### 2. Test decoder với GStreamer:
```bash
# Test openh264dec
gst-launch-1.0 rtspsrc location=rtsp://localhost:8554/live/camera_demo_sang_vehicle protocols=tcp latency=0 ! application/x-rtp,media=video ! rtph264depay ! h264parse ! openh264dec ! videoconvert ! fakesink

# Test avdec_h264
gst-launch-1.0 rtspsrc location=rtsp://localhost:8554/live/camera_demo_sang_vehicle protocols=tcp latency=0 ! application/x-rtp,media=video ! rtph264depay ! h264parse ! avdec_h264 ! videoconvert ! fakesink

# Test với decodebin (auto-detect)
gst-launch-1.0 rtspsrc location=rtsp://localhost:8554/live/camera_demo_sang_vehicle protocols=tcp latency=0 ! application/x-rtp,media=video ! rtph264depay ! h264parse ! decodebin ! videoconvert ! fakesink
```

#### 3. Bật GStreamer debug để xem chi tiết:
```bash
export GST_DEBUG=rtspsrc:4,openh264dec:4,appsink:4
./bin/edge_ai_api
```

### Decoder có sẵn trên hệ thống:
- `avdec_h264` (libav H.264 decoder) - ❌ Không hoạt động
- `openh264dec` (OpenH264 decoder) - ❌ Không hoạt động  
- `vulkanh264dec` (Vulkan H.264 decoder) - ⏳ Chưa thử

### Cập nhật config để thử decoder khác:
Trong `example_ba_crossline_in_rtsp_out_rtmp.json`, thay đổi:
```json
"GST_DECODER_NAME": "vulkanh264dec"
```

### Lưu ý:
- SDK CVEDIX hardcode pipeline, không thể thêm caps filter trực tiếp
- Vấn đề có thể nằm ở cách SDK lấy sample từ appsink
- Cần kiểm tra với CVEDIX SDK team về vấn đề này

