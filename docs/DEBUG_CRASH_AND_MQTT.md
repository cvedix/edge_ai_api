# Debug: Server Crash và MQTT Không Nhận Được Data

## 🔴 Vấn đề

1. **Server crash** với lỗi "Resource deadlock avoided"
2. **MQTT không nhận được data** mặc dù đã connect thành công

## 🔍 Phân tích

### 1. Server Crash - "Resource deadlock avoided"

**Nguyên nhân:**
- RTSP connection fail gây ra crash khi cleanup
- Deadlock xảy ra khi stop instance trong lúc RTSP retry loop đang chạy
- Mutex lock conflict trong quá trình cleanup

**Từ log:**
```
[ WARN:0@400.932] global cap_gstreamer.cpp:1181 isPipelinePlaying OpenCV | GStreamer warning: unable to query pipeline state
[ WARN:0@400.933] global cap_gstreamer.cpp:2839 handleMessage OpenCV | GStreamer warning: Embedded video playback halted; module rtspsrc0 reported: Could not open resource for reading and writing.
[ WARN:0@400.992] global cap_gstreamer.cpp:2839 handleMessage OpenCV | GStreamer warning: Embedded video playback halted; module rtspsrc0 reported: Unhandled error
2025-12-12 01:40:28.136 ERROR [892407] [terminateHandler@474] [CRITICAL] Uncaught exception: Resource deadlock avoided
```

### 2. MQTT Không Nhận Được Data

**Nguyên nhân:**
- Pipeline crash trước khi có data từ `ba_crossline` node
- RTSP connection fail → không có frames → không có detections → không có BA events → không có MQTT messages

**Từ log:**
```
[PipelineBuilder] [MQTT] Connected successfully!
[PipelineBuilder] [MQTT] Node will publish to topic: 'ba_crossline/events'
[PipelineBuilder] [MQTT] NOTE: Callback will be called when json_mqtt_broker_node receives data
```

Nhưng không thấy log:
```
[MQTT] Published successfully to topic 'ba_crossline/events': XXX bytes
```

## ✅ Giải pháp

### Bước 1: Kiểm tra RTSP Stream

```bash
# Kiểm tra RTSP stream có đang chạy không
ffprobe rtsp://localhost:8554/mystream

# Hoặc test với gst-launch
gst-launch-1.0 rtspsrc location=rtsp://localhost:8554/mystream ! fakesink
```

**Nếu RTSP stream không hoạt động:**
- Start RTSP server hoặc stream
- Hoặc test với file video thay vì RTSP

### Bước 2: Test với File Video

Để test MQTT mà không bị ảnh hưởng bởi RTSP issues, sử dụng file video:

```json
{
  "name": "ba_crossline_file_mqtt_test",
  "group": "demo",
  "solution": "ba_crossline_with_mqtt",
  "autoStart": false,
  "additionalParams": {
    "FILE_PATH": "/home/cvedix/project/edge_ai_api/cvedix_data/test_video/vehicle.mp4",
    "WEIGHTS_PATH": "/home/cvedix/project/edge_ai_api/cvedix_data/models/det_cls/yolov3-tiny-2022-0721_best.weights",
    "CONFIG_PATH": "/home/cvedix/project/edge_ai_api/cvedix_data/models/det_cls/yolov3-tiny-2022-0721.cfg",
    "LABELS_PATH": "/home/cvedix/project/edge_ai_api/cvedix_data/models/det_cls/yolov3_tiny_5classes.txt",
    "ENABLE_SCREEN_DES": "false",
    "RESIZE_RATIO": "0.4",
    "BROKE_FOR": "NORMAL",
    "MQTT_BROKER_URL": "mqtt.goads.com.vn",
    "MQTT_PORT": "1883",
    "MQTT_TOPIC": "ba_crossline/events",
    "MQTT_USERNAME": "",
    "MQTT_PASSWORD": ""
  }
}
```

### Bước 3: Kiểm tra MQTT Connection

```bash
# Subscribe để xem có messages không
mosquitto_sub -h mqtt.goads.com.vn -p 1883 -t "ba_crossline/events" -v
```

### Bước 4: Enable Debug Logging

Thêm logging để debug MQTT callback:

1. **Kiểm tra xem callback có được gọi không:**
   - Thêm log trong `mqtt_publish_func` callback
   - Check xem `json_mqtt_broker_node` có nhận được data từ `ba_crossline` không

2. **Kiểm tra data flow:**
   - `ba_crossline` node có output events không?
   - `json_mqtt_broker_node` có nhận được data không?
   - `broke_for` parameter có match với data type không?

## 🛠️ Fix Code Issues

### 1. Improve RTSP Error Handling

Cần thêm better error handling để tránh crash khi RTSP fail:

- Add timeout cho RTSP connection
- Better cleanup khi RTSP fail
- Prevent deadlock khi stop RTSP node

### 2. Add MQTT Debug Logging

Thêm logging để debug MQTT:

- Log khi callback được gọi
- Log khi có data từ ba_crossline
- Log khi publish thành công/thất bại

## 📝 Checklist Debug

- [ ] RTSP stream đang chạy và accessible
- [ ] MQTT broker accessible và connected
- [ ] Pipeline không crash (test với file video)
- [ ] ba_crossline node output events
- [ ] json_mqtt_broker node nhận được data
- [ ] MQTT callback được gọi
- [ ] Messages được publish thành công

## 🎯 Next Steps

1. **Test với file video** để verify MQTT hoạt động
2. **Fix RTSP connection issues** hoặc sử dụng file video
3. **Add debug logging** để track data flow
4. **Monitor MQTT messages** với mosquitto_sub

