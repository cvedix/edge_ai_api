# TensorRT Inference Nodes

Thư mục này chứa các ví dụ sử dụng các inference nodes với TensorRT models.

## Model Types

- **TensorRT Engine** (`.engine`, `.trt`) - Optimized models cho NVIDIA GPUs
- Hỗ trợ YOLOv8, Vehicle Detection, Vehicle Plate Detection, InsightFace, etc.

## Các Ví dụ

### YOLOv8 Models
- `example_trt_yolov8_detector.json` - YOLOv8 object detection
- `example_trt_yolov8_segmentation.json` - YOLOv8 instance segmentation
- `example_trt_yolov8_pose.json` - YOLOv8 pose estimation

### Vehicle Models
- `example_trt_vehicle_detector.json` - Vehicle detection
- `example_trt_vehicle_plate_detector.json` - Vehicle plate detection và recognition

## Tham số Model

- `MODEL_PATH`: Đường dẫn đến TensorRT engine file (`.engine` hoặc `.trt`)
- `LABELS_PATH`: Đường dẫn đến file labels (optional)
- `FACE_DETECTION_MODEL_PATH`: Đường dẫn đến face detection model (cho InsightFace)
- `FACE_RECOGNITION_MODEL_PATH`: Đường dẫn đến face recognition model (cho InsightFace)

## Yêu cầu

- NVIDIA GPU với CUDA support
- TensorRT library đã được cài đặt
- CVEDIX_WITH_TRT flag được enable khi build

