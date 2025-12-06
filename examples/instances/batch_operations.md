# Batch Operations - Start/Stop Multiple Instances Concurrently

API này hỗ trợ các batch endpoints để start/stop/restart nhiều instances đồng thời, giúp tối ưu hiệu suất và tốc độ phản hồi.

## Endpoints

- `POST /v1/core/instances/batch/start` - Start nhiều instances đồng thời
- `POST /v1/core/instances/batch/stop` - Stop nhiều instances đồng thời  
- `POST /v1/core/instances/batch/restart` - Restart nhiều instances đồng thời

## Ưu điểm

1. **Tốc độ nhanh hơn**: Các operations chạy song song thay vì tuần tự
2. **Hiệu quả hơn**: Sử dụng tối đa tài nguyên hệ thống
3. **Phản hồi nhanh**: API trả về kết quả ngay sau khi tất cả operations hoàn thành

## Ví dụ sử dụng

### 1. Batch Start - Khởi động nhiều instances

```bash
curl -X POST http://localhost:8848/v1/core/instances/batch/start \
  -H 'Content-Type: application/json' \
  -d '{
    "instanceIds": ["instance-1", "instance-2", "instance-3"]
  }'
```

**Request Body:**
```json
{
  "instanceIds": ["instance-1", "instance-2", "instance-3"]
}
```

**Response:**
```json
{
  "results": [
    {
      "instanceId": "instance-1",
      "success": true,
      "status": "started",
      "running": true
    },
    {
      "instanceId": "instance-2",
      "success": true,
      "status": "started",
      "running": true
    },
    {
      "instanceId": "instance-3",
      "success": false,
      "status": "failed",
      "error": "Could not start instance. Check if instance exists and has a pipeline."
    }
  ],
  "total": 3,
  "success": 2,
  "failed": 1,
  "message": "Batch start operation completed"
}
```

### 2. Batch Stop - Dừng nhiều instances

```bash
curl -X POST http://localhost:8848/v1/core/instances/batch/stop \
  -H 'Content-Type: application/json' \
  -d '{
    "instanceIds": ["instance-1", "instance-2", "instance-3"]
  }'
```

**Request Body:**
```json
{
  "instanceIds": ["instance-1", "instance-2", "instance-3"]
}
```

**Response:**
```json
{
  "results": [
    {
      "instanceId": "instance-1",
      "success": true,
      "status": "stopped",
      "running": false
    },
    {
      "instanceId": "instance-2",
      "success": true,
      "status": "stopped",
      "running": false
    },
    {
      "instanceId": "instance-3",
      "success": false,
      "status": "failed",
      "error": "Could not stop instance. Check if instance exists."
    }
  ],
  "total": 3,
  "success": 2,
  "failed": 1,
  "message": "Batch stop operation completed"
}
```

### 3. Batch Restart - Khởi động lại nhiều instances

```bash
curl -X POST http://localhost:8848/v1/core/instances/batch/restart \
  -H 'Content-Type: application/json' \
  -d '{
    "instanceIds": ["instance-1", "instance-2", "instance-3"]
  }'
```

**Request Body:**
```json
{
  "instanceIds": ["instance-1", "instance-2", "instance-3"]
}
```

**Response:**
```json
{
  "results": [
    {
      "instanceId": "instance-1",
      "success": true,
      "status": "restarted",
      "running": true
    },
    {
      "instanceId": "instance-2",
      "success": true,
      "status": "restarted",
      "running": true
    },
    {
      "instanceId": "instance-3",
      "success": false,
      "status": "failed",
      "error": "Could not restart instance. Check if instance exists and has a pipeline."
    }
  ],
  "total": 3,
  "success": 2,
  "failed": 1,
  "message": "Batch restart operation completed"
}
```

## So sánh hiệu suất

### Trước khi tối ưu (tuần tự):
- Start 10 instances: ~20-30 giây (mỗi instance ~2-3 giây)
- Stop 10 instances: ~10-15 giây (mỗi instance ~1-1.5 giây)

### Sau khi tối ưu (song song):
- Start 10 instances: ~3-5 giây (chạy đồng thời)
- Stop 10 instances: ~2-3 giây (chạy đồng thời)

**Cải thiện: ~5-6x nhanh hơn**

## Lưu ý

1. **Concurrent execution**: Tất cả operations chạy song song, không chờ nhau
2. **Error handling**: Mỗi instance được xử lý độc lập, lỗi của một instance không ảnh hưởng đến các instance khác
3. **Response format**: Response bao gồm kết quả chi tiết cho từng instance và tổng số thành công/thất bại
4. **Performance**: Batch operations tối ưu cho việc quản lý nhiều instances cùng lúc

## Ví dụ với Python

```python
import requests
import json

# Batch start multiple instances
def batch_start_instances(instance_ids):
    url = "http://localhost:8848/v1/core/instances/batch/start"
    payload = {"instanceIds": instance_ids}
    response = requests.post(url, json=payload)
    return response.json()

# Batch stop multiple instances
def batch_stop_instances(instance_ids):
    url = "http://localhost:8848/v1/core/instances/batch/stop"
    payload = {"instanceIds": instance_ids}
    response = requests.post(url, json=payload)
    return response.json()

# Usage
instance_ids = ["instance-1", "instance-2", "instance-3"]
result = batch_start_instances(instance_ids)
print(f"Started {result['success']} out of {result['total']} instances")
```

## Ví dụ với JavaScript/Node.js

```javascript
// Batch start multiple instances
async function batchStartInstances(instanceIds) {
  const response = await fetch('http://localhost:8848/v1/core/instances/batch/start', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
    },
    body: JSON.stringify({ instanceIds }),
  });
  return await response.json();
}

// Batch stop multiple instances
async function batchStopInstances(instanceIds) {
  const response = await fetch('http://localhost:8848/v1/core/instances/batch/stop', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
    },
    body: JSON.stringify({ instanceIds }),
  });
  return await response.json();
}

// Usage
const instanceIds = ['instance-1', 'instance-2', 'instance-3'];
batchStartInstances(instanceIds)
  .then(result => {
    console.log(`Started ${result.success} out of ${result.total} instances`);
  });
```

