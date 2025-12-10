# Node Pool Manager - Hướng dẫn sử dụng

## Tổng quan

Node Pool Manager cho phép bạn:
1. **Có sẵn 20+ node templates** đã được cấu hình sẵn
2. **User tự chọn nodes** từ pool để tạo pipeline solution
3. **Tái sử dụng nodes** đã được cấu hình

## Kiến trúc

```
Node Templates (Định nghĩa)
    ↓
Pre-configured Nodes (Instances đã cấu hình)
    ↓
Solution Config (Pipeline từ các nodes đã chọn)
    ↓
Instance (Sử dụng solution)
```

## API Endpoints

### 1. Lấy danh sách Node Templates

**GET** `/v1/core/nodes/templates`

Trả về tất cả các node templates có sẵn.

**Response:**
```json
{
  "templates": [
    {
      "templateId": "rtsp_src_template",
      "nodeType": "rtsp_src",
      "displayName": "RTSP Source",
      "description": "Receive video stream from RTSP URL",
      "category": "source",
      "defaultParameters": {
        "channel": "0",
        "resize_ratio": "1.0"
      },
      "requiredParameters": ["rtsp_url"],
      "optionalParameters": ["channel", "resize_ratio"],
      "isPreConfigured": false
    },
    ...
  ],
  "total": 20
}
```

### 2. Lấy templates theo category

**GET** `/v1/core/nodes/templates/{category}`

Categories: `source`, `detector`, `processor`, `destination`, `broker`

### 3. Tạo Pre-configured Node

**POST** `/v1/core/nodes/preconfigured`

**Request:**
```json
{
  "templateId": "rtsp_src_template",
  "parameters": {
    "rtsp_url": "rtsp://localhost:8554/stream1",
    "channel": "0",
    "resize_ratio": "1.0"
  }
}
```

**Response:**
```json
{
  "nodeId": "node_a1b2c3d4",
  "templateId": "rtsp_src_template",
  "status": "created",
  "message": "Pre-configured node created successfully"
}
```

### 4. Lấy danh sách Pre-configured Nodes

**GET** `/v1/core/nodes/preconfigured`

**Response:**
```json
{
  "nodes": [
    {
      "nodeId": "node_a1b2c3d4",
      "templateId": "rtsp_src_template",
      "displayName": "RTSP Source",
      "category": "source",
      "parameters": {
        "rtsp_url": "rtsp://localhost:8554/stream1"
      },
      "inUse": false,
      "createdAt": "2025-01-15T10:30:00Z"
    },
    ...
  ],
  "total": 20,
  "available": 15,
  "inUse": 5
}
```

### 5. Lấy Available Nodes

**GET** `/v1/core/nodes/preconfigured/available`

Chỉ trả về các nodes chưa được sử dụng.

### 6. Tạo Solution từ Selected Nodes

**POST** `/v1/core/nodes/build-solution`

**Request:**
```json
{
  "solutionId": "my_custom_solution",
  "solutionName": "My Custom Solution",
  "nodeIds": [
    "node_a1b2c3d4",  // RTSP Source
    "node_e5f6g7h8",  // YuNet Face Detector
    "node_i9j0k1l2"   // File Destination
  ]
}
```

**Response:**
```json
{
  "solutionId": "my_custom_solution",
  "solutionName": "My Custom Solution",
  "pipeline": [
    {
      "nodeType": "rtsp_src",
      "nodeName": "RTSP Source_{instanceId}",
      "parameters": {
        "rtsp_url": "rtsp://localhost:8554/stream1"
      }
    },
    {
      "nodeType": "yunet_face_detector",
      "nodeName": "YuNet Face Detector_{instanceId}",
      "parameters": {
        "model_path": "/path/to/model.onnx",
        "score_threshold": "0.7"
      }
    },
    {
      "nodeType": "file_des",
      "nodeName": "File Destination_{instanceId}",
      "parameters": {
        "save_dir": "./output/{instanceId}"
      }
    }
  ],
  "message": "Solution created successfully. You can now use this solutionId to create instances."
}
```

### 7. Lấy Statistics

**GET** `/v1/core/nodes/stats`

**Response:**
```json
{
  "totalTemplates": 20,
  "totalPreConfiguredNodes": 15,
  "availableNodes": 10,
  "inUseNodes": 5,
  "nodesByCategory": {
    "source": 3,
    "detector": 5,
    "processor": 4,
    "destination": 2,
    "broker": 1
  }
}
```

## Workflow sử dụng

### Bước 1: Xem các Node Templates có sẵn
```bash
curl http://localhost:8080/v1/core/nodes/templates
```

### Bước 2: Tạo Pre-configured Nodes từ Templates
```bash
curl -X POST http://localhost:8080/v1/core/nodes/preconfigured \
  -H "Content-Type: application/json" \
  -d '{
    "templateId": "rtsp_src_template",
    "parameters": {
      "rtsp_url": "rtsp://localhost:8554/stream1"
    }
  }'
```

### Bước 3: Xem các Pre-configured Nodes đã tạo
```bash
curl http://localhost:8080/v1/core/nodes/preconfigured/available
```

### Bước 4: Chọn nodes và tạo Solution
```bash
curl -X POST http://localhost:8080/v1/core/nodes/build-solution \
  -H "Content-Type: application/json" \
  -d '{
    "solutionId": "my_pipeline",
    "solutionName": "My Pipeline",
    "nodeIds": ["node_1", "node_2", "node_3"]
  }'
```

### Bước 5: Sử dụng Solution để tạo Instance
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d '{
    "name": "My Instance",
    "solution": "my_pipeline",
    "autoStart": true
  }'
```

## Lợi ích

1. **Tái sử dụng**: Nodes đã cấu hình có thể dùng lại cho nhiều solutions
2. **Linh hoạt**: User tự chọn và sắp xếp nodes theo nhu cầu
3. **Dễ quản lý**: Tập trung quản lý nodes ở một nơi
4. **Hiệu quả**: Không cần cấu hình lại nodes mỗi lần tạo solution

## Ví dụ thực tế

### Tạo Pipeline: RTSP → Face Detection → RTMP Stream

1. Tạo RTSP Source node:
```json
POST /v1/core/nodes/preconfigured
{
  "templateId": "rtsp_src_template",
  "parameters": {"rtsp_url": "rtsp://camera1/stream"}
}
→ node_rtsp_1
```

2. Tạo Face Detector node:
```json
POST /v1/core/nodes/preconfigured
{
  "templateId": "yunet_face_detector_template",
  "parameters": {"model_path": "/models/yunet.onnx"}
}
→ node_face_1
```

3. Tạo RTMP Destination node:
```json
POST /v1/core/nodes/preconfigured
{
  "templateId": "rtmp_des_template",
  "parameters": {"rtmp_url": "rtmp://server/live/stream"}
}
→ node_rtmp_1
```

4. Tạo Solution từ 3 nodes:
```json
POST /v1/core/nodes/build-solution
{
  "solutionId": "face_detection_stream",
  "solutionName": "Face Detection Stream",
  "nodeIds": ["node_rtsp_1", "node_face_1", "node_rtmp_1"]
}
```

5. Tạo Instance sử dụng solution:
```json
POST /v1/core/instance
{
  "name": "Camera 1 Face Detection",
  "solution": "face_detection_stream",
  "autoStart": true
}
```

## Lưu ý

- Pre-configured nodes có thể được tái sử dụng cho nhiều solutions
- Nodes đang được sử dụng (`inUse: true`) không thể xóa
- Solution được tạo từ nodes sẽ được đăng ký vào SolutionRegistry
- Bạn có thể tạo nhiều solutions từ cùng một set nodes

