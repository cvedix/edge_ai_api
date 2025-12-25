# Examples

Thư mục này chứa các examples, documentation và scripts để làm việc với Edge AI API.

## Cấu trúc

```
examples/
├── default_solutions/ # Default solutions sẵn có để chọn và sử dụng
│   ├── *.json        # Solution configuration files
│   ├── index.json    # Catalog danh sách solutions
│   └── *.sh          # Helper scripts
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

### ⭐ `default_solutions/`
**Default solutions sẵn có để người dùng chọn và sử dụng ngay:**
- Các solution đã được cấu hình sẵn theo category
- File `index.json` chứa catalog đầy đủ
- Helper scripts để list và create solutions
- Documentation chi tiết cho từng solution

**Cách sử dụng nhanh:**
```bash
# Xem danh sách solutions có sẵn
./default_solutions/list_solutions.sh

# Tạo một solution
./default_solutions/create_solution.sh default_face_detection_file
```

Xem [default_solutions/README.md](./default_solutions/README.md) để biết chi tiết.

## Quick Start

### 0. Sử dụng Default Solutions (Khuyến nghị cho người mới)

```bash
# Xem danh sách tất cả solutions (bao gồm cả default solutions chưa load)
curl http://localhost:8080/v1/core/solution

# Lấy example request body để tạo instance (sẽ tự động load default solution nếu chưa có)
curl http://localhost:8080/v1/core/solution/default_face_detection_file/instance-body

# Sau đó tạo instance với solution
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d '{
    "name": "my_face_detection",
    "solution": "default_face_detection_file",
    "additionalParams": {
      "FILE_PATH": "/path/to/video.mp4",
      "MODEL_PATH": "/path/to/model.onnx"
    }
  }'
```

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

- **[Default Solutions](./default_solutions/README.md)** ⭐ - **Bắt đầu từ đây!** Danh sách solutions sẵn có
- [Instances Examples](./instances/README.md) - Hướng dẫn sử dụng instance examples
- [Solutions Reference](../docs/DEFAULT_SOLUTIONS_REFERENCE.md) - Documentation về các solutions
- [Instance Guide](../docs/INSTANCE_GUIDE.md) - Hướng dẫn tạo và cập nhật instances

## Lưu ý

1. **Model Paths**: Các đường dẫn model trong example files là ví dụ, cần cập nhật cho phù hợp với môi trường của bạn
2. **API Endpoint**: Mặc định là `http://localhost:8080`, cần cập nhật nếu khác
3. **Permissions**: Đảm bảo scripts có quyền thực thi: `chmod +x instances/scripts/*.sh`

## Tài liệu tham khảo

- [API Documentation](../docs/INSTANCE_GUIDE.md)
- [Node Integration Plan](../develop_doc/NODE_INTEGRATION_PLAN.md)
- [Solution Registry](../src/solutions/solution_registry.cpp)
