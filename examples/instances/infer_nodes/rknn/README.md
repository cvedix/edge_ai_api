# RKNN Inference Nodes

Thư mục này chứa các ví dụ sử dụng các inference nodes với RKNN models.

## Model Types

- **RKNN** (`.rknn`) - Optimized models cho Rockchip NPU
- Hỗ trợ YOLOv8, YOLOv11, Face Detection trên RKNN platform

## Các Ví dụ

- `example_rknn_face_detector.json` - Face detection trên RKNN NPU
- `example_rknn_yolov8_detector.json` - YOLOv8 detection trên RKNN (nếu có)
- `example_rknn_yolov11_detector.json` - YOLOv11 detection trên RKNN (nếu có)

## Tham số Model

- `MODEL_PATH`: Đường dẫn đến RKNN model file (`.rknn`)
- `SCORE_THRESHOLD`: Ngưỡng score (default: 0.5)
- `NMS_THRESHOLD`: Ngưỡng NMS (default: 0.5)
- `INPUT_WIDTH`: Chiều rộng input (default: 640)
- `INPUT_HEIGHT`: Chiều cao input (default: 640)
- `NUM_CLASSES`: Số lượng classes (default: 80 cho COCO)

## Yêu cầu

- Rockchip NPU hardware
- RKNN toolkit đã được cài đặt
- CVEDIX_WITH_RKNN flag được enable khi build

