# Behavior Analysis Crossline - YOLO Models

Thư mục này chứa các ví dụ sử dụng Behavior Analysis Crossline với các model YOLO.

## Model Types

- **YOLO v3** (`.weights`, `.cfg`) - Model phát hiện đối tượng cho behavior analysis
- Sử dụng YOLO để detect vehicles và track để đếm số lượng qua các đường line

## Các Ví dụ

### Format Tên File
Tất cả file đã được đổi tên theo format thống nhất: `{prefix}_ba_crossline_{input}_{output}[_{special}].json`

Xem chi tiết: [NAMING_CONVENTION.md](./NAMING_CONVENTION.md)

### Example Files (Demo/Showcase)
- `example_ba_crossline_file_mqtt.json` - File input với MQTT output
- `example_ba_crossline_file_rtmp.json` - File input với RTMP output
- `example_ba_crossline_file_rtmp_mqtt.json` - File input với RTMP + MQTT output
- `example_ba_crossline_rtsp_mqtt.json` - RTSP input với MQTT output
- `example_ba_crossline_rtsp_rtmp.json` - RTSP input với RTMP output
- `example_ba_crossline_rtsp_rtmp_mqtt.json` - RTSP input với RTMP + MQTT output
- `example_ba_crossline_file_mqtt_with_crossing_lines.json` - File input, MQTT output với crossing lines config
- `example_ba_crossline_rtsp_rtmp_with_crossing_lines.json` - RTSP input, RTMP output với crossing lines config
- `example_ba_crossline_file_rtmp_with_crossing_lines.json` - File input, RTMP output với crossing lines config
- `example_ba_crossline_file_screen_without_line_ids.json` - File input, Screen output không có line IDs
- `example_ba_crossline_mqtt_event_with_counts.json` - Example về MQTT event format với counts
- `example_ba_crossline_report_body.json` - Example về report body format

### Test Files (Với test parameters)
- `test_ba_crossline_file_mqtt.json` - File input với MQTT output (có persistent, detectionSensitivity)
- `test_ba_crossline_file_rtmp.json` - File input với RTMP output
- `test_ba_crossline_file_screen_mqtt.json` - File input với Screen + MQTT output
- `test_ba_crossline_rtsp_rtmp.json` - RTSP input với RTMP output
- `test_ba_crossline_rtsp_rtmp_mqtt.json` - RTSP input với RTMP + MQTT output
- `test_ba_crossline_rtsp_screen_mqtt.json` - RTSP input với Screen + MQTT output

### Flexible Files (Cấu hình linh hoạt)
- `flexible_ba_crossline_file_screen.json` - File input, Screen output
- `flexible_ba_crossline_file_screen_mqtt.json` - File input, Screen + MQTT output
- `flexible_ba_crossline_rtsp_rtmp.json` - RTSP input, RTMP output
- `flexible_ba_crossline_rtmp_rtmp_mqtt.json` - RTMP input, RTMP + MQTT output

## Tham số Model

- `WEIGHTS_PATH`: Đường dẫn đến YOLO weights file (`.weights`)
- `CONFIG_PATH`: Đường dẫn đến YOLO config file (`.cfg`)
- `LABELS_PATH`: Đường dẫn đến file labels (`.txt`)
- `RESIZE_RATIO`: Tỷ lệ resize frame (0.0 - 1.0)

## Crossing Lines Configuration

Sử dụng tham số `CrossingLines` để định nghĩa các đường line cần đếm:
```json
"CrossingLines": "[{\"id\":\"line1\",\"name\":\"Main Road\",\"coordinates\":[{\"x\":0,\"y\":250},{\"x\":700,\"y\":220}],\"direction\":\"Both\",\"classes\":[\"Vehicle\"],\"color\":[255,0,0,255]}]"
```

