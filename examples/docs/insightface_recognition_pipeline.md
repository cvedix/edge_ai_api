# InsightFace Recognition Pipeline - Hướng dẫn Chi tiết

## Tổng quan

Solution `insightface_recognition` là một pipeline hoàn chỉnh để nhận diện khuôn mặt sử dụng InsightFace model, một trong những mô hình face recognition tốt nhất hiện nay.

### Use Case
- **Face Recognition**: Nhận diện khuôn mặt trong video stream
- **Access Control**: Kiểm soát ra vào bằng nhận diện khuôn mặt
- **Security Monitoring**: Giám sát an ninh với nhận diện tự động
- **Attendance System**: Hệ thống chấm công bằng nhận diện khuôn mặt

## Pipeline Architecture

```
[RTSP Source] → [YuNet Face Detector] → [InsightFace Recognition] → [File Destination]
```

### Luồng xử lý

1. **RTSP Source**: Nhận video stream từ RTSP camera
2. **YuNet Face Detector**: Phát hiện khuôn mặt trong từng frame
3. **InsightFace Recognition**: Nhận diện khuôn mặt và extract features
4. **File Destination**: Lưu kết quả recognition vào file với OSD overlay

## Cấu hình Instance

### File JSON: `example_insightface_recognition.json`

```json
{
  "name": "insightface_recognition_demo_1",
  "group": "demo",
  "solution": "insightface_recognition",
  "persistent": false,
  "autoStart": false,
  "detectionSensitivity": "Medium",
  "additionalParams": {
    "RTSP_URL": "rtsp://localhost:8554/stream",
    "FACE_DETECTION_MODEL_PATH": "/opt/cvedix/models/face/yunet.onnx",
    "FACE_RECOGNITION_MODEL_PATH": "/opt/cvedix/models/face/insightface.onnx",
    "SAVE_DIR": "/tmp/output/insightface_recognition"
  }
}
```

### Các tham số

| Tham số | Mô tả | Ví dụ |
|---------|-------|-------|
| `name` | Tên instance | `insightface_recognition_demo_1` |
| `group` | Nhóm instance | `demo` |
| `solution` | Solution ID | `insightface_recognition` |
| `RTSP_URL` | URL RTSP stream | `rtsp://server:port/stream` |
| `FACE_DETECTION_MODEL_PATH` | Đường dẫn YuNet model | `/path/to/yunet.onnx` |
| `FACE_RECOGNITION_MODEL_PATH` | Đường dẫn InsightFace model | `/path/to/insightface.onnx` |
| `SAVE_DIR` | Thư mục lưu kết quả | `/tmp/output/insightface_recognition` |

## Chi tiết các Nodes

### 1. RTSP Source Node (`rtsp_src`)

**Chức năng**: Nhận video stream từ RTSP source

**Tham số**:
- `rtsp_url`: URL RTSP stream
- `channel`: Channel ID (mặc định: 0)
- `resize_ratio`: Tỷ lệ resize (mặc định: 1.0)

### 2. YuNet Face Detector Node (`yunet_face_detector`)

**Chức năng**: Phát hiện khuôn mặt trong video frame

**Tham số**:
- `model_path`: Đường dẫn YuNet model
- `score_threshold`: Ngưỡng confidence (mặc định: 0.7)
- `nms_threshold`: Ngưỡng NMS (mặc định: 0.5)
- `top_k`: Số lượng khuôn mặt tối đa (mặc định: 50)

**Đầu ra**: 
- Bounding boxes của các khuôn mặt được phát hiện
- Landmarks (điểm mốc) của khuôn mặt

### 3. InsightFace Recognition Node (`insight_face_recognition`)

**Chức năng**: Nhận diện khuôn mặt và extract features

**Tham số**:
- `model_path`: Đường dẫn InsightFace model

**Quy trình**:
1. Nhận khuôn mặt đã được phát hiện từ YuNet
2. Extract face features/embeddings
3. So sánh với database (nếu có)
4. Trả về identity và confidence score

**Model Variants**:
- `insightface.onnx`: Model chuẩn
- `insightface.trt`: TensorRT optimized (nếu có TensorRT support)

**Features**:
- High accuracy face recognition
- Fast inference speed
- Support for large-scale face database
- Robust to lighting and pose variations

### 4. File Destination Node (`file_des`)

**Chức năng**: Lưu video với recognition results và OSD overlay

**Tham số**:
- `save_dir`: Thư mục lưu file
- `name_prefix`: Tiền tố tên file (mặc định: `insightface_recognition`)
- `osd`: Bật/tắt OSD overlay (mặc định: `true`)

**Output Format**:
- Video file với recognition boxes và labels
- Timestamp trong tên file
- Format: `{name_prefix}_{timestamp}.mp4`

## Cách sử dụng

### 1. Tạo Instance qua API

```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @examples/instances/example_insightface_recognition.json
```

### 2. Kiểm tra Instance Status

```bash
curl http://localhost:8080/v1/core/instances/{instanceId}
```

### 3. Start Instance

```bash
curl -X POST http://localhost:8080/v1/core/instances/{instanceId}/start
```

### 4. Xem Output Files

```bash
ls -lh /tmp/output/insightface_recognition/
```

## Tùy chỉnh Cấu hình

### Thay đổi Model

Để sử dụng model khác:

```json
{
  "additionalParams": {
    "FACE_RECOGNITION_MODEL_PATH": "/opt/cvedix/models/face/insightface_v2.onnx"
  }
}
```

### Điều chỉnh Face Detection Sensitivity

```json
{
  "detectionSensitivity": "High"  // Low, Medium, High
}
```

### Sử dụng TensorRT Model (nếu có TensorRT support)

```json
{
  "additionalParams": {
    "FACE_RECOGNITION_MODEL_PATH": "/opt/cvedix/models/face/insightface.trt"
  }
}
```

## Performance Tips

1. **Model Selection**:
   - Standard ONNX model: Tương thích tốt, dễ sử dụng
   - TensorRT model: Tốc độ cao hơn nhưng cần TensorRT support

2. **Face Detection**:
   - Sử dụng YuNet model mới nhất
   - Điều chỉnh `score_threshold` phù hợp với use case

3. **Processing Speed**:
   - Giảm `resize_ratio` để tăng tốc độ
   - Sử dụng GPU nếu có thể
   - Giảm số lượng khuôn mặt tối đa (`top_k`)

## Troubleshooting

### 1. Face không được phát hiện

**Triệu chứng**: Không có recognition xảy ra

**Giải pháp**:
- Kiểm tra chất lượng video input
- Giảm `score_threshold` trong YuNet detector
- Kiểm tra ánh sáng trong video
- Tăng kích thước khuôn mặt tối thiểu

### 2. Recognition không chính xác

**Triệu chứng**: Nhận diện sai hoặc không nhận diện được

**Giải pháp**:
- Kiểm tra chất lượng model
- Đảm bảo khuôn mặt rõ ràng trong video
- Kiểm tra database face (nếu có)
- Sử dụng model lớn hơn hoặc tốt hơn

### 3. Model không load được

**Triệu chứng**: "Failed to load model"

**Giải pháp**:
- Kiểm tra đường dẫn model file
- Verify file tồn tại và có quyền đọc
- Kiểm tra format file (phải là ONNX hoặc TRT)

## Kết quả và Output

### Output Files

Pipeline sẽ lưu các file video với:
- Recognition boxes và labels
- Identity information (nếu có)
- Timestamp trong tên file
- OSD overlay với thông tin recognition

### Log Output

Pipeline sẽ log các thông tin:
- Model loading status
- Recognition results
- Processing FPS
- Error messages (nếu có)

## Tài liệu tham khảo

- [InsightFace Documentation](https://github.com/deepinsight/insightface)
- [YuNet Face Detector](https://github.com/opencv/opencv_zoo)
- [Solution Registry](../../src/solutions/solution_registry.cpp)
- [Pipeline Builder](../../src/core/pipeline_builder.cpp)
- [API Documentation](../../docs/INSTANCE_GUIDE.md)

