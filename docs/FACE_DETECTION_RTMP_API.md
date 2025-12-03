# Face Detection với RTMP Streaming - Hướng Dẫn Sử Dụng API

Tài liệu này hướng dẫn cách tạo một instance face detection với RTMP streaming thông qua API, tương đương với code C++ đã cung cấp.

## Pipeline

Pipeline bao gồm các node sau (theo thứ tự):
1. **File Source** - Đọc video từ file
2. **YuNet Face Detector** - Phát hiện khuôn mặt
3. **SFace Feature Encoder** - Mã hóa đặc trưng khuôn mặt
4. **Face OSD v2** - Vẽ overlay lên video
5. **RTMP Destination** - Stream video ra RTMP server

## Tạo Instance qua API

### Endpoint
```
POST /v1/core/instance
```

### Request Body

```json
{
  "name": "face_detection_demo_1",
  "group": "demo",
  "solution": "face_detection_rtmp",
  "persistent": true,
  "autoStart": true,
  "detectionSensitivity": "Low",
  "additionalParams": {
    "FILE_PATH": "/home/pnsang/project/edge_ai_sdk/cvedix_data/test_video/face.mp4",
    "RTMP_URL": "rtmp://localhost:1935/live/camera_demo_1",
    "MODEL_PATH": "/usr/share/cvedix/cvedix_data/models/face/face_detection_yunet_2022mar.onnx",
    "SFACE_MODEL_PATH": "/home/pnsang/project/edge_ai_sdk/cvedix_data/models/face/face_recognition_sface_2021dec.onnx",
    "RESIZE_RATIO": "1.0"
  }
}
```

### Các Tham Số

#### Tham số bắt buộc:
- `name`: Tên instance (pattern: `^[A-Za-z0-9 -_]+$`)
- `solution`: Phải là `"face_detection_rtmp"`

#### Tham số tùy chọn:
- `group`: Nhóm instance
- `persistent`: Lưu instance sau khi restart (default: `false`)
- `autoStart`: Tự động start pipeline sau khi tạo (default: `false`)
- `detectionSensitivity`: Độ nhạy phát hiện - `"Low"`, `"Medium"`, hoặc `"High"` (default: `"Low"`)

#### Tham số trong `additionalParams`:
- `FILE_PATH`: Đường dẫn đến file video input (required)
- `RTMP_URL`: URL RTMP server để stream (required)
- `MODEL_PATH`: Đường dẫn đến model YuNet face detector (optional, có default)
- `SFACE_MODEL_PATH`: Đường dẫn đến model SFace encoder (optional, có default)
- `RESIZE_RATIO`: Tỷ lệ resize video (optional, default: `"0.5"`). Sử dụng `"1.0"` để không resize
- `MODEL_NAME`: Tên model (có thể dùng thay cho MODEL_PATH, format: `"category:modelname"` hoặc `"modelname"`)
- `SFACE_MODEL_NAME`: Tên model SFace (tương tự MODEL_NAME)

### Ví dụ sử dụng cURL

```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d '{
    "name": "face_detection_demo_1",
    "group": "demo",
    "solution": "face_detection_rtmp",
    "persistent": true,
    "autoStart": true,
    "detectionSensitivity": "Low",
    "additionalParams": {
      "FILE_PATH": "/home/pnsang/project/edge_ai_sdk/cvedix_data/test_video/face.mp4",
      "RTMP_URL": "rtmp://localhost:1935/live/camera_demo_1",
      "MODEL_PATH": "/usr/share/cvedix/cvedix_data/models/face/face_detection_yunet_2022mar.onnx",
      "SFACE_MODEL_PATH": "/home/pnsang/project/edge_ai_sdk/cvedix_data/models/face/face_recognition_sface_2021dec.onnx",
      "RESIZE_RATIO": "1.0"
    }
  }'
```

### Response

```json
{
  "instanceId": "abc123-def456-ghi789",
  "displayName": "face_detection_demo",
  "group": "demo",
  "solutionId": "face_detection_rtmp",
  "solutionName": "Face Detection with RTMP Streaming",
  "persistent": false,
  "loaded": true,
  "running": true,
  "fps": 0.0,
  "version": "1.0.0",
  "frameRateLimit": 0,
  "metadataMode": false,
  "statisticsMode": false,
  "diagnosticsMode": false,
  "debugMode": false,
  "readOnly": false,
  "autoStart": true,
  "autoRestart": false,
  "systemInstance": false,
  "inputPixelLimit": 0,
  "inputOrientation": 0,
  "detectorMode": "SmartDetection",
  "detectionSensitivity": "Low",
  "movementSensitivity": "Low",
  "sensorModality": "RGB",
  "originator": {
    "address": "127.0.0.1"
  }
}
```

## Lưu ý

1. **RTMP URL**: RTMP node tự động thêm suffix `"_0"` vào stream key. Ví dụ:
   - URL cung cấp: `rtmp://server:1935/live/camera_demo_1`
   - URL thực tế: `rtmp://server:1935/live/camera_demo_1_0`

2. **RTSP Alternative**: Nếu server hỗ trợ auto-conversion, có thể xem qua RTSP:
   ```
   rtsp://localhost:8554/live/camera_demo_1_0
   ```

3. **Model Paths**: Nếu không cung cấp `MODEL_PATH` hoặc `SFACE_MODEL_PATH`, hệ thống sẽ tự động tìm model trong các thư mục:
   - `./cvedix_data/models/face/`
   - `/usr/share/cvedix/cvedix_data/models/face/`
   - Hoặc từ biến môi trường `CVEDIX_DATA_ROOT` hoặc `CVEDIX_SDK_ROOT`

4. **File Path**: Đảm bảo file video tồn tại tại đường dẫn được chỉ định trong `FILE_PATH`.

5. **⚠️ Shape Mismatch Error**: Nếu bạn gặp lỗi shape mismatch như:
   ```
   [ERROR] OPENCV/DNN: [Eltwise]:(onnx_node!Add_44): getMemoryShapes() throws exception
   inputs[0] = [ 1 64 11 20 ]
   inputs[1] = [ 1 64 10 20 ]
   ```
   
   **Nguyên nhân**: Ngay cả với model YuNet 2023mar, lỗi shape mismatch vẫn có thể xảy ra nếu:
   - Video có resolution không đều (một số frames có kích thước khác nhau)
   - Resize không đảm bảo tất cả frames có cùng kích thước chính xác
   - Model nhận frames với kích thước khác nhau trong quá trình xử lý
   
   **Giải pháp** (theo thứ tự ưu tiên):
   
   1. **Re-encode video với resolution cố định** (Khuyến nghị nhất):
      
      **Lưu ý**: Một số resolution có thể không tương thích với model. Hãy thử các resolution khác nhau:
      
      ```bash
      # Option 1: 320x240 (4:3 aspect ratio, thường ổn định hơn)
      ffmpeg -i input.mp4 -vf "scale=320:240:force_original_aspect_ratio=decrease,pad=320:240:(ow-iw)/2:(oh-ih)/2" \
             -c:v libx264 -preset fast -crf 23 -c:a copy output_320x240.mp4
      
      # Option 2: 480x270 (16:9 aspect ratio)
      ffmpeg -i input.mp4 -vf "scale=480:270:force_original_aspect_ratio=decrease,pad=480:270:(ow-iw)/2:(oh-ih)/2" \
             -c:v libx264 -preset fast -crf 23 -c:a copy output_480x270.mp4
      
      # Option 3: 640x360 (16:9, đã test nhưng vẫn có thể gặp lỗi)
      ffmpeg -i input.mp4 -vf "scale=640:360:force_original_aspect_ratio=decrease,pad=640:360:(ow-iw)/2:(oh-ih)/2" \
             -c:v libx264 -preset fast -crf 23 -c:a copy output_640x360.mp4
      
      # Option 4: 320x180 (16:9, nhỏ hơn, nhanh hơn)
      ffmpeg -i input.mp4 -vf "scale=320:180:force_original_aspect_ratio=decrease,pad=320:180:(ow-iw)/2:(oh-ih)/2" \
             -c:v libx264 -preset fast -crf 23 -c:a copy output_320x180.mp4
      ```
      
      **Quan trọng**: Sau khi re-encode, sử dụng `resize_ratio = 1.0` (không resize) trong config để tránh double-resize:
      ```json
      "additionalParams": {
        "FILE_PATH": "/path/to/re-encoded_video.mp4",
        "RESIZE_RATIO": "1.0"
      }
      ```
      
      Hoặc nếu không override, hệ thống mặc định đã dùng `resize_ratio = 1.0`.
   
   2. **Sử dụng model YuNet 2023mar** (đã được cập nhật trong config):
      ```json
      "MODEL_PATH": "./cvedix_data/models/face/face_detection_yunet_2023mar.onnx"
      ```
      Model 2023mar xử lý dynamic input tốt hơn 2022mar.
   
   3. **Thử resize_ratio khác** (nếu không thể re-encode):
      - Mặc định hiện tại: `0.5` (640x360 từ 1280x720)
      - Thử `0.25` (320x180) nếu vẫn gặp lỗi
      - Thử `0.125` (160x90) cho input rất nhỏ
      
      Có thể override trong request:
      ```json
      "additionalParams": {
        "FILE_PATH": "...",
        "RESIZE_RATIO": "0.25"
      }
      ```
   
   4. **Kiểm tra video resolution**:
      ```bash
      # Kiểm tra xem video có resolution đều không
      ffprobe -v error -select_streams v:0 -show_entries frame=width,height \
              -of csv=s=x:p=0 input.mp4 | sort -u
      ```
      Nếu output có nhiều kích thước khác nhau, video cần được re-encode.

## Quản lý Instance

### Xem danh sách instances
```
GET /v1/core/instances
```

### Xem thông tin instance cụ thể
```
GET /v1/core/instance/{instanceId}
```

### Dừng instance
```
POST /v1/core/instance/{instanceId}/stop
```

### Khởi động lại instance
```
POST /v1/core/instance/{instanceId}/start
```

### Xóa instance
```
DELETE /v1/core/instance/{instanceId}
```

## Vấn Đề Khi Restart

### Vấn đề: Lỗi khi restart nhưng create thì chạy được

**Triệu chứng:**
- Khi tạo instance mới (create), video chạy bình thường
- Khi restart instance (stop rồi start lại), gặp lỗi shape mismatch:
  ```
  OpenCV(4.6.0) ./modules/dnn/src/layers/eltwise_layer.cpp:251: error: (-215:Assertion failed) 
  inputs[vecIdx][j] == inputs[i][j] in function 'getMemoryShapes'
  ```

**Nguyên nhân:**
1. **OpenCV DNN state cache**: Khi restart, OpenCV DNN có thể vẫn giữ state cũ từ lần chạy trước
2. **Model chưa sẵn sàng**: Model cần thời gian để reload và clear state cũ trước khi xử lý frame đầu tiên
3. **Timing issue**: Frame đầu tiên có thể được gửi đến model trước khi model sẵn sàng xử lý

**Giải pháp đã được áp dụng:**
- Tăng delay sau khi rebuild pipeline (2 giây) để OpenCV DNN clear state cũ
- Tăng delay trước khi start file source (500ms cho restart vs 200ms cho create)
- Tăng delay sau khi start file source (3 giây cho restart vs 1 giây cho create)
- Đảm bảo model có đủ thời gian để initialize trước khi nhận frame đầu tiên

**Nếu vẫn gặp lỗi sau khi áp dụng fix:**
1. **Re-encode video với resolution cố định** (khuyến nghị nhất):
   ```bash
   ffmpeg -i input.mp4 -vf "scale=320:240:force_original_aspect_ratio=decrease,pad=320:240:(ow-iw)/2:(oh-ih)/2" \
          -c:v libx264 -preset fast -crf 23 -c:a copy output_320x240.mp4
   ```
   Sau đó sử dụng `RESIZE_RATIO: "1.0"` trong `additionalParams`.

2. **Sử dụng model YuNet 2023mar** thay vì 2022mar:
   ```json
   "MODEL_PATH": "./cvedix_data/models/face/face_detection_yunet_2023mar.onnx"
   ```

3. **Kiểm tra video có resolution đều không**:
   ```bash
   ffprobe -v error -select_streams v:0 -show_entries frame=width,height \
           -of csv=s=x:p=0 input.mp4 | sort -u
   ```
   Nếu output có nhiều kích thước khác nhau, video cần được re-encode.

