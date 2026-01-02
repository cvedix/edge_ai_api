# Hướng Dẫn Sử Dụng Edge AI API

## Tổng Quan

Edge AI API là REST API server cho phép điều khiển và giám sát các AI processing instances trên thiết bị biên. API cung cấp các chức năng quản lý instance, nhận diện khuôn mặt, cấu hình hệ thống, và nhiều tính năng khác.

**Base URL**: `http://localhost:8080`

**Swagger UI**: `http://localhost:8080/swagger` (dùng để test API trực tiếp)

---

## 1. Core API - Kiểm Tra Hệ Thống

### Health Check
Kiểm tra trạng thái hoạt động của API server.

```bash
GET /v1/core/health
```

**Ví dụ:**
```bash
curl http://localhost:8080/v1/core/health
```

**Kết quả:** Trả về `"status": "healthy"` nếu server đang hoạt động tốt.

### Version Information
Lấy thông tin phiên bản API.

```bash
GET /v1/core/version
```

### System Info
Lấy thông tin phần cứng hệ thống (CPU, RAM, GPU, Disk).

```bash
GET /v1/core/system/info
```

### System Status
Lấy trạng thái runtime (CPU/RAM usage, uptime).

```bash
GET /v1/core/system/status
```

### Metrics
Lấy metrics theo định dạng Prometheus.

```bash
GET /v1/core/metrics
```

---

## 2. Instances API - Quản Lý AI Instance

AI Instance là một pipeline xử lý AI (ví dụ: nhận diện khuôn mặt, phát hiện vật thể).

### Liệt Kê Tất Cả Instances
```bash
GET /v1/core/instance
```

### Tạo Instance Mới
Tạo một AI instance mới với cấu hình cụ thể.

```bash
POST /v1/core/instance
Content-Type: application/json

{
  "name": "Camera 1",
  "solution": "face_detection",
  "autoStart": true,
  "persistent": true,
  "additionalParams": {
    "RTSP_URL": "rtsp://192.168.1.100:8554/stream"
  }
}
```

**Các tham số quan trọng:**
- `name`: Tên instance
- `solution`: Loại solution (face_detection, object_detection, ...)
- `autoStart`: Tự động khởi động sau khi tạo
- `persistent`: Lưu cấu hình để tự động load khi restart server
- `additionalParams`: Tham số cho solution (RTSP_URL, MODEL_PATH, ...)

### Lấy Thông Tin Instance
```bash
GET /v1/core/instance/{instanceId}
```

### Cập Nhật Instance
```bash
PUT /v1/core/instance/{instanceId}
Content-Type: application/json

{
  "name": "Camera 1 Updated",
  "additionalParams": {
    "RTSP_URL": "rtsp://192.168.1.101:8554/stream"
  }
}
```

### Xóa Instance
```bash
DELETE /v1/core/instance/{instanceId}
```

### Khởi Động Instance
```bash
POST /v1/core/instance/{instanceId}/start
```

### Dừng Instance
```bash
POST /v1/core/instance/{instanceId}/stop
```

### Khởi Động Lại Instance
```bash
POST /v1/core/instance/{instanceId}/restart
```

### Batch Operations
Khởi động/dừng/khởi động lại nhiều instances cùng lúc.

```bash
POST /v1/core/instance/batch/start
POST /v1/core/instance/batch/stop
POST /v1/core/instance/batch/restart

Content-Type: application/json
{
  "instanceIds": ["id1", "id2", "id3"]
}
```

### Lấy Thống Kê Instance
Lấy thông tin FPS, latency, số lượng object được phát hiện, ...

```bash
GET /v1/core/instance/{instanceId}/statistics
```

### Lấy Frame Cuối Cùng
Lấy frame cuối cùng đã được xử lý (dạng base64 JPEG).

```bash
GET /v1/core/instance/{instanceId}/frame
```

### Lấy Preview Frame
Lấy preview frame với các annotation (bounding boxes, labels).

```bash
GET /v1/core/instance/{instanceId}/preview
```

### Lấy Output
Lấy output từ instance (FILE/RTMP/RTSP stream).

```bash
GET /v1/core/instance/{instanceId}/output
```

### Cấu Hình Input Source
Thay đổi nguồn input (RTSP URL, file path, ...).

```bash
POST /v1/core/instance/{instanceId}/input
Content-Type: application/json

{
  "source": "rtsp://192.168.1.100:8554/new_stream"
}
```

---

## 3. Solutions API - Quản Lý Solution Templates

Solution là template định nghĩa pipeline xử lý AI (các nodes và cách kết nối).

### Liệt Kê Tất Cả Solutions
```bash
GET /v1/core/solution
```

### Lấy Thông Tin Solution
```bash
GET /v1/core/solution/{solutionId}
```

### Tạo Solution Mới
Tạo custom solution với pipeline tùy chỉnh.

```bash
POST /v1/core/solution
Content-Type: application/json

{
  "solutionId": "my_custom_solution",
  "solutionName": "My Custom Solution",
  "solutionType": "face_detection",
  "pipeline": [
    {
      "nodeType": "rtsp_src",
      "nodeName": "source_{instanceId}",
      "parameters": {
        "url": "rtsp://localhost/stream"
      }
    },
    {
      "nodeType": "yunet_face_detector",
      "nodeName": "detector_{instanceId}",
      "parameters": {
        "model_path": "models/face/yunet.onnx"
      }
    }
  ]
}
```

### Cập Nhật Solution
```bash
PUT /v1/core/solution/{solutionId}
```

### Xóa Solution
```bash
DELETE /v1/core/solution/{solutionId}
```

**Lưu ý:** Không thể xóa default solutions, chỉ xóa được custom solutions.

---

## 4. Groups API - Quản Lý Nhóm Instances

Groups giúp tổ chức và quản lý nhiều instances cùng lúc.

### Liệt Kê Tất Cả Groups
```bash
GET /v1/core/groups
```

### Tạo Group Mới
```bash
POST /v1/core/groups
Content-Type: application/json

{
  "groupId": "camera_group_1",
  "groupName": "Camera Group 1",
  "description": "Nhóm camera tầng 1"
}
```

### Lấy Thông Tin Group
```bash
GET /v1/core/groups/{groupId}
```

### Lấy Instances Trong Group
```bash
GET /v1/core/groups/{groupId}/instances
```

### Cập Nhật Group
```bash
PUT /v1/core/groups/{groupId}
```

### Xóa Group
```bash
DELETE /v1/core/groups/{groupId}
```

---

## 5. Lines API - Quản Lý Crossing Lines

Lines dùng cho solution `ba_crossline` để đếm số lượng object vượt qua đường thẳng.

### Liệt Kê Tất Cả Lines
```bash
GET /v1/core/instance/{instanceId}/lines
```

### Tạo Line Mới
```bash
POST /v1/core/instance/{instanceId}/lines
Content-Type: application/json

{
  "lineId": "line_1",
  "points": [
    {"x": 100, "y": 200},
    {"x": 500, "y": 200}
  ],
  "direction": "left_to_right"
}
```

### Cập Nhật Line
```bash
PUT /v1/core/instance/{instanceId}/lines/{lineId}
```

### Xóa Line
```bash
DELETE /v1/core/instance/{instanceId}/lines/{lineId}
```

### Xóa Tất Cả Lines
```bash
DELETE /v1/core/instance/{instanceId}/lines
```

---

## 6. Recognition API - Nhận Diện Khuôn Mặt

### Nhận Diện Khuôn Mặt Từ Ảnh
Upload ảnh và nhận diện khuôn mặt trong ảnh đó.

```bash
POST /v1/recognition/recognize
Content-Type: multipart/form-data

file: [ảnh file]
limit: 0
prediction_count: 1
det_prob_threshold: 0.5
```

**Ví dụ với curl:**
```bash
curl -X POST http://localhost:8080/v1/recognition/recognize \
  -F "file=@image.jpg" \
  -F "limit=5" \
  -F "prediction_count=1"
```

### Đăng Ký Khuôn Mặt Mới
Lưu khuôn mặt vào database để training.

```bash
POST /v1/recognition/faces
Content-Type: multipart/form-data

file: [ảnh file]
subject: "ten_nguoi"
```

### Liệt Kê Tất Cả Subjects
```bash
GET /v1/recognition/faces?page=0&size=20&subject=
```

### Xóa Subject
```bash
DELETE /v1/recognition/faces/{imageId}
```

hoặc xóa theo tên subject:

```bash
DELETE /v1/recognition/faces?subject=ten_nguoi
```

### Xóa Nhiều Subjects
```bash
POST /v1/recognition/faces/delete
Content-Type: application/json

{
  "imageIds": ["id1", "id2"],
  "subjects": ["subject1", "subject2"]
}
```

### Xóa Tất Cả Subjects
```bash
DELETE /v1/recognition/faces/all
```

### Tìm Kiếm Khuôn Mặt
Tìm khuôn mặt tương tự trong database.

```bash
POST /v1/recognition/search
Content-Type: multipart/form-data

file: [ảnh file]
limit: 5
```

### Cấu Hình Database
Cấu hình kết nối MySQL/PostgreSQL cho face database.

```bash
POST /v1/recognition/face-database/connection
Content-Type: application/json

{
  "type": "mysql",
  "host": "localhost",
  "port": 3306,
  "database": "face_db",
  "username": "user",
  "password": "pass"
}
```

---

## 7. Models API - Quản Lý Model Files

### Upload Model File
```bash
POST /v1/core/models
Content-Type: multipart/form-data

file: [model file]
```

### Liệt Kê Model Files
```bash
GET /v1/core/models
```

### Đổi Tên Model File
```bash
PUT /v1/core/models/{fileName}?newName=new_name.onnx
```

### Xóa Model File
```bash
DELETE /v1/core/models/{fileName}
```

---

## 8. Video API - Quản Lý Video Files

### Upload Video File
```bash
POST /v1/core/video
Content-Type: multipart/form-data

file: [video file]
```

### Liệt Kê Video Files
```bash
GET /v1/core/video
```

### Đổi Tên Video File
```bash
PUT /v1/core/video/{fileName}?newName=new_name.mp4
```

### Xóa Video File
```bash
DELETE /v1/core/video/{fileName}
```

---

## 9. Fonts API - Quản Lý Font Files

### Upload Font File
```bash
POST /v1/core/fonts
Content-Type: multipart/form-data

file: [font file]
```

### Liệt Kê Font Files
```bash
GET /v1/core/fonts
```

### Đổi Tên Font File
```bash
PUT /v1/core/fonts/{fileName}?newName=new_font.ttf
```

---

## 10. Node API - Quản Lý Node Pool

Node Pool chứa các nodes đã được cấu hình sẵn để tái sử dụng.

### Liệt Kê Nodes
```bash
GET /v1/core/node?available=true&category=source
```

**Query parameters:**
- `available`: Lọc nodes đang available (true/false)
- `category`: Lọc theo category (source, detector, processor, destination, broker)

### Tạo Node Mới
```bash
POST /v1/core/node
Content-Type: application/json

{
  "templateId": "rtsp_src_template",
  "displayName": "My RTSP Source",
  "parameters": {
    "rtsp_url": "rtsp://192.168.1.100:8554/stream"
  }
}
```

### Lấy Thông Tin Node
```bash
GET /v1/core/node/{nodeId}
```

### Cập Nhật Node
```bash
PUT /v1/core/node/{nodeId}
```

### Xóa Node
```bash
DELETE /v1/core/node/{nodeId}
```

### Liệt Kê Node Templates
Lấy danh sách các node templates có sẵn.

```bash
GET /v1/core/node/template
```

### Lấy Thông Tin Template
```bash
GET /v1/core/node/template/{templateId}
```

---

## 11. Config API - Quản Lý Cấu Hình Hệ Thống

### Lấy Toàn Bộ Config
```bash
GET /v1/core/config
```

### Lấy Config Section
```bash
GET /v1/core/config/{section}
```

Ví dụ: `GET /v1/core/config/logging`

### Cập Nhật Config (Merge)
Cập nhật một phần config (merge với config hiện tại).

```bash
POST /v1/core/config
Content-Type: application/json

{
  "logging": {
    "level": "DEBUG"
  }
}
```

### Thay Thế Toàn Bộ Config
```bash
PUT /v1/core/config
Content-Type: application/json

{
  // toàn bộ config object
}
```

### Cập Nhật Config Section
```bash
PUT /v1/core/config/{section}
Content-Type: application/json

{
  // section config object
}
```

### Xóa Config Section
```bash
DELETE /v1/core/config/{section}
```

### Reset Config Về Mặc Định
```bash
POST /v1/core/config/reset
```

---

## 12. Logs API - Xem Logs

### Liệt Kê Tất Cả Log Files
```bash
GET /v1/core/log
```

### Lấy Logs Theo Category
```bash
GET /v1/core/log/{category}?level=ERROR&tail=100
```

**Categories:** `api`, `instance`, `sdk_output`, `general`

**Query parameters:**
- `level`: Lọc theo log level (INFO, ERROR, WARNING, ...)
- `from`: Thời gian bắt đầu (ISO 8601 format)
- `to`: Thời gian kết thúc (ISO 8601 format)
- `tail`: Lấy N dòng cuối cùng

### Lấy Logs Theo Ngày
```bash
GET /v1/core/log/{category}/{date}
```

Ví dụ: `GET /v1/core/log/api/2024-01-01`

---

## 13. AI API - Xử Lý Ảnh/Frame Đơn

### Xử Lý Ảnh/Frame Đơn
Gửi một ảnh để xử lý qua AI pipeline.

```bash
POST /v1/core/ai/process
Content-Type: application/json

{
  "image": "base64_encoded_image_string",
  "config": "{\"model\": \"yolo\", \"threshold\": 0.5}",
  "priority": "medium"
}
```

### Lấy Trạng Thái AI Processing
```bash
GET /v1/core/ai/status
```

### Lấy Metrics AI Processing
```bash
GET /v1/core/ai/metrics
```

---

## Các Solution Types Phổ Biến

- `face_detection`: Nhận diện khuôn mặt từ RTSP stream
- `face_detection_file`: Nhận diện khuôn mặt từ video file
- `object_detection`: Phát hiện vật thể (YOLO)
- `ba_crossline`: Đếm số lượng object vượt qua đường thẳng
- `face_detection_rtmp`: Nhận diện khuôn mặt với RTMP streaming

---

## Lưu Ý Quan Trọng

1. **Instance ID**: Khi tạo instance mới, server sẽ trả về `instanceId` (UUID). Dùng ID này để điều khiển instance.

2. **Persistent Mode**: Nếu set `persistent: true`, instance sẽ được lưu và tự động load khi server restart.

3. **Auto Start**: Nếu set `autoStart: true`, instance sẽ tự động khởi động sau khi tạo.

4. **RTSP_URL**: Hầu hết solutions cần `RTSP_URL` trong `additionalParams` để chỉ định nguồn video stream.

5. **Swagger UI**: Truy cập `http://localhost:8080/swagger` để xem tất cả endpoints và test trực tiếp.

6. **Error Handling**: API trả về HTTP status codes:
   - `200`: Thành công
   - `400`: Bad request (thiếu tham số, format sai)
   - `404`: Không tìm thấy resource
   - `500`: Server error
   - `503`: Service unavailable

---

## Ví Dụ Workflow Hoàn Chỉnh

### 1. Tạo Instance Nhận Diện Khuôn Mặt
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Camera 1",
    "solution": "face_detection",
    "autoStart": true,
    "persistent": true,
    "additionalParams": {
      "RTSP_URL": "rtsp://192.168.1.100:8554/stream"
    }
  }'
```

### 2. Kiểm Tra Trạng Thái Instance
```bash
curl http://localhost:8080/v1/core/instance/{instanceId}
```

### 3. Xem Thống Kê
```bash
curl http://localhost:8080/v1/core/instance/{instanceId}/statistics
```

### 4. Lấy Frame Preview
```bash
curl http://localhost:8080/v1/core/instance/{instanceId}/preview
```

### 5. Dừng Instance
```bash
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/stop
```

---

## Tài Liệu Tham Khảo

- **API Document đầy đủ**: `docs/API_document.md`
- **Architecture**: `docs/ARCHITECTURE.md`
- **Recognition API Guide**: `docs/RECOGNITION_API_GUIDE.md`
- **Swagger UI**: http://localhost:8080/swagger
- **OpenAPI Spec**: http://localhost:8080/openapi.yaml

