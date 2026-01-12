# Video Enhancement Examples

Thư mục này chứa các ví dụ instances cho Video Enhancement/Restoration.

## Cấu trúc

```
video_enhancement/
└── onnx/
    ├── example_restoration_file_output.json
    └── example_restoration_rtmp.json
```

## Examples

### 1. `example_restoration_file_output.json`
- **Mô tả**: Video Enhancement với file output
- **Input**: Video file (low quality)
- **Output**: Enhanced video file
- **Features**: Super-resolution, denoising

### 2. `example_restoration_rtmp.json`
- **Mô tả**: Video Enhancement với RTMP streaming
- **Input**: Video file (low quality)
- **Output**: RTMP stream (enhanced)
- **Features**: Real-time enhancement và streaming

## Cách sử dụng

1. Điều chỉnh đường dẫn model và video trong JSON file
2. Tạo instance từ solution:

```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @examples/instances/video_enhancement/onnx/example_restoration_file_output.json
```

## Yêu cầu

- Restoration model (.onnx)
  - Background restoration model (required)
  - Face restoration model (optional)
- Video file hoặc RTSP stream
- RTMP server (nếu dùng RTMP output)

## Model Options

- **Background Restoration**: Super-resolution cho toàn bộ frame
- **Face Restoration**: Super-resolution riêng cho faces (optional)
- **Restoration to OSD**: Overlay enhanced frame lên original (optional)

