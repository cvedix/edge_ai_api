# Other Solutions - TensorRT Models

Thư mục này chứa các ví dụ sử dụng các solution khác với TensorRT models.

## Các Ví dụ

- `example_trt_insightface_recognition.json` - InsightFace recognition với TensorRT
- `example_full_config.json` - Full configuration example với TensorRT model

## Model Types

- **TensorRT Engine** (`.engine`, `.trt`) - Optimized models cho NVIDIA GPUs
- **InsightFace TensorRT** (`.trt`) - Face recognition model optimized cho TensorRT

## Tham số Model

### InsightFace TensorRT
- `FACE_DETECTION_MODEL_PATH`: Đường dẫn đến face detection model (ONNX)
- `FACE_RECOGNITION_MODEL_PATH`: Đường dẫn đến InsightFace TensorRT model (`.trt`)

### Full Config
- `MODEL_PATH`: Đường dẫn đến TensorRT model file

## Yêu cầu

- NVIDIA GPU với CUDA support
- TensorRT library đã được cài đặt
- CVEDIX_WITH_TRT flag được enable khi build
