# BA Area Enter/Exit API Examples

## 📚 API Endpoints

BA Area Enter/Exit solution được quản lý qua **SecuRT API**. Tất cả các endpoints đều có prefix: `/v1/securt/instance`

### Base URL
```
http://localhost:8080/v1/securt/instance
```

**Lưu ý:** Solution `ba_area_enter_exit` tương thích với SecuRT API và có thể được quản lý qua các endpoint này.

---

## 1. POST - Tạo Instance Mới

Tạo một BA area enter/exit instance mới với ID tự động qua SecuRT API.

### Request

```bash
curl -X POST http://localhost:8080/v1/securt/instance \
  -H "Content-Type: application/json" \
  -d '{
    "name": "BA Area Test Instance",
    "group": "demo",
    "autoStart": false,
    "additionalParams": {
      "input": {
        "FILE_PATH": "/opt/edge_ai_api/videos/vehicle_count.mp4",
        "WEIGHTS_PATH": "/opt/edge_ai_api/models/det_cls/yolov3-tiny-2022-0721_best.weights",
        "CONFIG_PATH": "/opt/edge_ai_api/models/det_cls/yolov3-tiny-2022-0721.cfg",
        "LABELS_PATH": "/opt/edge_ai_api/models/det_cls/yolov3_tiny_5classes.txt",
        "RESIZE_RATIO": "0.6"
      },
      "output": {
        "ENABLE_SCREEN_DES": "false",
        "RTMP_DES_URL": "rtmp://127.0.0.1/live/9000"
      },
      "Areas": "[{\"x\":50,\"y\":150,\"width\":200,\"height\":200},{\"x\":350,\"y\":160,\"width\":200,\"height\":200}]",
      "AreaConfigs": "[{\"alertOnEnter\":true,\"alertOnExit\":true,\"name\":\"Entrance\",\"color\":[0,220,0]},{\"alertOnEnter\":true,\"alertOnExit\":true,\"name\":\"Restricted\",\"color\":[0,0,220]}]"
    }
  }'
```

### Response

```json
{
  "instanceId": "550e8400-e29b-41d4-a716-446655440000",
  "name": "BA Area Test Instance",
  "group": "demo",
  "solution": "ba_area_enter_exit",
  "running": false,
  "loaded": true,
  "autoStart": false,
  "autoRestart": false,
  "persistent": false
}
```

**Lưu ý:** Bạn cần chỉ định `solution: "ba_area_enter_exit"` trong request body hoặc sử dụng Core API với solution này.

---

## 2. PUT - Tạo Instance với ID Cụ Thể

Tạo một BA area instance với instance ID được chỉ định qua SecuRT API.

### Request

```bash
curl -X PUT http://localhost:8080/v1/securt/instance/test-ba-area-001 \
  -H "Content-Type: application/json" \
  -d '{
    "name": "BA Area Instance with ID",
    "group": "demo",
    "autoStart": false,
    "additionalParams": {
      "input": {
        "FILE_PATH": "/opt/edge_ai_api/videos/vehicle_count.mp4",
        "WEIGHTS_PATH": "/opt/edge_ai_api/models/det_cls/yolov3-tiny-2022-0721_best.weights",
        "CONFIG_PATH": "/opt/edge_ai_api/models/det_cls/yolov3-tiny-2022-0721.cfg",
        "LABELS_PATH": "/opt/edge_ai_api/models/det_cls/yolov3_tiny_5classes.txt",
        "RESIZE_RATIO": "0.6"
      },
      "output": {
        "ENABLE_SCREEN_DES": "false"
      },
      "Areas": "[{\"x\":50,\"y\":150,\"width\":200,\"height\":200}]",
      "AreaConfigs": "[{\"alertOnEnter\":true,\"alertOnExit\":true,\"name\":\"Entrance\",\"color\":[0,220,0]}]"
    }
  }'
```

### Response

```json
{
  "instanceId": "test-ba-area-001",
  "name": "BA Area Instance with ID",
  "group": "demo",
  "solution": "ba_area_enter_exit",
  "running": false,
  "loaded": true
}
```

---

## 3. GET - Lấy Thông Tin Instance

Lấy thông tin chi tiết của một instance.

### Request

```bash
# Lấy thông tin qua Core API (khuyến nghị)
curl http://localhost:8080/v1/core/instance/{instanceId}

# Hoặc qua SecuRT API nếu instance đã được đăng ký trong SecuRT
curl http://localhost:8080/v1/securt/instance/{instanceId}
```

### Response

```json
{
  "instanceId": "550e8400-e29b-41d4-a716-446655440000",
  "name": "BA Area Test Instance",
  "group": "demo",
  "solution": "ba_area_enter_exit",
  "running": false,
  "loaded": true,
  "autoStart": false,
  "autoRestart": false,
  "persistent": false,
  "frameRateLimit": 0,
  "metadataMode": false,
  "statisticsMode": false,
  "diagnosticsMode": false,
  "debugMode": false,
  "detectorMode": "SmartDetection",
  "detectionSensitivity": "Low",
  "movementSensitivity": "Low",
  "sensorModality": "RGB"
}
```

### Error Response (404)

```json
{
  "error": "Not Found",
  "message": "Instance not found: {instanceId}"
}
```

---

## 4. GET - Lấy Statistics

Lấy thống kê của instance (fps, latency, frames processed, etc.).

### Request

```bash
curl http://localhost:8080/v1/securt/instance/{instanceId}/stats
```

### Response (Before Start)

```json
{
  "instanceId": "550e8400-e29b-41d4-a716-446655440000",
  "current_framerate": 0,
  "latency": 0,
  "frames_processed": 0,
  "start_time": 0,
  "is_running": false
}
```

### Response (After Start)

```json
{
  "instanceId": "550e8400-e29b-41d4-a716-446655440000",
  "current_framerate": 15.5,
  "latency": 65.2,
  "frames_processed": 78,
  "start_time": 1234567890.123,
  "is_running": true
}
```

---

## 5. DELETE - Xóa Instance

Xóa một BA area instance.

### Request

```bash
curl -X DELETE http://localhost:8080/v1/securt/instance/{instanceId}
```

### Response

```json
{
  "instanceId": "550e8400-e29b-41d4-a716-446655440000",
  "message": "Instance deleted successfully"
}
```

### Error Response (404)

```json
{
  "error": "Not Found",
  "message": "Instance not found: {instanceId}"
}
```

---

## 6. Ví Dụ với Multiple Areas

Tạo instance với nhiều areas (3 areas).

### Request

```bash
curl -X POST http://localhost:8080/v1/securt/instance \
  -H "Content-Type: application/json" \
  -d '{
    "name": "BA Area Multiple Areas",
    "group": "demo",
    "autoStart": false,
    "additionalParams": {
      "input": {
        "FILE_PATH": "/opt/edge_ai_api/videos/vehicle_count.mp4",
        "WEIGHTS_PATH": "/opt/edge_ai_api/models/det_cls/yolov3-tiny-2022-0721_best.weights",
        "CONFIG_PATH": "/opt/edge_ai_api/models/det_cls/yolov3-tiny-2022-0721.cfg",
        "LABELS_PATH": "/opt/edge_ai_api/models/det_cls/yolov3_tiny_5classes.txt",
        "RESIZE_RATIO": "0.6"
      },
      "output": {
        "ENABLE_SCREEN_DES": "false"
      },
      "Areas": "[{\"x\":50,\"y\":150,\"width\":200,\"height\":200},{\"x\":350,\"y\":160,\"width\":200,\"height\":200},{\"x\":650,\"y\":170,\"width\":200,\"height\":200}]",
      "AreaConfigs": "[{\"alertOnEnter\":true,\"alertOnExit\":false,\"name\":\"Entrance\",\"color\":[0,220,0]},{\"alertOnEnter\":true,\"alertOnExit\":true,\"name\":\"Restricted\",\"color\":[0,0,220]},{\"alertOnEnter\":false,\"alertOnExit\":true,\"name\":\"Exit\",\"color\":[220,0,0]}]"
    }
  }'
```

---

## 7. Ví Dụ với RTSP Source

Tạo instance với RTSP source thay vì file.

### Request

```bash
curl -X POST http://localhost:8080/v1/securt/instance \
  -H "Content-Type: application/json" \
  -d '{
    "name": "BA Area RTSP Source",
    "group": "demo",
    "autoStart": false,
    "additionalParams": {
      "input": {
        "RTSP_URL": "rtsp://192.168.1.100:554/stream1",
        "WEIGHTS_PATH": "/opt/edge_ai_api/models/det_cls/yolov3-tiny-2022-0721_best.weights",
        "CONFIG_PATH": "/opt/edge_ai_api/models/det_cls/yolov3-tiny-2022-0721.cfg",
        "LABELS_PATH": "/opt/edge_ai_api/models/det_cls/yolov3_tiny_5classes.txt",
        "RESIZE_RATIO": "0.6"
      },
      "output": {
        "ENABLE_SCREEN_DES": "false",
        "RTMP_DES_URL": "rtmp://127.0.0.1/live/9000"
      },
      "Areas": "[{\"x\":50,\"y\":150,\"width\":200,\"height\":200}]",
      "AreaConfigs": "[{\"alertOnEnter\":true,\"alertOnExit\":true,\"name\":\"Entrance\",\"color\":[0,220,0]}]"
    }
  }'
```

---

## 8. Tạo Object Enter/Exit Area

Tạo area để phát hiện đối tượng vào/ra vùng.

### Request

```bash
curl -X POST http://localhost:8080/v1/securt/instance/{instanceId}/area/objectEnterExit \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Entrance Area",
    "coordinates": [
      {"x": 50, "y": 150},
      {"x": 250, "y": 150},
      {"x": 250, "y": 350},
      {"x": 50, "y": 350}
    ],
    "classes": ["Person", "Vehicle"],
    "color": [0, 220, 0, 255],
    "alertOnEnter": true,
    "alertOnExit": true
  }'
```

### Response

```json
{
  "areaId": "550e8400-e29b-41d4-a716-446655440000"
}
```

### Tạo Area với ID Cụ Thể

```bash
curl -X PUT http://localhost:8080/v1/securt/instance/{instanceId}/area/objectEnterExit/area-001 \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Restricted Zone",
    "coordinates": [
      {"x": 350, "y": 160},
      {"x": 550, "y": 160},
      {"x": 550, "y": 360},
      {"x": 350, "y": 360}
    ],
    "classes": ["Person"],
    "color": [0, 0, 220, 255],
    "alertOnEnter": true,
    "alertOnExit": false
  }'
```

### Lấy Tất Cả Areas

```bash
curl http://localhost:8080/v1/securt/instance/{instanceId}/areas
```

**Response:**
```json
{
  "objectEnterExit": [
    {
      "id": "550e8400-e29b-41d4-a716-446655440000",
      "name": "Entrance Area",
      "coordinates": [
        {"x": 50, "y": 150},
        {"x": 250, "y": 150},
        {"x": 250, "y": 350},
        {"x": 50, "y": 350}
      ],
      "classes": ["Person", "Vehicle"],
      "color": [0, 220, 0, 255],
      "alertOnEnter": true,
      "alertOnExit": true
    }
  ]
}
```

### Xóa Area

```bash
curl -X DELETE http://localhost:8080/v1/securt/instance/{instanceId}/area/{areaId}
```

---

## 9. Workflow Hoàn Chỉnh

Ví dụ workflow từ tạo instance đến xử lý video:

```bash
# 1. Tạo instance qua Core API với solution ba_area_enter_exit
INSTANCE_ID=$(curl -s -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d '{
    "name": "BA Area Workflow Test",
    "group": "demo",
    "autoStart": false,
    "additionalParams": {
      "input": {
        "FILE_PATH": "/opt/edge_ai_api/videos/vehicle_count.mp4",
        "WEIGHTS_PATH": "/opt/edge_ai_api/models/det_cls/yolov3-tiny-2022-0721_best.weights",
        "CONFIG_PATH": "/opt/edge_ai_api/models/det_cls/yolov3-tiny-2022-0721.cfg",
        "LABELS_PATH": "/opt/edge_ai_api/models/det_cls/yolov3_tiny_5classes.txt",
        "RESIZE_RATIO": "0.6"
      },
      "output": {
        "ENABLE_SCREEN_DES": "false"
      },
      "Areas": "[{\"x\":50,\"y\":150,\"width\":200,\"height\":200}]",
      "AreaConfigs": "[{\"alertOnEnter\":true,\"alertOnExit\":true,\"name\":\"Entrance\",\"color\":[0,220,0]}]"
    }
  }' | jq -r '.instanceId')

echo "Created instance: ${INSTANCE_ID}"

# 2. Kiểm tra instance info
curl http://localhost:8080/v1/core/instance/${INSTANCE_ID} | jq .

# 3. Start instance
curl -X POST http://localhost:8080/v1/core/instance/${INSTANCE_ID}/start

# 4. Đợi vài giây
sleep 5

# 5. Kiểm tra statistics
curl http://localhost:8080/v1/securt/instance/${INSTANCE_ID}/stats | jq .

# 6. Stop instance
curl -X POST http://localhost:8080/v1/core/instance/${INSTANCE_ID}/stop

# 7. Xóa instance
curl -X DELETE http://localhost:8080/v1/securt/instance/${INSTANCE_ID}
```

---

## 10. Error Handling

### Validation Error (400)

```json
{
  "error": "Invalid request",
  "message": "Missing required field: name"
}
```

### Conflict Error (409)

```json
{
  "error": "Conflict",
  "message": "Instance already exists or creation failed"
}
```

### Not Found Error (404)

```json
{
  "error": "Not Found",
  "message": "Instance not found: {instanceId}"
}
```

### Internal Server Error (500)

```json
{
  "error": "Internal server error",
  "message": "Instance manager not initialized"
}
```

---

## 📝 Notes

1. **SecuRT API Integration**: BA Area Enter/Exit solution được quản lý qua SecuRT API (`/v1/securt/instance`). Solution `ba_area_enter_exit` tương thích với SecuRT API.

2. **Solution Required**: Bạn cần chỉ định `solution: "ba_area_enter_exit"` trong request body khi tạo instance.

3. **Areas Management**: Có 2 cách để quản lý areas:
   - **Cách 1 (Legacy)**: Sử dụng `Areas` và `AreaConfigs` trong `additionalParams` khi tạo instance (JSON string format)
   - **Cách 2 (Khuyến nghị)**: Sử dụng API `/v1/securt/instance/{instanceId}/area/objectEnterExit` để tạo/quản lý areas động

4. **Object Enter/Exit Area API**: 
   - `POST /v1/securt/instance/{instanceId}/area/objectEnterExit` - Tạo area mới
   - `PUT /v1/securt/instance/{instanceId}/area/objectEnterExit/{areaId}` - Tạo area với ID cụ thể
   - `GET /v1/securt/instance/{instanceId}/areas` - Lấy tất cả areas (bao gồm objectEnterExit)
   - `DELETE /v1/securt/instance/{instanceId}/area/{areaId}` - Xóa area

5. **Area Format**: 
   - `coordinates`: Array các điểm tạo thành polygon (tối thiểu 3 điểm)
   - `classes`: Array các class cần phát hiện: `["Person", "Vehicle", "Animal", "Face", "Unknown"]`
   - `color`: RGBA array `[R, G, B, A]` (0-255)
   - `alertOnEnter`: `true` nếu muốn cảnh báo khi vào vùng
   - `alertOnExit`: `true` nếu muốn cảnh báo khi ra khỏi vùng

6. **Compatibility**: Instance tạo qua Core API với `solution = "ba_area_enter_exit"` sẽ tự động được đăng ký trong SecuRT và có thể quản lý qua SecuRT API.

7. **Start/Stop**: Sử dụng Core API để start/stop instance:
   - `POST /v1/core/instance/{instanceId}/start`
   - `POST /v1/core/instance/{instanceId}/stop`

