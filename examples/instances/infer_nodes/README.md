# Inference Nodes Examples

Thư mục này chứa các example files cho các inference nodes được hỗ trợ bởi API.

## Phân biệt với các Example Files khác

### Thư mục gốc (`examples/instances/`)
Chứa các example files cho các solution cơ bản đã được định nghĩa sẵn:
- `create_face_detection_*.json` - Face detection với các nguồn khác nhau
- `create_object_detection.json` - Object detection cơ bản
- `create_thermal_detection.json` - Thermal detection
- `create_minimal.json` - Minimal example
- `example_face_detection_rtmp.json` - Example face detection với RTMP

Các file này sử dụng các solution đã được đăng ký sẵn trong hệ thống.

### Thư mục này (`examples/instances/infer_nodes/`)
Chứa các example files cho các inference nodes cụ thể:
- **TensorRT nodes**: YOLOv8 (detector, segmentation, pose), Vehicle (detector, plate detector, classifiers)
- **RKNN nodes**: YOLOv8 detector, Face detector
- **Other nodes**: YOLO detector, ENet segmentation, Mask RCNN, OpenPose, Classifier, Lane detector, Restoration

Các file này yêu cầu bạn phải tạo solution config tương ứng trước khi sử dụng.

## Cấu trúc Files

### TensorRT Inference Nodes
- `example_trt_yolov8_detector.json` - YOLOv8 object detection
- `example_trt_yolov8_segmentation.json` - YOLOv8 instance segmentation
- `example_trt_yolov8_pose.json` - YOLOv8 pose estimation
- `example_trt_vehicle_detector.json` - Vehicle detection
- `example_trt_vehicle_plate_detector.json` - Vehicle plate detection và recognition

### RKNN Inference Nodes
- `example_rknn_face_detector.json` - Face detection trên RKNN NPU

### Other Inference Nodes
- `example_yolo_detector.json` - YOLO detector (OpenCV DNN)
- `example_enet_segmentation.json` - ENet semantic segmentation
- `example_mask_rcnn.json` - Mask RCNN instance segmentation
- `example_openpose.json` - OpenPose pose estimation
- `example_classifier.json` - Generic image classifier
- `example_lane_detector.json` - Lane detection
- `example_restoration.json` - Image restoration (Real-ESRGAN)

## Cách Sử dụng

### Hướng dẫn Chi tiết
- **[INFER_NODES_GUIDE.md](./INFER_NODES_GUIDE.md)** - Hướng dẫn các node types và parameters
- **[../../docs/CREATE_INSTANCE_GUIDE.md](../../docs/CREATE_INSTANCE_GUIDE.md)** - Hướng dẫn tạo instance cho từng case cụ thể

### Quick Start

1. **Tạo Solution Config** (nếu chưa có)
2. **Tạo Instance** với solution ID và parameters
3. **Kiểm tra** instance status và output

Xem ví dụ cụ thể trong [CREATE_INSTANCE_GUIDE.md](../../docs/CREATE_INSTANCE_GUIDE.md)

## Lưu ý

1. **Solution Config Required**: Các example files này yêu cầu bạn phải tạo solution config tương ứng trước khi sử dụng. Solution config phải định nghĩa pipeline với các node types tương ứng.

2. **Model Paths**: Các đường dẫn model trong example files là ví dụ. Bạn cần cập nhật cho phù hợp với môi trường của mình.

3. **Conditional Compilation**: Một số nodes chỉ có sẵn khi compile với flags tương ứng:
   - TensorRT nodes: `CVEDIX_WITH_TRT`
   - RKNN nodes: `CVEDIX_WITH_RKNN`
   - PaddleOCR nodes: `CVEDIX_WITH_PADDLE`

