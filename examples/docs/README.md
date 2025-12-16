# Examples Documentation

Thư mục này chứa các tài liệu hướng dẫn chi tiết về các solutions và pipelines trong Edge AI API.

## Danh sách Documentation

### Solutions Mới

#### 1. [YOLOv11 Detection Pipeline](./yolov11_detection_pipeline.md)
Hướng dẫn sử dụng solution `yolov11_detection` cho object detection với YOLOv11 model.

**Example**: `../instances/example_yolov11_detection.json`

#### 2. [Face Swap Pipeline](./face_swap_pipeline.md)
Hướng dẫn sử dụng solution `face_swap` cho face replacement trong video stream.

**Example**: `../instances/example_face_swap.json`

#### 3. [InsightFace Recognition Pipeline](./insightface_recognition_pipeline.md)
Hướng dẫn sử dụng solution `insightface_recognition` cho face recognition.

**Example**: `../instances/example_insightface_recognition.json`

#### 4. [MLLM Analysis Pipeline](./mllm_analysis_pipeline.md)
Hướng dẫn sử dụng solution `mllm_analysis` cho video content analysis với Multimodal LLM.

**Example**: `../instances/example_mllm_analysis.json`

#### 5. [New Solutions Overview](./new_solutions_overview.md)
Tổng quan về tất cả các solutions mới đã được tích hợp.

### Solutions Hiện có

#### 6. [BA Crossline RTMP Pipeline](./ba_crossline_rtmp_pipeline.md)
Hướng dẫn sử dụng solution `ba_crossline` cho behavior analysis với crossline detection.

**Example**: `../instances/example_ba_crossline_rtmp.json`

## Cấu trúc Documentation

Mỗi file documentation bao gồm:

1. **Tổng quan**: Mô tả solution và use cases
2. **Pipeline Architecture**: Sơ đồ và luồng xử lý
3. **Cấu hình Instance**: Hướng dẫn cấu hình chi tiết
4. **Chi tiết các Nodes**: Mô tả từng node trong pipeline
5. **Cách sử dụng**: Hướng dẫn step-by-step
6. **Tùy chỉnh Cấu hình**: Các tùy chọn customization
7. **Performance Tips**: Mẹo tối ưu hiệu năng
8. **Troubleshooting**: Giải quyết các vấn đề thường gặp
9. **Kết quả và Output**: Mô tả output và logs
10. **Tài liệu tham khảo**: Links đến các tài liệu liên quan

## Quick Start

### 1. Chọn Solution

Xem [New Solutions Overview](./new_solutions_overview.md) để chọn solution phù hợp với use case của bạn.

### 2. Đọc Documentation

Đọc documentation chi tiết của solution đã chọn để hiểu rõ cách hoạt động và cấu hình.

### 3. Sử dụng Example

Sử dụng example file tương ứng trong `../instances/` làm template:

```bash
# Copy example file
cp ../instances/example_{solution_id}.json my_instance.json

# Edit với thông tin của bạn
nano my_instance.json

# Tạo instance
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @my_instance.json
```

### 4. Start Instance

```bash
curl -X POST http://localhost:8080/v1/core/instances/{instanceId}/start
```

## Requirements

### Model Files

Các solutions yêu cầu các model files tương ứng. Xem documentation của từng solution để biết chi tiết.

### Compilation Flags

Một số solutions yêu cầu compilation flags:
- `CVEDIX_WITH_RKNN`: Cho RKNN solutions
- `CVEDIX_WITH_TRT`: Cho TensorRT solutions
- `CVEDIX_WITH_LLM`: Cho MLLM solutions

## Troubleshooting

Nếu gặp vấn đề:

1. Kiểm tra documentation của solution tương ứng
2. Xem section Troubleshooting trong documentation
3. Kiểm tra logs trong `/var/lib/edge_ai_api/logs/`
4. Tham khảo [API Documentation](../../docs/INSTANCE_GUIDE.md)

## Tài liệu tham khảo

- [NODE_INTEGRATION_PLAN.md](../../develop_doc/NODE_INTEGRATION_PLAN.md)
- [Solution Registry](../../src/solutions/solution_registry.cpp)
- [Pipeline Builder](../../src/core/pipeline_builder.cpp)
- [API Documentation](../../docs/INSTANCE_GUIDE.md)
- [Instance Examples README](../instances/README.md)

## Liên hệ và Hỗ trợ

Nếu cần hỗ trợ thêm:
1. Kiểm tra documentation chi tiết của solution
2. Xem troubleshooting section
3. Tham khảo API documentation
4. Kiểm tra logs và error messages

