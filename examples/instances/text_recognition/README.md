# Text Recognition (OCR) Examples

Thư mục này chứa các ví dụ instances cho Text Recognition sử dụng PaddleOCR.

## Cấu trúc

```
text_recognition/
└── paddle/
    ├── example_ocr_file_rtmp.json
    └── example_ocr_image_src.json
```

## Examples

### 1. `example_ocr_file_rtmp.json`
- **Mô tả**: OCR với video file source và RTMP output
- **Input**: Video file
- **Output**: RTMP stream
- **Features**: Text detection và recognition trong video

### 2. `example_ocr_image_src.json`
- **Mô tả**: OCR với image source và screen output
- **Input**: Image files (sequence)
- **Output**: Screen display
- **Features**: Text detection và recognition trong images

## Cách sử dụng

1. Điều chỉnh các đường dẫn model trong JSON file
2. Tạo instance từ solution:

```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @examples/instances/text_recognition/paddle/example_ocr_file_rtmp.json
```

## Yêu cầu

- PaddleOCR detection model directory
- PaddleOCR recognition model directory
- PaddleOCR classification model directory (optional)
- CVEDIX_WITH_PADDLE enabled
- Font file (cho text overlay)

## PaddleOCR Model Structure

```
models/paddleocr/
├── det/          # Detection model
├── rec/          # Recognition model
└── cls/          # Classification model (optional)
```

