# BA Stop Examples

Thư mục này chứa các ví dụ instances cho Behavior Analysis - Stop Detection.

## Cấu trúc

```
ba_stop/
└── tensorrt/
    ├── example_ba_stop_file_rtmp.json
    ├── example_ba_stop_rtsp_rtmp.json
    └── example_ba_stop_file_mqtt.json
```

## Examples

### 1. `example_ba_stop_file_rtmp.json`
- **Mô tả**: BA Stop với file source và RTMP output
- **Input**: Video file
- **Output**: RTMP stream
- **StopZones**: Được định nghĩa trong additionalParams.StopZones

### 2. `example_ba_stop_rtsp_rtmp.json`
- **Mô tả**: BA Stop với RTSP source và RTMP output
- **Input**: RTSP stream
- **Output**: RTMP stream
- **StopZones**: Được định nghĩa trong additionalParams.StopZones

### 3. `example_ba_stop_file_mqtt.json`
- **Mô tả**: BA Stop với file source, screen output và MQTT events
- **Input**: Video file
- **Output**: Screen display + MQTT events
- **StopZones**: Được định nghĩa trong additionalParams.StopZones

## StopZones Format

StopZones được định nghĩa trong `additionalParams.StopZones` dưới dạng JSON string:

```json
[
  {
    "id": "zone1",
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
  }
]
```

## Cách sử dụng

1. Điều chỉnh các đường dẫn model và video trong JSON file
2. Định nghĩa StopZones trong additionalParams
3. Tạo instance từ solution:

```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @examples/instances/ba_stop/tensorrt/example_ba_stop_file_rtmp.json
```

## Yêu cầu

- Vehicle detection model (.trt)
- Video file hoặc RTSP stream
- RTMP server (nếu dùng RTMP output)
- MQTT broker (nếu dùng MQTT events)

