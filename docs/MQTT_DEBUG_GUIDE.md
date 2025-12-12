# MQTT Debug Guide - BA Crossline với MQTT

## Vấn đề

MQTT broker node không gửi được messages từ `ba_crossline` node.

## Kiểm tra đã thực hiện

### 1. MQTT Connection
- ✅ MQTT client được tạo thành công
- ✅ Kết nối đến broker thành công (`localhost:1883`)
- ✅ Network loop đã start

### 2. Pipeline Connection
- ✅ Pipeline được build đúng thứ tự:
  ```
  ba_crossline → json_mqtt_broker → ba_crossline_osd
  ```
- ✅ Node được attach đúng cách

### 3. Configuration
- ✅ `broke_for`: "NORMAL" (phù hợp cho detection/BA events)
- ✅ `broking_cache_warn_threshold`: 200
- ✅ `broking_cache_ignore_threshold`: 2000
- ✅ MQTT topic: "ba_crossline/events"

## Debugging Steps

### Bước 1: Kiểm tra Logs

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

### Bước 2: Kiểm tra Data Flow

`ba_crossline` node output behavior analysis events (crossline crossing events), không phải normal detection metadata. 

**Vấn đề có thể:**
- `json_mqtt_broker_node` với `broke_for::NORMAL` có thể expect detection metadata (bounding boxes, classes)
- Behavior analysis events có thể có format khác

### Bước 3: Giải pháp thay thế

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

## Code Changes

Đã thêm logging vào `src/core/pipeline_builder.cpp`:

1. **Connection state check:** Kiểm tra client đã connected trước khi publish
2. **Success logging:** Log khi publish thành công với preview của JSON
3. **Error logging:** Log chi tiết khi có lỗi
4. **Setup logging:** Log khi node được tạo với thông tin cấu hình

## Next Steps

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
   mosquitto_sub -h localhost -p 1883 -t "ba_crossline/events" -v
   ```

4. **Nếu vẫn không có messages:**
   - Kiểm tra xem `ba_crossline` có output events không (xem logs của ba_crossline node)
   - Thử thay đổi `broke_for` sang các giá trị khác
   - Xem xét sử dụng `ba_socket_broker` thay vì `json_mqtt_broker`

## References

- Sample code: `simple_rtmp_mqtt_sample.cpp` (sử dụng `cvedix_json_enhanced_mqtt_broker_node` với `broke_for::FACE`)
- BA Socket Broker example: `examples/instances/infer_nodes/example_ba_socket_broker.json`
- CVEDIX SDK documentation về `cvedix_json_mqtt_broker_node`

