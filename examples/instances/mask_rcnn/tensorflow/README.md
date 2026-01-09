# Mask R-CNN Detection - TensorFlow Models

Thư mục này chứa các ví dụ sử dụng Mask R-CNN với các model TensorFlow.

## Model Types

- **Mask R-CNN** (`.pb`, `.pbtxt`) - Model instance segmentation từ TensorFlow
- Sử dụng TensorFlow frozen graph format

## Các Ví dụ

### Input Sources
- `test_file_source.json` - Sử dụng file video làm nguồn input

### Output Types
- `test_rtmp_output.json` - Output qua RTMP stream
- `example_mask_rcnn_rtmp.json` - Ví dụ đầy đủ với RTMP output

## Tham số Model

- `MODEL_PATH`: Đường dẫn đến model file (`.pb`)
- `MODEL_CONFIG_PATH`: Đường dẫn đến config file (`.pbtxt`)
- `LABELS_PATH`: Đường dẫn đến file labels (`.txt`)
- `INPUT_WIDTH`: Chiều rộng input (default: 416)
- `INPUT_HEIGHT`: Chiều cao input (default: 416)
- `SCORE_THRESHOLD`: Ngưỡng score (default: 0.5)
- `RESIZE_RATIO`: Tỷ lệ resize frame (0.0 - 1.0)
