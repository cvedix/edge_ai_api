# YOLOv11 Detection Pipeline - Hướng dẫn Chi tiết

## Tổng quan

Solution `yolov11_detection` là một pipeline hoàn chỉnh để phát hiện đối tượng sử dụng YOLOv11 model, một trong những mô hình object detection mới nhất và hiệu quả nhất.

### Use Case
- **Object Detection**: Phát hiện các đối tượng trong video stream
- **Real-time Monitoring**: Giám sát real-time qua RTSP stream
- **Video Analysis**: Phân tích video và lưu kết quả
- **Security**: Giám sát an ninh với phát hiện đối tượng tự động

## Pipeline Architecture

```
[RTSP Source] → [YOLOv11 Detector] → [File Destination]
```

### Luồng xử lý

1. **RTSP Source**: Nhận video stream từ RTSP camera hoặc stream server
2. **YOLOv11 Detector**: Phát hiện đối tượng trong từng frame sử dụng YOLOv11 model
3. **File Destination**: Lưu kết quả detection vào file với OSD overlay

## Cấu hình Instance

### File JSON: `example_yolov11_detection.json`

```json
{
  "name": "yolov11_detection_demo_1",
  "group": "demo",
  "solution": "yolov11_detection",
  "persistent": false,
  "autoStart": false,
  "detectionSensitivity": "Medium",
  "additionalParams": {
    "RTSP_URL": "rtsp://localhost:8554/stream",
    "MODEL_PATH": "/opt/cvedix/models/yolov11/yolov11n.onnx",
    "SAVE_DIR": "/tmp/output/yolov11_detection"
  }
}
```

### Các tham số

| Tham số | Mô tả | Ví dụ |
|---------|-------|-------|
| `name` | Tên instance | `yolov11_detection_demo_1` |
| `group` | Nhóm instance | `demo` |
| `solution` | Solution ID | `yolov11_detection` |
| `autoStart` | Tự động start khi tạo | `false` |
| `detectionSensitivity` | Độ nhạy phát hiện | `Low`, `Medium`, `High` |
| `RTSP_URL` | URL RTSP stream | `rtsp://server:port/stream` |
| `MODEL_PATH` | Đường dẫn model YOLOv11 | `/path/to/yolov11n.onnx` |
| `SAVE_DIR` | Thư mục lưu kết quả | `/tmp/output/yolov11_detection` |

## Chi tiết các Nodes

### 1. RTSP Source Node (`rtsp_src`)

**Chức năng**: Nhận video stream từ RTSP source

**Tham số**:
- `rtsp_url`: URL RTSP stream (từ `RTSP_URL`)
- `channel`: Channel ID (mặc định: 0)
- `resize_ratio`: Tỷ lệ resize (mặc định: 1.0 - không resize)

**Hỗ trợ**:
- RTSP protocol (rtsp://)
- H.264/H.265 codec
- TCP/UDP transport

### 2. YOLOv11 Detector Node (`yolov11_detector`)

**Chức năng**: Phát hiện đối tượng sử dụng YOLOv11 model

**Tham số**:
- `model_path`: Đường dẫn file model ONNX (từ `MODEL_PATH`)

**Đầu ra**: 
- Bounding boxes với class labels
- Confidence scores
- Object classes (COCO dataset: 80 classes)

**Model Variants**:
- `yolov11n.onnx`: Nano - nhỏ nhất, nhanh nhất
- `yolov11s.onnx`: Small - cân bằng
- `yolov11m.onnx`: Medium - độ chính xác tốt
- `yolov11l.onnx`: Large - độ chính xác cao
- `yolov11x.onnx`: XLarge - độ chính xác cao nhất

**Classes được phát hiện**:
- Person, bicycle, car, motorcycle, airplane, bus, train, truck
- Traffic light, fire hydrant, stop sign, parking meter, bench
- Bird, cat, dog, horse, sheep, cow, elephant, bear, zebra, giraffe
- Và nhiều classes khác (tổng cộng 80 classes)

### 3. File Destination Node (`file_des`)

**Chức năng**: Lưu video với detection results và OSD overlay

**Tham số**:
- `save_dir`: Thư mục lưu file (từ `SAVE_DIR`)
- `name_prefix`: Tiền tố tên file (mặc định: `yolov11_detection`)
- `osd`: Bật/tắt OSD overlay (mặc định: `true`)

**Output Format**:
- Video file với detection boxes và labels
- Timestamp trong tên file
- Format: `{name_prefix}_{timestamp}.mp4`

## Cách sử dụng

### 1. Tạo Instance qua API

```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @examples/instances/example_yolov11_detection.json
```

### 2. Kiểm tra Instance Status

```bash
curl http://localhost:8080/v1/core/instances/{instanceId}
```

### 3. Start Instance

```bash
curl -X POST http://localhost:8080/v1/core/instances/{instanceId}/start
```

### 4. Stop Instance

```bash
curl -X POST http://localhost:8080/v1/core/instances/{instanceId}/stop
```

### 5. Xem Output Files

```bash
ls -lh /tmp/output/yolov11_detection/
```

## Tùy chỉnh Cấu hình

### Thay đổi Model

Để sử dụng model YOLOv11 khác:

```json
{
  "additionalParams": {
    "MODEL_PATH": "/opt/cvedix/models/yolov11/yolov11s.onnx"
  }
}
```

### Thay đổi Detection Sensitivity

```json
{
  "detectionSensitivity": "High"  // Low, Medium, High
}
```

### Thay đổi Resize Ratio

Để tăng tốc độ xử lý, có thể resize video:

Trong solution config, thay đổi `resize_ratio` từ `1.0` sang `0.5` hoặc `0.75`.

## Performance Tips

1. **Model Selection**:
   - `yolov11n`: Tốc độ cao nhất, phù hợp cho edge devices
   - `yolov11s`: Cân bằng tốc độ và độ chính xác
   - `yolov11m/l/x`: Độ chính xác cao hơn nhưng chậm hơn

2. **Resize Ratio**:
   - Giảm `resize_ratio` để tăng tốc độ xử lý
   - Tăng `resize_ratio` để tăng độ chính xác

3. **Hardware Acceleration**:
   - Sử dụng GPU nếu có thể
   - Model ONNX hỗ trợ CUDA/OpenVINO

## Troubleshooting

### 1. Model không load được

**Triệu chứng**: "Failed to load model" hoặc "Model file not found"

**Giải pháp**:
- Kiểm tra đường dẫn model file
- Verify file tồn tại và có quyền đọc
- Kiểm tra format file (phải là ONNX)

### 2. RTSP Stream không kết nối được

**Triệu chứng**: "Failed to connect to RTSP stream"

**Giải pháp**:
- Kiểm tra RTSP URL có đúng không
- Verify RTSP server đang chạy
- Kiểm tra network connectivity
- Kiểm tra firewall rules

### 3. Detection không chính xác

**Triệu chứng**: Bỏ sót đối tượng hoặc false positives

**Giải pháp**:
- Thử model lớn hơn (yolov11s, yolov11m)
- Tăng `detectionSensitivity`
- Kiểm tra chất lượng video input
- Đảm bảo không resize quá nhiều

## Kết quả và Output

### Output Files

Pipeline sẽ lưu các file video với:
- Detection boxes và labels
- Timestamp trong tên file
- OSD overlay với thông tin detection

### Log Output

Pipeline sẽ log các thông tin:
- Model loading status
- Detection results
- Processing FPS
- Error messages (nếu có)

## Tài liệu tham khảo

- [YOLOv11 Documentation](https://github.com/ultralytics/ultralytics)
- [Solution Registry](../../src/solutions/solution_registry.cpp)
- [Pipeline Builder](../../src/core/pipeline_builder.cpp)
- [API Documentation](../../docs/INSTANCE_GUIDE.md)

