# CVEDIX JSON MQTT Broker Node API Analysis

## 📋 Constructor Signature

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

## ✅ Code Hiện Tại Đang Dùng

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

## 🔧 Các Methods Có Thể Dùng

### 1. `set_mqtt_publisher()`
```cpp
void set_mqtt_publisher(std::function<void(const std::string&)> publisher);
```

**Có thể dùng để:**
- Set publisher sau khi tạo node (nếu cần)
- Update publisher runtime (nếu cần reconnect)

**Hiện tại:** Không cần vì đã set trong constructor

### 2. `set_json_transformer()`
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

### 3. `get_mqtt_publisher()` / `get_json_transformer()`
```cpp
std::function<void(const std::string&)> get_mqtt_publisher() const;
std::function<std::string(const std::string&)> get_json_transformer() const;
```

**Có thể dùng để:**
- Debug: Check xem publisher có được set không
- Validation: Verify configuration

## 💡 Cải Tiến Có Thể Thực Hiện

### 1. **Thêm JSON Transformer để Add Metadata**

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

### 2. **Sử dụng set_mqtt_publisher() Nếu Cần Update Runtime**

Nếu cần reconnect MQTT và update publisher:

```cpp
// Create node first
auto node = std::make_shared<cvedix_nodes::cvedix_json_mqtt_broker_node>(
    nodeName, brokeFor, warnThreshold, ignoreThreshold, nullptr, nullptr
);

// Set publisher later (after MQTT connection established)
node->set_mqtt_publisher(mqtt_publish_func);
```

## 📊 So Sánh: Code Hiện Tại vs Có Thể Cải Tiến

| Feature | Code Hiện Tại | Có Thể Cải Tiến |
|---------|---------------|-----------------|
| Constructor | ✅ Đúng signature | ✅ OK |
| MQTT Publisher | ✅ Non-blocking với background thread | ✅ OK |
| JSON Transformer | ❌ nullptr (use original) | ⚠️ Có thể thêm metadata |
| Runtime Update | ❌ Không support | ⚠️ Có thể dùng set_mqtt_publisher() |

## 🎯 Kết Luận

1. **Code hiện tại đã đúng:** Constructor signature match hoàn toàn ✅
2. **Không cần thay đổi:** Implementation hiện tại đã tốt
3. **Có thể cải tiến (optional):**
   - Thêm JSON transformer để add metadata
   - Sử dụng setter methods nếu cần runtime update

## 🔍 Kiểm Tra

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

