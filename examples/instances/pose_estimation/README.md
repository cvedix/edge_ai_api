# Pose Estimation Examples

Thư mục này chứa các ví dụ instances cho Pose Estimation.

## Cấu trúc

```
pose_estimation/
├── caffe/
│   └── example_openpose_file_rtmp.json
└── tensorrt/
    └── example_yolov8_pose_file_rtmp.json
```

## Examples

### 1. `caffe/example_openpose_file_rtmp.json`
- **Mô tả**: Pose Estimation sử dụng OpenPose
- **Model Type**: Caffe
- **Input**: Video file
- **Output**: RTMP stream
- **Features**: Body pose estimation với 25 keypoints

### 2. `tensorrt/example_yolov8_pose_file_rtmp.json`
- **Mô tả**: Pose Estimation sử dụng TensorRT YOLOv8 Pose
- **Model Type**: TensorRT
- **Input**: Video file
- **Output**: RTMP stream
- **Features**: Body pose estimation (nhanh hơn OpenPose)

## Cách sử dụng

### OpenPose:
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @examples/instances/pose_estimation/caffe/example_openpose_file_rtmp.json
```

### YOLOv8 Pose:
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @examples/instances/pose_estimation/tensorrt/example_yolov8_pose_file_rtmp.json
```

## Yêu cầu

### OpenPose:
- OpenPose model (.caffemodel)
- OpenPose prototxt (.prototxt)

### YOLOv8 Pose:
- YOLOv8 Pose model (.engine)

