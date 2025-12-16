# Flexible Input/Output cho Tất Cả Instances

## 📋 Tổng Quan

Từ bây giờ, **TẤT CẢ** các instance của bạn đều có thể tự do chọn input và output mà SDK hỗ trợ, **KHÔNG CẦN** định nghĩa trong solution configuration.

## 🎯 Tính Năng

### ✅ Input Tự Động Phát Hiện

Pipeline builder tự động phát hiện loại input từ parameters:

- **File Video**: `FILE_PATH` với đường dẫn file
- **RTSP Stream**: `RTSP_SRC_URL` hoặc `FILE_PATH` với URL `rtsp://...`
- **RTMP Stream**: `RTMP_SRC_URL` hoặc `FILE_PATH` với URL `rtmp://...`
- **HLS/HTTP**: `HLS_URL`, `HTTP_URL` hoặc `FILE_PATH` với URL `http://...` hoặc `.m3u8`

### ✅ Output Tự Động Thêm

Pipeline builder tự động thêm các output nodes nếu có cấu hình tương ứng:

- **MQTT Events**: Tự động thêm nếu có `MQTT_BROKER_URL`
  - Nếu có `ba_crossline` trong pipeline → Thêm `json_crossline_mqtt_broker`
  - Ngược lại → Thêm `json_mqtt_broker`
- **RTMP Streaming**: Tự động thêm nếu có `RTMP_URL`
- **Screen Display**: Tự động thêm nếu có `ENABLE_SCREEN_DES=true`
- **File Recording**: Tự động thêm nếu có `RECORD_PATH` (đã có sẵn)

## 🔧 Cách Sử Dụng

### Ví Dụ 1: Solution đơn giản, thêm input/output tùy ý

**Solution** (chỉ có core pipeline):
```json
{
  "solutionId": "face_detection",
  "pipeline": [
    {"nodeType": "file_src", ...},
    {"nodeType": "yunet_face_detector", ...},
    {"nodeType": "face_osd_v2", ...}
  ]
}
```

**Instance** (tự do chọn input/output):
```json
{
  "name": "my_face_detection",
  "solution": "face_detection",
  "additionalParams": {
    "RTSP_SRC_URL": "rtsp://camera-ip:8554/stream",  // Input: RTSP
    "MQTT_BROKER_URL": "localhost",                   // Output: MQTT
    "MQTT_PORT": "1883",
    "MQTT_TOPIC": "face_events",
    "RTMP_URL": "rtmp://server:1935/live/stream",     // Output: RTMP
    "ENABLE_SCREEN_DES": "true"                        // Output: Screen
  }
}
```

**Kết quả**: Pipeline sẽ tự động:
1. Thay `file_src` thành `rtsp_src` (từ RTSP_SRC_URL)
2. Thêm `json_mqtt_broker` node (từ MQTT_BROKER_URL)
3. Thêm `rtmp_des` node (từ RTMP_URL)
4. Thêm `screen_des` node (từ ENABLE_SCREEN_DES=true)

### Ví Dụ 2: BA Crossline với input/output linh hoạt

**Solution** (có thể đơn giản hoặc đầy đủ):
```json
{
  "solutionId": "ba_crossline",
  "pipeline": [
    {"nodeType": "file_src", ...},
    {"nodeType": "yolo_detector", ...},
    {"nodeType": "sort_track", ...},
    {"nodeType": "ba_crossline", ...},
    {"nodeType": "ba_crossline_osd", ...}
  ]
}
```

**Instance** (chọn input/output tùy ý):
```json
{
  "name": "crossline_rtsp_mqtt",
  "solution": "ba_crossline",
  "additionalParams": {
    "RTSP_SRC_URL": "rtsp://camera:8554/stream",      // Input: RTSP
    "MQTT_BROKER_URL": "mqtt.broker.com",             // Output: MQTT (tự động dùng crossline broker)
    "MQTT_PORT": "1883",
    "MQTT_TOPIC": "ba_crossline/events",
    "ZONE_ID": "zone_1",
    "ZONE_NAME": "Main Road"
  }
}
```

**Kết quả**: Pipeline sẽ tự động:
1. Thay `file_src` thành `rtsp_src`
2. Thêm `json_crossline_mqtt_broker` node (vì có `ba_crossline` trong pipeline)

### Ví Dụ 3: Chỉ có input, không có output

```json
{
  "name": "simple_detection",
  "solution": "face_detection",
  "additionalParams": {
    "FILE_PATH": "/path/to/video.mp4"  // Chỉ input, không output
  }
}
```

**Kết quả**: Chỉ có input, không có output nodes nào được thêm.

## 📊 Logic Auto-Injection

### Input Detection Priority:
1. `RTSP_SRC_URL` → RTSP source
2. `RTMP_SRC_URL` → RTMP source  
3. `HLS_URL` → FFmpeg source (HLS)
4. `HTTP_URL` → FFmpeg source (HTTP)
5. `FILE_PATH`:
   - Nếu bắt đầu bằng `rtsp://` → RTSP source
   - Nếu bắt đầu bằng `rtmp://` → RTMP source
   - Nếu bắt đầu bằng `http://` hoặc `https://` → FFmpeg source
   - Ngược lại → File source

### Output Auto-Injection:
- **MQTT Broker**: 
  - Kiểm tra `MQTT_BROKER_URL` (không rỗng)
  - Nếu có `ba_crossline` node → Thêm `json_crossline_mqtt_broker`
  - Ngược lại → Thêm `json_mqtt_broker`
  - Chỉ thêm nếu chưa có broker node trong pipeline

- **RTMP Destination**:
  - Kiểm tra `RTMP_URL` (không rỗng)
  - Chỉ thêm nếu chưa có `rtmp_des` trong pipeline

- **Screen Destination**:
  - Kiểm tra `ENABLE_SCREEN_DES` (true/1/yes/on)
  - Chỉ thêm nếu chưa có `screen_des` trong pipeline

## ⚠️ Lưu Ý Quan Trọng

1. **Không Trùng Lặp**: Các nodes chỉ được tự động thêm nếu **CHƯA CÓ** trong pipeline
2. **Input Ưu Tiên**: Nếu solution đã có input node, nó sẽ được thay thế dựa trên parameters
3. **Output Kết Hợp**: Có thể bật nhiều output cùng lúc (MQTT + RTMP + Screen)
4. **Vị Trí Node**: Các output nodes được tự động attach vào node phù hợp cuối cùng trong pipeline

## 🚀 Lợi Ích

1. **Linh Hoạt**: Không cần tạo nhiều solution cho các input/output khác nhau
2. **Đơn Giản**: Chỉ cần một solution core, thêm parameters khi tạo instance
3. **Tự Động**: Pipeline builder tự động xử lý mọi thứ
4. **Tương Thích**: Hoạt động với tất cả solution hiện có

## 📝 Best Practices

1. **Solution Design**: Tạo solution với core pipeline (detection/analysis logic)
2. **Instance Configuration**: Thêm input/output parameters khi tạo instance
3. **Testing**: Test với các input/output khác nhau để đảm bảo hoạt động đúng
4. **Documentation**: Document các parameters cần thiết cho từng solution

## 🔍 Debugging

Khi tạo instance, pipeline builder sẽ log:
- `[PipelineBuilder] Auto-adding <node_type> node (<reason> detected)`
- `[PipelineBuilder] ✓ Auto-added <node_type> node`

Nếu không thấy log này, có thể:
- Parameter không được cung cấp hoặc rỗng
- Node đã có trong pipeline
- Có lỗi khi tạo node (xem log chi tiết)

---

# BA Crossline - Solution Chi Tiết

## 📋 Tổng Quan

Solution `ba_crossline` là một solution linh hoạt cho phép bạn:
- **Input linh hoạt**: Tự động phát hiện và hỗ trợ video file, RTSP, RTMP
- **Output tùy chọn**: Có thể chọn MQTT, RTMP, Screen hoặc kết hợp nhiều output
- **Tất cả giá trị được đọc từ cấu hình**, không hardcode

## 📁 Cấu Trúc Pipeline

```
[Input Source] → YOLO Detector → SORT Tracker → BA Crossline → [Optional: MQTT Broker] → OSD → [Optional: Screen/RTMP Output]
```

### Input Sources (tự động phát hiện):
- **File**: `FILE_PATH` với đường dẫn file video
- **RTSP**: `RTSP_SRC_URL` hoặc `FILE_PATH` với URL bắt đầu bằng `rtsp://`
- **RTMP**: `RTMP_SRC_URL` hoặc `FILE_PATH` với URL bắt đầu bằng `rtmp://`

### Output Options:
- **MQTT**: Tự động bật nếu có `MQTT_BROKER_URL` (sử dụng `json_crossline_mqtt_broker`)
- **RTMP**: Tự động bật nếu có `RTMP_URL`
- **Screen**: Điều khiển bằng `ENABLE_SCREEN_DES` (true/false)

## 🔧 Cấu Hình Parameters

### Parameters Bắt Buộc:
- `WEIGHTS_PATH`: Đường dẫn file weights của YOLO model
- `CONFIG_PATH`: Đường dẫn file config của YOLO model
- `LABELS_PATH`: Đường dẫn file labels của YOLO model
- `CROSSLINE_START_X`, `CROSSLINE_START_Y`: Điểm bắt đầu của line
- `CROSSLINE_END_X`, `CROSSLINE_END_Y`: Điểm kết thúc của line

### Parameters Tùy Chọn:

#### Input:
- `FILE_PATH`: Đường dẫn file video hoặc URL (rtsp://, rtmp://)
- `RTSP_SRC_URL`: URL RTSP source (ưu tiên hơn FILE_PATH nếu có)
- `RTMP_SRC_URL`: URL RTMP source (ưu tiên hơn FILE_PATH nếu có)
- `RESIZE_RATIO`: Tỷ lệ resize (mặc định: 0.4)

#### Output - MQTT:
- `MQTT_BROKER_URL`: Địa chỉ MQTT broker (bắt buộc để bật MQTT output)
- `MQTT_PORT`: Port MQTT broker (mặc định: 1883)
- `MQTT_TOPIC`: Topic để publish events (mặc định: "events")
- `MQTT_USERNAME`: Username MQTT (tùy chọn)
- `MQTT_PASSWORD`: Password MQTT (tùy chọn)
- `ZONE_ID`: ID của zone (mặc định: "default_zone")
- `ZONE_NAME`: Tên zone (mặc định: "CrosslineZone")

#### Output - RTMP:
- `RTMP_URL`: URL RTMP destination (bắt buộc để bật RTMP output)

#### Output - Screen:
- `ENABLE_SCREEN_DES`: Bật/tắt screen display (true/false, mặc định: false)

## 📝 Ví Dụ Sử Dụng BA Crossline

### 1. File Input + MQTT Output

```json
{
  "name": "ba_crossline_file_mqtt",
  "solution": "ba_crossline",
  "additionalParams": {
    "FILE_PATH": "/path/to/video.mp4",
    "WEIGHTS_PATH": "/path/to/weights.weights",
    "CONFIG_PATH": "/path/to/config.cfg",
    "LABELS_PATH": "/path/to/labels.txt",
    "CROSSLINE_START_X": "0",
    "CROSSLINE_START_Y": "250",
    "CROSSLINE_END_X": "700",
    "CROSSLINE_END_Y": "220",
    "MQTT_BROKER_URL": "localhost",
    "MQTT_PORT": "1883",
    "MQTT_TOPIC": "ba_crossline/events",
    "ENABLE_SCREEN_DES": "true"
  }
}
```

### 2. RTSP Input + RTMP Output

```json
{
  "name": "ba_crossline_rtsp_rtmp",
  "solution": "ba_crossline",
  "additionalParams": {
    "RTSP_SRC_URL": "rtsp://camera-ip:8554/stream",
    "WEIGHTS_PATH": "/path/to/weights.weights",
    "CONFIG_PATH": "/path/to/config.cfg",
    "LABELS_PATH": "/path/to/labels.txt",
    "CROSSLINE_START_X": "0",
    "CROSSLINE_START_Y": "250",
    "CROSSLINE_END_X": "700",
    "CROSSLINE_END_Y": "220",
    "RTMP_URL": "rtmp://server:1935/live/stream",
    "ENABLE_SCREEN_DES": "false"
  }
}
```

### 3. RTMP Input + MQTT + RTMP Output

```json
{
  "name": "ba_crossline_rtmp_mqtt_rtmp",
  "solution": "ba_crossline",
  "additionalParams": {
    "RTMP_SRC_URL": "rtmp://input-server:1935/live/input",
    "WEIGHTS_PATH": "/path/to/weights.weights",
    "CONFIG_PATH": "/path/to/config.cfg",
    "LABELS_PATH": "/path/to/labels.txt",
    "CROSSLINE_START_X": "0",
    "CROSSLINE_START_Y": "250",
    "CROSSLINE_END_X": "700",
    "CROSSLINE_END_Y": "220",
    "RTMP_URL": "rtmp://output-server:1935/live/output",
    "MQTT_BROKER_URL": "mqtt.broker.com",
    "MQTT_PORT": "1883",
    "MQTT_TOPIC": "ba_crossline/events"
  }
}
```

### 4. File Input Only (không có output)

```json
{
  "name": "ba_crossline_file_only",
  "solution": "ba_crossline",
  "additionalParams": {
    "FILE_PATH": "/path/to/video.mp4",
    "WEIGHTS_PATH": "/path/to/weights.weights",
    "CONFIG_PATH": "/path/to/config.cfg",
    "LABELS_PATH": "/path/to/labels.txt",
    "CROSSLINE_START_X": "0",
    "CROSSLINE_START_Y": "250",
    "CROSSLINE_END_X": "700",
    "CROSSLINE_END_Y": "220",
    "ENABLE_SCREEN_DES": "true"
  }
}
```

## 🔍 Auto-Detection Logic cho BA Crossline

### Input Detection:
1. Nếu có `RTSP_SRC_URL` → Sử dụng RTSP source
2. Nếu có `RTMP_SRC_URL` → Sử dụng RTMP source
3. Nếu có `FILE_PATH`:
   - Nếu bắt đầu bằng `rtsp://` → RTSP source
   - Nếu bắt đầu bằng `rtmp://` → RTMP source
   - Ngược lại → File source

### Output Detection:
- **MQTT**: Chỉ tạo node `json_crossline_mqtt_broker` nếu có `MQTT_BROKER_URL` (không rỗng)
- **RTMP**: Chỉ tạo node nếu có `RTMP_URL` (không rỗng)
- **Screen**: Tạo node nhưng có thể disable qua `ENABLE_SCREEN_DES=false`

## 📊 Event Format (MQTT)

Khi có MQTT output, events sẽ được publish với format:

```json
{
  "events": [
    {
      "best_thumbnail": {
        "confidence": 0.88,
        "image": "base64_encoded_image",
        "instance_id": "instance_name",
        "label": "cross line",
        "system_date": "2025-01-15T07:35:42Z",
        "tracks": [
          {
            "bbox": {"x": 0.35, "y": 0.40, "width": 0.20, "height": 0.25},
            "class_label": "Car",
            "id": "Tracker_123",
            "source_tracker_track_id": 123
          }
        ]
      },
      "type": "crossline",
      "zone_id": "zone_1",
      "zone_name": "Main Road Crossline"
    }
  ],
  "frame_id": 5678,
  "frame_time": 189.27,
  "system_date": "Mon Jan 15 14:35:42 2025",
  "system_timestamp": "1736940942000"
}
```

## ⚠️ Lưu Ý cho BA Crossline

1. **Input**: Chỉ cần cung cấp một trong các: `FILE_PATH`, `RTSP_SRC_URL`, hoặc `RTMP_SRC_URL`
2. **Output**: Có thể bật nhiều output cùng lúc (MQTT + RTMP + Screen)
3. **MQTT**: Nếu không có `MQTT_BROKER_URL`, MQTT broker node sẽ tự động bị skip
4. **RTMP**: Nếu không có `RTMP_URL`, RTMP destination node sẽ tự động bị skip
5. **Screen**: Mặc định tắt, cần set `ENABLE_SCREEN_DES=true` để bật

## 🚀 Quick Start cho BA Crossline

1. Tạo instance với solution `ba_crossline`
2. Cung cấp input (FILE_PATH hoặc RTSP_SRC_URL hoặc RTMP_SRC_URL)
3. Cấu hình crossline parameters (START_X, START_Y, END_X, END_Y)
4. (Tùy chọn) Thêm output: MQTT_BROKER_URL, RTMP_URL, hoặc ENABLE_SCREEN_DES
5. Start instance và kiểm tra kết quả

