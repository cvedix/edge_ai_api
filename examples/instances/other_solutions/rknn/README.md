# Other Solutions - RKNN Models

Thư mục này chứa các ví dụ sử dụng các solution khác với RKNN models.

## Các Ví dụ

- `example_rknn_yolov11_detection.json` - YOLOv11 detection với RKNN

## Model Types

- **RKNN** (`.rknn`) - Optimized models cho Rockchip NPU
- **YOLOv11 RKNN** (`.rknn`) - YOLOv11 detection model optimized cho RKNN

## Tham số Model

- `MODEL_PATH`: Đường dẫn đến RKNN model file (`.rknn`)
- `SAVE_DIR`: Thư mục lưu output (optional)

## Yêu cầu

- Rockchip NPU hardware
- RKNN toolkit đã được cài đặt
- CVEDIX_WITH_RKNN flag được enable khi build
