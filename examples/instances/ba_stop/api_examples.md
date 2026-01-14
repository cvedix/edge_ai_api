# BA Stop API Examples

## 📚 API Endpoints

Tất cả các endpoints đều có prefix: `/v1/core/instance/{instanceId}/stops`

### 1. GET - Lấy tất cả stop zones

```bash
curl http://localhost:8080/v1/core/instance/{instanceId}/stops
```

**Response:**
```json
[
  {
    "id": "zone1",
    "name": "Entrance Stop Zone",
    "roi": [
      {"x": 20, "y": 30},
      {"x": 600, "y": 40},
      {"x": 600, "y": 300},
      {"x": 10, "y": 300}
    ],
    "min_stop_seconds": 3,
    "check_interval_frames": 20,
    "check_min_hit_frames": 50,
    "check_max_distance": 5
  }
]
```

### 2. GET - Lấy một stop zone theo ID

```bash
curl http://localhost:8080/v1/core/instance/{instanceId}/stops/{stopId}
```

### 3. POST - Tạo stop zone mới

**Tạo một zone:**
```bash
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/stops \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Entrance Stop Zone",
    "roi": [
      {"x": 20, "y": 30},
      {"x": 600, "y": 40},
      {"x": 600, "y": 300},
      {"x": 10, "y": 300}
    ],
    "min_stop_seconds": 3,
    "check_interval_frames": 20,
    "check_min_hit_frames": 50,
    "check_max_distance": 5
  }'
```

**Tạo nhiều zones cùng lúc:**
```bash
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/stops \
  -H "Content-Type: application/json" \
  -d '[
    {
      "name": "Channel 0 Stop Zone",
      "roi": [{"x": 20, "y": 30}, {"x": 600, "y": 40}, {"x": 600, "y": 300}, {"x": 10, "y": 300}],
      "min_stop_seconds": 3
    },
    {
      "name": "Channel 1 Stop Zone",
      "roi": [{"x": 20, "y": 30}, {"x": 1000, "y": 40}, {"x": 1000, "y": 600}, {"x": 10, "y": 600}],
      "min_stop_seconds": 3
    }
  ]'
```

### 4. PUT - Cập nhật stop zone

```bash
curl -X PUT http://localhost:8080/v1/core/instance/{instanceId}/stops/{stopId} \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Updated Zone",
    "roi": [
      {"x": 50, "y": 50},
      {"x": 650, "y": 60},
      {"x": 650, "y": 320},
      {"x": 40, "y": 320}
    ],
    "min_stop_seconds": 5
  }'
```

### 5. DELETE - Xóa một stop zone

```bash
curl -X DELETE http://localhost:8080/v1/core/instance/{instanceId}/stops/{stopId}
```

### 6. DELETE - Xóa tất cả stop zones

```bash
curl -X DELETE http://localhost:8080/v1/core/instance/{instanceId}/stops
```

### 7. POST - Batch update nhiều zones

```bash
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/stops/batch \
  -H "Content-Type: application/json" \
  -d '[
    {
      "id": "zone1",
      "name": "Updated Zone 1",
      "roi": [{"x": 20, "y": 30}, {"x": 600, "y": 40}, {"x": 600, "y": 300}, {"x": 10, "y": 300}],
      "min_stop_seconds": 5
    },
    {
      "id": "zone2",
      "name": "Updated Zone 2",
      "roi": [{"x": 20, "y": 30}, {"x": 1000, "y": 40}, {"x": 1000, "y": 600}, {"x": 10, "y": 600}],
      "min_stop_seconds": 4
    }
  ]'
```

## 📝 Lưu ý

- Tất cả các thao tác thêm/sửa/xóa sẽ tự động restart instance để áp dụng thay đổi
- Các thay đổi được lưu vào config và sẽ persist qua restart
- `id` sẽ được tự động generate nếu không được cung cấp
- `roi` phải là array tối thiểu 3 điểm để tạo polygon

