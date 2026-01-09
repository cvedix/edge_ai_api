# BA Jam API Examples

## 📚 API Endpoints

Tất cả các endpoints đều có prefix: `/v1/core/instance/{instanceId}/jams`

### 1. GET - Lấy tất cả jam zones

```bash
curl http://localhost:8080/v1/core/instance/{instanceId}/jams
```

**Response:**
```json
[
  {
    "id": "zone1",
    "name": "Front Lane",
    "roi": [
      {"x": 100, "y": 300},
      {"x": 700, "y": 300},
      {"x": 700, "y": 400},
      {"x": 100, "y": 400}
    ],
    "checkMinStops": 30,
    "checkMaxDistance": 5,
    "checkIntervalFrames": 10,
    "checkNotifyInterval": 0
  }
]
```

### 2. GET - Lấy một jam zone theo ID

```bash
curl http://localhost:8080/v1/core/instance/{instanceId}/jams/{jamId}
```

### 3. POST - Tạo jam zone mới

**Tạo một zone:**
```bash
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/jams \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Entrance Zone",
    "roi": [
      {"x": 100, "y": 300},
      {"x": 700, "y": 300},
      {"x": 700, "y": 400},
      {"x": 100, "y": 400}
    ],
    "checkMinStops": 30,
    "checkMaxDistance": 5,
    "checkIntervalFrames": 10
  }'
```

**Tạo nhiều zones cùng lúc:**
```bash
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/jams \
  -H "Content-Type: application/json" \
  -d '[
    {
      "name": "Zone 1",
      "roi": [{"x": 100, "y": 300}, {"x": 700, "y": 300}, {"x": 700, "y": 400}, {"x": 100, "y": 400}],
      "checkMinStops": 30
    },
    {
      "name": "Zone 2",
      "roi": [{"x": 200, "y": 500}, {"x": 800, "y": 500}, {"x": 800, "y": 600}, {"x": 200, "y": 600}],
      "checkMinStops": 20
    }
  ]'
```

### 4. PUT - Cập nhật jam zone

```bash
curl -X PUT http://localhost:8080/v1/core/instance/{instanceId}/jams/{jamId} \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Updated Zone",
    "roi": [
      {"x": 150, "y": 350},
      {"x": 750, "y": 350},
      {"x": 750, "y": 450},
      {"x": 150, "y": 450}
    ],
    "checkMinStops": 25
  }'
```

### 5. DELETE - Xóa một jam zone

```bash
curl -X DELETE http://localhost:8080/v1/core/instance/{instanceId}/jams/{jamId}
```

### 6. DELETE - Xóa tất cả jam zones

```bash
curl -X DELETE http://localhost:8080/v1/core/instance/{instanceId}/jams
```

### 7. POST - Batch update nhiều zones

```bash
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/jams/batch \
  -H "Content-Type: application/json" \
  -d '[
    {
      "id": "zone1",
      "name": "Updated Zone 1",
      "roi": [{"x": 100, "y": 300}, {"x": 700, "y": 300}, {"x": 700, "y": 400}, {"x": 100, "y": 400}],
      "checkMinStops": 35
    },
    {
      "id": "zone2",
      "name": "Updated Zone 2",
      "roi": [{"x": 200, "y": 500}, {"x": 800, "y": 500}, {"x": 800, "y": 600}, {"x": 200, "y": 600}],
      "checkMinStops": 25
    }
  ]'
```

## 📝 Lưu ý

- Tất cả các thao tác thêm/sửa/xóa sẽ tự động restart instance để áp dụng thay đổi
- Các thay đổi được lưu vào config và sẽ persist qua restart
- `id` sẽ được tự động generate nếu không được cung cấp
- `roi` phải là array tối thiểu 3 điểm để tạo polygon

