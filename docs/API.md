# API Reference - Edge AI API

Tài liệu tham khảo đầy đủ về API endpoints, instance management, face recognition và node types.

## Mục Lục

1. [Core APIs](#core-apis)
2. [Instance Management](#instance-management)
3. [Solution APIs](#solution-apis)
4. [Face Recognition APIs](#face-recognition-apis)
5. [Node & Model APIs](#node--model-apis)
6. [Config & System APIs](#config--system-apis)
7. [Node Types Reference](#node-types-reference)

---

## Core APIs

| Endpoint | Method | Mô Tả |
|----------|--------|-------|
| `/v1/core/health` | GET | Health check |
| `/v1/core/version` | GET | Version info |
| `/v1/core/watchdog` | GET | Watchdog status |
| `/v1/core/endpoints` | GET | List all endpoints |

```bash
curl http://localhost:8080/v1/core/health
```

---

## Instance Management

### Instance Endpoints

| Endpoint | Method | Mô Tả |
|----------|--------|-------|
| `/v1/core/instance` | POST | Tạo instance |
| `/v1/core/instances` | GET | List instances |
| `/v1/core/instances/{id}` | GET | Chi tiết instance |
| `/v1/core/instances/{id}` | PUT | Update instance |
| `/v1/core/instances/{id}` | DELETE | Xóa instance |
| `/v1/core/instances/{id}/start` | POST | Start instance |
| `/v1/core/instances/{id}/stop` | POST | Stop instance |
| `/v1/core/instances/{id}/restart` | POST | Restart instance |
| `/v1/core/instance/{id}/config` | GET/POST | Instance config |
| `/v1/core/instance/{id}/input` | POST | Change input |
| `/v1/core/instances/{id}/output` | GET | Get output |
| `/v1/core/instances/{id}/frame` | GET | Get latest frame |
| `/v1/core/instance/{id}/statistics` | GET | Get statistics |
| `/v1/core/instance/{id}/output/stream` | GET/POST | Stream/Record output |
| `/v1/core/instances/batch/start` | POST | Batch start |
| `/v1/core/instances/batch/stop` | POST | Batch stop |

### Pipeline là gì?

Pipeline là chuỗi nodes xử lý video/ảnh:

```
[Source] → [Detector] → [Processor] → [Broker] → [Destination]
```

### Tạo Instance

```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d '{
    "name": "camera_1_face_detection",
    "solution": "face_detection",
    "persistent": true,
    "autoStart": true,
    "detectionSensitivity": "Medium",
    "additionalParams": {
      "RTSP_URL": "rtsp://localhost:554/stream",
      "MODEL_PATH": "/opt/models/face.trt",
      "RTMP_URL": "rtmp://localhost:1935/live/stream1"
    }
  }'
```

### Request Fields

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `name` | string | Yes | Tên instance (pattern: `^[A-Za-z0-9 -_]+$`) |
| `solution` | string | No | Solution ID |
| `persistent` | bool | No | Lưu để load lại khi restart |
| `autoStart` | bool | No | Tự động start |
| `detectionSensitivity` | string | No | Low/Medium/High/Normal/Slow |
| `detectorMode` | string | No | SmartDetection/FullRegionInference/MosaicInference |
| `frameRateLimit` | int | No | Giới hạn FPS |
| `additionalParams` | object | No | Parameters cho nodes |

### Update Instance

```bash
curl -X PUT http://localhost:8080/v1/core/instances/{instanceId} \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Updated Name",
    "autoStart": true,
    "frameRateLimit": 20,
    "detectionSensitivity": "High"
  }'
```

### Get Frame & Statistics

```bash
# Get latest frame (base64 JPEG)
curl http://localhost:8080/v1/core/instances/{id}/frame

# Get statistics
curl http://localhost:8080/v1/core/instance/{id}/statistics
```

```json
{
  "frames_processed": 1250,
  "source_framerate": 30.0,
  "current_framerate": 25.5,
  "latency": 200.0,
  "resolution": "1280x720"
}
```

### Stream/Record Output

```bash
# Enable RTMP streaming
curl -X POST http://localhost:8080/v1/core/instance/{id}/output/stream \
  -H "Content-Type: application/json" \
  -d '{"enabled": true, "uri": "rtmp://localhost:1935/live/stream"}'

# Enable recording
curl -X POST http://localhost:8080/v1/core/instance/{id}/output/stream \
  -H "Content-Type: application/json" \
  -d '{"enabled": true, "path": "/mnt/data"}'

# Disable
curl -X POST http://localhost:8080/v1/core/instance/{id}/output/stream \
  -H "Content-Type: application/json" \
  -d '{"enabled": false}'
```

### Flexible Input Source

Hệ thống auto-detect input type từ `FILE_PATH`:

| Input | Example |
|-------|---------|
| Local File | `FILE_PATH="/path/to/video.mp4"` |
| RTSP | `FILE_PATH="rtsp://..."` hoặc `RTSP_SRC_URL` |
| RTMP | `FILE_PATH="rtmp://..."` hoặc `RTMP_SRC_URL` |
| HLS | `FILE_PATH="http://.../playlist.m3u8"` |

---

## Solution APIs

| Endpoint | Method | Mô Tả |
|----------|--------|-------|
| `/v1/core/solutions` | GET | List solutions |
| `/v1/core/solutions` | POST | Create solution |
| `/v1/core/solutions/{id}` | GET/PUT/DELETE | CRUD solution |
| `/v1/core/solutions/{id}/parameters` | GET | Get parameters |

### Tạo Solution

```bash
curl -X POST http://localhost:8080/v1/core/solutions \
  -H "Content-Type: application/json" \
  -d '{
    "solutionId": "face_detection_mqtt",
    "solutionName": "Face Detection Pipeline",
    "pipeline": [
      {"nodeType": "rtsp_src", "nodeName": "rtsp_src_0", "parameters": {"rtsp_url": "${RTSP_URL}"}},
      {"nodeType": "yunet_face_detector", "nodeName": "face_detector_0"},
      {"nodeType": "json_mqtt_broker", "nodeName": "mqtt_broker_0", "parameters": {"mqtt_broker": "${MQTT_BROKER}"}}
    ]
  }'
```

---

## Face Recognition APIs

### Endpoints

| Endpoint | Method | Mô Tả |
|----------|--------|-------|
| `/v1/recognition/faces` | POST | Register face |
| `/v1/recognition/faces` | GET | List faces |
| `/v1/recognition/recognize` | POST | Recognize face |
| `/v1/recognition/face-database/connection` | POST | Configure database connection (MySQL/PostgreSQL) |
| `/v1/recognition/face-database/connection` | GET | Get database connection configuration |

### Database Location

Face database được lưu theo thứ tự:
1. **Database connection** (nếu đã cấu hình) - MySQL/PostgreSQL
2. `FACE_DATABASE_PATH` env var
3. `/opt/edge_ai_api/data/face_database.txt`
4. `~/.local/share/edge_ai_api/face_database.txt`
5. `./face_database.txt`

> 📖 **Xem thêm:** [Face Database Connection Guide](./FACE_DATABASE_CONNECTION.md) - Hướng dẫn chi tiết về cấu hình database connection

### Configure Database Connection

Cấu hình kết nối MySQL/PostgreSQL để lưu trữ face data:

```bash
curl -X POST http://localhost:8080/v1/recognition/face-database/connection \
  -H "Content-Type: application/json" \
  -d '{
    "type": "mysql",
    "host": "localhost",
    "port": 3306,
    "database": "face_recognition",
    "username": "face_user",
    "password": "your_password",
    "charset": "utf8mb4"
  }'
```

**Tắt database connection (dùng file mặc định):**
```bash
curl -X POST http://localhost:8080/v1/recognition/face-database/connection \
  -H "Content-Type: application/json" \
  -d '{"enabled": false}'
```

**Lấy cấu hình hiện tại:**
```bash
curl http://localhost:8080/v1/recognition/face-database/connection
```

### Register Face

```bash
curl -X POST "http://localhost:8080/v1/recognition/faces?subject=john_doe&det_prob_threshold=0.5" \
  -F "file=@/path/to/image.jpg"
```

Response:
```json
{"image_id": "uuid", "subject": "john_doe"}
```

### Recognize Face

```bash
curl -X POST 'http://localhost:8080/v1/recognition/recognize?det_prob_threshold=0.5&prediction_count=3' \
  -F 'file=@image.jpg;type=image/jpeg'
```

Parameters:
| Parameter | Default | Description |
|-----------|---------|-------------|
| `det_prob_threshold` | 0.5 | Detection threshold |
| `limit` | 0 | Max faces (0 = unlimited) |
| `prediction_count` | 1 | Top N subjects |

Response:
```json
{
  "result": [{
    "box": {"probability": 1.0, "x_min": 548, "y_min": 295, "x_max": 1420, "y_max": 1368},
    "landmarks": [[814, 713], [1104, 829], [832, 937], [704, 1030], [1017, 1133]],
    "subjects": [{"similarity": 0.97858, "subject": "john_doe"}],
    "execution_time": {"detector": 117.0, "calculator": 45.0}
  }]
}
```

### Code Examples

**Python:**
```python
import requests

# Register
with open("face.jpg", "rb") as f:
    r = requests.post("http://localhost:8080/v1/recognition/faces",
                      params={"subject": "john"}, files={"file": f})

# Recognize
with open("test.jpg", "rb") as f:
    r = requests.post("http://localhost:8080/v1/recognition/recognize",
                      params={"det_prob_threshold": 0.5}, files={"file": f})
```

**JavaScript:**
```javascript
const formData = new FormData();
formData.append('file', fileInput.files[0]);
fetch('http://localhost:8080/v1/recognition/recognize?det_prob_threshold=0.5', {
  method: 'POST', body: formData
}).then(r => r.json()).then(console.log);
```

---

## Node & Model APIs

### Node APIs

| Endpoint | Method | Mô Tả |
|----------|--------|-------|
| `/v1/core/nodes` | GET | List nodes |
| `/v1/core/nodes/templates` | GET | Node templates |
| `/v1/core/nodes/preconfigured` | GET | Pre-configured nodes |
| `/v1/core/nodes/build-solution` | POST | Build solution from nodes |

### Model APIs

| Endpoint | Method | Mô Tả |
|----------|--------|-------|
| `/v1/core/models/upload` | POST | Upload model |
| `/v1/core/models/list` | GET | List models |
| `/v1/core/models/{name}` | DELETE | Delete model |

---

## Config & System APIs

### Config APIs

| Endpoint | Method | Mô Tả |
|----------|--------|-------|
| `/v1/core/config` | GET | Get config |
| `/v1/core/config` | POST | Merge config |
| `/v1/core/config` | PUT | Replace config |
| `/v1/core/config` | PATCH | Update section |
| `/v1/core/config/reset` | POST | Reset to default |

```bash
# Get specific path
curl 'http://localhost:8080/v1/core/config?path=system/max_running_instances'

# Update
curl -X POST http://localhost:8080/v1/core/config \
  -H "Content-Type: application/json" \
  -d '{"system": {"max_running_instances": 10}}'
```

### System APIs

| Endpoint | Method | Mô Tả |
|----------|--------|-------|
| `/v1/core/system/info` | GET | Hardware info |
| `/v1/core/system/status` | GET | System status |
| `/v1/core/logs` | GET | List logs |
| `/v1/core/logs/{category}` | GET | Get logs by category |

```bash
# System info
curl http://localhost:8080/v1/core/system/info

# Logs
curl "http://localhost:8080/v1/core/logs/api?level=ERROR&tail=100"
```

---

## Node Types Reference

### Source Nodes (6)

| Node | Mô Tả | Parameters |
|------|-------|------------|
| `rtsp_src` | RTSP stream | `rtsp_url`, `channel`, `fps` |
| `file_src` | Video file | `file_path`, `loop` |
| `app_src` | Application | `width`, `height`, `format` |
| `image_src` | Image file | `image_path` |
| `rtmp_src` | RTMP stream | `rtmp_url` |
| `udp_src` | UDP stream | `udp_port` |

### Inference Nodes (23)

**TensorRT:**
- `trt_yolov8_detector` - YOLOv8 detection
- `trt_yolov8_seg_detector` - Instance segmentation
- `trt_yolov8_pose_detector` - Pose estimation
- `trt_yolov8_classifier` - Classification
- `trt_vehicle_detector`, `trt_vehicle_plate_detector`
- `trt_vehicle_color_classifier`, `trt_vehicle_type_classifier`

**RKNN:**
- `rknn_yolov8_detector`, `rknn_face_detector`

**Others:**
- `yunet_face_detector`, `sface_feature_encoder`
- `yolo_detector`, `ppocr_text_detector`
- `mask_rcnn_detector`, `openpose_detector`

### Processor Nodes

| Node | Mô Tả |
|------|-------|
| `sort_track` | Object tracking |
| `ba_crossline` | Crossline detection |
| `ba_crossline_osd` | Crossline overlay |
| `face_osd_v2` | Face overlay |

### Broker Nodes (12)

| Node | Mô Tả | Parameters |
|------|-------|------------|
| `json_console_broker` | Console output | - |
| `json_mqtt_broker` | MQTT publish | `mqtt_broker`, `mqtt_topic` |
| `json_kafka_broker` | Kafka publish | `kafka_broker`, `kafka_topic` |
| `xml_file_broker` | XML file | `output_path` |
| `xml_socket_broker` | XML socket | `socket_host`, `socket_port` |

### Destination Nodes (3)

| Node | Mô Tả | Parameters |
|------|-------|------------|
| `file_des` | Save to file | `save_dir`, `name_prefix` |
| `rtmp_des` | RTMP stream | `rtmp_url` |
| `screen_des` | Display | - |

---

## Error Codes

| Code | Mô Tả |
|------|-------|
| 400 | Bad Request - Invalid input |
| 404 | Not Found - Resource not found |
| 429 | Too Many Requests - Exceeded limit |
| 500 | Internal Server Error |

---

## Troubleshooting

### Instance không start
- Kiểm tra solution: `GET /v1/core/solutions/{id}`
- Kiểm tra logs: `GET /v1/core/logs/instance?tail=100`
- Verify parameters trong `additionalParams`

### Face Recognition trả về `{"result": []}`
- Giảm `det_prob_threshold` (0.2-0.3)
- Kiểm tra image format
- Kiểm tra log: `sudo journalctl -u edge-ai-api -f`

### Update không có hiệu lực
- Stop → Update → Start instance

---

## Swagger UI

- **All versions**: `http://localhost:8080/swagger`
- **API v1**: `http://localhost:8080/v1/swagger`
- **OpenAPI spec**: `http://localhost:8080/openapi.yaml`

---

## Related Documentation

- [DEVELOPMENT.md](DEVELOPMENT.md) - Development guide
- [GETTING_STARTED.md](GETTING_STARTED.md) - Quick start
- [ARCHITECTURE.md](ARCHITECTURE.md) - System architecture
