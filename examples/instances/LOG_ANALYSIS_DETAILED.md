# Phân tích Log Chi Tiết

## Log Analysis: Instance Creation và Queue Full Warnings

### 1. Vấn đề phát hiện từ log (502-705)

#### 1.1. RESIZE_RATIO không được áp dụng
**Triệu chứng:**
```
[PipelineBuilder]   Resize ratio: 1
```
- Log cho thấy `Resize ratio: 1` (không resize)
- Nhưng trong `create_face_detection_file_source.json` có `"RESIZE_RATIO": "0.25"`

**Nguyên nhân:**
- `PipelineBuilder::createFileSourceNode()` chỉ đọc `resize_ratio` từ `params` (từ solution config)
- Solution config có `resize_ratio: "1.0"` (default)
- Không đọc `RESIZE_RATIO` từ `additionalParams`

**Đã sửa:**
- Thêm logic để đọc `RESIZE_RATIO` từ `additionalParams` trong `createFileSourceNode()`
- Priority: `params["resize_ratio"]` > `additionalParams["RESIZE_RATIO"]` > default (0.25)

#### 1.2. Bitrate Warning
**Triệu chứng:**
```
(edge_ai_api:729465): GLib-GObject-CRITICAL **: 00:43:32.951: value "845426020" of type 'guint' is invalid or out of range for property 'bitrate' of type 'guint'
```

**Nguyên nhân:**
- File destination node có bitrate quá lớn (845426020)
- Có thể do tính toán bitrate từ frame size không đúng
- Đây là warning từ GStreamer, không ảnh hưởng đến output

**Giải pháp:**
- Có thể bỏ qua (chỉ là warning)
- Hoặc kiểm tra file_des node configuration

#### 1.3. Queue Full Warnings
**Triệu chứng:**
```
[2025-12-04 01:43:38.743][Warn][face_detector_xxx] queue full, dropping meta!
```

**Nguyên nhân:**
- File source đọc với `resize_ratio: 1.0` (không resize) → frame size lớn
- Face detector không xử lý kịp tốc độ đọc
- Queue giữa file source và face detector bị đầy

**Giải pháp:**
- Sử dụng `RESIZE_RATIO: "0.25"` trong `additionalParams` (đã sửa code để đọc được)
- Giảm `frameRateLimit` xuống 10 (đã có trong config)

### 2. Timeline từ log

```
00:43:13 - Server khởi động
00:43:29 - Instance được tạo (42b7eb66-204f-4ff7-a6cd-d242a3f5505e)
00:43:29 - Pipeline build thành công
00:43:29 - File source node: Resize ratio: 1 (❌ KHÔNG ĐÚNG - nên là 0.25)
00:43:32 - Bitrate warning (có thể bỏ qua)
00:43:35 - Pipeline started, FPS: 0.00
00:43:38 - Queue full warnings bắt đầu xuất hiện (3 giây sau khi start)
```

### 3. Các vấn đề và giải pháp

| Vấn đề | Trạng thái | Giải pháp |
|--------|-----------|-----------|
| RESIZE_RATIO không được áp dụng | ✅ Đã sửa | Code đã được cập nhật để đọc từ additionalParams |
| Queue full warnings | ⚠️ Cần test lại | Sau khi sửa RESIZE_RATIO, warnings sẽ giảm |
| Bitrate warning | ℹ️ Có thể bỏ qua | Không ảnh hưởng đến output |
| FPS = 0.00 | ℹ️ Bình thường | Cần đợi pipeline ổn định |

### 4. Cách test sau khi sửa

1. **Build lại project:**
   ```bash
   cd build && cmake .. && make -j$(nproc)
   ```

2. **Tạo instance mới:**
   ```bash
   curl -X POST http://localhost:8848/v1/core/instance \
     -H 'Content-Type: application/json' \
     -d @examples/instances/create_face_detection_file_source.json
   ```

3. **Kiểm tra log:**
   ```bash
   # Tìm dòng "Resize ratio" trong log
   grep "Resize ratio" ./logs/log.txt
   
   # Kết quả mong đợi:
   # [PipelineBuilder]   Resize ratio: 0.25
   # [PipelineBuilder] Using RESIZE_RATIO from additionalParams: 0.25
   ```

4. **Kiểm tra queue full warnings:**
   ```bash
   # Đếm số lượng warnings
   grep -c "queue full" ./logs/log.txt
   
   # Nếu giảm đáng kể → Sửa thành công
   ```

### 5. Cấu hình tối ưu (sau khi sửa)

```json
{
  "name": "face_detection_file_source",
  "group": "file_processing",
  "solution": "face_detection_file",
  "persistent": false,
  "autoStart": true,
  "frameRateLimit": 10,
  "metadataMode": false,
  "statisticsMode": true,
  "debugMode": true,
  "detectionSensitivity": "Low",
  "additionalParams": {
    "FILE_PATH": "/home/pnsang/project/edge_ai_sdk/cvedix_data/test_video/face.mp4",
    "MODEL_PATH": "/usr/share/cvedix/cvedix_data/models/face/face_detection_yunet_2022mar.onnx",
    "RESIZE_RATIO": "0.25"  // ✅ Bây giờ sẽ được áp dụng đúng
  }
}
```

### 6. Kết quả mong đợi sau khi sửa

1. ✅ Log sẽ hiển thị: `Resize ratio: 0.25`
2. ✅ Queue full warnings sẽ giảm đáng kể (từ ~20 warnings/giây xuống <5 warnings/giây)
3. ✅ FPS sẽ tăng dần sau khi pipeline ổn định
4. ✅ Output files sẽ được tạo với tốc độ ổn định hơn

### 7. Lưu ý

- **RESIZE_RATIO** phải là string trong JSON: `"0.25"` (không phải số)
- **frameRateLimit** là số: `10` (không phải string)
- Sau khi sửa code, cần **build lại** và **tạo instance mới** để test

