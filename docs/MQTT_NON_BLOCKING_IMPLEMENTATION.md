# Implementation: Non-Blocking MQTT Publisher

## ✅ Đã Implement

### 1. **Background Thread cho MQTT Publish**

- Tạo `NonBlockingMQTTPublisher` class với background thread riêng
- Thread name: `mqtt-publisher` (có thể debug dễ dàng)
- Thread chạy độc lập, không block node thread

### 2. **Bounded Queue với Non-Blocking Enqueue**

- Queue size: **1000 messages** (có thể config)
- Non-blocking enqueue: Nếu queue đầy, drop message cũ nhất (FIFO drop)
- Sử dụng `std::condition_variable` để notify thread khi có message mới

### 3. **Timeout Protection**

- Timeout cho mỗi publish: **100ms**
- Log warning nếu publish mất quá 100ms
- Thread không bị block vô hạn

### 4. **Tăng Buffer Capacity**

- `mosquitto_max_inflight_messages_set()`: Tăng từ 1000 → **5000**
- Cho phép buffer nhiều messages hơn trước khi blocking

### 5. **Batch Processing**

- Process tối đa **10 messages** mỗi iteration
- Giảm overhead của lock/unlock
- Tăng throughput

### 6. **Statistics Tracking**

- Track số messages đã publish
- Track số messages bị drop
- Log statistics khi destroy publisher

## 📝 Code Structure

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

## 🔄 Flow

1. **Node thread** gọi `mqtt_publish_func(json_data)`
2. Function **non-blocking** enqueue message vào queue
3. Nếu queue đầy → drop message cũ nhất
4. **Background thread** lấy messages từ queue và publish
5. Background thread không block node thread

## 🎯 Benefits

1. **Non-blocking**: Node thread không bao giờ bị block
2. **Timeout protection**: Publish không thể block vô hạn
3. **Queue management**: Drop messages khi queue đầy thay vì crash
4. **Better throughput**: Batch processing tăng hiệu suất
5. **Statistics**: Track dropped/published messages để monitor

## ⚙️ Configuration

Có thể điều chỉnh các tham số:

```cpp
static constexpr size_t MAX_QUEUE_SIZE = 1000;      // Queue size
static constexpr int PUBLISH_TIMEOUT_MS = 100;      // Timeout per publish
const size_t MAX_BATCH = 10;                        // Batch size
```

## 📊 Monitoring

Logs sẽ hiển thị:
- Số messages đã publish
- Số messages bị drop
- Warnings nếu publish mất quá 100ms
- Statistics khi destroy publisher

## 🚀 Testing

Để test:
1. Chạy instance với MQTT: `example_ba_crossline_file_mqtt_test.json`
2. Monitor logs để xem:
   - `[MQTT] Background publisher thread started`
   - `[MQTT] Published #X to topic...`
   - `[MQTT] Publisher statistics: Published=X, Dropped=Y`
3. Kiểm tra xem queue có còn đầy không
4. Kiểm tra xem có còn crash không

## 🔧 Troubleshooting

Nếu vẫn có vấn đề:

1. **Tăng MAX_QUEUE_SIZE**: Nếu messages bị drop quá nhiều
2. **Tăng PUBLISH_TIMEOUT_MS**: Nếu network chậm
3. **Tăng MAX_BATCH**: Nếu cần throughput cao hơn
4. **Kiểm tra MQTT broker**: Đảm bảo broker phản hồi nhanh

## 📌 Notes

- Background thread sẽ tự động cleanup khi publisher bị destroy
- Thread join có timeout 2 giây để tránh block shutdown
- Messages trong queue sẽ được publish trước khi thread stop

