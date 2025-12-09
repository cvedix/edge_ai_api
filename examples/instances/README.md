# Instance Examples

Thư mục này chứa các example files và scripts để làm việc với API instances.

## Cấu trúc

```
examples/instances/
├── create/              # Example files để tạo instances
├── update/              # Example files để cập nhật instances
├── scripts/             # Utility scripts
├── tests/               # Test files
├── infer_nodes/         # Inference nodes examples
├── example_*.json       # Solution examples (ở root)
├── batch_operations.md  # Documentation
└── README.md           # File này
```

### 1. Create Examples (`create/`)
Các example files cho các solution cơ bản đã được định nghĩa sẵn trong hệ thống.

Xem [create/README.md](./create/README.md) để biết chi tiết.

**Files**:
- `create_face_detection_basic.json` - Face detection cơ bản với RTSP source
- `create_face_detection_file_source.json` - Face detection với file source
- `create_face_detection_rtmp.json` - Face detection với RTMP output
- `create_object_detection.json` - Object detection cơ bản
- `create_thermal_detection.json` - Thermal detection
- `create_minimal.json` - Minimal example với các tham số tối thiểu

### 2. Solution Examples (`example_*.json` ở root)
Các example files cho các solutions mới đã được tích hợp:

- `example_yolov11_detection.json` - YOLOv11 Object Detection
- `example_face_swap.json` - Face Swap
- `example_insightface_recognition.json` - InsightFace Recognition
- `example_mllm_analysis.json` - MLLM Analysis
- `example_rknn_yolov11_detection.json` - RKNN YOLOv11 (conditional)
- `example_trt_insightface_recognition.json` - TensorRT InsightFace (conditional)
- `example_ba_crossline_rtmp.json` - BA Crossline RTMP
- `example_face_detection_rtmp.json` - Face Detection RTMP
- `example_full_config.json` - Full configuration example

### 3. Update Examples (`update/`)
Các example files cho việc cập nhật instance.

Xem [update/README.md](./update/README.md) để biết chi tiết.

**Files**:
- `update_change_model_path.json` - Thay đổi model path
- `update_change_name_group.json` - Thay đổi name và group
- `update_change_persistent_autostart.json` - Thay đổi persistent và autoStart
- `update_change_rtsp_url.json` - Thay đổi RTSP URL
- `update_change_settings.json` - Thay đổi các settings khác

### 4. Scripts (`scripts/`)
Utility scripts để làm việc với API.

Xem [scripts/README.md](./scripts/README.md) để biết chi tiết.

**Scripts**:
- `demo_script.sh` - Demo script để test API
- `check_instance_status.sh` - Kiểm tra trạng thái instance
- `monitor_instance.sh` - Monitor instance
- `test_output_api.sh` - Test output API
- `analyze_log.sh` - Phân tích logs

### 5. Tests (`tests/`)
Test files và examples để test API functionality.

Xem [tests/README.md](./tests/README.md) để biết chi tiết.

### 6. Inference Nodes Examples (`infer_nodes/`)
Xem [infer_nodes/README.md](./infer_nodes/README.md) để biết chi tiết về các example files cho các inference nodes cụ thể.

### 7. Documentation
- `README.md` - File này
- `batch_operations.md` - Hướng dẫn batch operations
- `infer_nodes/INFER_NODES_GUIDE.md` - Hướng dẫn sử dụng inference nodes

## Cách Sử dụng

### 1. Create Instance với Basic Solutions

Sử dụng các file trong `create/` với các solution đã được định nghĩa sẵn:

```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @create/create_face_detection_basic.json
```

### 2. Create Instance với Solution Examples

Sử dụng các file `example_*.json` ở root:

```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @example_yolov11_detection.json
```

### 3. Inference Nodes (Cần tạo solution config)

Sử dụng các file trong `infer_nodes/` sau khi đã tạo solution config tương ứng:

```bash
# Bước 1: Tạo solution config (nếu chưa có)
# Bước 2: Tạo instance với solution ID
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @infer_nodes/example_trt_yolov8_detector.json
```

Xem chi tiết trong [infer_nodes/INFER_NODES_GUIDE.md](./infer_nodes/INFER_NODES_GUIDE.md)

### 4. Update Instance

```bash
curl -X PUT http://localhost:8080/v1/core/instance/{instanceId} \
  -H "Content-Type: application/json" \
  -d @update/update_change_rtsp_url.json
```

### 5. Sử dụng Scripts

```bash
# Check instance status
./scripts/check_instance_status.sh {instanceId}

# Monitor instance
./scripts/monitor_instance.sh {instanceId}
```

## Phân biệt các loại Examples

| Loại | Thư mục | Solution Config | Mục đích |
|------|---------|----------------|----------|
| Basic Solutions | `create/` | Đã có sẵn | Sử dụng ngay với các solution đã định nghĩa |
| Solution Examples | Root (`example_*.json`) | Đã có sẵn | Examples cho các solutions mới |
| Inference Nodes | `infer_nodes/` | Cần tạo | Demo các inference nodes cụ thể |
| Update Examples | `update/` | N/A | Ví dụ về cách update instance |
| Scripts | `scripts/` | N/A | Utility scripts |
| Tests | `tests/` | N/A | Test files |

## Lưu ý

1. **Basic Solutions**: Có thể sử dụng ngay mà không cần cấu hình thêm
2. **Inference Nodes**: Yêu cầu tạo solution config trước khi sử dụng
3. **Model Paths**: Các đường dẫn trong example files là ví dụ, cần cập nhật cho phù hợp với môi trường của bạn
