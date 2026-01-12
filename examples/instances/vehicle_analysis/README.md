# Vehicle Analysis Examples

Thư mục này chứa các ví dụ instances cho Vehicle Detection & Analysis.

## Cấu trúc

```
vehicle_analysis/
└── tensorrt/
    ├── example_vehicle_analysis_full.json
    └── example_vehicle_analysis_plate_only.json
```

## Examples

### 1. `example_vehicle_analysis_full.json`
- **Mô tả**: Vehicle Analysis đầy đủ tính năng
- **Pipeline**: Vehicle Detection → Tracking → Plate Detection → Color Classification → Type Classification → Feature Encoding → OSD
- **Input**: Video file
- **Output**: RTMP stream
- **Features**: Detection, tracking, plate recognition, color classification, type classification, feature encoding

### 2. `example_vehicle_analysis_plate_only.json`
- **Mô tả**: Vehicle Analysis chỉ với plate detection
- **Pipeline**: Vehicle Detection → Tracking → Plate Detection → OSD
- **Input**: Video file
- **Output**: Screen display
- **Features**: Detection, tracking, plate recognition

## Cách sử dụng

1. Điều chỉnh các đường dẫn model trong JSON file
2. Tạo instance từ solution:

```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @examples/instances/vehicle_analysis/tensorrt/example_vehicle_analysis_full.json
```

## Yêu cầu

### Full Analysis:
- Vehicle detection model (.trt)
- Plate detection model (.trt)
- Plate recognition model (.trt)
- Color classifier model (.trt)
- Type classifier model (.trt)
- Feature encoder model (.trt)

### Plate Only:
- Vehicle detection model (.trt)
- Plate detection model (.trt)
- Plate recognition model (.trt)

