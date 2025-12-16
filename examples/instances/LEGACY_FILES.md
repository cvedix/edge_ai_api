# Legacy Files - Migration Guide

## 📋 Tổng Quan

Các file JSON ở root của `examples/instances/` là các examples cũ. Chúng đã được tổ chức lại vào các thư mục con tương ứng.

## 🔄 Mapping Files

### BA Crossline Files

| Legacy File | New Location | Notes |
|------------|--------------|-------|
| `example_ba_crossline_file_mqtt_only.json` | `ba_crossline/test_file_source_mqtt.json` | Đã được tối ưu |
| `example_ba_crossline_file_mqtt_test.json` | `ba_crossline/test_file_source_mqtt.json` | Duplicate, có thể xóa |
| `example_ba_crossline_in_rtsp_out_mqtt_only.json` | `ba_crossline/test_rtsp_source_mqtt_only.json` | Đã được tối ưu |
| `example_ba_crossline_in_rtsp_out_rtmp_mqtt.json` | `ba_crossline/test_rtsp_source_rtmp_mqtt.json` | Đã được tối ưu |
| `example_ba_crossline_in_rtsp_out_rtmp.json` | `ba_crossline/test_rtsp_source_rtmp_only.json` | Đã được tối ưu |
| `example_ba_crossline_rtmp_mqtt.json` | `ba_crossline/test_rtmp_output_only.json` | Đã được tối ưu |
| `example_ba_crossline_rtmp.json` | `ba_crossline/test_rtmp_output_only.json` | Đã được tối ưu |

### Face Detection Files

| Legacy File | New Location | Notes |
|------------|--------------|-------|
| `example_face_detection_rtmp.json` | `face_detection/test_rtmp_output.json` | Đã được tối ưu |

### MaskRCNN Files

| Legacy File | New Location | Notes |
|------------|--------------|-------|
| `example_mask_rcnn_rtmp.json` | `mask_rcnn/test_rtmp_output.json` | Đã được tối ưu |

### Other Files

| Legacy File | Status | Notes |
|------------|--------|-------|
| `example_face_swap.json` | Keep | Chưa có thư mục riêng |
| `example_insightface_recognition.json` | Keep | Chưa có thư mục riêng |
| `example_mllm_analysis.json` | Keep | Chưa có thư mục riêng |
| `example_rknn_yolov11_detection.json` | Keep | Conditional, chưa có thư mục riêng |
| `example_trt_insightface_recognition.json` | Keep | Conditional, chưa có thư mục riêng |
| `example_yolov11_detection.json` | Keep | Chưa có thư mục riêng |
| `example_full_config.json` | Keep | Reference example |
| `README_MASKRCNN_RTMP.md` | Keep | Documentation |

## 📝 Khuyến Nghị

1. **Sử dụng files mới** trong các thư mục con (`face_detection/`, `ba_crossline/`, `mask_rcnn/`)
2. **Files cũ** có thể được giữ lại cho tương thích ngược nhưng sẽ không được cập nhật
3. **Khi tạo instance mới**, sử dụng files trong thư mục con tương ứng

## 🔍 Tìm File Phù Hợp

1. Xác định loại instance bạn cần (face_detection, ba_crossline, mask_rcnn)
2. Vào thư mục tương ứng
3. Chọn file test phù hợp:
   - `test_file_source.json`: File video input
   - `test_rtsp_source.json`: RTSP stream input
   - `test_rtmp_output.json`: RTMP output
   - `test_mqtt_events.json`: MQTT events

## 📚 Xem Thêm

- [README.md](./README.md) - Tổng quan về instances
- [face_detection/README.md](./face_detection/README.md) - Face detection guide
- [ba_crossline/README.md](./ba_crossline/README.md) - BA crossline guide
- [mask_rcnn/README.md](./mask_rcnn/README.md) - MaskRCNN guide

