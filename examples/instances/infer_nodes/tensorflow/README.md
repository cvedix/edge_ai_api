# TensorFlow Inference Nodes

Thư mục này chứa các ví dụ sử dụng các inference nodes với TensorFlow models.

## Model Types

- **TensorFlow Frozen Graph** (`.pb`, `.pbtxt`) - TensorFlow models
- Hỗ trợ Mask R-CNN và các model TensorFlow khác

## Các Ví dụ

- `example_mask_rcnn.json` - Mask R-CNN instance segmentation

## Tham số Model

- `MODEL_PATH`: Đường dẫn đến model file (`.pb`)
- `MODEL_CONFIG_PATH`: Đường dẫn đến config file (`.pbtxt`)
- `LABELS_PATH`: Đường dẫn đến file labels (`.txt`)
- `INPUT_WIDTH`: Chiều rộng input (default: 416)
- `INPUT_HEIGHT`: Chiều cao input (default: 416)
- `SCORE_THRESHOLD`: Ngưỡng score (default: 0.5)

## Yêu cầu

- TensorFlow library
- Model files ở định dạng frozen graph
