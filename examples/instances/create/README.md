# Create Instance Examples

Thư mục này chứa các example files để tạo instances với các solutions cơ bản đã được định nghĩa sẵn trong hệ thống.

## Các Files

### Basic Solutions

- `create_face_detection_basic.json` - Face detection cơ bản với RTSP source
- `create_face_detection_file_source.json` - Face detection với file source
- `create_face_detection_rtmp.json` - Face detection với RTMP output
- `create_object_detection.json` - Object detection cơ bản
- `create_thermal_detection.json` - Thermal detection
- `create_minimal.json` - Minimal example với các tham số tối thiểu

## Cách sử dụng

```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @create_face_detection_basic.json
```

## Lưu ý

- Các file này sử dụng các solution đã được đăng ký sẵn trong hệ thống
- Có thể sử dụng ngay mà không cần cấu hình thêm
- Các đường dẫn model trong example files là ví dụ, cần cập nhật cho phù hợp với môi trường của bạn
