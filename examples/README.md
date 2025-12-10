# Examples

Thư mục này chứa các examples, documentation và scripts để làm việc với Edge AI API.

## Cấu trúc

```
examples/
├── docs/              # Documentation chi tiết về các solutions và pipelines
├── instances/         # Example files và scripts cho instances
│   ├── create/       # Examples để tạo instances
│   ├── update/       # Examples để cập nhật instances
│   ├── scripts/      # Utility scripts
│   ├── tests/        # Test files
│   ├── infer_nodes/  # Inference nodes examples
│   └── example_*.json # Solution examples
└── solutions/         # Solution examples và tests
```

## Thư mục con

### 📚 `docs/`
Documentation chi tiết về các solutions và pipelines:
- [New Solutions Overview](./docs/new_solutions_overview.md) - Tổng quan các solutions mới
- [YOLOv11 Detection Pipeline](./docs/yolov11_detection_pipeline.md)
- [Face Swap Pipeline](./docs/face_swap_pipeline.md)
- [InsightFace Recognition Pipeline](./docs/insightface_recognition_pipeline.md)
- [MLLM Analysis Pipeline](./docs/mllm_analysis_pipeline.md)
- [BA Crossline RTMP Pipeline](./docs/ba_crossline_rtmp_pipeline.md)

Xem [docs/README.md](./docs/README.md) để biết chi tiết.

### 📝 `instances/`
Example files và scripts để làm việc với instances:
- `create/` - Examples để tạo instances với basic solutions
- `update/` - Examples để cập nhật instances
- `scripts/` - Utility scripts (check status, monitor, analyze logs, etc.)
- `tests/` - Test files
- `infer_nodes/` - Inference nodes examples
- `example_*.json` - Solution examples ở root

Xem [instances/README.md](./instances/README.md) để biết chi tiết.

### 🔧 `solutions/`
Solution examples và tests:
- Solution configuration examples
- Test files cho solutions

Xem [solutions/README.md](./solutions/README.md) để biết chi tiết.

## Quick Start

### 1. Tạo Instance với Basic Solution

```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @instances/create/create_face_detection_basic.json
```

### 2. Tạo Instance với Solution Example

```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @instances/example_yolov11_detection.json
```

### 3. Update Instance

```bash
curl -X PUT http://localhost:8080/v1/core/instance/{instanceId} \
  -H "Content-Type: application/json" \
  -d @instances/update/update_change_rtsp_url.json
```

### 4. Sử dụng Scripts

```bash
# Check instance status
./instances/scripts/check_instance_status.sh {instanceId}

# Monitor instance
./instances/scripts/monitor_instance.sh {instanceId}
```

## Documentation

- [Instances Examples](./instances/README.md) - Hướng dẫn sử dụng instance examples
- [Solutions Documentation](./docs/README.md) - Documentation về các solutions
- [New Solutions Overview](./docs/new_solutions_overview.md) - Tổng quan solutions mới

## Lưu ý

1. **Model Paths**: Các đường dẫn model trong example files là ví dụ, cần cập nhật cho phù hợp với môi trường của bạn
2. **API Endpoint**: Mặc định là `http://localhost:8080`, cần cập nhật nếu khác
3. **Permissions**: Đảm bảo scripts có quyền thực thi: `chmod +x instances/scripts/*.sh`

## Tài liệu tham khảo

- [API Documentation](../docs/CREATE_INSTANCE_GUIDE.md)
- [Node Integration Plan](../develop_doc/NODE_INTEGRATION_PLAN.md)
- [Solution Registry](../src/solutions/solution_registry.cpp)

