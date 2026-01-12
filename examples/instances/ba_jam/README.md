# BA Jam Examples

Thư mục này chứa các ví dụ instances cho Behavior Analysis - Traffic Jam Detection.

## Cấu trúc

```
ba_jam/
└── tensorrt/
    ├── example_ba_jam_file_rtmp.json
    ├── example_ba_jam_rtsp_rtmp.json
    └── example_ba_jam_file_mqtt.json
```

## Examples

### 1. `example_ba_jam_file_rtmp.json`
- **Mô tả**: BA Jam với file source và RTMP output
- **Input**: Video file
- **Output**: RTMP stream
- **JamZones**: Được định nghĩa trong additionalParams.JamZones

### 2. `example_ba_jam_rtsp_rtmp.json`
- **Mô tả**: BA Jam với RTSP source và RTMP output
- **Input**: RTSP stream
- **Output**: RTMP stream
- **JamZones**: Được định nghĩa trong additionalParams.JamZones

### 3. `example_ba_jam_file_mqtt.json`
- **Mô tả**: BA Jam với file source, screen output và MQTT events
- **Input**: Video file
- **Output**: Screen display + MQTT events
- **JamZones**: Được định nghĩa trong additionalParams.JamZones

## JamZones Format

JamZones được định nghĩa trong `additionalParams.JamZones` dưới dạng JSON string:

```json
[
  {
    "id": "jam1",
    "name": "Jam Zone 1",
    "roi": [
      {"x": 20, "y": 360},
      {"x": 400, "y": 250},
      {"x": 535, "y": 250},
      {"x": 555, "y": 560},
      {"x": 30, "y": 550}
    ],
    "threshold": 5,
    "duration_seconds": 10
  }
]
```

## Cách sử dụng

1. Điều chỉnh các đường dẫn model và video trong JSON file
2. Định nghĩa JamZones trong additionalParams
3. Tạo instance từ solution:

```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @examples/instances/ba_jam/tensorrt/example_ba_jam_file_rtmp.json
```

## Yêu cầu

- Vehicle detection model (.trt)
- Video file hoặc RTSP stream
- RTMP server (nếu dùng RTMP output)
- MQTT broker (nếu dùng MQTT events)

