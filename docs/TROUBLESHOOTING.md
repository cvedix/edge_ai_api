# Troubleshooting Guide - Edge AI API

Tài liệu này tổng hợp các vấn đề thường gặp và cách khắc phục khi sử dụng Edge AI API.

## 📋 Mục Lục

1. [Crash Analysis - Queue Deadlock](#crash-analysis---queue-deadlock)
2. [Crash Analysis - RTSP + MQTT Deadlock](#crash-analysis---rtsp--mqtt-deadlock)
3. [MQTT vs RTMP Queue Crash](#mqtt-vs-rtmp-queue-crash)
4. [MQTT Debug Guide](#mqtt-debug-guide)
5. [RTSP Connection Issues](#rtsp-connection-issues)
6. [RTSP Decoder Issues](#rtsp-decoder-issues)

---

## Crash Analysis - Queue Deadlock

### 🔴 Vấn đề

Server crash với lỗi **"Resource deadlock avoided"** khi đang chạy pipeline với file video.

### 📊 Phân tích Log

#### 1. Queue Đầy Liên Tục

Từ log, thấy rất nhiều warnings:
```
[Warn] [yolo_detector_...] queue full, dropping meta!
[Warn] [json_mqtt_broker_...] queue full, dropping meta!
```

**Tần suất:** Hàng trăm warnings trong vài giây → Queue đầy liên tục

#### 2. BA Crossline Đang Hoạt Động

```
[Info] [ba_crossline_...] [channel 0] has found target cross line, total number of crossline: [1]
```

**Kết luận:** BA crossline đang phát hiện events, nhưng không thể gửi qua MQTT vì queue đầy.

#### 3. MQTT Connection Thành Công

```
[PipelineBuilder] [MQTT] Connected successfully!
```

**Nhưng:** Không thấy log `[MQTT] Callback called` hoặc `[MQTT] Published successfully` → Callback không được gọi vì queue đầy.

#### 4. Crash với Deadlock

```
2025-12-12 01:46:36.257 ERROR [898358] [terminateHandler@474] [CRITICAL] Uncaught exception: Resource deadlock avoided
[InstanceRegistry] WARNING: listInstances() timeout - mutex is locked, returning empty vector
```

### 🔍 Nguyên Nhân

#### 1. **Queue Size Quá Nhỏ**

CVEDIX SDK nodes có queue size mặc định nhỏ (có thể 10-50 items). Khi:
- Frame rate cao (video file)
- YOLO detector chậm hơn frame rate
- MQTT publish chậm

→ Queue đầy nhanh chóng → Data bị drop

#### 2. **MQTT Publish Blocking**

MQTT publish có thể blocking nếu:
- Network chậm
- Broker chậm
- QoS > 0 (waiting for ACK)

→ `json_mqtt_broker` node không thể consume queue nhanh → Queue đầy

#### 3. **Deadlock Khi Cleanup**

Khi cleanup:
- Threads đang lock mutex để access queue
- Queue đầy → threads đang chờ nhau
- Cleanup thread cũng cần lock → Deadlock

### ✅ Giải Pháp

#### Giải Pháp 1: Tăng SKIP_INTERVAL (Khuyến Nghị)

Giảm frame rate để giảm tải cho queue:

```json
{
  "additionalParams": {
    "SKIP_INTERVAL": "10",  // Skip 10 frames, process 1 frame
    // Hoặc
    "SKIP_INTERVAL": "20"   // Skip 20 frames, process 1 frame
  }
}
```

#### Giải Pháp 2: Tăng RESIZE_RATIO

Giảm resolution để tăng tốc độ xử lý:

```json
{
  "additionalParams": {
    "RESIZE_RATIO": "0.2"  // Giảm từ 0.4 xuống 0.2
  }
}
```

#### Giải Pháp 3: Sử Dụng Video Có FPS Thấp Hơn

Re-encode video với FPS thấp hơn:

```bash
ffmpeg -i input.mp4 -r 10 -c:v libx264 -preset fast -crf 23 output.mp4
```

#### Giải Pháp 4: Tăng Queue Size (Cần Modify SDK)

Nếu có quyền truy cập SDK code, tăng queue size trong CVEDIX SDK nodes.

#### Giải Pháp 5: Fix Deadlock trong Cleanup

Cải thiện cleanup code để tránh deadlock khi queue đầy.

---

## Crash Analysis - RTSP + MQTT Deadlock

### 🔴 Vấn đề

Server crash với lỗi **"Resource deadlock avoided"** khi chạy pipeline RTSP với MQTT output.

### 📊 Phân tích Log

#### 1. Placeholder Không Được Thay Thế

**Line 904 trong log:**
```
[PipelineBuilder] [MQTT] Broke for: ${BROKE_FOR}, Warn threshold: 1000, Ignore threshold: 10000
```

**Vấn đề:** Placeholder `${BROKE_FOR}` không được thay thế thành `"NORMAL"` từ `additionalParams`.

**Nguyên nhân:** Code chỉ xử lý các placeholder cụ thể (như `${WEIGHTS_PATH}`, `${CONFIG_PATH}`), không có handler tổng quát cho các placeholder khác như `${BROKE_FOR}`.

#### 2. RTSP Connection Thành Công

RTSP stream đã kết nối thành công.

#### 3. Pipeline Đang Chạy

Pipeline đã bắt đầu xử lý frames.

#### 4. Crash với Deadlock

```
2025-12-12 02:40:01.856 ERROR [948807] [terminateHandler@508] [CRITICAL] Uncaught exception: Resource deadlock avoided
[InstanceRegistry] WARNING: listInstances() timeout - mutex is locked, returning empty vector
```

### 🔍 Nguyên Nhân

#### 1. **Placeholder Không Được Thay Thế**

- `${BROKE_FOR}` không được thay thế → Node nhận giá trị literal `${BROKE_FOR}` thay vì `"NORMAL"`
- Code fallback về `NORMAL` nhưng có thể gây confusion và log sai

#### 2. **Queue Full → Deadlock**

Khi pipeline chạy:
- RTSP stream đang gửi frames liên tục
- YOLO detector xử lý chậm hơn frame rate
- MQTT publish có thể chậm (network latency, broker chậm)
- Queue đầy → Threads block trên mutex
- Cleanup thread cũng cần lock → **Deadlock**

### ✅ Giải Pháp Đã Áp Dụng

#### Fix 1: Generic Placeholder Substitution

**File:** `src/core/pipeline_builder.cpp`

**Thay đổi:** Thêm generic placeholder substitution handler để xử lý tất cả placeholders từ `additionalParams`:

```cpp
// Generic placeholder substitution: Replace ${VARIABLE_NAME} with values from additionalParams
std::regex placeholderPattern("\\$\\{([A-Za-z0-9_]+)\\}");
std::sregex_iterator iter(value.begin(), value.end(), placeholderPattern);
std::sregex_iterator end;
std::set<std::string> processedVars;

for (; iter != end; ++iter) {
    std::string varName = (*iter)[1].str();
    auto it = req.additionalParams.find(varName);
    if (it != req.additionalParams.end() && !it->second.empty()) {
        value = std::regex_replace(value, std::regex("\\$\\{" + varName + "\\}"), it->second);
    }
}
```

**Kết quả:** Bây giờ `${BROKE_FOR}` sẽ được thay thế thành `"NORMAL"` từ `additionalParams`.

#### Fix 2: Thêm Include `<regex>`

Thêm `#include <regex>` để support regex operations.

### 🛠️ Giải Pháp Bổ Sung (Để Tránh Deadlock)

#### 1. Tăng SKIP_INTERVAL

```json
{
  "additionalParams": {
    "SKIP_INTERVAL": "9",  // Đã có trong config
    // Có thể tăng lên 15-20 nếu vẫn bị deadlock
  }
}
```

#### 2. Giảm RESIZE_RATIO

```json
{
  "additionalParams": {
    "RESIZE_RATIO": "0.1",  // Đã có trong config
    // Có thể giảm xuống 0.05 nếu vẫn chậm
  }
}
```

#### 3. Kiểm Tra MQTT Broker

Đảm bảo MQTT broker có:
- Network latency thấp
- Không bị rate limit
- QoS = 0 (nếu có thể) để tránh blocking

---

## MQTT vs RTMP Queue Crash

### 🔴 Vấn Đề

Khi chạy instance với MQTT (`example_ba_crossline_file_mqtt_test.json`), queue đầy gây crash với lỗi "Resource deadlock avoided". Nhưng khi chạy với RTMP (`example_ba_crossline_rtmp.json`), queue đầy chỉ gây warnings mà không crash.

### 📊 So Sánh Pipeline

#### Pipeline với MQTT (có crash):
```
file_src → yolo_detector → sort_track → ba_crossline → json_mqtt_broker → ba_crossline_osd → screen_des → rtmp_des
```

#### Pipeline với RTMP (không crash):
```
file_src → yolo_detector → sort_track → ba_crossline → ba_crossline_osd → screen_des → rtmp_des
```

**Khác biệt chính:** Pipeline MQTT có thêm node `json_mqtt_broker` giữa `ba_crossline` và `ba_crossline_osd`.

### 🔍 Nguyên Nhân

#### 1. **MQTT Publish Callback Có Thể Blocking**

Trong `pipeline_builder.cpp`, MQTT publish callback được implement như sau:

```cpp
auto mqtt_publish_func = [mqtt_client_ptr, mqtt_topic](const std::string& json_data) {
    int result = mosquitto_publish(mqtt_client_ptr.get(), nullptr, mqtt_topic.c_str(), 
                                  json_data.length(), json_data.c_str(), 0, false);
};
```

**Vấn đề:**
- `mosquitto_publish()` có thể blocking nếu:
  - Network chậm
  - MQTT broker chậm hoặc không phản hồi
  - Internal buffer của mosquitto đầy
- Callback được gọi từ thread của `json_mqtt_broker` node → thread bị block → không thể consume queue

#### 2. **RTMP Node Không Có Callback Blocking**

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

#### 3. **Queue Full → Thread Blocking → Deadlock**

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

### ✅ Giải Pháp Đề Xuất

#### 1. **Làm MQTT Publish Non-Blocking (Ưu tiên cao)**

Sử dụng async publish hoặc timeout:

```cpp
auto mqtt_publish_func = [mqtt_client_ptr, mqtt_topic](const std::string& json_data) {
    int result = mosquitto_publish(mqtt_client_ptr.get(), nullptr, mqtt_topic.c_str(), 
                                  json_data.length(), json_data.c_str(), 0, false);
    
    // If publish fails due to buffer full, don't block
    if (result == MOSQ_ERR_OVERSIZE_PACKET || result == MOSQ_ERR_NO_CONN) {
        // Drop message, don't retry (non-blocking)
        return;
    }
};
```

#### 2. **Sử dụng Thread Pool cho MQTT Publish**

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

#### 3. **Timeout cho MQTT Publish**

Thêm timeout mechanism để tránh blocking vô hạn.

### 🎯 Khuyến Nghị Ngay Lập Tức

1. **Kiểm tra MQTT broker connection:**
   - Đảm bảo MQTT broker phản hồi nhanh
   - Kiểm tra network latency

2. **Giảm frame rate hoặc tăng RESIZE_RATIO:**
   - Trong `example_ba_crossline_file_mqtt_test.json`, `RESIZE_RATIO` là `0.1` (rất nhỏ)
   - Thử tăng lên `0.4` như trong RTMP config để giảm số lượng frames

3. **Tạm thời disable MQTT nếu không cần thiết:**
   - Sử dụng RTMP output thay vì MQTT nếu có thể

4. **Implement non-blocking MQTT publish:**
   - Sửa code trong `pipeline_builder.cpp` để MQTT publish không blocking

### 📌 Kết Luận

**Nguyên nhân chính:** MQTT publish callback có thể blocking khi network/broker chậm, làm cho `json_mqtt_broker` node thread bị block, không thể consume queue, dẫn đến deadlock khi cleanup.

**RTMP không bị vấn đề này** vì RTMP node không có blocking callback và có cơ chế drop frames tốt hơn.

**Giải pháp:** Làm MQTT publish non-blocking bằng cách sử dụng async publish, thread pool, hoặc timeout mechanism.

---

## MQTT Debug Guide

### Vấn đề

MQTT broker node không gửi được messages từ `ba_crossline` node.

### Kiểm tra đã thực hiện

#### 1. MQTT Connection
- ✅ MQTT client được tạo thành công
- ✅ Kết nối đến broker thành công (`mqtt.goads.com.vn:1883`)
- ✅ Network loop đã start

#### 2. Pipeline Connection
- ✅ Pipeline được build đúng thứ tự:
  ```
  ba_crossline → json_mqtt_broker → ba_crossline_osd
  ```
- ✅ Node được attach đúng cách

#### 3. Configuration
- ✅ `broke_for`: "NORMAL" (phù hợp cho detection/BA events)
- ✅ `broking_cache_warn_threshold`: 200
- ✅ `broking_cache_ignore_threshold`: 2000
- ✅ MQTT topic: "ba_crossline/events"

### Debugging Steps

#### Bước 1: Kiểm tra Logs

Sau khi rebuild và chạy lại instance, kiểm tra logs cho các messages sau:

1. **Khi node được tạo:**
   ```
   [PipelineBuilder] [MQTT] Creating broker node with callback function...
   [PipelineBuilder] [MQTT] Node will publish to topic: 'ba_crossline/events'
   ```

2. **Khi callback được gọi (nếu có data):**
   ```
   [MQTT] Published successfully to topic 'ba_crossline/events': XXX bytes. Preview: {...}
   ```

3. **Nếu không có messages:**
   - Callback không được gọi → `json_mqtt_broker_node` không nhận được data từ `ba_crossline`
   - Có thể do `broke_for` không match với data type

#### Bước 2: Kiểm tra Data Flow

`ba_crossline` node output behavior analysis events (crossline crossing events), không phải normal detection metadata. 

**Vấn đề có thể:**
- `json_mqtt_broker_node` với `broke_for::NORMAL` có thể expect detection metadata (bounding boxes, classes)
- Behavior analysis events có thể có format khác

#### Bước 3: Giải pháp thay thế

Nếu `json_mqtt_broker_node` không hoạt động với `ba_crossline`, có thể cần:

1. **Sử dụng `ba_socket_broker` thay vì `json_mqtt_broker`:**
   - `ba_socket_broker` được thiết kế đặc biệt cho behavior analysis
   - Output qua socket thay vì MQTT
   - Cần thêm một service để forward từ socket sang MQTT

2. **Kiểm tra CVEDIX SDK documentation:**
   - Xem `cvedix_json_mqtt_broker_node` có hỗ trợ behavior analysis events không
   - Xem có `broke_for` value nào khác phù hợp hơn không

3. **Custom broker node:**
   - Tạo custom node để lấy data từ `ba_crossline` và publish qua MQTT
   - Sử dụng `cvedix_mqtt_client` trực tiếp như trong sample code

### Code Changes

Đã thêm logging vào `src/core/pipeline_builder.cpp`:

1. **Connection state check:** Kiểm tra client đã connected trước khi publish
2. **Success logging:** Log khi publish thành công với preview của JSON
3. **Error logging:** Log chi tiết khi có lỗi
4. **Setup logging:** Log khi node được tạo với thông tin cấu hình

### Next Steps

1. **Rebuild và test:**
   ```bash
   cd build
   cmake ..
   make -j4
   ```

2. **Chạy lại instance và monitor logs:**
   ```bash
   ./bin/edge_ai_api 2>&1 | grep -i mqtt
   ```

3. **Kiểm tra MQTT broker:**
   ```bash
   mosquitto_sub -h mqtt.goads.com.vn -p 1883 -t "ba_crossline/events" -v
   ```

4. **Nếu vẫn không có messages:**
   - Kiểm tra xem `ba_crossline` có output events không (xem logs của ba_crossline node)
   - Thử thay đổi `broke_for` sang các giá trị khác
   - Xem xét sử dụng `ba_socket_broker` thay vì `json_mqtt_broker`

---

## RTSP Connection Issues

### 📋 Tóm tắt lỗi

**Lỗi:** RTSP không thể kết nối đến server  
**RTSP URL:** `rtsp://100.76.5.84:8554/mystream`  
**Thông báo lỗi:** `Could not connect to server. (Timeout while waiting for server response)`

### 🔍 Chi tiết lỗi từ log

#### Lỗi GStreamer RTSP:
```
ERROR rtspsrc gstrtspsrc.c:5492:gst_rtsp_conninfo_connect:<rtspsrc0> 
Could not connect to server. (Timeout while waiting for server response)

WARN rtspsrc gstrtspsrc.c:8442:gst_rtspsrc_retrieve_sdp:<rtspsrc0> 
error: Failed to connect. (Timeout while waiting for server response)
```

#### Retry attempts:
- Attempt 1: Failed sau ~20 giây
- Attempt 2: Failed sau ~40 giây  
- Attempt 3: Failed sau ~60 giây
- Attempt 4: Failed sau ~80 giây
- ... (tiếp tục retry đến 10 lần)

### ✅ Kết quả kiểm tra

#### 1. Ping test:
```bash
$ ping -c 3 100.76.5.84
PING 100.76.5.84 (100.76.5.84) 56(84) bytes of data.
--- 100.76.5.84 ping statistics ---
3 packets transmitted, 0 received, 100% packet loss
```
**Kết luận:** Server không thể truy cập từ mạng này

#### 2. Port test:
```bash
$ timeout 5 bash -c 'echo > /dev/tcp/100.76.5.84/8554'
Port 8554 is closed or unreachable
```
**Kết luận:** Port RTSP (8554) đóng hoặc bị firewall chặn

### 🎯 Nguyên nhân có thể

1. **RTSP Server không chạy**
   - Server tại `100.76.5.84:8554` không đang chạy
   - Service RTSP đã bị dừng

2. **Vấn đề mạng**
   - Server không thể truy cập từ máy hiện tại
   - Routing issue giữa các mạng
   - Server ở mạng khác (VPN cần thiết?)

3. **Firewall chặn**
   - Firewall trên server chặn port 8554
   - Firewall trên client chặn kết nối ra ngoài
   - Network security group rules

4. **IP Address sai**
   - IP `100.76.5.84` có thể đã thay đổi
   - IP là private IP và không route được

5. **RTSP Stream không tồn tại**
   - Stream path `/mystream` không tồn tại
   - Stream đã bị xóa hoặc đổi tên

### 🔧 Giải pháp

#### Giải pháp 1: Kiểm tra RTSP Server

**Trên server RTSP (`100.76.5.84`):**
```bash
# Kiểm tra RTSP service có chạy không
sudo systemctl status mediamtx  # hoặc service khác
sudo netstat -tlnp | grep 8554
sudo ss -tlnp | grep 8554

# Kiểm tra firewall
sudo ufw status
sudo iptables -L -n | grep 8554

# Test RTSP stream locally
ffprobe rtsp://localhost:8554/mystream
```

#### Giải pháp 2: Kiểm tra từ client

**Sử dụng script diagnostic:**
```bash
./scripts/rtsp_helper.sh <instanceId> rtsp://100.76.5.84:8554/mystream test
```

**Kiểm tra thủ công:**
```bash
# Test với ffprobe
ffprobe -v error -rtsp_transport tcp rtsp://100.76.5.84:8554/mystream

# Test với GStreamer
gst-launch-1.0 -v rtspsrc location=rtsp://100.76.5.84:8554/mystream protocols=tcp latency=0 ! fakesink

# Test với VLC (GUI)
vlc rtsp://100.76.5.84:8554/mystream
```

#### Giải pháp 3: Sửa firewall

**Nếu server ở cùng mạng:**
```bash
# Trên server
sudo ufw allow 8554/tcp
sudo ufw allow 8554/udp
```

**Nếu server ở mạng khác:**
- Kiểm tra security group rules (AWS, Azure, GCP)
- Mở port 8554 (TCP và UDP) trong firewall rules

#### Giải pháp 4: Kiểm tra VPN/Network

**Nếu server ở mạng riêng:**
```bash
# Kiểm tra VPN connection
ip addr show
route -n

# Kiểm tra có thể ping được gateway không
ping <gateway_ip>
```

#### Giải pháp 5: Thử RTSP URL khác

**Nếu có RTSP server khác để test:**
```json
{
  "additionalParams": {
    "RTSP_SRC_URL": "rtsp://<other_server>:<port>/<stream>",
    ...
  }
}
```

#### Giải pháp 6: Sử dụng file source tạm thời

**Để test pipeline hoạt động:**
```json
{
  "additionalParams": {
    "FILE_PATH": "/path/to/test/video.mp4",
    // Xóa RTSP_SRC_URL để dùng file source
    ...
  }
}
```

### 🎯 Kết luận

**Vấn đề chính:** RTSP server tại `100.76.5.84:8554` không thể truy cập từ máy hiện tại.

**Nguyên nhân:** 
- Server không chạy hoặc không thể truy cập (100% packet loss)
- Port 8554 đóng hoặc bị firewall chặn

**Hành động cần thiết:**
1. ✅ Kiểm tra RTSP server có đang chạy không
2. ✅ Kiểm tra firewall rules trên server
3. ✅ Kiểm tra network connectivity (ping, routing)
4. ✅ Xác nhận RTSP URL đúng và stream tồn tại
5. ✅ Test RTSP stream với ffprobe/VLC trước khi dùng trong API

**Khuyến nghị:** 
- Sửa vấn đề network/server trước khi tiếp tục
- Hoặc sử dụng file source để test pipeline trong khi chờ RTSP server sẵn sàng

---

## RTSP Decoder Issues

### Vấn đề: GStreamer CRITICAL errors với RTSP stream

#### Lỗi gặp phải:
```
GStreamer-CRITICAL **: gst_caps_get_structure: assertion 'GST_IS_CAPS (caps)' failed
GStreamer-CRITICAL **: gst_sample_get_caps: assertion 'GST_IS_SAMPLE (sample)' failed
retrieveVideoFrame GStreamer: gst_sample_get_caps() returns NULL
```

#### Nguyên nhân:
- Decoder không tương thích với stream format
- Caps negotiation giữa decoder và appsink thất bại
- SDK không lấy được sample từ appsink

#### Giải pháp đã thử:
1. ✅ Đổi từ `avdec_h264` → `openh264dec` (vẫn lỗi)
2. ⏳ Cần thử các decoder khác

### Các decoder có thể thử:

#### 1. Kiểm tra decoder có sẵn:
```bash
gst-inspect-1.0 | grep -E "h264.*dec|dec.*h264"
```

#### 2. Test decoder với GStreamer:
```bash
# Test openh264dec
gst-launch-1.0 rtspsrc location=rtsp://anhoidong.datacenter.cvedix.com:8554/live/camera_demo_sang_vehicle protocols=tcp latency=0 ! application/x-rtp,media=video ! rtph264depay ! h264parse ! openh264dec ! videoconvert ! fakesink

# Test avdec_h264
gst-launch-1.0 rtspsrc location=rtsp://anhoidong.datacenter.cvedix.com:8554/live/camera_demo_sang_vehicle protocols=tcp latency=0 ! application/x-rtp,media=video ! rtph264depay ! h264parse ! avdec_h264 ! videoconvert ! fakesink

# Test với decodebin (auto-detect)
gst-launch-1.0 rtspsrc location=rtsp://anhoidong.datacenter.cvedix.com:8554/live/camera_demo_sang_vehicle protocols=tcp latency=0 ! application/x-rtp,media=video ! rtph264depay ! h264parse ! decodebin ! videoconvert ! fakesink
```

#### 3. Bật GStreamer debug để xem chi tiết:
```bash
export GST_DEBUG=rtspsrc:4,openh264dec:4,appsink:4
./bin/edge_ai_api
```

### Decoder có sẵn trên hệ thống:
- `avdec_h264` (libav H.264 decoder) - ❌ Không hoạt động
- `openh264dec` (OpenH264 decoder) - ❌ Không hoạt động  
- `vulkanh264dec` (Vulkan H.264 decoder) - ⏳ Chưa thử

### Cập nhật config để thử decoder khác:
Trong `example_ba_crossline_in_rtsp_out_rtmp.json`, thay đổi:
```json
"GST_DECODER_NAME": "vulkanh264dec"
```

### Phân tích chi tiết lỗi RTSP Stream

#### Trạng thái:
RTSP kết nối thành công nhưng không nhận được video frames  
**Decoder đang dùng:** `vulkanh264dec`

#### Phân tích từng bước:

##### ✅ Bước 1: Pipeline khởi tạo thành công
- Pipeline được build thành công với 6 nodes
- RTMP destination node tạo thành công
- Instance đã start

##### ✅ Bước 2: RTSP Connection thành công
- RTSP handshake thành công
- SDP negotiation thành công
- Bandwidth được configure (2.5Mbps cho video, 160Kbps cho audio)
- Stream đã được setup

##### ❌ Bước 3: GStreamer CRITICAL Errors
```
GStreamer-CRITICAL **: gst_caps_get_structure: assertion 'GST_IS_CAPS (caps)' failed
GStreamer-CRITICAL **: gst_structure_get_int: assertion 'structure != NULL' failed
GStreamer-CRITICAL **: gst_structure_get_fraction: assertion 'structure != NULL' failed
```

**Nguyên nhân:**
- SDK CVEDIX cố gắng lấy **caps** (capabilities) từ `appsink` nhưng nhận được **NULL**
- Caps negotiation giữa decoder và appsink **thất bại**
- SDK không thể xác định format của video frames

**Vị trí lỗi:** 
- File: `/home/cvedix/core_ai_runtime/nodes/src/cvedix_rtsp_src_node.cpp`
- SDK đang cố gắng lấy width, height, framerate từ caps nhưng caps = NULL

##### ⚠️ Bước 4: RTSP Connection Opened nhưng không có data
- ✅ RTSP connection mở thành công
- ❌ Nhưng không có frame nào được nhận từ stream

##### 🔄 Bước 5: Instance Retry liên tục
- Instance retry vì `no_data=yes` (không có data)
- Instance inactive trong 70s, 100s...
- Sẽ retry đến 10 lần

### 🎯 Nguyên nhân gốc rễ

#### 1. **Caps Negotiation Failure**
```
Decoder (vulkanh264dec) → videoconvert → appsink
                          ↑
                    Caps negotiation fails here
```
- Decoder decode được frames nhưng không thể negotiate caps với appsink
- SDK không biết format của frames (NV12? RGB? BGR?)

#### 2. **SDK Code Issue**
- SDK code trong `cvedix_rtsp_src_node.cpp:152` cố gắng lấy caps từ sample
- Nhưng sample không có caps hoặc caps = NULL
- SDK không handle được trường hợp này

#### 3. **Pipeline thiếu Caps Filter**
Pipeline hiện tại:
```
rtspsrc ! rtph264depay ! h264parse ! vulkanh264dec ! videoconvert ! appsink
```

Pipeline cần có:
```
rtspsrc ! rtph264depay ! h264parse ! vulkanh264dec ! videoconvert ! video/x-raw,format=NV12 ! appsink
                                                                    ↑
                                                          Thiếu caps filter này
```

### 🔧 Giải pháp đã thử

#### ❌ Đã thử các decoder:
1. `avdec_h264` - ❌ Lỗi tương tự
2. `openh264dec` - ❌ Lỗi tương tự  
3. `vulkanh264dec` - ❌ Lỗi tương tự

#### ⚠️ Vấn đề:
- SDK CVEDIX **hardcode pipeline**, không thể thêm caps filter
- Tất cả decoder đều gặp lỗi tương tự → **không phải vấn đề decoder**

### 💡 Giải pháp đề xuất

#### 1. **Kiểm tra SDK CVEDIX Source Code**
- File: `/home/cvedix/core_ai_runtime/nodes/src/cvedix_rtsp_src_node.cpp:152`
- Xem cách SDK lấy caps từ appsink
- Có thể cần fix SDK để handle NULL caps

#### 2. **Thử với decodebin (auto-detect)**
- SDK có thể không hỗ trợ decodebin trực tiếp
- Nhưng có thể thử trong config

#### 3. **Bật GStreamer Debug**
```bash
export GST_DEBUG=rtspsrc:4,vulkanh264dec:4,appsink:4,videoconvert:4
./bin/edge_ai_api
```
- Xem chi tiết caps negotiation
- Xem decoder output format

#### 4. **Liên hệ CVEDIX SDK Team**
- Đây có thể là **bug trong SDK**
- SDK không handle được trường hợp caps = NULL
- Cần fix trong SDK code

### 🎯 Kết luận

**Vấn đề chính:** SDK CVEDIX không thể lấy caps từ appsink, dẫn đến không thể xử lý video frames.

**Nguyên nhân:** 
- Caps negotiation failure giữa decoder và appsink
- SDK code không handle được trường hợp caps = NULL

**Giải pháp:** 
- Cần fix SDK code để handle NULL caps
- Hoặc thêm caps filter vào pipeline (nhưng SDK hardcode pipeline)

**Khuyến nghị:** Liên hệ CVEDIX SDK team để fix bug này.

---

## 📚 Tài Liệu Liên Quan

- [Queue Monitoring Guide](./QUEUE_MONITORING.md) - Giải pháp monitoring queue
- [MQTT Guide](./MQTT_GUIDE.md) - Hướng dẫn MQTT non-blocking implementation
- [Development Setup](./DEVELOPMENT_SETUP.md) - Setup môi trường phát triển

