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

### 4. `test_mask_rcnn_detection.json`
- **Mô tả**: MaskRCNN instance segmentation với file source và file output
- **Pipeline**: File Source → MaskRCNN Detector → OSD v3 → File Destination
- **Use case**: Test MaskRCNN detection và segmentation trên video file
- **Yêu cầu**:
  - Model file (.pb)
  - Config file (.pbtxt)
  - Labels file (.txt)

### 5. `test_mask_rcnn_rtmp.json`
- **Mô tả**: MaskRCNN instance segmentation với file source và RTMP streaming output
- **Pipeline**: File Source → MaskRCNN Detector → OSD v3 → RTMP Destination
- **Use case**: Test MaskRCNN detection và streaming kết quả lên RTMP server

### 6. `ba_stop.json`
- **Mô tả**: Behavior Analysis - Stop Detection
- **Pipeline**: File Source → Vehicle Detector → SORT Tracker → BA Stop → BA Stop OSD → Screen/RTMP Destination
- **Use case**: Phát hiện xe dừng lại trong các vùng được định nghĩa
- **Yêu cầu**: 
  - Vehicle detection model (.trt)
  - StopZones hoặc RulesZones trong additionalParams

### 7. `ba_jam.json`
- **Mô tả**: Behavior Analysis - Traffic Jam Detection
- **Pipeline**: File Source → Vehicle Detector → SORT Tracker → BA Jam → BA Jam OSD → Screen/RTMP Destination
- **Use case**: Phát hiện kẹt xe trong các vùng được định nghĩa
- **Yêu cầu**: 
  - Vehicle detection model (.trt)
  - JamZones hoặc RulesZones trong additionalParams

### 8. `vehicle_analysis.json`
- **Mô tả**: Vehicle Detection & Analysis (đầy đủ tính năng)
- **Pipeline**: File Source → Vehicle Detector → SORT Tracker → Plate Detector → Color Classifier → Type Classifier → Feature Encoder → Plate OSD → OSD v3 → Screen/RTMP Destination
- **Use case**: Phát hiện và phân tích xe (biển số, màu sắc, loại xe, features)
- **Yêu cầu**: 
  - Vehicle detection model (.trt)
  - Plate detection model (.trt)
  - Plate recognition model (.trt)
  - Color classifier model (.trt)
  - Type classifier model (.trt)
  - Feature encoder model (.trt)

### 9. `pose_estimation.json`
- **Mô tả**: Pose Estimation sử dụng OpenPose
- **Pipeline**: File Source → OpenPose Detector → Pose OSD → Screen/RTMP Destination
- **Use case**: Ước tính tư thế người sử dụng OpenPose
- **Yêu cầu**: 
  - OpenPose model (.caffemodel)
  - OpenPose prototxt (.prototxt)

### 10. `pose_estimation_trt.json`
- **Mô tả**: Pose Estimation sử dụng TensorRT YOLOv8 Pose
- **Pipeline**: File Source → YOLOv8 Pose Detector → Pose OSD → Screen/RTMP Destination
- **Use case**: Ước tính tư thế người sử dụng YOLOv8 Pose (nhanh hơn OpenPose)
- **Yêu cầu**: 
  - YOLOv8 Pose model (.engine)

### 11. `text_recognition_ocr.json`
- **Mô tả**: Text Recognition (OCR) sử dụng PaddleOCR
- **Pipeline**: File Source → PaddleOCR Text Detector → Text OSD → Screen/RTMP Destination
- **Use case**: Nhận dạng văn bản trong video/hình ảnh
- **Yêu cầu**: 
  - PaddleOCR models (det, rec, cls)
  - CVEDIX_WITH_PADDLE enabled

### 12. `video_enhancement.json`
- **Mô tả**: Video Enhancement/Restoration
- **Pipeline**: File Source → Restoration → Screen/RTMP/File Destination
- **Use case**: Nâng cấp chất lượng video (super-resolution, denoising, etc.)
- **Yêu cầu**: 
  - Restoration model (.onnx)

### 13. `ba_stop_rtsp.json`
- **Mô tả**: Behavior Analysis - Stop Detection với RTSP output
- **Pipeline**: File Source → Vehicle Detector → SORT Tracker → BA Stop → BA Stop OSD → RTSP Destination
- **Use case**: Phát hiện xe dừng và stream qua RTSP
- **Yêu cầu**: 
  - Vehicle detection model (.trt)
  - StopZones hoặc RulesZones trong additionalParams

### 14. `vehicle_analysis_rtsp.json`
- **Mô tả**: Vehicle Detection & Analysis với RTSP output
- **Pipeline**: File Source → Vehicle Detector → SORT Tracker → Plate Detector → Plate OSD → OSD v3 → RTSP Destination
- **Use case**: Phân tích xe và stream qua RTSP
- **Yêu cầu**: 
  - Vehicle detection model (.trt)
  - Plate detection/recognition models (.trt)

### 15. `pose_estimation_rtsp.json`
- **Mô tả**: Pose Estimation với RTSP output
- **Pipeline**: File Source → OpenPose Detector → Pose OSD → RTSP Destination
- **Use case**: Ước tính tư thế và stream qua RTSP
- **Yêu cầu**: 
  - OpenPose model (.caffemodel)
  - OpenPose prototxt (.prototxt)

### 16. `face_detection_app.json`
- **Mô tả**: Face Detection với App Destination
- **Pipeline**: File Source → YuNet Face Detector → Face OSD v2 → App Destination
- **Use case**: Face detection và capture frames để xử lý trong application
- **Yêu cầu**: 
  - Face detection model (.onnx)

### 17. `vehicle_analysis_app.json`
- **Mô tả**: Vehicle Detection & Analysis với App Destination
- **Pipeline**: File Source → Vehicle Detector → SORT Tracker → Plate Detector → Plate OSD → OSD v3 → App Destination
- **Use case**: Vehicle analysis và capture frames để xử lý trong application
- **Yêu cầu**: 
  - Vehicle detection model (.trt)
  - Plate detection/recognition models (.trt)
- **Yêu cầu**:
  - Model file (.pb)
  - Config file (.pbtxt)
  - Labels file (.txt)
  - RTMP server

## Cách sử dụng

### 1. Tạo solution từ file JSON

```bash
curl -X POST http://localhost:8080/v1/core/solution \
  -H "Content-Type: application/json" \
  -d @examples/solutions/test_face_detection.json
```

### 2. Kiểm tra solution đã tạo

```bash
curl http://localhost:8080/v1/core/solution/test_face_detection
```

### 3. Tạo instance từ solution

**Ví dụ với Face Detection:**
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

**Ví dụ với MaskRCNN Detection:**
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d '{
    "name": "mask_rcnn_test",
    "solution": "test_mask_rcnn_detection",
    "additionalParams": {
      "FILE_PATH": "./cvedix_data/test_video/mask_rcnn.mp4",
      "MODEL_PATH": "./cvedix_data/models/mask_rcnn/frozen_inference_graph.pb",
      "MODEL_CONFIG_PATH": "./cvedix_data/models/mask_rcnn/mask_rcnn_inception_v2_coco_2018_01_28.pbtxt",
      "LABELS_PATH": "./cvedix_data/models/coco_80classes.txt",
      "INPUT_WIDTH": "416",
      "INPUT_HEIGHT": "416",
      "SCORE_THRESHOLD": "0.5"
    }
  }'
```

**Ví dụ với MaskRCNN RTMP:**
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d '{
    "name": "mask_rcnn_rtmp_test",
    "solution": "test_mask_rcnn_rtmp",
    "additionalParams": {
      "FILE_PATH": "./cvedix_data/test_video/mask_rcnn.mp4",
      "RTMP_URL": "rtmp://localhost:1935/live/mask_rcnn",
      "MODEL_PATH": "./cvedix_data/models/mask_rcnn/frozen_inference_graph.pb",
      "MODEL_CONFIG_PATH": "./cvedix_data/models/mask_rcnn/mask_rcnn_inception_v2_coco_2018_01_28.pbtxt",
      "LABELS_PATH": "./cvedix_data/models/coco_80classes.txt",
      "INPUT_WIDTH": "416",
      "INPUT_HEIGHT": "416",
      "SCORE_THRESHOLD": "0.5"
    }
  }'
```

## Variables và Placeholders

### Variables trong parameters:
- `${FILE_PATH}` - Đường dẫn file video
- `${RTSP_URL}` - URL RTSP stream
- `${RTMP_URL}` - URL RTMP stream
- `${MODEL_PATH}` - Đường dẫn model file
- `${MODEL_CONFIG_PATH}` - Đường dẫn config file (cho MaskRCNN)
- `${LABELS_PATH}` - Đường dẫn labels file
- `${INPUT_WIDTH}` - Chiều rộng input (cho MaskRCNN)
- `${INPUT_HEIGHT}` - Chiều cao input (cho MaskRCNN)
- `${SCORE_THRESHOLD}` - Ngưỡng confidence score
- `${RESIZE_RATIO}` - Tỷ lệ resize video
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

### Source Nodes
- `file_src`: File video source
- `rtsp_src`: RTSP stream source
- `rtmp_src`: RTMP stream source
- `app_src`: Application source (receive frames from app)

### Detector Nodes
- `yunet_face_detector`: YuNet face detector
- `mask_rcnn_detector`: MaskRCNN instance segmentation detector
- `yolo_detector`: YOLO object detector
- `yolov11_detector`: YOLOv11 detector
- `openpose_detector`: OpenPose pose estimation
- `enet_seg`: ENet semantic segmentation

### Processing Nodes
- `sface_feature_encoder`: SFace feature encoder
- `sort_track`: SORT tracker
- `osd_v3`: OSD overlay v3 (for masks and labels)

### Destination Nodes
- `file_des`: File destination (save output)
- `rtmp_des`: RTMP streaming destination
- `rtsp_des`: RTSP streaming destination (xem solutions: `ba_stop_rtsp`, `vehicle_analysis_rtsp`, `pose_estimation_rtsp`)
- `screen_des`: Screen display destination
- `app_des`: Application destination (capture frames for app processing, xem solutions: `face_detection_app`, `vehicle_analysis_app`)
- `face_osd_v2`: Face overlay/OSD node

## Xem thêm

- [INSTANCE_GUIDE.md](../../docs/INSTANCE_GUIDE.md)
- [INSTANCE_CONFIG_FIELDS.md](../../docs/INSTANCE_CONFIG_FIELDS.md)
- API Documentation: http://localhost:8080/v1/swagger
