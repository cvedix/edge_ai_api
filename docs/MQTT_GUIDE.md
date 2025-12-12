# MQTT Guide - Edge AI API

Tài liệu này hướng dẫn về MQTT implementation trong Edge AI API, bao gồm non-blocking publisher, debug guide, và API reference.

## 📋 Mục Lục

1. [Non-Blocking MQTT Implementation](#non-blocking-mqtt-implementation)
2. [MQTT Debug Guide](#mqtt-debug-guide)
3. [CVEDIX JSON MQTT Broker Node API](#cvedix-json-mqtt-broker-node-api)
4. [Troubleshooting](#troubleshooting)

---

## Non-Blocking MQTT Implementation

### ✅ Đã Implement

#### 1. **Background Thread cho MQTT Publish**

- Tạo `NonBlockingMQTTPublisher` class với background thread riêng
- Thread name: `mqtt-publisher` (có thể debug dễ dàng)
- Thread chạy độc lập, không block node thread

#### 2. **Bounded Queue với Non-Blocking Enqueue**

- Queue size: **1000 messages** (có thể config)
- Non-blocking enqueue: Nếu queue đầy, drop message cũ nhất (FIFO drop)
- Sử dụng `std::condition_variable` để notify thread khi có message mới

#### 3. **Timeout Protection**

- Timeout cho mỗi publish: **100ms**
- Log warning nếu publish mất quá 100ms
- Thread không bị block vô hạn

#### 4. **Tăng Buffer Capacity**

- `mosquitto_max_inflight_messages_set()`: Tăng từ 1000 → **5000**
- Cho phép buffer nhiều messages hơn trước khi blocking

#### 5. **Batch Processing**

- Process tối đa **10 messages** mỗi iteration
- Giảm overhead của lock/unlock
- Tăng throughput

#### 6. **Statistics Tracking**

- Track số messages đã publish
- Track số messages bị drop
- Log statistics khi destroy publisher

### 📝 Code Structure

```cpp
struct NonBlockingMQTTPublisher {
    std::shared_ptr<struct mosquitto> client;
    std::string topic;
    std::queue<std::string> message_queue;  // Bounded queue
    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    std::atomic<bool> running{true};
    std::thread publisher_thread;           // Background thread
    std::atomic<int> dropped_count{0};
    std::atomic<int> published_count{0};
    
    static constexpr size_t MAX_QUEUE_SIZE = 1000;
    static constexpr int PUBLISH_TIMEOUT_MS = 100;
    
    // Non-blocking enqueue - drops if queue full
    void enqueue(const std::string& json_data);
};
```

### 🔄 Flow

1. **Node thread** gọi `mqtt_publish_func(json_data)`
2. Function **non-blocking** enqueue message vào queue
3. Nếu queue đầy → drop message cũ nhất
4. **Background thread** lấy messages từ queue và publish
5. Background thread không block node thread

### 🎯 Benefits

1. **Non-blocking**: Node thread không bao giờ bị block
2. **Timeout protection**: Publish không thể block vô hạn
3. **Queue management**: Drop messages khi queue đầy thay vì crash
4. **Better throughput**: Batch processing tăng hiệu suất
5. **Statistics**: Track dropped/published messages để monitor

### ⚙️ Configuration

Có thể điều chỉnh các tham số:

```cpp
static constexpr size_t MAX_QUEUE_SIZE = 1000;      // Queue size
static constexpr int PUBLISH_TIMEOUT_MS = 100;      // Timeout per publish
const size_t MAX_BATCH = 10;                        // Batch size
```

### 📊 Monitoring

Logs sẽ hiển thị:
- Số messages đã publish
- Số messages bị drop
- Warnings nếu publish mất quá 100ms
- Statistics khi destroy publisher

### 🚀 Testing

Để test:
1. Chạy instance với MQTT: `example_ba_crossline_file_mqtt_test.json`
2. Monitor logs để xem:
   - `[MQTT] Background publisher thread started`
   - `[MQTT] Published #X to topic...`
   - `[MQTT] Publisher statistics: Published=X, Dropped=Y`
3. Kiểm tra xem queue có còn đầy không
4. Kiểm tra xem có còn crash không

### 🔧 Troubleshooting

Nếu vẫn có vấn đề:

1. **Tăng MAX_QUEUE_SIZE**: Nếu messages bị drop quá nhiều
2. **Tăng PUBLISH_TIMEOUT_MS**: Nếu network chậm
3. **Tăng MAX_BATCH**: Nếu cần throughput cao hơn
4. **Kiểm tra MQTT broker**: Đảm bảo broker phản hồi nhanh

### 📌 Notes

- Background thread sẽ tự động cleanup khi publisher bị destroy
- Thread join có timeout 2 giây để tránh block shutdown
- Messages trong queue sẽ được publish trước khi thread stop

---

## MQTT Debug Guide

### Vấn đề

MQTT broker node không gửi được messages từ `ba_crossline` node.

### Kiểm tra đã thực hiện

#### 1. MQTT Connection
- ✅ MQTT client được tạo thành công
- ✅ Kết nối đến broker thành công (`localhost:1883`)
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
   mosquitto_sub -h localhost -p 1883 -t "ba_crossline/events" -v
   ```

4. **Nếu vẫn không có messages:**
   - Kiểm tra xem `ba_crossline` có output events không (xem logs của ba_crossline node)
   - Thử thay đổi `broke_for` sang các giá trị khác
   - Xem xét sử dụng `ba_socket_broker` thay vì `json_mqtt_broker`

---

## CVEDIX JSON MQTT Broker Node API

### 📋 Constructor Signature

Từ header file `cvedix_json_mqtt_broker_node.h`:

```cpp
cvedix_json_mqtt_broker_node(
    std::string node_name,
    cvedix_broke_for broke_for = cvedix_broke_for::NORMAL,
    int broking_cache_warn_threshold = 50,
    int broking_cache_ignore_threshold = 200,
    std::function<std::string(const std::string&)> json_transformer = nullptr,
    std::function<void(const std::string&)> mqtt_publisher = nullptr
);
```

### ✅ Code Hiện Tại Đang Dùng

```cpp
auto node = std::make_shared<cvedix_nodes::cvedix_json_mqtt_broker_node>(
    nodeName,                    // ✓ node_name
    brokeFor,                    // ✓ broke_for
    warnThreshold,               // ✓ broking_cache_warn_threshold
    ignoreThreshold,             // ✓ broking_cache_ignore_threshold
    nullptr,                     // ✓ json_transformer (nullptr = use original JSON)
    mqtt_publish_func            // ✓ mqtt_publisher (non-blocking function)
);
```

**Kết luận:** Code hiện tại đã dùng đúng constructor signature! ✅

### 🔧 Các Methods Có Thể Dùng

#### 1. `set_mqtt_publisher()`
```cpp
void set_mqtt_publisher(std::function<void(const std::string&)> publisher);
```

**Có thể dùng để:**
- Set publisher sau khi tạo node (nếu cần)
- Update publisher runtime (nếu cần reconnect)

**Hiện tại:** Không cần vì đã set trong constructor

#### 2. `set_json_transformer()`
```cpp
void set_json_transformer(std::function<std::string(const std::string&)> transformer);
```

**Có thể dùng để:**
- Customize JSON format trước khi publish
- Add metadata (timestamp, instance_id, etc.)
- Filter/modify JSON structure

**Ví dụ:**
```cpp
node->set_json_transformer([](const std::string& json) {
    return "{\"timestamp\": " + std::to_string(time(nullptr)) + 
           ", \"data\": " + json + "}";
});
```

#### 3. `get_mqtt_publisher()` / `get_json_transformer()`
```cpp
std::function<void(const std::string&)> get_mqtt_publisher() const;
std::function<std::string(const std::string&)> get_json_transformer() const;
```

**Có thể dùng để:**
- Debug: Check xem publisher có được set không
- Validation: Verify configuration

### 💡 Cải Tiến Có Thể Thực Hiện

#### 1. **Thêm JSON Transformer để Add Metadata**

Có thể thêm timestamp, instance_id vào JSON:

```cpp
auto json_transformer = [nodeName](const std::string& json) {
    auto now = std::time(nullptr);
    return "{\"timestamp\":" + std::to_string(now) + 
           ",\"instance_id\":\"" + nodeName + "\"" +
           ",\"data\":" + json + "}";
};

auto node = std::make_shared<cvedix_nodes::cvedix_json_mqtt_broker_node>(
    nodeName, brokeFor, warnThreshold, ignoreThreshold, 
    json_transformer,  // Custom transformer
    mqtt_publish_func
);
```

#### 2. **Sử dụng set_mqtt_publisher() Nếu Cần Update Runtime**

Nếu cần reconnect MQTT và update publisher:

```cpp
// Create node first
auto node = std::make_shared<cvedix_nodes::cvedix_json_mqtt_broker_node>(
    nodeName, brokeFor, warnThreshold, ignoreThreshold, nullptr, nullptr
);

// Set publisher later (after MQTT connection established)
node->set_mqtt_publisher(mqtt_publish_func);
```

### 📊 So Sánh: Code Hiện Tại vs Có Thể Cải Tiến

| Feature | Code Hiện Tại | Có Thể Cải Tiến |
|---------|---------------|-----------------|
| Constructor | ✅ Đúng signature | ✅ OK |
| MQTT Publisher | ✅ Non-blocking với background thread | ✅ OK |
| JSON Transformer | ❌ nullptr (use original) | ⚠️ Có thể thêm metadata |
| Runtime Update | ❌ Không support | ⚠️ Có thể dùng set_mqtt_publisher() |

### 🎯 Kết Luận

1. **Code hiện tại đã đúng:** Constructor signature match hoàn toàn ✅
2. **Không cần thay đổi:** Implementation hiện tại đã tốt
3. **Có thể cải tiến (optional):**
   - Thêm JSON transformer để add metadata
   - Sử dụng setter methods nếu cần runtime update

### 🔍 Kiểm Tra

Code hiện tại (line 4224-4230):
```cpp
auto node = std::make_shared<cvedix_nodes::cvedix_json_mqtt_broker_node>(
    nodeName,           // ✓ Parameter 1: node_name
    brokeFor,           // ✓ Parameter 2: broke_for
    warnThreshold,      // ✓ Parameter 3: warn_threshold
    ignoreThreshold,    // ✓ Parameter 4: ignore_threshold
    nullptr,            // ✓ Parameter 5: json_transformer
    mqtt_publish_func   // ✓ Parameter 6: mqtt_publisher
);
```

**Match 100% với constructor signature!** ✅

---

## Troubleshooting

Xem [Troubleshooting Guide](./TROUBLESHOOTING.md) để biết thêm về:
- MQTT vs RTMP queue crash
- MQTT debug issues
- Connection problems

---

## 📚 Tài Liệu Liên Quan

- [Troubleshooting Guide](./TROUBLESHOOTING.md) - Phân tích các vấn đề crash và deadlock
- [Queue Monitoring Guide](./QUEUE_MONITORING.md) - Giải pháp monitoring queue
- [Resize Ratio Guide](./RESIZE_RATIO_GUIDE.md) - Hướng dẫn tối ưu RESIZE_RATIO cho MQTT

