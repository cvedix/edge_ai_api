# Hướng dẫn sử dụng Nodes - Ví dụ thực tế

## Tổng quan về bộ nodes của bạn

Bạn có **10 pre-configured nodes** với các loại sau:

- **Source (1 node)**: App Source
- **Processor (4 nodes)**: Face OSD v2, SORT Tracker, BA Crossline, BA Crossline OSD  
- **Destination (2 nodes)**: File Destination, Screen Destination
- **Broker (3 nodes)**: JSON Console Broker, Message Broker, JSON Enhanced Console Broker

Tất cả nodes đều có `inUse: false`, nghĩa là bạn có thể sử dụng chúng ngay!

---

## Các thao tác bạn có thể thực hiện

### 1. Xem chi tiết một node cụ thể

**GET** `/v1/core/nodes/{nodeId}`

**Ví dụ:**
```bash
curl http://localhost:8080/v1/core/nodes/node_b3ed65e5
```

**Response:**
```json
{
  "nodeId": "node_b3ed65e5",
  "templateId": "file_des_template",
  "displayName": "File Destination",
  "category": "destination",
  "description": "Save video to file",
  "nodeType": "file_des",
  "parameters": {
    "name_prefix": "object_detection",
    "osd": "true",
    "save_dir": "./output/{instanceId}"
  },
  "inUse": false,
  "createdAt": "2025-12-08T23:58:27Z"
}
```

---

### 2. Cập nhật tham số của node

**PUT** `/v1/core/nodes/{nodeId}`

**Lưu ý:** Chỉ có thể cập nhật node khi `inUse: false`

**Ví dụ - Cập nhật File Destination:**
```bash
curl -X PUT http://localhost:8080/v1/core/nodes/node_b3ed65e5 \
  -H "Content-Type: application/json" \
  -d '{
    "parameters": {
      "name_prefix": "face_detection",
      "osd": "true",
      "save_dir": "./output/videos/{instanceId}"
    }
  }'
```

**Response:**
```json
{
  "nodeId": "node_xxxxx",  // Node ID mới (vì update = delete + recreate)
  "oldNodeId": "node_b3ed65e5",
  "message": "Node updated successfully",
  ...
}
```

---

### 3. Xóa node

**DELETE** `/v1/core/nodes/{nodeId}`

**Lưu �ý:** Chỉ có thể xóa node khi `inUse: false`

**Ví dụ:**
```bash
curl -X DELETE http://localhost:8080/v1/core/nodes/node_cdeaa847
```

**Response:**
```json
{
  "message": "Node deleted successfully",
  "nodeId": "node_cdeaa847"
}
```

---

### 4. Tạo Solution từ các nodes đã chọn ⭐ (Quan trọng nhất!)

**POST** `/v1/core/nodes/build-solution`

Đây là bước quan trọng để tạo pipeline từ các nodes của bạn.

#### Ví dụ 1: Pipeline Face Detection với Crossline Detection

```bash
curl -X POST http://localhost:8080/v1/core/nodes/build-solution \
  -H "Content-Type: application/json" \
  -d '{
    "solutionId": "face_crossline_detection",
    "solutionName": "Face Detection với Crossline Detection",
    "nodeIds": [
      "node_1dec8f47",  // App Source
      "node_850b7224",  // Face OSD v2
      "node_f2298aa4",  // BA Crossline
      "node_d3c2fe43",  // BA Crossline OSD
      "node_b3ed65e5",  // File Destination
      "node_5c1091a6"   // JSON Console Broker
    ]
  }'
```

**Giải thích pipeline:**
1. **App Source** → Nhận video frames từ ứng dụng
2. **Face OSD v2** → Phát hiện và overlay kết quả face detection
3. **BA Crossline** → Phát hiện hành vi vượt đường
4. **BA Crossline OSD** → Overlay kết quả crossline detection
5. **File Destination** → Lưu video ra file
6. **JSON Console Broker** → Xuất kết quả detection ra console dạng JSON

#### Ví dụ 2: Pipeline Tracking với Screen Display

```bash
curl -X POST http://localhost:8080/v1/core/nodes/build-solution \
  -H "Content-Type: application/json" \
  -d '{
    "solutionId": "object_tracking_display",
    "solutionName": "Object Tracking với Screen Display",
    "nodeIds": [
      "node_1dec8f47",  // App Source
      "node_f090d710",  // SORT Tracker
      "node_c49e9a66",  // Screen Destination
      "node_cdeaa847"   // JSON Enhanced Console Broker
    ]
  }'
```

**Giải thích pipeline:**
1. **App Source** → Nhận video frames
2. **SORT Tracker** → Track objects bằng SORT algorithm
3. **Screen Destination** → Hiển thị video trên màn hình
4. **JSON Enhanced Console Broker** → Xuất kết quả tracking ra console

#### Ví dụ 3: Pipeline đơn giản - Chỉ lưu file

```bash
curl -X POST http://localhost:8080/v1/core/nodes/build-solution \
  -H "Content-Type: application/json" \
  -d '{
    "solutionId": "simple_file_save",
    "solutionName": "Simple File Save",
    "nodeIds": [
      "node_1dec8f47",  // App Source
      "node_b3ed65e5"   // File Destination
    ]
  }'
```

**Response (cho tất cả các ví dụ):**
```json
{
  "solutionId": "face_crossline_detection",
  "solutionName": "Face Detection với Crossline Detection",
  "pipeline": [
    {
      "nodeType": "app_src",
      "nodeName": "App Source_{instanceId}",
      "parameters": {
        "channel": "0"
      }
    },
    {
      "nodeType": "face_osd_v2",
      "nodeName": "Face OSD v2_{instanceId}",
      "parameters": {}
    },
    {
      "nodeType": "ba_crossline",
      "nodeName": "BA Crossline_{instanceId}",
      "parameters": {
        "line_channel": "0",
        "line_end_x": "700",
        "line_end_y": "220",
        "line_start_x": "0",
        "line_start_y": "250"
      }
    },
    {
      "nodeType": "ba_crossline_osd",
      "nodeName": "BA Crossline OSD_{instanceId}",
      "parameters": {}
    },
    {
      "nodeType": "file_des",
      "nodeName": "File Destination_{instanceId}",
      "parameters": {
        "name_prefix": "object_detection",
        "osd": "true",
        "save_dir": "./output/{instanceId}"
      }
    },
    {
      "nodeType": "json_console_broker",
      "nodeName": "JSON Console Broker_{instanceId}",
      "parameters": {
        "broke_for": "NORMAL"
      }
    }
  ],
  "message": "Solution created successfully. You can now use this solutionId to create instances."
}
```

---

### 5. Tạo Instance từ Solution ⭐ (Bước cuối cùng!)

Sau khi đã tạo Solution, bạn có thể tạo Instance để chạy pipeline.

**POST** `/v1/core/instance`

**Ví dụ:**
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Face Detection Instance 1",
    "solution": "face_crossline_detection",
    "autoStart": true
  }'
```

**Response:**
```json
{
  "instanceId": "inst_xxxxx",
  "name": "Face Detection Instance 1",
  "status": "running",
  "solution": "face_crossline_detection",
  "message": "Instance created and started successfully"
}
```

---

## Workflow hoàn chỉnh

### Bước 1: Kiểm tra nodes có sẵn
```bash
curl http://localhost:8080/v1/core/nodes/preconfigured/available
```

### Bước 2: Chọn nodes và tạo Solution
```bash
curl -X POST http://localhost:8080/v1/core/nodes/build-solution \
  -H "Content-Type: application/json" \
  -d '{
    "solutionId": "my_custom_pipeline",
    "solutionName": "My Custom Pipeline",
    "nodeIds": ["node_1dec8f47", "node_850b7224", "node_b3ed65e5"]
  }'
```

### Bước 3: Tạo Instance từ Solution
```bash
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d '{
    "name": "My Instance",
    "solution": "my_custom_pipeline",
    "autoStart": true
  }'
```

### Bước 4: Kiểm tra trạng thái Instance
```bash
curl http://localhost:8080/v1/core/instance/{instanceId}
```

---

## Các kết hợp nodes phổ biến

### 1. Source → Processor → Destination
```
App Source → Face OSD v2 → File Destination
```

### 2. Source → Processor → Processor → Destination
```
App Source → Face OSD v2 → BA Crossline → File Destination
```

### 3. Source → Processor → Destination + Broker
```
App Source → SORT Tracker → Screen Destination + JSON Console Broker
```

### 4. Source → Processor → Processor → Destination + Broker
```
App Source → Face OSD v2 → BA Crossline → BA Crossline OSD → File Destination + JSON Enhanced Console Broker
```

---

## ⚠️ Khi Node có `inUse: true` thì sao?

### Node `inUse: true` nghĩa là gì?

Khi một node có `inUse: true`, nghĩa là node đó **đang được sử dụng bởi một hoặc nhiều Instances đang chạy**.

### Những gì bạn **KHÔNG THỂ** làm với node `inUse: true`:

#### 1. ❌ **KHÔNG THỂ cập nhật node**
```bash
curl -X PUT http://localhost:8080/v1/core/nodes/node_xxxxx \
  -H "Content-Type: application/json" \
  -d '{"parameters": {...}}'
```

**Response (Lỗi 409 Conflict):**
```json
{
  "error": "Conflict",
  "message": "Cannot update node that is currently in use"
}
```

#### 2. ❌ **KHÔNG THỂ xóa node**
```bash
curl -X DELETE http://localhost:8080/v1/core/nodes/node_xxxxx
```

**Response (Lỗi 409 Conflict):**
```json
{
  "error": "Conflict",
  "message": "Cannot delete node that is currently in use"
}
```

### Những gì bạn **VẪN CÓ THỂ** làm với node `inUse: true`:

#### 1. ✅ **Vẫn có thể xem chi tiết node**
```bash
curl http://localhost:8080/v1/core/nodes/node_xxxxx
```
→ Hoạt động bình thường

#### 2. ✅ **Vẫn có thể sử dụng node trong Solution mới**
```bash
curl -X POST http://localhost:8080/v1/core/nodes/build-solution \
  -H "Content-Type: application/json" \
  -d '{
    "solutionId": "new_solution",
    "solutionName": "New Solution",
    "nodeIds": ["node_xxxxx", "node_yyyyy"]  // node_xxxxx có inUse: true vẫn OK
  }'
```
→ Hoạt động bình thường! Node có thể được tái sử dụng trong nhiều solutions.

#### 3. ✅ **Vẫn có thể tạo Instance từ Solution chứa node đó**
→ Hoạt động bình thường! Một node có thể được dùng bởi nhiều instances.

### Làm thế nào để giải phóng node (`inUse: true` → `inUse: false`)?

**Cách 1: Dừng tất cả Instances đang sử dụng node đó**

1. Tìm các Instances đang chạy:
```bash
curl http://localhost:8080/v1/core/instances
```

2. Dừng các Instances đó:
```bash
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/stop
```

3. Sau khi dừng Instance, node sẽ tự động được giải phóng (`inUse: false`)

**Cách 2: Xóa Instance đang sử dụng node**

```bash
curl -X DELETE http://localhost:8080/v1/core/instance/{instanceId}
```

Sau khi xóa Instance, node sẽ tự động được giải phóng.

### Kiểm tra trạng thái nodes

**Xem tất cả nodes và trạng thái:**
```bash
curl http://localhost:8080/v1/core/nodes/preconfigured
```

**Response:**
```json
{
  "available": 8,
  "inUse": 2,
  "nodes": [
    {
      "nodeId": "node_xxxxx",
      "inUse": true,  // ← Node đang được sử dụng
      ...
    },
    {
      "nodeId": "node_yyyyy",
      "inUse": false,  // ← Node chưa được sử dụng
      ...
    }
  ],
  "total": 10
}
```

**Chỉ xem nodes chưa sử dụng:**
```bash
curl http://localhost:8080/v1/core/nodes/preconfigured/available
```

**Xem thống kê:**
```bash
curl http://localhost:8080/v1/core/nodes/stats
```

**Response:**
```json
{
  "totalTemplates": 20,
  "totalPreConfiguredNodes": 10,
  "availableNodes": 8,
  "inUseNodes": 2,  // ← Số nodes đang được sử dụng
  "nodesByCategory": {...}
}
```

### Ví dụ tình huống thực tế

**Tình huống:** Bạn muốn cập nhật tham số của node `node_b3ed65e5` (File Destination) nhưng node đang có `inUse: true`.

**Giải pháp:**

1. **Kiểm tra node đang được sử dụng bởi Instance nào:**
```bash
# Xem tất cả instances
curl http://localhost:8080/v1/core/instances

# Tìm instance nào đang dùng solution có chứa node_b3ed65e5
```

2. **Dừng Instance đó:**
```bash
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/stop
```

3. **Đợi node được giải phóng:**
```bash
# Kiểm tra lại
curl http://localhost:8080/v1/core/nodes/node_b3ed65e5
# → inUse: false
```

4. **Bây giờ có thể cập nhật:**
```bash
curl -X PUT http://localhost:8080/v1/core/nodes/node_b3ed65e5 \
  -H "Content-Type: application/json" \
  -d '{
    "parameters": {
      "name_prefix": "new_prefix",
      "osd": "true",
      "save_dir": "./output/new_dir/{instanceId}"
    }
  }'
```

5. **Khởi động lại Instance nếu cần:**
```bash
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/start
```

### Tóm tắt

| Trạng thái Node | Có thể xem? | Có thể update? | Có thể delete? | Có thể dùng trong Solution? |
|-----------------|-------------|----------------|----------------|------------------------------|
| `inUse: false` | ✅ Có | ✅ Có | ✅ Có | ✅ Có |
| `inUse: true` | ✅ Có | ❌ Không | ❌ Không | ✅ Có |

---

## Lưu ý quan trọng

1. **Thứ tự nodes quan trọng**: Nodes được xử lý theo thứ tự trong mảng `nodeIds`
   - Luôn bắt đầu với **Source node**
   - Tiếp theo là **Processor nodes** (có thể nhiều)
   - Cuối cùng là **Destination** và/hoặc **Broker**

2. **Nodes có thể tái sử dụng**: Một node có thể được dùng trong nhiều solutions khác nhau, và một solution có thể tạo nhiều instances

3. **Không thể xóa/cập nhật node đang sử dụng**: Nếu `inUse: true`, bạn phải dừng/xóa các Instances đang sử dụng node đó trước

4. **Một Solution có thể tạo nhiều Instances**: Sau khi tạo Solution, bạn có thể tạo nhiều Instances từ cùng một Solution

---

## Danh sách đầy đủ các nodes của bạn

| Node ID | Category | Display Name | Node Type | Mô tả |
|---------|----------|--------------|-----------|-------|
| `node_1dec8f47` | source | App Source | app_src | Nhận video frames từ ứng dụng |
| `node_850b7224` | processor | Face OSD v2 | face_osd_v2 | Overlay face detection results |
| `node_f090d710` | processor | SORT Tracker | sort_track | Track objects bằng SORT algorithm |
| `node_f2298aa4` | processor | BA Crossline | ba_crossline | Phát hiện hành vi vượt đường |
| `node_d3c2fe43` | processor | BA Crossline OSD | ba_crossline_osd | Overlay crossline detection results |
| `node_b3ed65e5` | destination | File Destination | file_des | Lưu video ra file |
| `node_c49e9a66` | destination | Screen Destination | screen_des | Hiển thị video trên màn hình |
| `node_5c1091a6` | broker | JSON Console Broker | json_console_broker | Xuất kết quả ra console dạng JSON |
| `node_407d684a` | broker | Message Broker | msg_broker | Generic message broker |
| `node_cdeaa847` | broker | JSON Enhanced Console Broker | json_enhanced_console_broker | Enhanced JSON console output |

---

## API Endpoints tổng hợp

| Method | Endpoint | Mô tả |
|--------|----------|-------|
| GET | `/v1/core/nodes` | Lấy danh sách tất cả nodes |
| GET | `/v1/core/nodes/{nodeId}` | Xem chi tiết một node |
| PUT | `/v1/core/nodes/{nodeId}` | Cập nhật node |
| DELETE | `/v1/core/nodes/{nodeId}` | Xóa node |
| GET | `/v1/core/nodes/preconfigured` | Lấy danh sách pre-configured nodes |
| GET | `/v1/core/nodes/preconfigured/available` | Lấy nodes chưa sử dụng |
| POST | `/v1/core/nodes/build-solution` | Tạo Solution từ nodes |
| POST | `/v1/core/instance` | Tạo Instance từ Solution |
| GET | `/v1/core/nodes/stats` | Xem thống kê nodes |

---

Chúc bạn sử dụng thành công! 🚀

