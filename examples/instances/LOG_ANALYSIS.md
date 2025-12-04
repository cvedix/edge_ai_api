# Phân tích Log và Xử lý Vấn đề

## 1. Warning: "queue full, dropping meta!"

### Triệu chứng
```
[Warn][./nodes/common/cvedix_node.cpp:147] [face_detector_xxx] queue full, dropping meta!
```

### Nguyên nhân
- **File source đọc quá nhanh**: File source node đọc video với tốc độ cao (có thể 30-60 FPS)
- **Face detector xử lý chậm hơn**: Face detector node không xử lý kịp tốc độ đọc
- **Queue overflow**: Queue giữa file source và face detector bị đầy, metadata bị drop

### Giải pháp

#### Giải pháp 1: Thêm frameRateLimit (Khuyến nghị)
```json
{
  "frameRateLimit": 10
}
```
Giới hạn tốc độ đọc file để phù hợp với tốc độ xử lý. Giá trị thấp hơn = ít queue full hơn.

#### Giải pháp 2: Giảm kích thước frame (RESIZE_RATIO)
```json
{
  "additionalParams": {
    "RESIZE_RATIO": "0.25"
  }
}
```
Giảm kích thước frame giúp face detector xử lý nhanh hơn:
- `0.25` = 1280x720 → 320x180 (nhanh nhất, khuyến nghị)
- `0.5` = 1280x720 → 640x360 (cân bằng)
- `1.0` = Không resize (chậm nhất, dễ queue full)

#### Giải pháp 3: Kết hợp cả hai (Tối ưu nhất)
```json
{
  "frameRateLimit": 10,
  "additionalParams": {
    "RESIZE_RATIO": "0.25"
  }
}
```

### Kiểm tra hiệu quả
```bash
# Kiểm tra số lượng warnings
grep -c "queue full" /var/log/edge_ai_api.log

# Nếu giảm đáng kể → Giải pháp hiệu quả
# Nếu vẫn nhiều → Thử giảm frameRateLimit hoặc RESIZE_RATIO hơn nữa
```

## 2. Lỗi GStreamer khi cycle video

### Triệu chứng
```
[ WARN:0@188.667] global ./modules/videoio/src/cap_gstreamer.cpp (2401) handleMessage OpenCV | GStreamer warning: Embedded video playback halted; module qtdemux0 reported: Internal data stream error.
[ WARN:0@188.667] global ./modules/videoio/src/cap_gstreamer.cpp (897) startPipeline OpenCV | GStreamer warning: unable to start pipeline
[2025-12-04 01:37:23.609][Info][./nodes/src/cvedix_file_src_node.cpp:72] [file_src_xxx] reading frame complete, total frame==>2985
[2025-12-04 01:37:23.609][Info][./nodes/src/cvedix_file_src_node.cpp:74] [file_src_xxx] cycle flag is true, continue!
```

### Nguyên nhân
- **File source tự động cycle**: CVEDIX SDK tự động cycle lại video khi đọc hết (cycle flag = true)
- **GStreamer pipeline lỗi**: Khi cycle lại, GStreamer pipeline không thể khởi động lại đúng cách
- **Đây là lỗi từ CVEDIX SDK**: Không phải lỗi từ code của chúng ta

### Giải pháp

#### Giải pháp 1: Stop instance sau khi video kết thúc (Khuyến nghị)
```bash
# Monitor instance và stop khi video kết thúc
# Sử dụng script monitor_instance.sh
./examples/instances/monitor_instance.sh {instanceId}
```

#### Giải pháp 2: Bỏ qua warnings (Nếu không quan trọng)
- Warnings này không ảnh hưởng đến output files đã được tạo
- Instance vẫn hoạt động, chỉ là không cycle được
- Có thể bỏ qua nếu chỉ cần xử lý video một lần

#### Giải pháp 3: Sử dụng video ngắn hơn
- Nếu video quá dài, có thể cắt thành các đoạn ngắn hơn
- Xử lý từng đoạn một thay vì cycle

### Lưu ý
- **Đây là limitation của CVEDIX SDK**: Không có parameter để tắt cycle
- **Output files vẫn được tạo thành công**: Lỗi chỉ xảy ra khi cycle, không ảnh hưởng đến frames đã xử lý
- **Có thể restart instance**: Nếu cần xử lý lại, có thể restart instance thay vì cycle

## 3. FPS = 0.00 nhưng instance đang running

### Triệu chứng
- Instance `running: true`
- `fps: 0.00`
- Có output files được tạo

### Nguyên nhân
- Instance đang xử lý nhưng FPS chưa được cập nhật
- Queue đầy khiến frames bị drop, không được đếm vào FPS
- File destination node đang ghi nhưng pipeline chưa ổn định

### Giải pháp
1. Đợi thêm 10-30 giây để pipeline ổn định
2. Kiểm tra output files có được tạo không:
   ```bash
   ls -lht ./build/output/{instanceId}/
   ```
3. Nếu có files → Instance đang hoạt động, chỉ là FPS chưa cập nhật
4. Nếu không có files → Có thể có lỗi trong pipeline

## 4. Output files được tạo nhưng có warnings

### Trường hợp này là BÌNH THƯỜNG
- Output files vẫn được tạo thành công
- Warnings "queue full" chỉ là cảnh báo về performance
- Instance vẫn hoạt động, chỉ là không tối ưu

### Cách xử lý
1. **Nếu không quan trọng về tốc độ**: Có thể bỏ qua warnings
2. **Nếu muốn tối ưu**: Áp dụng các giải pháp ở trên

## 5. Checklist kiểm tra

### ✅ Instance hoạt động tốt nếu:
- [ ] Output files được tạo liên tục
- [ ] Files có kích thước hợp lý (> 0 bytes)
- [ ] Files có timestamp gần đây
- [ ] FPS > 0 (sau khi ổn định)

### ⚠️ Cần điều chỉnh nếu:
- [ ] Có quá nhiều "queue full" warnings (> 100)
- [ ] FPS = 0 sau 30 giây
- [ ] Không có output files sau 1 phút

## 6. Cấu hình tối ưu

### Cho file source với face detection (Khuyến nghị):
```json
{
  "frameRateLimit": 10,
  "additionalParams": {
    "RESIZE_RATIO": "0.25",
    "FILE_PATH": "/path/to/video.mp4",
    "MODEL_PATH": "/path/to/model.onnx"
  }
}
```
**Lý do:**
- `frameRateLimit: 10` - Giảm tốc độ đọc để tránh queue full
- `RESIZE_RATIO: "0.25"` - Giảm kích thước frame để xử lý nhanh hơn (320x180 từ 1280x720)

### Cho RTSP source:
```json
{
  "frameRateLimit": 25,
  "additionalParams": {
    "RTSP_URL": "rtsp://example.com:8554/live/stream",
    "RESIZE_RATIO": "1.0"
  }
}
```

## 7. Script phân tích log

Sử dụng script `analyze_log.sh` để phân tích tự động:

```bash
./examples/instances/analyze_log.sh {instanceId}
```

Script sẽ:
- Kiểm tra instance status
- Đếm số lượng warnings
- Kiểm tra output files
- Đề xuất giải pháp

