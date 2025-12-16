# New Solutions Overview - Tổng quan các Solutions Mới

## Giới thiệu

Tài liệu này cung cấp tổng quan về các solutions mới đã được tích hợp vào hệ thống Edge AI API, bao gồm các node mới từ CVEDIX SDK.

## Danh sách Solutions Mới

### 1. YOLOv11 Detection (`yolov11_detection`)

**Mô tả**: Object detection sử dụng YOLOv11 model - một trong những mô hình detection mới nhất và hiệu quả nhất.

**Pipeline**: RTSP Source → YOLOv11 Detector → File Destination

**Use Cases**:
- Object detection trong video stream
- Real-time monitoring
- Security surveillance
- Traffic analysis

**Documentation**: [yolov11_detection_pipeline.md](./yolov11_detection_pipeline.md)

**Example**: [example_yolov11_detection.json](../instances/example_yolov11_detection.json)

---

### 2. Face Swap (`face_swap`)

**Mô tả**: Thay thế khuôn mặt trong video stream real-time sử dụng InsightFace và các model face processing.

**Pipeline**: RTSP Source → YuNet Face Detector → Face Swap → RTMP Destination

**Use Cases**:
- Face replacement trong video
- Privacy protection
- Entertainment effects
- Content creation

**Documentation**: [face_swap_pipeline.md](./face_swap_pipeline.md)

**Example**: [example_face_swap.json](../instances/example_face_swap.json)

---

### 3. InsightFace Recognition (`insightface_recognition`)

**Mô tả**: Nhận diện khuôn mặt sử dụng InsightFace model - một trong những mô hình face recognition tốt nhất.

**Pipeline**: RTSP Source → YuNet Face Detector → InsightFace Recognition → File Destination

**Use Cases**:
- Face recognition trong video stream
- Access control
- Security monitoring
- Attendance system

**Documentation**: [insightface_recognition_pipeline.md](./insightface_recognition_pipeline.md)

**Example**: [example_insightface_recognition.json](../instances/example_insightface_recognition.json)

---

### 4. MLLM Analysis (`mllm_analysis`)

**Mô tả**: Phân tích video sử dụng Multimodal Large Language Model (MLLM), cho phép mô tả và phân tích nội dung video bằng ngôn ngữ tự nhiên.

**Pipeline**: RTSP Source → MLLM Analyser → JSON Console Broker

**Use Cases**:
- Video content analysis
- Automated reporting
- Intelligent monitoring
- Content understanding

**Documentation**: [mllm_analysis_pipeline.md](./mllm_analysis_pipeline.md)

**Example**: [example_mllm_analysis.json](../instances/example_mllm_analysis.json)

---

### 5. RKNN YOLOv11 Detection (`rknn_yolov11_detection`) - Conditional

**Mô tả**: Object detection sử dụng YOLOv11 model được tối ưu cho Rockchip NPU (RKNN).

**Pipeline**: RTSP Source → RKNN YOLOv11 Detector → File Destination

**Use Cases**:
- Object detection trên Rockchip devices
- Edge AI applications
- Low-power inference

**Requirements**: CVEDIX_WITH_RKNN compilation flag

**Example**: [example_rknn_yolov11_detection.json](../instances/example_rknn_yolov11_detection.json)

---

### 6. TensorRT InsightFace Recognition (`trt_insightface_recognition`) - Conditional

**Mô tả**: Face recognition sử dụng InsightFace model được tối ưu cho NVIDIA TensorRT.

**Pipeline**: RTSP Source → YuNet Face Detector → TensorRT InsightFace Recognition → File Destination

**Use Cases**:
- High-performance face recognition
- GPU-accelerated inference
- Real-time processing

**Requirements**: CVEDIX_WITH_TRT compilation flag

**Example**: [example_trt_insightface_recognition.json](../instances/example_trt_insightface_recognition.json)

---

## So sánh các Solutions

| Solution | Input | Output | Performance | Use Case |
|----------|-------|--------|-------------|----------|
| YOLOv11 Detection | RTSP | File | High | Object detection |
| Face Swap | RTSP | RTMP | Medium | Face replacement |
| InsightFace Recognition | RTSP | File | High | Face recognition |
| MLLM Analysis | RTSP | JSON | Low-Medium | Content analysis |
| RKNN YOLOv11 | RTSP | File | Very High (NPU) | Edge detection |
| TRT InsightFace | RTSP | File | Very High (GPU) | GPU recognition |

## Cách sử dụng

### 1. Kiểm tra Solutions có sẵn

```bash
curl http://localhost:8080/v1/core/solutions
```

### 2. Tạo Instance với Solution

```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @examples/instances/example_{solution_id}.json
```

### 3. Start Instance

```bash
curl -X POST http://localhost:8080/v1/core/instances/{instanceId}/start
```

## Requirements

### Model Files

Các solutions mới yêu cầu các model files tương ứng:

- **YOLOv11**: `/opt/cvedix/models/yolov11/yolov11n.onnx`
- **Face Swap**: 
  - `/opt/cvedix/models/face/yunet.onnx`
  - `/opt/cvedix/models/face/buffalo_l.onnx`
  - `/opt/cvedix/models/face/emap.onnx`
  - `/opt/cvedix/models/face/face_swap.onnx`
- **InsightFace**: `/opt/cvedix/models/face/insightface.onnx`
- **MLLM**: Cần Ollama server hoặc API service

### Compilation Flags

Một số solutions yêu cầu compilation flags:

- `CVEDIX_WITH_RKNN`: Cho RKNN YOLOv11 Detection
- `CVEDIX_WITH_TRT`: Cho TensorRT InsightFace Recognition
- `CVEDIX_WITH_LLM`: Cho MLLM Analysis

## Migration từ Solutions Cũ

### Từ Object Detection sang YOLOv11 Detection

**Trước**:
```json
{
  "solution": "object_detection"
}
```

**Sau**:
```json
{
  "solution": "yolov11_detection",
  "additionalParams": {
    "MODEL_PATH": "/opt/cvedix/models/yolov11/yolov11n.onnx"
  }
}
```

### Từ Face Detection sang InsightFace Recognition

**Trước**:
```json
{
  "solution": "face_detection"
}
```

**Sau**:
```json
{
  "solution": "insightface_recognition",
  "additionalParams": {
    "FACE_RECOGNITION_MODEL_PATH": "/opt/cvedix/models/face/insightface.onnx"
  }
}
```

## Best Practices

1. **Model Selection**: Chọn model phù hợp với use case và hardware
2. **Performance Tuning**: Điều chỉnh resize_ratio và detectionSensitivity
3. **Resource Management**: Monitor CPU/GPU/Memory usage
4. **Error Handling**: Implement proper error handling và retry logic

## Troubleshooting

Xem documentation chi tiết của từng solution:
- [YOLOv11 Detection](./yolov11_detection_pipeline.md)
- [Face Swap](./face_swap_pipeline.md)
- [InsightFace Recognition](./insightface_recognition_pipeline.md)
- [MLLM Analysis](./mllm_analysis_pipeline.md)

## Tài liệu tham khảo

- [NODE_INTEGRATION_PLAN.md](../../develop_doc/NODE_INTEGRATION_PLAN.md)
- [Solution Registry](../../src/solutions/solution_registry.cpp)
- [Pipeline Builder](../../src/core/pipeline_builder.cpp)
- [API Documentation](../../docs/INSTANCE_GUIDE.md)

