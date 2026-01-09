# Other Solutions - ONNX Models

Thư mục này chứa các ví dụ sử dụng các solution khác với ONNX models.

## Các Ví dụ

- `example_yolov11_detection.json` - YOLOv11 object detection với ONNX
- `example_insightface_recognition.json` - InsightFace face recognition với ONNX
- `example_face_swap.json` - Face swap với các model ONNX

## Model Types

- **YOLOv11** (`.onnx`) - YOLOv11 detection model
- **InsightFace** (`.onnx`) - Face recognition model
- **YuNet** (`.onnx`) - Face detection model
- **Buffalo L** (`.onnx`) - Face encoding model
- **Face Swap** (`.onnx`) - Face swap model

## Tham số Model

### YOLOv11
- `MODEL_PATH`: Đường dẫn đến YOLOv11 ONNX model

### InsightFace
- `FACE_DETECTION_MODEL_PATH`: Đường dẫn đến YuNet face detection model
- `FACE_RECOGNITION_MODEL_PATH`: Đường dẫn đến InsightFace recognition model

### Face Swap
- `FACE_DETECTION_MODEL_PATH`: Đường dẫn đến YuNet face detection model
- `BUFFALO_L_FACE_ENCODING_MODEL`: Đường dẫn đến Buffalo L encoding model
- `EMAP_FILE_FOR_EMBEDDINGS`: Đường dẫn đến emap file
- `FACE_SWAP_MODEL_PATH`: Đường dẫn đến face swap model
- `SWAP_SOURCE_IMAGE`: Đường dẫn đến source face image
