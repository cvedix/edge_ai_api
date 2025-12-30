# YOLO Inference Nodes

Thư mục này chứa các ví dụ sử dụng YOLO detector với OpenCV DNN.

## Model Types

- **YOLO** (`.weights`, `.cfg`) - YOLO models sử dụng OpenCV DNN backend
- Hỗ trợ YOLOv3, YOLOv4, YOLOv5, etc.

## Các Ví dụ

- `example_yolo_detector.json` - YOLO object detection cơ bản

## Tham số Model

- `MODEL_PATH`: Đường dẫn đến YOLO weights file (`.weights`)
- `MODEL_CONFIG_PATH`: Đường dẫn đến YOLO config file (`.cfg`)
- `LABELS_PATH`: Đường dẫn đến file labels (`.txt`)
- `INPUT_WIDTH`: Chiều rộng input (default: 416)
- `INPUT_HEIGHT`: Chiều cao input (default: 416)
- `SCORE_THRESHOLD`: Ngưỡng score (default: 0.5)
- `CONFIDENCE_THRESHOLD`: Ngưỡng confidence (default: 0.5)
- `NMS_THRESHOLD`: Ngưỡng NMS (default: 0.5)

## Yêu cầu

- OpenCV với DNN module
- YOLO weights và config files

