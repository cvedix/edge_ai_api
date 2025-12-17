# MaskRCNN Instance - Hướng Dẫn Test

## 📋 Tổng Quan

Instance này thực hiện instance segmentation sử dụng mô hình MaskRCNN, có thể phát hiện và phân đoạn nhiều loại đối tượng (80 classes từ COCO dataset).

## 🎯 Tính Năng

- ✅ Instance segmentation với MaskRCNN
- ✅ Phát hiện và phân loại đối tượng (80 COCO classes)
- ✅ Tạo mask cho từng đối tượng
- ✅ Tracking với SORT tracker
- ✅ RTMP streaming output (tùy chọn)
- ✅ Screen display với OSD v3 hiển thị mask và labels

## 📁 Cấu Trúc Files

```
mask_rcnn/
├── README.md                    # File này
├── test_file_source.json        # Test với file source
├── test_rtmp_output.json        # Test với RTMP output
└── report_body_example.json     # Ví dụ report body từ MQTT
```

## 🔧 Solution Config

### Solution ID: `mask_rcnn_detection`

**Mặc định có sẵn** trong hệ thống.

**Pipeline:**
```
File Source → MaskRCNN Detector → SORT Tracker → OSD v3 → Screen Display
```

### Solution ID: `mask_rcnn_rtmp`

**Mặc định có sẵn** trong hệ thống.

**Pipeline:**
```
File Source → MaskRCNN Detector → SORT Tracker → OSD v3 → Split → [Screen | RTMP]
```

## 📝 Manual Testing Guide

### 1. Test Cơ Bản với File Source

**Bước 1:** Tạo instance
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @mask_rcnn/test_file_source.json
```

**Bước 2:** Kiểm tra status
```bash
curl http://localhost:8080/v1/core/instance/{instanceId}
```

**Bước 3:** Start instance
```bash
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/start
```

**Bước 4:** Kiểm tra screen display
- Mở cửa sổ hiển thị video
- Kiểm tra mask được vẽ trên từng đối tượng
- Kiểm tra labels và confidence scores

**Bước 5:** Kiểm tra statistics
```bash
curl http://localhost:8080/v1/core/instance/{instanceId}/statistics
```

### 2. Test với RTMP Output

**Yêu cầu:**
- RTMP server (nginx-rtmp hoặc tương tự)
- RTMP URL hợp lệ

**Các bước:**
```bash
# Tạo instance với RTMP output
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @mask_rcnn/test_rtmp_output.json

# Kiểm tra RTMP stream
ffplay rtmp://your-server:1935/live/mask_rcnn_stream
```

## 📊 Kiểm Tra Kết Quả

### 1. Kiểm Tra Screen Display

**Expected output:**
- Mask được vẽ trên từng đối tượng (colored overlay)
- Bounding boxes quanh đối tượng
- Labels hiển thị class name và confidence score
- Track IDs (nếu có tracking)

**Các classes phổ biến:**
- person
- car, truck, bus, motorcycle
- bicycle
- dog, cat
- chair, couch, bed
- laptop, mouse, keyboard
- bottle, cup, bowl
- ... (tổng cộng 80 classes)

### 2. Kiểm Tra Statistics

```bash
curl http://localhost:8080/v1/core/instance/{instanceId}/statistics
```

**Expected output:**
```json
{
  "frames_processed": 2500,
  "source_framerate": 30.0,
  "current_framerate": 15.0,
  "latency": 400.0,
  "resolution": "1280x720"
}
```

**Lưu ý:** MaskRCNN chậm hơn YOLO do độ chính xác cao, nên FPS thấp hơn là bình thường.

### 3. Kiểm Tra Model Files

**Required files:**
- Model file (.pb): `frozen_inference_graph.pb`
- Config file (.pbtxt): `mask_rcnn_inception_v2_coco_2018_01_28.pbtxt`
- Labels file (.txt): `coco_80classes.txt`

```bash
# Kiểm tra model files
ls -la /path/to/models/mask_rcnn/*.pb
ls -la /path/to/models/mask_rcnn/*.pbtxt
ls -la /path/to/models/coco_80classes.txt
```

## 🔍 Troubleshooting

### Lỗi: Model không tìm thấy

**Kiểm tra:**
```bash
# Model file
ls -la /path/to/models/mask_rcnn/frozen_inference_graph.pb

# Config file
ls -la /path/to/models/mask_rcnn/mask_rcnn_inception_v2_coco_2018_01_28.pbtxt

# Labels file
ls -la /path/to/models/coco_80classes.txt
```

**Giải pháp:**
- Download model từ TensorFlow Model Zoo
- Cập nhật đường dẫn trong JSON config

### Lỗi: Out of Memory

**Nguyên nhân:** MaskRCNN model lớn, cần nhiều RAM/VRAM

**Giải pháp:**
- Giảm input size (416x416 → 320x320)
- Giảm batch_size (mặc định: 1)
- Sử dụng GPU nếu có
- Giảm số lượng objects được detect (tăng score_threshold)

### Lỗi: FPS quá thấp

**Nguyên nhân:** MaskRCNN là model nặng, chậm hơn YOLO

**Giải pháp:**
- Giảm input size
- Sử dụng GPU
- Tăng score_threshold để giảm số objects
- Nếu cần tốc độ cao, cân nhắc dùng YOLOv8 Seg

### Lỗi: Mask không hiển thị đúng

**Nguyên nhân có thể:**
- OSD v3 không được cấu hình đúng
- Font file không tìm thấy

**Giải pháp:**
- Kiểm tra OSD v3 node trong pipeline
- Kiểm tra font file: `./cvedix_data/font/NotoSansCJKsc-Medium.otf`

## 💡 Tips & Best Practices

### 1. Tối Ưu Tốc Độ

- Giảm input size: `INPUT_WIDTH=320`, `INPUT_HEIGHT=320`
- Tăng score_threshold: `SCORE_THRESHOLD=0.7`
- Sử dụng GPU nếu có

### 2. Tối Ưu Độ Chính Xác

- Tăng input size: `INPUT_WIDTH=512`, `INPUT_HEIGHT=512`
- Giảm score_threshold: `SCORE_THRESHOLD=0.3`
- Sử dụng model lớn hơn (ResNet-101 thay vì Inception v2)

### 3. Debug

- Sử dụng analysis board để xem performance metrics
- Kiểm tra FPS và memory usage
- Xem output OSD để verify kết quả

## 📚 Tài Liệu Tham Khảo

- Sample code: `sample/mask_rcnn_sample.cpp`
- Testing guide: `sample/MASKRCNN_TESTING_GUIDE.md`
- TensorFlow Model Zoo: https://github.com/tensorflow/models/blob/master/research/object_detection/g3doc/tf2_detection_zoo.md
