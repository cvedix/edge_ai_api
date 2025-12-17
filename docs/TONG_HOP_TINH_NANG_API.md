# Tổng Hợp Tính Năng API - Edge AI SDK

## Mục Lục
1. [Tổng Quan Hệ Thống](#tổng-quan-hệ-thống)
2. [Flow Tương Tác Tổng Quan](#flow-tương-tác-tổng-quan)
3. [Các Tính Năng API Chính](#các-tính-năng-api-chính)
4. [Chi Tiết Từng Tính Năng](#chi-tiết-từng-tính-năng)
5. [Các Loại Nodes Hỗ Trợ](#các-loại-nodes-hỗ-trợ)

---

## Tổng Quan Hệ Thống

### Kiến Trúc Hệ Thống

Hệ thống Edge AI API là một RESTful API server được xây dựng trên Drogon Framework, cung cấp giao diện điều khiển cho CVEDIX Edge AI SDK. Hệ thống cho phép:

- **Quản lý Instances**: Tạo, khởi động, dừng, xóa các AI processing instances
- **Quản lý Solutions**: Định nghĩa và quản lý các pipeline templates
- **Quản lý Nodes**: Tạo và quản lý các processing nodes
- **Quản lý Models**: Upload, list, delete AI models
- **Cấu hình Hệ thống**: Quản lý cấu hình toàn hệ thống
- **Giám sát**: Health check, system info, instance status

### Các Thành Phần Chính

1. **REST API Server** (Drogon Framework)
   - HTTP/HTTPS server
   - JSON API responses
   - Swagger UI documentation
   - CORS support

2. **Instance Manager**
   - Quản lý vòng đời instances
   - Pipeline building và execution
   - Persistent storage

3. **SDK Integration** (CVEDIX Edge AI SDK)
   - 43+ processing nodes
   - Real-time video/image processing
   - AI inference engines (TensorRT, RKNN, OpenCV DNN)
   - Behavior analysis và tracking

4. **Data Broker**
   - Message routing giữa nodes
   - Output publishing (MQTT, Kafka, Socket, Console)
   - Event streaming

---

## Flow Tương Tác Tổng Quan

### Flow Tạo và Chạy Instance

```
┌─────────────┐
│   Client    │
│ Application │
└──────┬──────┘
       │
       │ 1. POST /v1/core/solutions
       │    (Tạo Solution Template)
       ▼
┌─────────────────────┐
│  REST API Server     │
│  (Drogon Framework)  │
└──────┬──────────────┘
       │
       │ 2. Validate & Store Solution
       ▼
┌─────────────────────┐
│  Solution Manager   │
└──────┬──────────────┘
       │
       │ 3. POST /v1/core/instance
       │    (Tạo Instance từ Solution)
       ▼
┌─────────────────────┐
│  Instance Manager   │
└──────┬──────────────┘
       │
       │ 4. Build Pipeline từ Solution
       │    + Instance Parameters
       ▼
┌─────────────────────┐
│  Pipeline Builder    │
│  - Parse Solution    │
│  - Create Nodes      │
│  - Connect Nodes     │
└──────┬──────────────┘
       │
       │ 5. POST /v1/core/instances/{id}/start
       │    (Khởi động Instance)
       ▼
┌─────────────────────┐
│  CVEDIX SDK          │
│  - Source Nodes      │
│  - Inference Nodes   │
│  - Processor Nodes   │
│  - Broker Nodes      │
│  - Destination Nodes │
└──────┬──────────────┘
       │
       │ 6. Real-time Processing
       │    Video/Image → AI Inference → Output
       ▼
┌─────────────────────┐
│  Output Destinations │
│  - RTMP Stream       │
│  - File Storage      │
│  - MQTT/Kafka        │
│  - Socket            │
└─────────────────────┘
```

### Flow Quản Lý Instance

```
Client Request
    │
    ├─→ GET /v1/core/instances
    │   └─→ List tất cả instances
    │
    ├─→ GET /v1/core/instances/{id}
    │   └─→ Xem chi tiết instance
    │
    ├─→ POST /v1/core/instances/{id}/start
    │   └─→ Khởi động instance
    │
    ├─→ POST /v1/core/instances/{id}/stop
    │   └─→ Dừng instance
    │
    ├─→ POST /v1/core/instances/{id}/restart
    │   └─→ Khởi động lại instance
    │
    ├─→ PUT /v1/core/instances/{id}
    │   └─→ Cập nhật thông tin instance
    │
    ├─→ POST /v1/core/instance/{id}/config
    │   └─→ Cập nhật cấu hình instance
    │
    ├─→ POST /v1/core/instance/{id}/input
    │   └─→ Thay đổi input source
    │
    └─→ DELETE /v1/core/instances/{id}
        └─→ Xóa instance
```

### Flow Xử Lý Dữ Liệu (Pipeline Execution)

```
┌──────────────┐
│ Input Source │
│ (RTSP/File/  │
│  App/Image)  │
└──────┬───────┘
       │
       │ Video Frames
       ▼
┌──────────────┐
│ Source Node  │
│ (rtsp_src/   │
│  file_src)   │
└──────┬───────┘
       │
       │ Raw Frames
       ▼
┌──────────────┐
│ Inference    │
│ Node         │
│ (Detector/   │
│  Classifier) │
└──────┬───────┘
       │
       │ Detections + Metadata
       ▼
┌──────────────┐
│ Processor    │
│ Node         │
│ (Tracker/    │
│  BA/OSD)     │
└──────┬───────┘
       │
       │ Processed Data
       ▼
┌──────────────┐
│ Broker Node  │
│ (MQTT/Kafka/ │
│  Socket)     │
└──────┬───────┘
       │
       │ Messages/Events
       ▼
┌──────────────┐
│ Destination  │
│ Node         │
│ (RTMP/File)  │
└──────────────┘
```

---

## Các Tính Năng API Chính

### 1. Core APIs (Hệ Thống Cốt Lõi)

| Endpoint | Method | Mô Tả |
|----------|--------|-------|
| `/v1/core/health` | GET | Kiểm tra trạng thái sức khỏe service |
| `/v1/core/version` | GET | Lấy thông tin version |
| `/v1/core/watchdog` | GET | Trạng thái watchdog và health monitor |
| `/v1/core/endpoints` | GET | Danh sách tất cả endpoints |
| `/v1/core/system/info` | GET | Thông tin phần cứng hệ thống |
| `/v1/core/system/status` | GET | Trạng thái hệ thống (CPU, RAM, etc.) |

### 2. Instance Management APIs

| Endpoint | Method | Mô Tả |
|----------|--------|-------|
| `/v1/core/instance` | POST | Tạo instance mới |
| `/v1/core/instances` | GET | List tất cả instances |
| `/v1/core/instances/{id}` | GET | Xem chi tiết instance |
| `/v1/core/instances/{id}` | PUT | Cập nhật instance |
| `/v1/core/instances/{id}` | DELETE | Xóa instance |
| `/v1/core/instances/{id}/start` | POST | Khởi động instance |
| `/v1/core/instances/{id}/stop` | POST | Dừng instance |
| `/v1/core/instances/{id}/restart` | POST | Khởi động lại instance |
| `/v1/core/instance/status/summary` | GET | Tổng hợp trạng thái instances |
| `/v1/core/instances/batch/start` | POST | Khởi động nhiều instances |
| `/v1/core/instances/batch/stop` | POST | Dừng nhiều instances |
| `/v1/core/instances/batch/restart` | POST | Khởi động lại nhiều instances |
| `/v1/core/instance/{id}/input` | POST | Thay đổi input source |
| `/v1/core/instance/{id}/config` | GET | Lấy cấu hình instance |
| `/v1/core/instance/{id}/config` | POST | Cập nhật cấu hình instance |
| `/v1/core/instances/{id}/output` | GET | Lấy output/processing results |
| `/v1/core/instance/{id}/output/stream` | GET | Cấu hình stream output |
| `/v1/core/instance/{id}/output/stream` | POST | Cấu hình stream output (RTMP/RTSP/HLS) |

### 3. Solution Management APIs

| Endpoint | Method | Mô Tả |
|----------|--------|-------|
| `/v1/core/solutions` | GET | List tất cả solutions |
| `/v1/core/solutions` | POST | Tạo solution mới |
| `/v1/core/solutions/{id}` | GET | Xem chi tiết solution |
| `/v1/core/solutions/{id}` | PUT | Cập nhật solution |
| `/v1/core/solutions/{id}` | DELETE | Xóa solution |
| `/v1/core/solutions/{id}/parameters` | GET | Lấy danh sách parameters của solution |

### 4. Node Management APIs

| Endpoint | Method | Mô Tả |
|----------|--------|-------|
| `/v1/core/nodes` | GET | List tất cả nodes |
| `/v1/core/nodes/{id}` | GET | Xem chi tiết node |
| `/v1/core/nodes/{id}` | PUT | Cập nhật node |
| `/v1/core/nodes/{id}` | DELETE | Xóa node |
| `/v1/core/nodes/templates` | GET | List node templates |
| `/v1/core/nodes/preconfigured` | GET | List pre-configured nodes |
| `/v1/core/nodes/preconfigured/available` | GET | List nodes chưa sử dụng |
| `/v1/core/nodes/build-solution` | POST | Tạo solution từ nodes |
| `/v1/core/nodes/stats` | GET | Thống kê nodes |

### 5. Model Management APIs

| Endpoint | Method | Mô Tả |
|----------|--------|-------|
| `/v1/core/models/upload` | POST | Upload model file |
| `/v1/core/models/list` | GET | List uploaded models |
| `/v1/core/models/{modelName}` | DELETE | Xóa model file |

### 6. Configuration APIs

| Endpoint | Method | Mô Tả |
|----------|--------|-------|
| `/v1/core/config` | GET | Lấy cấu hình hệ thống |
| `/v1/core/config` | POST | Cập nhật cấu hình (merge) |
| `/v1/core/config` | PUT | Thay thế toàn bộ cấu hình |
| `/v1/core/config` | PATCH | Cập nhật section cụ thể |
| `/v1/core/config` | DELETE | Xóa section cấu hình |
| `/v1/core/config/{path}` | GET | Lấy section cấu hình theo path |
| `/v1/core/config/reset` | POST | Reset về cấu hình mặc định |

### 7. Documentation APIs

| Endpoint | Method | Mô Tả |
|----------|--------|-------|
| `/swagger` | GET | Swagger UI (tất cả versions) |
| `/v1/swagger` | GET | Swagger UI cho API v1 |
| `/v2/swagger` | GET | Swagger UI cho API v2 |
| `/openapi.yaml` | GET | OpenAPI specification |
| `/v1/openapi.yaml` | GET | OpenAPI spec cho v1 |

---

## Chi Tiết Từng Tính Năng

### 1. Instance Management (Quản Lý Instance)

#### 1.1. Tạo Instance

**Endpoint**: `POST /v1/core/instance`

**Mô tả**: Tạo một instance mới từ solution template với các tham số cụ thể.

**Request Body**:
```json
{
  "name": "face_detection_camera_1",
  "group": "surveillance",
  "solution": "face_detection_pipeline",
  "persistent": true,
  "autoStart": true,
  "detectionSensitivity": "Medium",
  "additionalParams": {
    "RTSP_URL": "rtsp://192.168.1.100:554/stream",
    "MODEL_PATH": "models/face/yunet.onnx",
    "RTMP_URL": "rtmp://192.168.1.200/live/stream1"
  }
}
```

**Chức năng**:
- Tạo instance từ solution template
- Hỗ trợ persistent storage (lưu vào file để khôi phục sau restart)
- Tự động khởi động nếu `autoStart: true`
- Validate parameters và pipeline structure
- Tự động tạo unique instance ID

**Response**:
```json
{
  "instanceId": "inst_abc123",
  "name": "face_detection_camera_1",
  "status": "running",
  "createdAt": "2024-01-01T00:00:00Z"
}
```

#### 1.2. Quản Lý Vòng Đời Instance

**Start Instance**: `POST /v1/core/instances/{id}/start`
- Khởi động pipeline processing
- Validate dependencies
- Khởi tạo các nodes theo thứ tự
- Kết nối nodes trong pipeline
- Bắt đầu xử lý dữ liệu

**Stop Instance**: `POST /v1/core/instances/{id}/stop`
- Dừng pipeline processing
- Giải phóng resources
- Lưu trạng thái nếu persistent
- Cleanup buffers

**Restart Instance**: `POST /v1/core/instances/{id}/restart`
- Stop → Start trong một lệnh
- Hữu ích khi cần reload cấu hình

**Batch Operations**:
- `/v1/core/instances/batch/start` - Khởi động nhiều instances cùng lúc
- `/v1/core/instances/batch/stop` - Dừng nhiều instances cùng lúc
- `/v1/core/instances/batch/restart` - Khởi động lại nhiều instances

#### 1.3. Cập Nhật Instance

**Update Instance Info**: `PUT /v1/core/instances/{id}`
- Cập nhật metadata (name, group, description)
- Không ảnh hưởng đến pipeline đang chạy

**Update Instance Config**: `POST /v1/core/instance/{id}/config`
- Cập nhật cấu hình tại path cụ thể
- Hỗ trợ nested paths (ví dụ: `"detector/confidence_threshold"`)
- Có thể áp dụng ngay hoặc sau khi restart

**Change Input Source**: `POST /v1/core/instance/{id}/input`
- Thay đổi input source mà không cần restart
- Hỗ trợ: RTSP URL, File path, App source

#### 1.4. Monitoring Instance

**Get Instance Status**: `GET /v1/core/instances/{id}`
- Trạng thái hiện tại (running, stopped, error)
- Thông tin pipeline
- Statistics (frames processed, detections, etc.)
- Resource usage

**Get Instance Output**: `GET /v1/core/instances/{id}/output`
- Processing results
- Detection events
- Error logs

**Status Summary**: `GET /v1/core/instance/status/summary`
- Tổng hợp: total, running, stopped instances
- Quick overview cho dashboard

### 2. Solution Management (Quản Lý Solution)

#### 2.1. Tạo Solution

**Endpoint**: `POST /v1/core/solutions`

**Mô tả**: Định nghĩa một pipeline template có thể tái sử dụng.

**Request Body**:
```json
{
  "solutionId": "face_detection_pipeline",
  "solutionName": "Face Detection Pipeline",
  "description": "Pipeline phát hiện khuôn mặt từ RTSP stream",
  "pipeline": [
    {
      "nodeType": "rtsp_src",
      "nodeName": "rtsp_src_{instanceId}",
      "parameters": {
        "rtsp_url": "${RTSP_URL}",
        "channel": "0"
      }
    },
    {
      "nodeType": "yunet_face_detector",
      "nodeName": "face_detector_{instanceId}",
      "parameters": {
        "model_path": "${MODEL_PATH}"
      }
    },
    {
      "nodeType": "json_mqtt_broker",
      "nodeName": "mqtt_broker_{instanceId}",
      "parameters": {
        "mqtt_broker": "${MQTT_BROKER}",
        "mqtt_topic": "${MQTT_TOPIC}"
      }
    }
  ]
}
```

**Chức năng**:
- Định nghĩa pipeline structure
- Hỗ trợ placeholders (`${PARAM_NAME}`) để parameterize
- Validate node types và connections
- Lưu trữ template để tái sử dụng

#### 2.2. Quản Lý Solutions

**List Solutions**: `GET /v1/core/solutions`
- Danh sách tất cả solutions
- Summary information
- Filter và pagination

**Get Solution Details**: `GET /v1/core/solutions/{id}`
- Chi tiết pipeline structure
- Node configurations
- Parameters list

**Update Solution**: `PUT /v1/core/solutions/{id}`
- Cập nhật pipeline definition
- Validate trước khi lưu

**Get Solution Parameters**: `GET /v1/core/solutions/{id}/parameters`
- Danh sách parameters cần thiết
- Parameter types và defaults
- Validation rules

### 3. Node Management (Quản Lý Nodes)

#### 3.1. Node Templates

**Endpoint**: `GET /v1/core/nodes/templates`

**Mô tả**: Lấy danh sách tất cả node types có sẵn.

**Response**:
```json
{
  "templates": [
    {
      "nodeType": "rtsp_src",
      "category": "source",
      "displayName": "RTSP Source",
      "description": "Receive video from RTSP stream",
      "requiredParameters": ["rtsp_url"],
      "optionalParameters": ["channel", "fps"]
    },
    {
      "nodeType": "trt_yolov8_detector",
      "category": "detector",
      "displayName": "TensorRT YOLOv8 Detector",
      "description": "Object detection using YOLOv8 with TensorRT",
      "requiredParameters": ["model_path"],
      "optionalParameters": ["labels_path", "conf_threshold"]
    }
  ]
}
```

#### 3.2. Pre-configured Nodes

**Endpoint**: `GET /v1/core/nodes/preconfigured`

**Mô tả**: Lấy danh sách các nodes đã được cấu hình sẵn.

**Chức năng**:
- Nodes đã được tạo với cấu hình cụ thể
- Có thể tái sử dụng trong nhiều instances
- Quản lý trạng thái `inUse` để tránh conflict

**Available Nodes**: `GET /v1/core/nodes/preconfigured/available`
- Chỉ lấy nodes chưa được sử dụng (`inUse: false`)
- Hữu ích khi tạo instance mới

#### 3.3. Build Solution từ Nodes

**Endpoint**: `POST /v1/core/nodes/build-solution`

**Mô tả**: Tạo solution từ danh sách node IDs.

**Request Body**:
```json
{
  "solutionId": "custom_pipeline",
  "solutionName": "Custom Pipeline",
  "nodeIds": [
    "node_abc123",  // RTSP Source
    "node_def456",  // YOLOv8 Detector
    "node_ghi789"   // MQTT Broker
  ]
}
```

**Chức năng**:
- Tự động tạo pipeline từ nodes
- Validate node compatibility
- Tự động kết nối nodes theo thứ tự

### 4. Model Management (Quản Lý Models)

#### 4.1. Upload Model

**Endpoint**: `POST /v1/core/models/upload`

**Mô tả**: Upload AI model file lên server.

**Request**: Multipart form data
- File: Model file (.onnx, .engine, .trt, .rknn, etc.)
- Metadata: Model name, description

**Chức năng**:
- Validate model format
- Lưu vào thư mục models
- Tạo metadata record
- Hỗ trợ các format: ONNX, TensorRT, RKNN, etc.

#### 4.2. List Models

**Endpoint**: `GET /v1/core/models/list`

**Response**:
```json
{
  "models": [
    {
      "name": "yolov8n.onnx",
      "size": 12345678,
      "uploadedAt": "2024-01-01T00:00:00Z",
      "format": "onnx"
    }
  ]
}
```

#### 4.3. Delete Model

**Endpoint**: `DELETE /v1/core/models/{modelName}`

**Mô tả**: Xóa model file và metadata.

**Lưu ý**: Kiểm tra xem model có đang được sử dụng không trước khi xóa.

### 5. Configuration Management (Quản Lý Cấu Hình)

#### 5.1. Get Configuration

**Endpoint**: `GET /v1/core/config`

**Query Parameters**:
- `path` (optional): Path đến section cụ thể (ví dụ: `system/max_running_instances`)

**Chức năng**:
- Lấy toàn bộ cấu hình hoặc section cụ thể
- Hỗ trợ nested paths với `/` hoặc `.` separators
- Trả về JSON structure

**Ví dụ**:
```bash
GET /v1/core/config?path=system/max_running_instances
```

#### 5.2. Update Configuration

**Merge Config**: `POST /v1/core/config`
- Merge JSON vào cấu hình hiện tại
- Chỉ cập nhật fields được cung cấp
- Giữ nguyên các fields khác

**Replace Config**: `PUT /v1/core/config`
- Thay thế toàn bộ cấu hình
- Cảnh báo: Mất tất cả cấu hình cũ

**Update Section**: `PATCH /v1/core/config?path=system/web_server`
- Cập nhật section cụ thể
- Hỗ trợ nested paths

**Delete Section**: `DELETE /v1/core/config?path=system/web_server`
- Xóa section cấu hình

**Reset Config**: `POST /v1/core/config/reset`
- Reset về cấu hình mặc định
- Cảnh báo: Mất tất cả cấu hình tùy chỉnh

### 6. System Monitoring (Giám Sát Hệ Thống)

#### 6.1. Health Check

**Endpoint**: `GET /v1/core/health`

**Response**:
```json
{
  "status": "healthy",
  "timestamp": "2024-01-01T00:00:00.000Z",
  "uptime": 3600,
  "service": "edge_ai_api",
  "version": "1.0.0",
  "checks": {
    "uptime": true,
    "service": true
  }
}
```

**Chức năng**:
- Kiểm tra service health
- Uptime tracking
- Health checks validation
- Sử dụng cho load balancer health checks

#### 6.2. System Information

**Endpoint**: `GET /v1/core/system/info`

**Response**:
```json
{
  "cpu": {
    "model": "Intel Core i7",
    "cores": 8,
    "threads": 16
  },
  "gpu": [
    {
      "name": "NVIDIA RTX 3080",
      "memory": "10GB"
    }
  ],
  "ram": {
    "total": "32GB",
    "available": "16GB"
  },
  "disk": {
    "total": "500GB",
    "available": "200GB"
  },
  "os": {
    "name": "Ubuntu 22.04",
    "kernel": "6.8.0"
  }
}
```

**Chức năng**:
- Thông tin phần cứng chi tiết
- CPU, GPU, RAM, Disk
- OS information
- Mainboard info

#### 6.3. System Status

**Endpoint**: `GET /v1/core/system/status`

**Response**:
```json
{
  "cpu": {
    "usage_percent": 45.5,
    "cores": 8
  },
  "memory": {
    "used_mb": 8192,
    "total_mb": 32768,
    "usage_percent": 25.0
  },
  "load_average": [1.2, 1.5, 1.8],
  "uptime": 86400
}
```

**Chức năng**:
- Real-time system metrics
- CPU usage
- Memory usage
- Load average
- Uptime

#### 6.4. Watchdog Status

**Endpoint**: `GET /v1/core/watchdog`

**Response**:
```json
{
  "watchdog": {
    "enabled": true,
    "check_interval_ms": 5000,
    "timeout_ms": 30000,
    "last_check": "2024-01-01T00:00:00.000Z",
    "stats": {
      "total_heartbeats": 1000,
      "missed_heartbeats": 0,
      "recovery_actions": 0
    }
  },
  "health_monitor": {
    "enabled": true,
    "check_interval_ms": 1000,
    "last_heartbeat": "2024-01-01T00:00:00.000Z"
  }
}
```

**Chức năng**:
- Monitor application health
- Heartbeat tracking
- Recovery actions
- Health monitor status

---

## Các Loại Nodes Hỗ Trợ

### 1. Source Nodes (6 nodes) - Đầu Vào

#### 1.1. RTSP Source (`rtsp_src`)
- **Mô tả**: Nhận video stream từ RTSP camera hoặc server
- **Parameters**:
  - `rtsp_url` (required): RTSP URL (ví dụ: `rtsp://192.168.1.100:554/stream`)
  - `channel` (optional): Channel number
  - `fps` (optional): Frame rate limit
- **Use Cases**: IP camera surveillance, RTSP streaming

#### 1.2. File Source (`file_src`)
- **Mô tả**: Đọc video từ file
- **Parameters**:
  - `file_path` (required): Đường dẫn đến video file
  - `loop` (optional): Lặp lại video
- **Use Cases**: Video file processing, batch processing

#### 1.3. App Source (`app_src`)
- **Mô tả**: Nhận frames từ application code (push frames)
- **Parameters**:
  - `width`, `height`, `format`: Frame specifications
- **Use Cases**: Custom application integration

#### 1.4. Image Source (`image_src`)
- **Mô tả**: Đọc ảnh từ file hoặc UDP
- **Parameters**:
  - `image_path` hoặc `udp_port`
- **Use Cases**: Image processing, batch image analysis

#### 1.5. RTMP Source (`rtmp_src`)
- **Mô tả**: Nhận video từ RTMP stream
- **Parameters**:
  - `rtmp_url` (required): RTMP URL
- **Use Cases**: RTMP streaming input

#### 1.6. UDP Source (`udp_src`)
- **Mô tả**: Nhận video từ UDP stream
- **Parameters**:
  - `udp_port` (required): UDP port
- **Use Cases**: UDP streaming, network video

### 2. Inference Nodes (23 nodes) - AI Suy Luận

#### 2.1. TensorRT Nodes (11 nodes)

**TensorRT YOLOv8 Detector** (`trt_yolov8_detector`)
- Object detection với YOLOv8
- Tối ưu hóa cho NVIDIA GPUs
- Parameters: `model_path`, `labels_path`, `conf_threshold`

**TensorRT YOLOv8 Segmentation** (`trt_yolov8_seg_detector`)
- Instance segmentation
- Parameters: `model_path`, `labels_path`

**TensorRT YOLOv8 Pose** (`trt_yolov8_pose_detector`)
- Pose estimation
- Parameters: `model_path`

**TensorRT YOLOv8 Classifier** (`trt_yolov8_classifier`)
- Image classification
- Parameters: `model_path`, `labels_path`, `class_ids_applied_to`

**TensorRT Vehicle Detector** (`trt_vehicle_detector`)
- Vehicle detection chuyên biệt
- Parameters: `model_path`

**TensorRT Vehicle Plate Detector** (`trt_vehicle_plate_detector`, `trt_vehicle_plate_detector_v2`)
- Biển số xe detection
- Parameters: `model_path`

**TensorRT Vehicle Color Classifier** (`trt_vehicle_color_classifier`)
- Phân loại màu xe
- Parameters: `model_path`

**TensorRT Vehicle Type Classifier** (`trt_vehicle_type_classifier`)
- Phân loại loại xe
- Parameters: `model_path`

**TensorRT Vehicle Feature Encoder** (`trt_vehicle_feature_encoder`)
- Mã hóa đặc trưng xe
- Parameters: `model_path`

**TensorRT Vehicle Scanner** (`trt_vehicle_scanner`)
- Quét và phân tích xe
- Parameters: `model_path`

#### 2.2. RKNN Nodes (2 nodes)

**RKNN YOLOv8 Detector** (`rknn_yolov8_detector`)
- YOLOv8 detection cho Rockchip NPU
- Parameters: `model_path`

**RKNN Face Detector** (`rknn_face_detector`)
- Face detection cho Rockchip NPU
- Parameters: `model_path`

#### 2.3. Other Inference Nodes (10 nodes)

**YuNet Face Detector** (`yunet_face_detector`)
- Face detection với YuNet
- Parameters: `model_path`

**SFace Feature Encoder** (`sface_feature_encoder`)
- Face feature encoding
- Parameters: `model_path`

**YOLO Detector** (`yolo_detector`)
- Generic YOLO detection (OpenCV DNN)
- Parameters: `model_path`, `config_path`

**ENet Segmentation** (`enet_seg`)
- Semantic segmentation
- Parameters: `model_path`

**Mask RCNN Detector** (`mask_rcnn_detector`)
- Instance segmentation
- Parameters: `model_path`

**OpenPose Detector** (`openpose_detector`)
- Pose estimation
- Parameters: `model_path`

**Classifier** (`classifier`)
- Generic image classifier
- Parameters: `model_path`, `labels_path`

**Feature Encoder** (`feature_encoder`)
- Generic feature encoding
- Parameters: `model_path`

**Lane Detector** (`lane_detector`)
- Lane detection
- Parameters: `model_path`

**PaddleOCR Text Detector** (`ppocr_text_detector`)
- Text detection và recognition
- Parameters: `model_path`

**Restoration** (`restoration`)
- Image restoration/enhancement (Real-ESRGAN)
- Parameters: `model_path`

### 3. Processor Nodes - Xử Lý

#### 3.1. Tracking Nodes

**SORT Tracker** (`sort_track`)
- Object tracking với SORT algorithm
- Parameters: (auto-configured)
- **Chức năng**: Theo dõi objects qua các frames

#### 3.2. Behavior Analysis Nodes

**BA Crossline** (`ba_crossline`)
- Phát hiện vượt đường
- Parameters:
  - `line_channel`: Channel number
  - `line_start_x`, `line_start_y`: Điểm bắt đầu
  - `line_end_x`, `line_end_y`: Điểm kết thúc
- **Chức năng**: Phát hiện khi object vượt qua đường line

**BA Crossline OSD** (`ba_crossline_osd`)
- Overlay kết quả crossline detection
- Parameters: (auto-configured)

#### 3.3. OSD Nodes

**Face OSD v2** (`face_osd_v2`)
- Overlay face detection results
- Parameters: (auto-configured)
- **Chức năng**: Vẽ bounding boxes và labels lên frame

#### 3.4. Other Processors

**Classifier** (`classifier`)
- Image classification
- Parameters: `model_path`

**Lane Detector** (`lane_detector`)
- Lane detection
- Parameters: `model_path`

**Restoration** (`restoration`)
- Image restoration
- Parameters: `model_path`

### 4. Broker Nodes (12 nodes) - Xuất Kết Quả

#### 4.1. JSON Brokers

**JSON Console Broker** (`json_console_broker`)
- Xuất JSON ra console
- Parameters: (auto-configured)

**JSON Enhanced Console Broker** (`json_enhanced_console_broker`)
- Enhanced JSON console output
- Parameters: (auto-configured)

**JSON MQTT Broker** (`json_mqtt_broker`)
- Publish JSON messages qua MQTT
- Parameters:
  - `mqtt_broker`: MQTT broker URL
  - `mqtt_topic`: Topic name
  - `mqtt_username`, `mqtt_password`: Authentication

**JSON Kafka Broker** (`json_kafka_broker`)
- Publish JSON messages qua Kafka
- Parameters:
  - `kafka_broker`: Kafka broker URL
  - `kafka_topic`: Topic name

#### 4.2. XML Brokers

**XML File Broker** (`xml_file_broker`)
- Xuất XML ra file
- Parameters:
  - `output_path`: File path
  - `format`: XML format

**XML Socket Broker** (`xml_socket_broker`)
- Gửi XML qua socket
- Parameters:
  - `socket_host`: Host address
  - `socket_port`: Port number

#### 4.3. Specialized Brokers

**Message Broker** (`msg_broker`)
- Generic message broker
- Parameters: (configurable)

**BA Socket Broker** (`ba_socket_broker`)
- Behavior analysis socket broker
- Parameters: `socket_host`, `socket_port`

**Embeddings Socket Broker** (`embeddings_socket_broker`)
- Face/object embeddings socket broker
- Parameters: `socket_host`, `socket_port`

**Embeddings Properties Socket Broker** (`embeddings_properties_socket_broker`)
- Embeddings với properties
- Parameters: `socket_host`, `socket_port`

**Plate Socket Broker** (`plate_socket_broker`)
- License plate socket broker
- Parameters: `socket_host`, `socket_port`

**Expression Socket Broker** (`expr_socket_broker`)
- Expression/emotion socket broker
- Parameters: `socket_host`, `socket_port`

### 5. Destination Nodes (2 nodes) - Đầu Ra

#### 5.1. File Destination (`file_des`)
- **Mô tả**: Lưu video vào file
- **Parameters**:
  - `save_dir` (required): Thư mục lưu file
  - `name_prefix` (optional): Prefix tên file
  - `osd` (optional): Có overlay OSD không
- **Use Cases**: Video recording, archive

#### 5.2. RTMP Destination (`rtmp_des`)
- **Mô tả**: Gửi video qua RTMP stream
- **Parameters**:
  - `rtmp_url` (required): RTMP URL
  - `channel` (optional): Channel number
- **Use Cases**: Live streaming, broadcasting

#### 5.3. Screen Destination (`screen_des`)
- **Mô tả**: Hiển thị video trên màn hình
- **Parameters**: (auto-configured)
- **Use Cases**: Local display, monitoring

---

## Tổng Kết

### Thống Kê Tính Năng

| Loại | Số Lượng | Mô Tả |
|------|----------|-------|
| **API Endpoints** | 50+ | RESTful APIs đầy đủ |
| **Source Nodes** | 6 | Input sources đa dạng |
| **Inference Nodes** | 23 | AI models phong phú |
| **Processor Nodes** | 10+ | Tracking, BA, OSD |
| **Broker Nodes** | 12 | Output formats đa dạng |
| **Destination Nodes** | 3 | Output destinations |

### Use Cases Chính

1. **Video Surveillance**
   - RTSP camera → Face/Vehicle detection → MQTT/Kafka
   - Real-time monitoring và alerting

2. **Video Processing**
   - File video → Object detection → RTMP stream
   - Batch processing và live streaming

3. **Real-time Analytics**
   - Multiple streams → Multiple detectors → Multiple brokers
   - Scalable processing

4. **Edge AI Applications**
   - Camera input → AI inference → Cloud messaging
   - Edge computing solutions

5. **Behavior Analysis**
   - Video stream → Detection → Tracking → BA → Alerts
   - Smart surveillance

### Điểm Mạnh

✅ **Tính năng đầy đủ**: Hỗ trợ đầy đủ các loại nodes từ SDK
✅ **Dễ sử dụng**: RESTful API đơn giản, Swagger UI
✅ **Linh hoạt**: Pipeline có thể tùy chỉnh, parameterize
✅ **Scalable**: Hỗ trợ nhiều instances đồng thời
✅ **Production-ready**: Persistent storage, health monitoring, logging
✅ **Đa dạng AI models**: TensorRT, RKNN, OpenCV DNN
✅ **Đa dạng output**: MQTT, Kafka, Socket, File, RTMP

---

*Tài liệu này được tạo tự động dựa trên phân tích codebase và SDK documentation.*
