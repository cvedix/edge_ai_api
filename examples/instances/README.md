# Instance Examples

Thư mục này chứa các example files và scripts để làm việc với API instances.

## Cấu trúc

### Example Files

#### 1. Basic Solution Examples (`create_*.json`)
Các example files cho các solution cơ bản đã được định nghĩa sẵn trong hệ thống:

- `create_face_detection_basic.json` - Face detection cơ bản với RTSP source
- `create_face_detection_file_source.json` - Face detection với file source
- `create_face_detection_rtmp.json` - Face detection với RTMP output
- `create_object_detection.json` - Object detection cơ bản
- `create_thermal_detection.json` - Thermal detection
- `create_minimal.json` - Minimal example với các tham số tối thiểu

Các file này sử dụng các solution đã được đăng ký sẵn trong hệ thống và có thể sử dụng ngay.

#### 2. Legacy Example (`example_*.json`)
- `example_face_detection_rtmp.json` - Example cũ cho face detection với RTMP

#### 3. Inference Nodes Examples (`infer_nodes/`)
Xem [infer_nodes/README.md](./infer_nodes/README.md) để biết chi tiết về các example files cho các inference nodes cụ thể.

### Update Examples (`update_*.json`)
Các example files cho việc cập nhật instance:
- `update_change_model_path.json` - Thay đổi model path
- `update_change_name_group.json` - Thay đổi name và group
- `update_change_persistent_autostart.json` - Thay đổi persistent và autoStart
- `update_change_rtsp_url.json` - Thay đổi RTSP URL
- `update_change_settings.json` - Thay đổi các settings khác

### Scripts
- `demo_script.sh` - Demo script để test API
- `check_instance_status.sh` - Kiểm tra trạng thái instance
- `monitor_instance.sh` - Monitor instance
- `test_output_api.sh` - Test output API
- `analyze_log.sh` - Phân tích logs

### Documentation
- `README.md` - File này
- `HOW_TO_CHECK_SUCCESS.md` - Hướng dẫn kiểm tra thành công
- `LOG_ANALYSIS.md` - Phân tích logs
- `LOG_ANALYSIS_DETAILED.md` - Phân tích logs chi tiết
- `batch_operations.md` - Hướng dẫn batch operations
- `infer_nodes/INFER_NODES_GUIDE.md` - Hướng dẫn sử dụng inference nodes

## Cách Sử dụng

### 1. Basic Solutions (Đã có sẵn)

Sử dụng các file `create_*.json` với các solution đã được định nghĩa sẵn:

```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @create_face_detection_basic.json
```

### 2. Inference Nodes (Cần tạo solution config)

Sử dụng các file trong `infer_nodes/` sau khi đã tạo solution config tương ứng:

```bash
# Bước 1: Tạo solution config (nếu chưa có)
# Bước 2: Tạo instance với solution ID
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @infer_nodes/example_trt_yolov8_detector.json
```

Xem chi tiết trong [infer_nodes/INFER_NODES_GUIDE.md](./infer_nodes/INFER_NODES_GUIDE.md)

### 3. Update Instance

```bash
curl -X PUT http://localhost:8080/v1/core/instance/{instanceId} \
  -H "Content-Type: application/json" \
  -d @update_change_rtsp_url.json
```

## Phân biệt các loại Examples

| Loại | Thư mục | Solution Config | Mục đích |
|------|---------|----------------|----------|
| Basic Solutions | Root | Đã có sẵn | Sử dụng ngay với các solution đã định nghĩa |
| Inference Nodes | `infer_nodes/` | Cần tạo | Demo các inference nodes cụ thể |
| Update Examples | Root | N/A | Ví dụ về cách update instance |

## Lưu ý

1. **Basic Solutions**: Có thể sử dụng ngay mà không cần cấu hình thêm
2. **Inference Nodes**: Yêu cầu tạo solution config trước khi sử dụng
3. **Model Paths**: Các đường dẫn trong example files là ví dụ, cần cập nhật cho phù hợp với môi trường của bạn
