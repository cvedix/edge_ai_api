# Face Swap Pipeline - Hướng dẫn Chi tiết

## Tổng quan

Solution `face_swap` là một pipeline hoàn chỉnh để thay thế khuôn mặt trong video stream real-time sử dụng InsightFace và các model face processing.

### Use Case
- **Face Replacement**: Thay thế khuôn mặt trong video stream
- **Privacy Protection**: Bảo vệ quyền riêng tư bằng cách thay thế khuôn mặt
- **Entertainment**: Tạo hiệu ứng giải trí với face swap
- **Content Creation**: Tạo nội dung video với face swap

## Pipeline Architecture

```
[RTSP Source] → [YuNet Face Detector] → [Face Swap] → [RTMP Destination]
```

### Luồng xử lý

1. **RTSP Source**: Nhận video stream từ RTSP camera
2. **YuNet Face Detector**: Phát hiện khuôn mặt trong từng frame
3. **Face Swap**: Thay thế khuôn mặt được phát hiện bằng khuôn mặt từ source image
4. **RTMP Destination**: Stream kết quả qua RTMP

## Cấu hình Instance

### File JSON: `example_face_swap.json`

```json
{
  "name": "face_swap_demo_1",
  "group": "demo",
  "solution": "face_swap",
  "persistent": false,
  "autoStart": false,
  "detectionSensitivity": "Medium",
  "additionalParams": {
    "RTSP_URL": "rtsp://localhost:8554/stream",
    "FACE_DETECTION_MODEL_PATH": "/opt/cvedix/models/face/yunet.onnx",
    "BUFFALO_L_FACE_ENCODING_MODEL": "/opt/cvedix/models/face/buffalo_l.onnx",
    "EMAP_FILE_FOR_EMBEDDINGS": "/opt/cvedix/models/face/emap.onnx",
    "FACE_SWAP_MODEL_PATH": "/opt/cvedix/models/face/face_swap.onnx",
    "SWAP_SOURCE_IMAGE": "/opt/cvedix/models/face/source_face.jpg",
    "RTMP_URL": "rtmp://localhost:1935/live/face_swap_stream"
  }
}
```

### Các tham số

| Tham số | Mô tả | Ví dụ |
|---------|-------|-------|
| `name` | Tên instance | `face_swap_demo_1` |
| `group` | Nhóm instance | `demo` |
| `solution` | Solution ID | `face_swap` |
| `RTSP_URL` | URL RTSP stream | `rtsp://server:port/stream` |
| `FACE_DETECTION_MODEL_PATH` | Đường dẫn YuNet model | `/path/to/yunet.onnx` |
| `BUFFALO_L_FACE_ENCODING_MODEL` | Đường dẫn Buffalo L model | `/path/to/buffalo_l.onnx` |
| `EMAP_FILE_FOR_EMBEDDINGS` | Đường dẫn EMap file | `/path/to/emap.onnx` |
| `FACE_SWAP_MODEL_PATH` | Đường dẫn Face Swap model | `/path/to/face_swap.onnx` |
| `SWAP_SOURCE_IMAGE` | Đường dẫn ảnh khuôn mặt nguồn | `/path/to/source_face.jpg` |
| `RTMP_URL` | URL RTMP server | `rtmp://server:port/live/stream` |

## Chi tiết các Nodes

### 1. RTSP Source Node (`rtsp_src`)

**Chức năng**: Nhận video stream từ RTSP source

**Tham số**:
- `rtsp_url`: URL RTSP stream
- `channel`: Channel ID (mặc định: 0)
- `resize_ratio`: Tỷ lệ resize (mặc định: 1.0)

### 2. YuNet Face Detector Node (`yunet_face_detector`)

**Chức năng**: Phát hiện khuôn mặt trong video frame

**Tham số**:
- `model_path`: Đường dẫn YuNet model
- `score_threshold`: Ngưỡng confidence (mặc định: 0.7)
- `nms_threshold`: Ngưỡng NMS (mặc định: 0.5)
- `top_k`: Số lượng khuôn mặt tối đa (mặc định: 50)

**Đầu ra**: 
- Bounding boxes của các khuôn mặt được phát hiện
- Landmarks (điểm mốc) của khuôn mặt

### 3. Face Swap Node (`face_swap`)

**Chức năng**: Thay thế khuôn mặt được phát hiện bằng khuôn mặt từ source image

**Tham số**:
- `yunet_face_detect_model`: Model phát hiện khuôn mặt
- `buffalo_l_face_encoding_model`: Model encoding khuôn mặt
- `emap_file_for_embeddings`: File EMap cho embeddings
- `insightface_swap_model`: Model face swap
- `swap_source_image`: Ảnh khuôn mặt nguồn
- `swap_source_face_index`: Index khuôn mặt trong ảnh (mặc định: 0)
- `min_face_w_h`: Kích thước khuôn mặt tối thiểu (mặc định: 50)
- `swap_on_osd`: Bật/tắt swap trên OSD (mặc định: true)
- `act_as_primary_detector`: Hoạt động như detector chính (mặc định: false)

**Quy trình**:
1. Phát hiện khuôn mặt trong frame
2. Extract features từ khuôn mặt nguồn
3. Thay thế khuôn mặt trong frame bằng khuôn mặt nguồn
4. Blend và smooth để tự nhiên

### 4. RTMP Destination Node (`rtmp_des`)

**Chức năng**: Stream video qua RTMP

**Tham số**:
- `rtmp_url`: URL RTMP server
- `channel`: Channel ID (mặc định: 0)

## Cách sử dụng

### 1. Chuẩn bị Source Image

Chuẩn bị ảnh khuôn mặt nguồn:
- Format: JPG, PNG
- Chất lượng tốt, khuôn mặt rõ ràng
- Kích thước phù hợp (khuyến nghị: 512x512 hoặc lớn hơn)

### 2. Tạo Instance qua API

```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @examples/instances/example_face_swap.json
```

### 3. Start Instance

```bash
curl -X POST http://localhost:8080/v1/core/instances/{instanceId}/start
```

### 4. Xem RTMP Stream

Sử dụng VLC hoặc player hỗ trợ RTMP:
```
rtmp://localhost:1935/live/face_swap_stream_0
```

## Tùy chỉnh Cấu hình

### Thay đổi Source Face Image

```json
{
  "additionalParams": {
    "SWAP_SOURCE_IMAGE": "/path/to/new_source_face.jpg",
    "SWAP_SOURCE_FACE_INDEX": "0"
  }
}
```

### Điều chỉnh Face Detection Sensitivity

```json
{
  "detectionSensitivity": "High"  // Low, Medium, High
}
```

### Thay đổi Minimum Face Size

Trong solution config, thay đổi `min_face_w_h`:
- Giá trị nhỏ hơn: Phát hiện khuôn mặt nhỏ hơn nhưng có thể có nhiều false positives
- Giá trị lớn hơn: Chỉ phát hiện khuôn mặt lớn, giảm false positives

## Performance Tips

1. **Model Selection**:
   - Sử dụng YuNet model mới nhất để có độ chính xác cao
   - Buffalo L model cho encoding tốt nhất

2. **Source Image Quality**:
   - Sử dụng ảnh chất lượng cao
   - Khuôn mặt rõ ràng, không bị che khuất
   - Ánh sáng tốt

3. **Processing Speed**:
   - Giảm `resize_ratio` để tăng tốc độ
   - Sử dụng GPU nếu có thể

## Troubleshooting

### 1. Face không được phát hiện

**Triệu chứng**: Không có face swap xảy ra

**Giải pháp**:
- Kiểm tra chất lượng video input
- Giảm `score_threshold` trong YuNet detector
- Kiểm tra ánh sáng trong video
- Tăng kích thước khuôn mặt tối thiểu

### 2. Face Swap không tự nhiên

**Triệu chứng**: Khuôn mặt thay thế trông không tự nhiên

**Giải pháp**:
- Sử dụng source image chất lượng cao
- Đảm bảo góc khuôn mặt tương tự
- Kiểm tra model face swap có đúng không

### 3. Performance chậm

**Triệu chứng**: FPS thấp, lag

**Giải pháp**:
- Giảm `resize_ratio`
- Sử dụng GPU acceleration
- Giảm số lượng khuôn mặt tối đa (`top_k`)

## Kết quả và Output

### RTMP Stream

Stream output bao gồm:
- Video với face swap đã được áp dụng
- Real-time processing
- H.264 encoded

### Log Output

Pipeline sẽ log các thông tin:
- Số lượng khuôn mặt được phát hiện
- Face swap status
- Processing FPS
- Error messages (nếu có)

## Tài liệu tham khảo

- [InsightFace Documentation](https://github.com/deepinsight/insightface)
- [YuNet Face Detector](https://github.com/opencv/opencv_zoo)
- [Solution Registry](../../src/solutions/solution_registry.cpp)
- [Pipeline Builder](../../src/core/pipeline_builder.cpp)
- [API Documentation](../../docs/INSTANCE_GUIDE.md)

