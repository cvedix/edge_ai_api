# Example Test Solutions

Thư mục này chứa các example test solution để tham khảo và test hệ thống.

## Cấu trúc Solution

Mỗi solution file là một JSON với cấu trúc sau:

```json
{
    "solutionId": "unique_solution_id",
    "solutionName": "Display Name",
    "solutionType": "face_detection",
    "isDefault": false,
    "pipeline": [
        {
            "nodeType": "node_type",
            "nodeName": "node_name_{instanceId}",
            "parameters": {
                "param1": "value1",
                "param2": "${VARIABLE}"
            }
        }
    ],
    "defaults": {
        "key1": "value1",
        "key2": "value2"
    }
}
```

## Các Example Files

### 1. `test_face_detection.json`
- **Mô tả**: Face detection với file source
- **Pipeline**: File Source → YuNet Face Detector → File Destination
- **Use case**: Test face detection trên video file

### 2. `test_rtsp_face_detection.json`
- **Mô tả**: Face detection với RTSP stream
- **Pipeline**: RTSP Source → YuNet Face Detector → File Destination
- **Use case**: Test face detection trên RTSP stream

### 3. `test_minimal.json`
- **Mô tả**: Solution tối giản với cấu hình cơ bản
- **Pipeline**: File Source → YuNet Face Detector → File Destination
- **Use case**: Test với cấu hình đơn giản nhất

## Cách sử dụng

### 1. Tạo solution từ file JSON

```bash
curl -X POST http://localhost:8080/v1/core/solutions \
  -H "Content-Type: application/json" \
  -d @examples/solutions/test_face_detection.json
```

### 2. Kiểm tra solution đã tạo

```bash
curl http://localhost:8080/v1/core/solutions/test_face_detection
```

### 3. Tạo instance từ solution

```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d '{
    "solutionId": "test_face_detection",
    "additionalParams": {
      "FILE_PATH": "./cvedix_data/test_video/face.mp4",
      "MODEL_PATH": "./models/yunet.onnx"
    }
  }'
```

## Variables và Placeholders

### Variables trong parameters:
- `${FILE_PATH}` - Đường dẫn file video
- `${RTSP_URL}` - URL RTSP stream
- `${MODEL_PATH}` - Đường dẫn model file
- `${detectionSensitivity}` - Độ nhạy phát hiện (từ defaults)
- `${instanceId}` - ID của instance (tự động thay thế trong nodeName)

### Placeholders trong nodeName:
- `{instanceId}` - Sẽ được thay thế bằng ID instance thực tế

## Lưu ý

1. **isDefault**: Đặt `false` để có thể xóa solution sau này
2. **resize_ratio**: Phải > 0 và <= 1.0. Khuyến nghị dùng `1.0` nếu video đã có resolution cố định
3. **score_threshold**: Giá trị từ 0.0 đến 1.0, càng cao càng nghiêm ngặt
4. **nms_threshold**: Non-maximum suppression threshold, thường từ 0.3 đến 0.7

## Node Types phổ biến

- `file_src`: File video source
- `rtsp_src`: RTSP stream source
- `yunet_face_detector`: YuNet face detector
- `file_des`: File destination (save output)
- `rtmp_des`: RTMP streaming destination
- `face_osd_v2`: Face overlay/OSD node
- `sface_feature_encoder`: SFace feature encoder

## Xem thêm

- [CREATE_INSTANCE_GUIDE.md](../../docs/CREATE_INSTANCE_GUIDE.md)
- [INSTANCE_CONFIG_FIELDS.md](../../docs/INSTANCE_CONFIG_FIELDS.md)
- API Documentation: http://localhost:8080/v1/swagger

