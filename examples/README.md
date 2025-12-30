# Examples

Thư mục này chứa các examples, documentation và scripts để làm việc với Edge AI API.

## ⚠️ LƯU Ý QUAN TRỌNG

**Các file example là template**, không phải file sẵn sàng để chạy. Trước khi tạo instance, bạn **PHẢI** cập nhật:

- ✅ **Đường dẫn model files** - Thay thế `/home/cvedix/...` bằng đường dẫn thực tế của bạn
- ✅ **Đường dẫn video files** - Cập nhật đường dẫn video test
- ✅ **RTSP/RTMP URLs** - Cập nhật nếu dùng RTSP source hoặc RTMP output
- ✅ **MQTT configuration** - Cập nhật nếu dùng MQTT events

**Xem chi tiết:** [IMPORTANT_NOTES.md](./IMPORTANT_NOTES.md) - Hướng dẫn đầy đủ về các vấn đề cần lưu ý và cách cập nhật.

## Cấu trúc

```
examples/
├── default_solutions/ # Default solutions sẵn có để chọn và sử dụng
│   ├── *.json        # Solution configuration files
│   ├── index.json    # Catalog danh sách solutions
│   └── *.sh          # Helper scripts
├── instances/         # Example files và scripts cho instances
│   ├── update/       # Examples để cập nhật instances
│   ├── tests/        # Test files
│   ├── infer_nodes/  # Inference nodes examples
│   └── example_*.json # Solution examples
└── solutions/         # Solution examples và tests
```

## Thư mục con

### 📝 `instances/`
Example files để làm việc với instances:
- `{solution}/{model_type}/` - Examples được tổ chức theo solution và model type
- `update/` - Examples để cập nhật instances
- `tests/` - Test files
- `infer_nodes/` - Inference nodes examples
- `example_*.json` - Solution examples ở root

Xem [instances/README.md](./instances/README.md) để biết chi tiết.

### 🔧 `solutions/`
Solution examples và tests:
- Solution configuration examples
- Test files cho solutions

Xem [solutions/README.md](./solutions/README.md) để biết chi tiết.

### ⭐ `default_solutions/`
**Default solutions sẵn có để người dùng chọn và sử dụng ngay:**
- Các solution đã được cấu hình sẵn theo category
- File `index.json` chứa catalog đầy đủ
- Helper scripts để list và create solutions
- Documentation chi tiết cho từng solution

**Cách sử dụng nhanh:**
```bash
# Xem danh sách solutions có sẵn
./default_solutions/list_solutions.sh

# Tạo một solution
./default_solutions/create_solution.sh default_face_detection_file
```

Xem [default_solutions/README.md](./default_solutions/README.md) để biết chi tiết.

## Quick Start

### 0. Sử dụng Default Solutions (Khuyến nghị cho người mới)

```bash
# Xem danh sách tất cả solutions (bao gồm cả default solutions chưa load)
curl http://localhost:8080/v1/core/solution

# Lấy example request body để tạo instance (sẽ tự động load default solution nếu chưa có)
curl http://localhost:8080/v1/core/solution/default_face_detection_file/instance-body

# Sau đó tạo instance với solution
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d '{
    "name": "my_face_detection",
    "solution": "default_face_detection_file",
    "additionalParams": {
      "FILE_PATH": "/path/to/video.mp4",
      "MODEL_PATH": "/path/to/model.onnx"
    }
  }'
```

### 1. Tạo Instance với Basic Solution

```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @instances/face_detection/onnx/test_rtsp_source.json
```

### 2. Tạo Instance với Solution Example

```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @instances/example_yolov11_detection.json
```

### 3. Update Instance

```bash
curl -X PUT http://localhost:8080/v1/core/instance/{instanceId} \
  -H "Content-Type: application/json" \
  -d @instances/update/update_change_rtsp_url.json
```

### 4. Kiểm tra Instance Status

```bash
# Check instance status
curl http://localhost:8080/v1/core/instance/{instanceId}

# Get instance statistics
curl http://localhost:8080/v1/core/instance/{instanceId}/statistics
```

## 📋 Manual Testing Guide - Hướng Dẫn Test Thủ Công

Hướng dẫn chi tiết để test tất cả các loại instances có sẵn trong project.

### 🔍 Kiểm Tra Trước Khi Test

**1. Kiểm tra API server đang chạy:**
```bash
curl http://localhost:8080/v1/core/health
# Hoặc
curl http://localhost:8080/v1/core/system/info
```

**2. Kiểm tra danh sách solutions có sẵn:**
```bash
curl http://localhost:8080/v1/core/solution
```

**3. Kiểm tra danh sách instances hiện tại:**
```bash
curl http://localhost:8080/v1/core/instance
```

### 🎯 Test Face Detection Instances

#### Test 1: Face Detection với File Source (Cơ bản)

**File test:** `instances/face_detection/test_file_source.json`

**Các bước:**

1. **Tạo instance:**
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @instances/face_detection/test_file_source.json
```

2. **Lưu instance ID từ response:**
```bash
# Response sẽ có dạng: {"id": "face_detection_file_test", ...}
INSTANCE_ID="face_detection_file_test"
```

3. **Kiểm tra status:**
```bash
curl http://localhost:8080/v1/core/instance/${INSTANCE_ID}
```

4. **Start instance:**
```bash
curl -X POST http://localhost:8080/v1/core/instance/${INSTANCE_ID}/start
```

5. **Kiểm tra statistics:**
```bash
curl http://localhost:8080/v1/core/instance/${INSTANCE_ID}/statistics
```

6. **Kiểm tra screen display:**
- Mở cửa sổ hiển thị video
- Xác nhận bounding boxes quanh khuôn mặt
- Kiểm tra track IDs và confidence scores

7. **Stop instance:**
```bash
curl -X POST http://localhost:8080/v1/core/instance/${INSTANCE_ID}/stop
```

#### Test 2: Face Detection với RTSP Source

**File test:** `instances/face_detection/test_rtsp_source.json`

**Yêu cầu:**
- RTSP camera hoặc RTSP stream server
- RTSP URL hợp lệ (ví dụ: `rtsp://server:8554/stream`)

**Các bước tương tự Test 1, nhưng:**
- Sử dụng file `test_rtsp_source.json`
- Đảm bảo RTSP URL trong file JSON đúng với server của bạn
- Kiểm tra network connectivity đến RTSP server

#### Test 3: Face Detection với RTMP Output

**File test:** `instances/face_detection/test_rtmp_output.json`

**Yêu cầu:**
- RTMP server (nginx-rtmp hoặc tương tự)
- RTMP URL hợp lệ

**Các bước:**

1. **Tạo instance:**
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @instances/face_detection/test_rtmp_output.json
```

2. **Start instance và kiểm tra RTMP stream:**
```bash
curl -X POST http://localhost:8080/v1/core/instance/${INSTANCE_ID}/start

# Kiểm tra RTMP stream bằng ffplay
ffplay rtmp://your-server:1935/live/stream_key
```

#### Test 4: Face Detection với MQTT Events

**File test:** `instances/face_detection/test_mqtt_events.json`

**Yêu cầu:**
- MQTT broker (mosquitto) đang chạy
- MQTT client để subscribe

**Các bước:**

1. **Khởi động MQTT broker (nếu chưa chạy):**
```bash
sudo systemctl start mosquitto
# Hoặc
mosquitto -v
```

2. **Subscribe MQTT topic trong terminal riêng:**
```bash
mosquitto_sub -h localhost -t face_detection/events -v
```

3. **Tạo và start instance:**
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @instances/face_detection/test_mqtt_events.json

curl -X POST http://localhost:8080/v1/core/instance/${INSTANCE_ID}/start
```

4. **Kiểm tra events trong terminal MQTT subscriber**

### 🚗 Test Behavior Analysis Crossline Instances

#### Test 1: BA Crossline với File Source + MQTT

**File test:** `instances/ba_crossline/test_file_source_mqtt.json`

**Các bước:**

1. **Tạo instance:**
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @instances/ba_crossline/test_file_source_mqtt.json
```

2. **Subscribe MQTT:**
```bash
mosquitto_sub -h localhost -t ba_crossline/events -v
```

3. **Start instance:**
```bash
curl -X POST http://localhost:8080/v1/core/instance/${INSTANCE_ID}/start
```

4. **Kiểm tra:**
- Screen display: Xác nhận line được vẽ, số lượng đếm được hiển thị
- MQTT events: Xác nhận events `crossline_enter` và `crossline_exit` được gửi

#### Test 2: BA Crossline với RTSP Source + RTMP + MQTT

**File test:** `instances/ba_crossline/test_rtsp_source_rtmp_mqtt.json`

**Yêu cầu:**
- RTSP camera/stream
- RTMP server
- MQTT broker

**Các bước tương tự Test 1, nhưng:**
- Kiểm tra RTMP stream: `ffplay rtmp://your-server:1935/live/stream_key`
- Đảm bảo RTSP URL và RTMP URL đúng

#### Test 3: BA Crossline với CrossingLines Format (Khuyến nghị)

**File test:** `instances/ba_crossline/example_ba_crossline_with_crossing_lines.json`

**Ưu điểm:** Hỗ trợ nhiều lines, quản lý qua API

**Các bước:**

1. **Tạo instance với CrossingLines:**
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @instances/ba_crossline/example_ba_crossline_with_crossing_lines.json
```

2. **Quản lý lines qua API:**
```bash
# Lấy tất cả lines
curl http://localhost:8080/v1/core/instance/${INSTANCE_ID}/lines

# Tạo line mới
curl -X POST http://localhost:8080/v1/core/instance/${INSTANCE_ID}/lines \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Entry Line",
    "coordinates": [{"x": 0, "y": 250}, {"x": 700, "y": 220}],
    "direction": "Up",
    "classes": ["Vehicle"],
    "color": [255, 0, 0, 255]
  }'

# Cập nhật line
curl -X PUT http://localhost:8080/v1/core/instance/${INSTANCE_ID}/lines/{lineId} \
  -H "Content-Type: application/json" \
  -d '{
    "coordinates": [{"x": 100, "y": 300}, {"x": 800, "y": 280}]
  }'
```

### 🎭 Test MaskRCNN Instances

#### Test 1: MaskRCNN Detection với File Source

**File test:** `instances/mask_rcnn/test_file_source.json`

**Các bước:**

1. **Tạo instance:**
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @instances/mask_rcnn/test_file_source.json
```

2. **Start instance:**
```bash
curl -X POST http://localhost:8080/v1/core/instance/${INSTANCE_ID}/start
```

3. **Kiểm tra:**
- Screen display: Xác nhận mask được vẽ trên từng đối tượng
- Labels hiển thị class name và confidence score
- Bounding boxes quanh đối tượng

4. **Kiểm tra statistics:**
```bash
curl http://localhost:8080/v1/core/instance/${INSTANCE_ID}/statistics
```

**Lưu ý:** MaskRCNN chậm hơn YOLO, FPS thấp hơn là bình thường.

#### Test 2: MaskRCNN với RTMP Output

**File test:** `instances/mask_rcnn/test_rtmp_output.json`

**Các bước tương tự Test 1, nhưng:**
- Kiểm tra RTMP stream: `ffplay rtmp://your-server:1935/live/mask_rcnn_stream`

### 🔧 Test Other Solutions

Các solution khác có trong `instances/other_solutions/`:

- **YOLOv11 Detection:** `example_yolov11_detection.json`
- **RKNN YOLOv11:** `example_rknn_yolov11_detection.json`
- **InsightFace Recognition:** `example_insightface_recognition.json`
- **TRT InsightFace:** `example_trt_insightface_recognition.json`
- **Face Swap:** `example_face_swap.json`
- **MLLM Analysis:** `example_mllm_analysis.json`
- **Full Config:** `example_full_config.json`

**Cách test tương tự:**
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @instances/other_solutions/example_yolov11_detection.json
```

### 📊 Workflow Test Hoàn Chỉnh

**Workflow test một instance từ đầu đến cuối:**

```bash
# 1. Tạo instance
RESPONSE=$(curl -s -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @instances/face_detection/test_file_source.json)

# 2. Lấy instance ID (cần jq hoặc parse JSON)
INSTANCE_ID=$(echo $RESPONSE | jq -r '.id')
echo "Instance ID: $INSTANCE_ID"

# 3. Kiểm tra status
curl http://localhost:8080/v1/core/instance/${INSTANCE_ID}

# 4. Start instance
curl -X POST http://localhost:8080/v1/core/instance/${INSTANCE_ID}/start

# 5. Đợi vài giây để instance chạy
sleep 5

# 6. Kiểm tra statistics
curl http://localhost:8080/v1/core/instance/${INSTANCE_ID}/statistics

# 7. Monitor instance (kiểm tra status định kỳ)
watch -n 2 "curl -s http://localhost:8080/v1/core/instance/${INSTANCE_ID} | jq '.status'"

# 8. Stop instance
curl -X POST http://localhost:8080/v1/core/instance/${INSTANCE_ID}/stop

# 9. Xóa instance (nếu cần)
curl -X DELETE http://localhost:8080/v1/core/instance/${INSTANCE_ID}
```

### 🔍 Troubleshooting Common Issues

#### Lỗi: Instance không start

**Kiểm tra:**
```bash
# 1. Kiểm tra logs
tail -f /opt/edge_ai_api/logs/edge_ai_api.log

# 2. Kiểm tra status chi tiết
curl http://localhost:8080/v1/core/instance/${INSTANCE_ID}

# 3. Kiểm tra system info
curl http://localhost:8080/v1/core/system/info
```

#### Lỗi: Model không tìm thấy

**Giải pháp:**
- Kiểm tra đường dẫn model trong JSON config
- Đảm bảo model files tồn tại và có quyền đọc
- Cập nhật đường dẫn cho phù hợp với môi trường

#### Lỗi: RTSP/RTMP connection failed

**Kiểm tra:**
```bash
# Test RTSP connection
ffplay rtsp://server:8554/stream

# Test RTMP server
ffmpeg -re -i test.mp4 -c copy -f flv rtmp://server:1935/live/test

# Kiểm tra firewall
sudo ufw status
sudo ufw allow 8554/tcp  # RTSP
sudo ufw allow 1935/tcp  # RTMP
```

#### Lỗi: MQTT connection failed

**Kiểm tra:**
```bash
# Kiểm tra MQTT broker
sudo systemctl status mosquitto

# Test connection
mosquitto_sub -h localhost -t test -v

# Kiểm tra credentials trong JSON config
```

#### Lỗi: Out of Memory

**Giải pháp:**
- Giảm input size (RESIZE_RATIO, INPUT_WIDTH/HEIGHT)
- Giảm batch_size
- Sử dụng GPU nếu có
- Tăng score_threshold để giảm số objects

### 📝 Checklist Test

**Trước khi test:**
- [ ] API server đang chạy
- [ ] Model files tồn tại và đường dẫn đúng
- [ ] Test video files có sẵn (nếu dùng file source)
- [ ] RTSP/RTMP servers đang chạy (nếu cần)
- [ ] MQTT broker đang chạy (nếu cần)
- [ ] Network connectivity OK

**Khi test:**
- [ ] Instance được tạo thành công
- [ ] Instance start không có lỗi
- [ ] Screen display hiển thị đúng (nếu có)
- [ ] Statistics có dữ liệu hợp lý
- [ ] RTMP stream hoạt động (nếu có)
- [ ] MQTT events được gửi (nếu có)
- [ ] Instance stop thành công

**Sau khi test:**
- [ ] Instance được cleanup (stop/delete)
- [ ] Logs được kiểm tra nếu có lỗi
- [ ] Kết quả được ghi lại

## Documentation

- **[Default Solutions](./default_solutions/README.md)** ⭐ - **Bắt đầu từ đây!** Danh sách solutions sẵn có
- [Instances Examples](./instances/README.md) - Hướng dẫn sử dụng instance examples
- [Solutions Reference](../docs/DEFAULT_SOLUTIONS_REFERENCE.md) - Documentation về các solutions
- [Instance Guide](../docs/INSTANCE_GUIDE.md) - Hướng dẫn tạo và cập nhật instances

## Lưu ý

1. **Model Paths**: Các đường dẫn model trong example files là ví dụ, cần cập nhật cho phù hợp với môi trường của bạn
2. **API Endpoint**: Mặc định là `http://localhost:8080`, cần cập nhật nếu khác

## Tài liệu tham khảo

- [API Documentation](../docs/INSTANCE_GUIDE.md)
- [Node Integration Plan](../develop_doc/NODE_INTEGRATION_PLAN.md)
- [Solution Registry](../src/solutions/solution_registry.cpp)
