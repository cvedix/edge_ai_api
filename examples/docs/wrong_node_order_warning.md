# Cảnh báo: Thứ tự Nodes trong Pipeline

## Vấn đề

**Hệ thống hiện tại KHÔNG validate thứ tự các nodes trong pipeline.** Nếu bạn tạo một Solution với thứ tự nodes ngẫu nhiên (không đúng), pipeline có thể được build thành công nhưng sẽ **KHÔNG hoạt động đúng** hoặc gây lỗi khi chạy.

## Cách Pipeline Builder hoạt động

### Logic hiện tại

1. **Build nodes theo thứ tự**: Pipeline builder duyệt qua các nodes trong `solution.pipeline` theo thứ tự được định nghĩa
2. **Kết nối tự động**: Mỗi node (từ node thứ 2 trở đi) sẽ tự động attach vào node trước đó
3. **Không có validation**: Không có kiểm tra xem thứ tự có đúng không (source → processor → destination)

### Code tham khảo

```cpp
// Build nodes in pipeline order
for (const auto& nodeConfig : solution.pipeline) {
    auto node = createNode(nodeConfig, req, instanceId);
    if (node) {
        nodes.push_back(node);
        
        // Connect to previous node
        if (nodes.size() > 1) {
            node->attach_to({nodes[nodes.size() - 2]});
        }
    }
}
```

## Các trường hợp có thể gây lỗi

### ❌ Trường hợp 1: Destination trước Source

**Ví dụ sai:**
```json
{
  "pipeline": [
    {
      "nodeType": "file_des",  // ❌ Destination đầu tiên
      "nodeName": "destination_{instanceId}"
    },
    {
      "nodeType": "rtsp_src",  // ❌ Source sau destination
      "nodeName": "source_{instanceId}"
    }
  ]
}
```

**Hậu quả:**
- Pipeline build thành công
- Node destination không có data để nhận
- Instance có thể start nhưng không có output

### ❌ Trường hợp 2: Processor trước Source

**Ví dụ sai:**
```json
{
  "pipeline": [
    {
      "nodeType": "yunet_face_detector",  // ❌ Processor đầu tiên
      "nodeName": "detector_{instanceId}"
    },
    {
      "nodeType": "rtsp_src",  // ❌ Source sau processor
      "nodeName": "source_{instanceId}"
    }
  ]
}
```

**Hậu quả:**
- Pipeline build thành công
- Processor không có input data
- Có thể gây crash hoặc hang khi chạy

### ❌ Trường hợp 3: Thứ tự ngẫu nhiên

**Ví dụ sai:**
```json
{
  "pipeline": [
    {
      "nodeType": "file_des",
      "nodeName": "destination_{instanceId}"
    },
    {
      "nodeType": "yunet_face_detector",
      "nodeName": "detector_{instanceId}"
    },
    {
      "nodeType": "rtsp_src",
      "nodeName": "source_{instanceId}"
    }
  ]
}
```

**Hậu quả:**
- Pipeline build thành công
- Data flow không đúng: destination → detector → source (ngược lại!)
- Instance không hoạt động đúng

## ✅ Thứ tự đúng

### Quy tắc chung

```
[Source] → [Processor 1] → [Processor 2] → ... → [Destination/Broker]
```

### Ví dụ đúng

```json
{
  "pipeline": [
    {
      "nodeType": "rtsp_src",  // ✅ Source đầu tiên
      "nodeName": "source_{instanceId}"
    },
    {
      "nodeType": "yunet_face_detector",  // ✅ Processor tiếp theo
      "nodeName": "detector_{instanceId}"
    },
    {
      "nodeType": "file_des",  // ✅ Destination cuối cùng
      "nodeName": "destination_{instanceId}"
    }
  ]
}
```

## Các loại Nodes

### Source Nodes (Phải đứng đầu)
- `rtsp_src` - RTSP source
- `file_src` - File source
- `app_src` - App source
- `image_src` - Image source
- `rtmp_src` - RTMP source
- `udp_src` - UDP source

### Processor Nodes (Ở giữa)
- `yunet_face_detector` - Face detector
- `yolov11_detector` - Object detector
- `insight_face_recognition` - Face recognition
- `face_swap` - Face swap
- `mllm_analyser` - MLLM analyser
- `sort_track` - Tracker
- `ba_crossline` - Behavior analysis
- `face_osd_v2` - OSD overlay
- Và các processor nodes khác

### Destination Nodes (Ở cuối)
- `file_des` - File destination
- `rtmp_des` - RTMP destination
- `screen_des` - Screen destination

### Broker Nodes (Ở cuối, có thể nhiều)
- `json_console_broker` - JSON console broker
- `json_mqtt_broker` - JSON MQTT broker
- `json_kafka_broker` - JSON Kafka broker
- Và các broker nodes khác

## Lưu ý đặc biệt

### Multiple Destinations

Bạn có thể có nhiều destination nodes từ cùng một source:

```json
{
  "pipeline": [
    {
      "nodeType": "rtsp_src",
      "nodeName": "source_{instanceId}"
    },
    {
      "nodeType": "yunet_face_detector",
      "nodeName": "detector_{instanceId}"
    },
    {
      "nodeType": "file_des",  // ✅ Destination 1
      "nodeName": "file_dest_{instanceId}"
    },
    {
      "nodeType": "rtmp_des",  // ✅ Destination 2 (cùng source)
      "nodeName": "rtmp_dest_{instanceId}"
    }
  ]
}
```

Hệ thống có logic đặc biệt để xử lý trường hợp này - các destination nodes sẽ attach vào cùng một source node.

## Khuyến nghị

### 1. Luôn đặt Source đầu tiên

```json
{
  "pipeline": [
    {"nodeType": "rtsp_src", ...},  // ✅ Luôn đầu tiên
    ...
  ]
}
```

### 2. Processors theo thứ tự xử lý

```json
{
  "pipeline": [
    {"nodeType": "rtsp_src", ...},
    {"nodeType": "yunet_face_detector", ...},  // ✅ Detect trước
    {"nodeType": "insight_face_recognition", ...},  // ✅ Recognize sau
    ...
  ]
}
```

### 3. Destinations/Brokers ở cuối

```json
{
  "pipeline": [
    ...
    {"nodeType": "file_des", ...},  // ✅ Ở cuối
    {"nodeType": "json_console_broker", ...}  // ✅ Có thể nhiều
  ]
}
```

## Cách kiểm tra

### 1. Kiểm tra Solution

```bash
curl http://localhost:8080/v1/core/solutions/{solutionId}
```

Xem thứ tự nodes trong `pipeline` array.

### 2. Test Instance

```bash
# Tạo instance
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @your_instance.json

# Start và kiểm tra logs
curl -X POST http://localhost:8080/v1/core/instances/{instanceId}/start
```

### 3. Kiểm tra Logs

Xem logs để phát hiện lỗi:
- "No data received"
- "Pipeline not working"
- "Node not receiving input"

## Tài liệu tham khảo

- [Solution Registry](../../src/solutions/solution_registry.cpp)
- [Pipeline Builder](../../src/core/pipeline_builder.cpp)
- [Instance Guide](../../docs/INSTANCE_GUIDE.md)

