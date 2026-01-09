# ONNX Inference Nodes

Thư mục này chứa các ví dụ sử dụng các inference nodes với ONNX models.

## Model Types

- **ONNX** (`.onnx`) - Open Neural Network Exchange format
- Hỗ trợ nhiều loại models: YOLOv11, InsightFace, Face Detection, etc.

## Các Ví dụ

Các ví dụ ONNX thường được tổ chức trong các solution cụ thể:
- Face Detection: `../face_detection/onnx/`
- YOLOv11 Detection: `../other_solutions/onnx/example_yolov11_detection.json`
- InsightFace Recognition: `../other_solutions/onnx/example_insightface_recognition.json`

## Tham số Model

- `MODEL_PATH`: Đường dẫn đến ONNX model file (`.onnx`)
- `LABELS_PATH`: Đường dẫn đến file labels (optional)
- `FACE_DETECTION_MODEL_PATH`: Đường dẫn đến face detection model (cho face recognition)
- `FACE_RECOGNITION_MODEL_PATH`: Đường dẫn đến face recognition model

## Yêu cầu

- ONNX Runtime hoặc OpenCV với DNN module hỗ trợ ONNX
- ONNX model files
