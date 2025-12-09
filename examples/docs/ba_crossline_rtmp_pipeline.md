# BA Crossline RTMP Pipeline - Hướng dẫn Chi tiết

## Tổng quan

Instance `ba_crossline_demo_1` là một pipeline hoàn chỉnh để phát hiện và đếm số lượng đối tượng (xe cộ, người, v.v.) đi qua một đường line được định nghĩa trước trong video, sau đó stream kết quả qua RTMP.

### Use Case
- **Đếm xe cộ** qua một đường line trên đường phố
- **Đếm người** qua cửa ra vào
- **Phân tích lưu lượng** tại các điểm kiểm soát
- **Giám sát hành vi** tại các khu vực cụ thể

## Pipeline Architecture

```
[File Source] → [YOLO Detector] → [SORT Tracker] → [BA Crossline] → [OSD] → [Screen DES] ┐
                                                                                          ├─→ [RTMP DES]
                                                                                          └─→ [RTMP DES]
```

### Luồng xử lý

1. **File Source**: Đọc video từ file
2. **YOLO Detector**: Phát hiện đối tượng trong từng frame
3. **SORT Tracker**: Theo dõi đối tượng qua các frame để gán ID
4. **BA Crossline**: Phát hiện khi đối tượng đi qua đường line
5. **OSD**: Vẽ overlay (line, số đếm, ID) lên frame
6. **Screen DES**: Hiển thị kết quả trên màn hình (optional - chỉ khi có DISPLAY)
7. **RTMP DES**: Stream kết quả qua RTMP

## Cấu hình Instance

### File JSON: `example_ba_crossline_rtmp.json`

```json
{
  "name": "ba_crossline_demo_1",
  "group": "demo",
  "solution": "ba_crossline",
  "autoStart": true,
  "additionalParams": {
    "FILE_PATH": "/home/pnsang/project/edge_ai_sdk/cvedix_data/test_video/vehicle_count.mp4",
    "WEIGHTS_PATH": "/home/pnsang/project/edge_ai_sdk/cvedix_data/models/det_cls/yolov3-tiny-2022-0721_best.weights",
    "CONFIG_PATH": "/home/pnsang/project/edge_ai_sdk/cvedix_data/models/det_cls/yolov3-tiny-2022-0721.cfg",
    "LABELS_PATH": "/home/pnsang/project/edge_ai_sdk/cvedix_data/models/det_cls/yolov3_tiny_5classes.txt",
    "RTMP_URL": "rtmp://anhoidong.datacenter.cvedix.com:1935/live/camera_demo_1"
  }
}
```

### Các tham số

| Tham số | Mô tả | Ví dụ |
|---------|-------|-------|
| `name` | Tên instance | `ba_crossline_demo_1` |
| `group` | Nhóm instance | `demo` |
| `solution` | Solution ID | `ba_crossline` |
| `autoStart` | Tự động start khi tạo | `true` |
| `FILE_PATH` | Đường dẫn file video | `/path/to/video.mp4` |
| `WEIGHTS_PATH` | Đường dẫn file weights YOLO | `/path/to/yolo.weights` |
| `CONFIG_PATH` | Đường dẫn file config YOLO | `/path/to/yolo.cfg` |
| `LABELS_PATH` | Đường dẫn file labels | `/path/to/labels.txt` |
| `RTMP_URL` | URL RTMP server | `rtmp://server:port/live/stream` |

## Chi tiết các Nodes

### 1. File Source Node (`file_src`)

**Chức năng**: Đọc video từ file

**Tham số**:
- `file_path`: Đường dẫn file video (từ `FILE_PATH`)
- `channel`: Channel ID (mặc định: 0)
- `resize_ratio`: Tỷ lệ resize (mặc định: 0.4)

**Định dạng hỗ trợ**: MP4, AVI, MOV, MKV, v.v.

### 2. YOLO Detector Node (`yolo_detector`)

**Chức năng**: Phát hiện đối tượng sử dụng YOLO (You Only Look Once)

**Tham số**:
- `weights_path`: Đường dẫn file weights (từ `WEIGHTS_PATH`)
- `config_path`: Đường dẫn file config (từ `CONFIG_PATH`)
- `labels_path`: Đường dẫn file labels (từ `LABELS_PATH`)

**Đầu ra**: Danh sách bounding boxes với class và confidence score

### 3. SORT Tracker Node (`sort_track`)

**Chức năng**: Theo dõi đối tượng qua các frame sử dụng SORT (Simple Online and Realtime Tracking)

**Tham số**: Không có tham số cấu hình

**Đầu ra**: Đối tượng được gán ID duy nhất và theo dõi qua các frame

**Lợi ích**:
- Tránh đếm trùng lặp
- Theo dõi đối tượng qua nhiều frame
- Xử lý trường hợp đối tượng bị che khuất

### 4. BA Crossline Node (`ba_crossline`)

**Chức năng**: Phát hiện khi đối tượng đi qua đường line được định nghĩa

**Tham số**:
- `line_channel`: Channel ID (mặc định: 0)
- `line_start_x`: Tọa độ X điểm đầu (mặc định: 0)
- `line_start_y`: Tọa độ Y điểm đầu (mặc định: 250)
- `line_end_x`: Tọa độ X điểm cuối (mặc định: 700)
- `line_end_y`: Tọa độ Y điểm cuối (mặc định: 220)

**Cấu hình Line**:
- Line được định nghĩa bằng 2 điểm: `start(x, y)` và `end(x, y)`
- Tọa độ phải nằm trong phạm vi kích thước frame
- Có thể định nghĩa nhiều line cho nhiều channel

**Đầu ra**: 
- Số lượng đối tượng đã đi qua line
- Thông tin về từng lần đi qua (timestamp, ID, direction)

**Lưu ý**: 
- Tọa độ line phải được điều chỉnh phù hợp với kích thước video thực tế
- Nếu video được resize, tọa độ cần tính toán lại

### 5. BA Crossline OSD Node (`ba_crossline_osd`)

**Chức năng**: Vẽ overlay lên frame

**Tham số**: Không có tham số cấu hình

**Overlay bao gồm**:
- Đường line được định nghĩa
- Số lượng đối tượng đã đi qua
- ID và bounding box của đối tượng đang được theo dõi
- Hướng đi qua line (nếu có)

### 6. Screen Destination Node (`screen_des`)

**Chức năng**: Hiển thị kết quả trên màn hình

**Tham số**:
- `channel`: Channel ID (mặc định: 0)

**Lưu ý**:
- **Optional**: Node này sẽ được skip tự động nếu không có DISPLAY/WAYLAND_DISPLAY
- Yêu cầu X server hoặc Wayland đang chạy
- Nếu không có display, pipeline vẫn hoạt động bình thường với RTMP output

### 7. RTMP Destination Node (`rtmp_des`)

**Chức năng**: Stream video qua RTMP

**Tham số**:
- `rtmp_url`: URL RTMP server (từ `RTMP_URL`)
- `channel`: Channel ID (mặc định: 0)

**Định dạng stream**:
- Codec: H.264
- Container: FLV
- Bitrate: 1024 kbps (mặc định)

**Lưu ý**:
- RTMP URL sẽ tự động thêm suffix `_0` vào stream key
- Ví dụ: `rtmp://server:port/live/stream` → `rtmp://server:port/live/stream_0`

## Cách sử dụng

### 1. Tạo Instance qua API

```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @examples/instances/example_ba_crossline_rtmp.json
```

### 2. Kiểm tra Instance Status

```bash
curl http://localhost:8080/v1/core/instances/{instanceId}
```

### 3. Start Instance (nếu chưa autoStart)

```bash
curl -X POST http://localhost:8080/v1/core/instances/{instanceId}/start
```

### 4. Stop Instance

```bash
curl -X POST http://localhost:8080/v1/core/instances/{instanceId}/stop
```

### 5. Xem RTMP Stream

Sử dụng VLC hoặc player hỗ trợ RTMP:
```
rtmp://anhoidong.datacenter.cvedix.com:1935/live/camera_demo_1_0
```

## Tùy chỉnh Cấu hình

### Thay đổi Line Detection

Để thay đổi vị trí line, bạn có thể:

1. **Sửa trong Solution Config** (nếu có quyền):
   - Sửa các tham số `line_start_x`, `line_start_y`, `line_end_x`, `line_end_y`

2. **Tạo Custom Solution**:
   - Copy solution `ba_crossline`
   - Sửa các tham số line theo nhu cầu
   - Tạo instance với custom solution

### Thay đổi Resize Ratio

Resize ratio ảnh hưởng đến:
- **Performance**: Tỷ lệ nhỏ hơn = xử lý nhanh hơn nhưng độ chính xác thấp hơn
- **Line Coordinates**: Nếu resize, cần điều chỉnh lại tọa độ line

Mặc định: `0.4` (resize xuống 40% kích thước gốc)

### Thay đổi Model YOLO

Để sử dụng model YOLO khác:

1. Chuẩn bị các file:
   - `*.weights`: File weights
   - `*.cfg`: File config
   - `*.txt`: File labels

2. Cập nhật `additionalParams`:
```json
{
  "WEIGHTS_PATH": "/path/to/new_model.weights",
  "CONFIG_PATH": "/path/to/new_model.cfg",
  "LABELS_PATH": "/path/to/new_labels.txt"
}
```

## Troubleshooting

### 1. Screen DES không hoạt động

**Triệu chứng**: Log hiển thị "screen_des node skipped"

**Nguyên nhân**: Không có DISPLAY/WAYLAND_DISPLAY

**Giải pháp**:
- Đây là hành vi bình thường, không phải lỗi
- Pipeline vẫn hoạt động với RTMP output
- Nếu cần screen display, set `DISPLAY=:0` hoặc `WAYLAND_DISPLAY=wayland-0`

### 2. Line Detection không chính xác

**Triệu chứng**: Đếm sai hoặc không phát hiện được

**Nguyên nhân**: 
- Tọa độ line không phù hợp với video
- Video được resize nhưng tọa độ không được điều chỉnh

**Giải pháp**:
- Kiểm tra kích thước video thực tế
- Điều chỉnh tọa độ line theo tỷ lệ resize
- Ví dụ: Nếu resize 0.4 và video gốc 1920x1080, frame thực tế là 768x432

### 3. RTMP Stream không kết nối được

**Triệu chứng**: Không nhận được stream

**Nguyên nhân**:
- RTMP server không khả dụng
- Firewall chặn port 1935
- URL không đúng

**Giải pháp**:
- Kiểm tra RTMP server đang chạy
- Kiểm tra firewall rules
- Verify RTMP URL format

### 4. Model không load được

**Triệu chứng**: "cv::dnn::readNet load network failed!"

**Nguyên nhân**:
- Đường dẫn file không đúng
- File bị corrupt
- Format không hỗ trợ

**Giải pháp**:
- Kiểm tra đường dẫn file
- Verify file tồn tại và có quyền đọc
- Kiểm tra format file (YOLO v3/v4)

## Performance Tips

1. **Resize Ratio**: 
   - Giảm `resize_ratio` để tăng tốc độ xử lý
   - Tăng `resize_ratio` để tăng độ chính xác

2. **Model Selection**:
   - YOLOv3-tiny: Nhanh nhưng độ chính xác thấp
   - YOLOv4: Chậm hơn nhưng chính xác hơn

3. **Hardware Acceleration**:
   - Sử dụng GPU nếu có thể
   - Cấu hình CUDA/OpenVINO trong config

## Kết quả và Output

### Log Output

Pipeline sẽ log các thông tin:
- Số lượng đối tượng đã đi qua line
- Timestamp của mỗi lần đi qua
- ID của đối tượng

Ví dụ log:
```
[ba_crossline] [channel 0] has found target cross line, total number of crossline: [5]
[ba_crossline] [channel 0] image & video record file names are: [crossline_image__20251209163420806 & ]
```

### RTMP Stream

Stream output bao gồm:
- Video với overlay (line, bounding boxes, số đếm)
- Real-time processing
- H.264 encoded

### Screen Display (nếu có)

Hiển thị trực tiếp trên màn hình với:
- Overlay đầy đủ
- Real-time updates
- Debug information

## Tài liệu tham khảo

- [Solution Registry](../src/solutions/solution_registry.cpp)
- [Pipeline Builder](../src/core/pipeline_builder.cpp)
- [Sample Code](../../sample/ba_crossline_sample.cpp)
- [API Documentation](../../docs/CREATE_INSTANCE_GUIDE.md)

## Liên hệ và Hỗ trợ

Nếu gặp vấn đề, vui lòng:
1. Kiểm tra logs trong `/var/lib/edge_ai_api/logs/`
2. Xem troubleshooting section ở trên
3. Tham khảo documentation chính thức

