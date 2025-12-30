# Caffe Inference Nodes

Thư mục này chứa các ví dụ sử dụng các inference nodes với Caffe models.

## Model Types

- **Caffe** (`.caffemodel`, `.prototxt`) - Caffe models
- Hỗ trợ OpenPose và các model Caffe khác

## Các Ví dụ

- `example_openpose.json` - OpenPose pose estimation

## Tham số Model

- `MODEL_PATH`: Đường dẫn đến model file (`.caffemodel`)
- `MODEL_CONFIG_PATH`: Đường dẫn đến config file (`.prototxt`)
- `INPUT_WIDTH`: Chiều rộng input (default: 368)
- `INPUT_HEIGHT`: Chiều cao input (default: 368)
- `SCORE_THRESHOLD`: Ngưỡng score (default: 0.1)

## Yêu cầu

- OpenCV với DNN module hỗ trợ Caffe
- Caffe model files

