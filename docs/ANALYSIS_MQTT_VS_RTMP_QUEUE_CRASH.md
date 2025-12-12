# Phân Tích: Tại Sao MQTT Queue Đầy Gây Crash Nhưng RTMP Không

## 🔴 Vấn Đề

Khi chạy instance với MQTT (`example_ba_crossline_file_mqtt_test.json`), queue đầy gây crash với lỗi "Resource deadlock avoided". Nhưng khi chạy với RTMP (`example_ba_crossline_rtmp.json`), queue đầy chỉ gây warnings mà không crash.

## 📊 So Sánh Pipeline

### Pipeline với MQTT (có crash):
```
file_src → yolo_detector → sort_track → ba_crossline → json_mqtt_broker → ba_crossline_osd → screen_des → rtmp_des
```

### Pipeline với RTMP (không crash):
```
file_src → yolo_detector → sort_track → ba_crossline → ba_crossline_osd → screen_des → rtmp_des
```

**Khác biệt chính:** Pipeline MQTT có thêm node `json_mqtt_broker` giữa `ba_crossline` và `ba_crossline_osd`.

## 🔍 Nguyên Nhân

### 1. **MQTT Publish Callback Có Thể Blocking**

Trong `pipeline_builder.cpp` (line 4061-4108), MQTT publish callback được implement như sau:

```cpp
auto mqtt_publish_func = [mqtt_client_ptr, mqtt_topic](const std::string& json_data) {
    // ...
    int result = mosquitto_publish(mqtt_client_ptr.get(), nullptr, mqtt_topic.c_str(), 
                                  json_data.length(), json_data.c_str(), 0, false);
    // ...
};
```

**Vấn đề:**
- `mosquitto_publish()` có thể blocking nếu:
  - Network chậm
  - MQTT broker chậm hoặc không phản hồi
  - Internal buffer của mosquitto đầy
- Mặc dù đã set `mosquitto_max_inflight_messages_set(mqtt_client, 1000)`, nhưng nếu buffer đầy, `mosquitto_publish()` vẫn có thể block
- Callback được gọi từ thread của `json_mqtt_broker` node → thread bị block → không thể consume queue

### 2. **RTMP Node Không Có Callback Blocking**

RTMP node (`cvedix_rtmp_des_node`) được tạo đơn giản:

```cpp
auto node = std::make_shared<cvedix_nodes::cvedix_rtmp_des_node>(
    nodeName,
    channel,
    rtmpUrl
);
```

**Khác biệt:**
- RTMP node không có callback function phức tạp
- RTMP streaming thường non-blocking (GStreamer pipeline)
- RTMP node có thể drop frames nếu cần (buffer management tốt hơn)
- Không có blocking operation trong callback

### 3. **Queue Full → Thread Blocking → Deadlock**

**Kịch bản với MQTT:**

1. `ba_crossline` node phát hiện events và gửi vào queue của `json_mqtt_broker`
2. `json_mqtt_broker` node gọi callback `mqtt_publish_func` để publish
3. Nếu MQTT broker chậm hoặc network có vấn đề:
   - `mosquitto_publish()` blocking
   - Callback không return
   - Thread của `json_mqtt_broker` bị block
   - Node không thể consume queue
4. Queue đầy → upstream nodes (`ba_crossline`, `sort_track`, `yolo_detector`) cũng bị block khi cố gắng push vào queue
5. Khi cleanup:
   - Cleanup thread cần lock mutex để access queue
   - Nhưng threads đang block đang giữ lock
   - → **Deadlock**

**Kịch bản với RTMP:**

1. `ba_crossline_osd` node gửi frames vào queue của `rtmp_des`
2. `rtmp_des` node xử lý frames (non-blocking)
3. Nếu RTMP server chậm:
   - RTMP node có thể drop frames
   - GStreamer pipeline xử lý async
   - Node vẫn có thể consume queue (không bị block hoàn toàn)
4. Queue có thể đầy nhưng không gây deadlock vì:
   - Không có blocking callback
   - Threads không bị block hoàn toàn
   - Cleanup có thể proceed

## 📝 Log Evidence

Từ terminal output (lines 126-988), thấy:
- Hàng trăm warnings: `[yolo_detector_...] queue full, dropping meta!`
- Nhưng **KHÔNG** thấy warnings từ `json_mqtt_broker` node
- Điều này cho thấy:
  - `yolo_detector` queue đầy vì downstream (`sort_track` → `ba_crossline` → `json_mqtt_broker`) xử lý chậm
  - `json_mqtt_broker` node có thể đã bị block hoàn toàn (không thể log warnings)

## ✅ Giải Pháp Đề Xuất

### 1. **Làm MQTT Publish Non-Blocking (Ưu tiên cao)**

Sử dụng async publish hoặc timeout:

```cpp
auto mqtt_publish_func = [mqtt_client_ptr, mqtt_topic](const std::string& json_data) {
    // Use try_publish or check if buffer is full first
    // If buffer full, drop message instead of blocking
    int result = mosquitto_publish(mqtt_client_ptr.get(), nullptr, mqtt_topic.c_str(), 
                                  json_data.length(), json_data.c_str(), 0, false);
    
    // If publish fails due to buffer full, don't block
    if (result == MOSQ_ERR_OVERSIZE_PACKET || result == MOSQ_ERR_NO_CONN) {
        // Drop message, don't retry (non-blocking)
        return;
    }
};
```

### 2. **Tăng Queue Size cho MQTT Node**

Nếu có thể config queue size của CVEDIX SDK nodes, tăng queue size cho `json_mqtt_broker` node.

### 3. **Sử dụng Thread Pool cho MQTT Publish**

Publish MQTT messages trong separate thread pool để không block node thread:

```cpp
// Create thread pool for MQTT publishing
static ThreadPool mqtt_pool(4); // 4 worker threads

auto mqtt_publish_func = [mqtt_client_ptr, mqtt_topic](const std::string& json_data) {
    // Submit to thread pool (non-blocking)
    mqtt_pool.enqueue([mqtt_client_ptr, mqtt_topic, json_data]() {
        mosquitto_publish(mqtt_client_ptr.get(), nullptr, mqtt_topic.c_str(), 
                         json_data.length(), json_data.c_str(), 0, false);
    });
};
```

### 4. **Timeout cho MQTT Publish**

Thêm timeout mechanism để tránh blocking vô hạn:

```cpp
// Use async publish with timeout
// If publish takes too long, drop message
```

### 5. **Monitor và Restart Instance Khi Queue Đầy**

Đã có code trong `main.cpp` (lines 2049-2258) nhưng bị disable. Có thể enable lại và tune thresholds.

## 🎯 Khuyến Nghị Ngay Lập Tức

1. **Kiểm tra MQTT broker connection:**
   - Đảm bảo MQTT broker (`localhost:1883`) phản hồi nhanh
   - Kiểm tra network latency

2. **Giảm frame rate hoặc tăng RESIZE_RATIO:**
   - Trong `example_ba_crossline_file_mqtt_test.json`, `RESIZE_RATIO` là `0.1` (rất nhỏ)
   - Thử tăng lên `0.4` như trong RTMP config để giảm số lượng frames

3. **Tạm thời disable MQTT nếu không cần thiết:**
   - Sử dụng RTMP output thay vì MQTT nếu có thể

4. **Implement non-blocking MQTT publish:**
   - Sửa code trong `pipeline_builder.cpp` để MQTT publish không blocking

## 📌 Kết Luận

**Nguyên nhân chính:** MQTT publish callback có thể blocking khi network/broker chậm, làm cho `json_mqtt_broker` node thread bị block, không thể consume queue, dẫn đến deadlock khi cleanup.

**RTMP không bị vấn đề này** vì RTMP node không có blocking callback và có cơ chế drop frames tốt hơn.

**Giải pháp:** Làm MQTT publish non-blocking bằng cách sử dụng async publish, thread pool, hoặc timeout mechanism.

