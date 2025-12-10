# Statistics API Documentation

Tài liệu này hướng dẫn sử dụng API endpoint để lấy thống kê thời gian thực (real-time statistics) của các instance trong Edge AI API Server.

## Tổng Quan

Statistics API cung cấp endpoint để theo dõi hiệu suất và trạng thái xử lý của các instance đang chạy:

- **GET /v1/core/instance/{instanceId}/statistics** - Lấy thống kê thời gian thực của instance

Thống kê bao gồm:
- Số lượng frames đã xử lý
- Tốc độ xử lý (FPS)
- Độ trễ (latency)
- Độ phân giải
- Kích thước queue
- Số frames bị drop
- Thời gian bắt đầu

**Lưu ý quan trọng:**
- Thống kê được tính toán theo thời gian thực (real-time)
- Thống kê sẽ reset về 0 khi instance được restart
- Thống kê không được lưu trữ persistent (chỉ tính toán khi được yêu cầu)
- Instance phải đang chạy (running) mới có thể lấy được thống kê

## Endpoints

### 1. Get Instance Statistics

**Endpoint:** `GET /v1/core/instance/{instanceId}/statistics`

**Mô tả:** Trả về thống kê thời gian thực của instance bao gồm frames processed, framerate, latency, và resolution.

**Path Parameters:**
- `instanceId` (required): Unique identifier của instance (UUID format)

**Request Headers:**
```
Accept: application/json
```

**Response (200 OK):**
```json
{
  "frames_processed": 1250,
  "source_framerate": 30.0,
  "current_framerate": 25.5,
  "latency": 200.0,
  "start_time": 1764900520,
  "resolution": "1280x720",
  "format": "BGR",
  "source_resolution": "1920x1080"
}
```

**Response khi instance không tồn tại hoặc không chạy (404 Not Found):**
```json
{
  "error": "Not found",
  "message": "Instance not found or not running: a5204fc9-9a59-f80f-fb9f-bf3b42214943"
}
```

**Chi tiết các trường:**

#### frames_processed
- **Type:** `integer` (int64)
- **Mô tả:** Tổng số frames đã được xử lý kể từ khi instance bắt đầu chạy
- **Ví dụ:** `1250`

#### source_framerate
- **Type:** `number` (double)
- **Mô tả:** Tốc độ frame từ source (FPS từ RTSP stream hoặc video file)
- **Ví dụ:** `30.0`
- **Lưu ý:** Có thể là 0 nếu không thể lấy được từ source node

#### current_framerate
- **Type:** `number` (double)
- **Mô tả:** Tốc độ xử lý hiện tại (FPS đang được xử lý)
- **Ví dụ:** `25.5`
- **Lưu ý:** Có thể khác với `source_framerate` do giới hạn xử lý hoặc dropped frames

#### latency
- **Type:** `number` (double)
- **Mô tả:** Độ trễ trung bình xử lý mỗi frame (milliseconds)
- **Ví dụ:** `200.0`
- **Công thức:** Được tính từ thời gian bắt đầu và số frames đã xử lý

#### start_time
- **Type:** `integer` (int64)
- **Mô tả:** Thời gian instance bắt đầu chạy (Unix timestamp, đơn vị: seconds)
- **Ví dụ:** `1764900520`
- **Lưu ý:** Có thể convert sang datetime để hiển thị

#### resolution
- **Type:** `string`
- **Mô tả:** Độ phân giải hiện tại đang xử lý (format: "WIDTHxHEIGHT")
- **Ví dụ:** `"1280x720"`
- **Lưu ý:** Có thể khác với `source_resolution` nếu có resize

#### format
- **Type:** `string`
- **Mô tả:** Format của frame (thường là "BGR" cho CVEDIX SDK)
- **Ví dụ:** `"BGR"`

#### source_resolution
- **Type:** `string`
- **Mô tả:** Độ phân giải gốc từ source (format: "WIDTHxHEIGHT")
- **Ví dụ:** `"1920x1080"`
- **Lưu ý:** Có thể là "0x0" nếu không thể lấy được từ source node

## Ví Dụ Sử Dụng

### Sử dụng curl

```bash
# Lấy thống kê của instance
curl --request GET \
  --url http://192.168.1.188:3546/v1/core/instance/a5204fc9-9a59-f80f-fb9f-bf3b42214943/statistics \
  --header 'Accept: application/json'
```

### Sử dụng httpie

```bash
# Lấy thống kê của instance
http GET http://192.168.1.188:3546/v1/core/instance/a5204fc9-9a59-f80f-fb9f-bf3b42214943/statistics Accept:application/json
```

### Sử dụng Python

```python
import requests

# Lấy thống kê của instance
instance_id = "a5204fc9-9a59-f80f-fb9f-bf3b42214943"
url = f"http://192.168.1.188:3546/v1/core/instance/{instance_id}/statistics"

headers = {
    "Accept": "application/json"
}

response = requests.get(url, headers=headers)

if response.status_code == 200:
    stats = response.json()
    print(f"Frames processed: {stats['frames_processed']}")
    print(f"Current FPS: {stats['current_framerate']}")
    print(f"Latency: {stats['latency']} ms")
    print(f"Resolution: {stats['resolution']}")
else:
    print(f"Error: {response.status_code} - {response.json()}")
```

### Sử dụng JavaScript (Node.js)

```javascript
const fetch = require('node-fetch');

async function getInstanceStatistics(instanceId) {
    const url = `http://192.168.1.188:3546/v1/core/instance/${instanceId}/statistics`;
    
    const response = await fetch(url, {
        method: 'GET',
        headers: {
            'Accept': 'application/json'
        }
    });
    
    if (response.ok) {
        const stats = await response.json();
        console.log(`Frames processed: ${stats.frames_processed}`);
        console.log(`Current FPS: ${stats.current_framerate}`);
        console.log(`Latency: ${stats.latency} ms`);
        console.log(`Resolution: ${stats.resolution}`);
        return stats;
    } else {
        const error = await response.json();
        console.error(`Error: ${response.status} - ${error.message}`);
        throw new Error(error.message);
    }
}

// Sử dụng
getInstanceStatistics('a5204fc9-9a59-f80f-fb9f-bf3b42214943')
    .then(stats => {
        console.log('Statistics:', stats);
    })
    .catch(error => {
        console.error('Failed to get statistics:', error);
    });
```

## Use Cases

### 1. Monitoring Instance Performance

Sử dụng API này để theo dõi hiệu suất của instance trong thời gian thực:

```bash
# Polling mỗi 5 giây để monitor
watch -n 5 'curl -s http://192.168.1.188:3546/v1/core/instance/{instanceId}/statistics | jq'
```

### 2. Alerting khi có vấn đề

Tạo script để cảnh báo khi có dropped frames hoặc latency cao:

```python
import requests
import time

def check_instance_health(instance_id, threshold_latency=500):
    url = f"http://192.168.1.188:3546/v1/core/instance/{instance_id}/statistics"
    response = requests.get(url)
    
    if response.status_code == 200:
        stats = response.json()
        
        if stats['latency'] > threshold_latency:
            print(f"⚠️  Warning: High latency: {stats['latency']} ms")
        
        return stats
    else:
        print(f"❌ Error: Cannot get statistics - {response.status_code}")
        return None

# Check mỗi 10 giây
while True:
    check_instance_health('a5204fc9-9a59-f80f-fb9f-bf3b42214943')
    time.sleep(10)
```

### 3. Dashboard Visualization

Sử dụng API để hiển thị thống kê trên dashboard:

```javascript
// Polling mỗi 2 giây để update dashboard
setInterval(async () => {
    const stats = await getInstanceStatistics(instanceId);
    
    // Update UI
    updateFPSChart(stats.current_framerate);
    updateLatencyChart(stats.latency);
    updateFramesProcessed(stats.frames_processed);
}, 2000);
```

### 4. Performance Analysis

Thu thập thống kê theo thời gian để phân tích hiệu suất:

```python
import requests
import json
from datetime import datetime

def collect_statistics_over_time(instance_id, duration_seconds=60, interval=5):
    """Thu thập thống kê trong một khoảng thời gian"""
    url = f"http://192.168.1.188:3546/v1/core/instance/{instance_id}/statistics"
    data_points = []
    
    start_time = datetime.now()
    while (datetime.now() - start_time).total_seconds() < duration_seconds:
        response = requests.get(url)
        if response.status_code == 200:
            stats = response.json()
            stats['timestamp'] = datetime.now().isoformat()
            data_points.append(stats)
        time.sleep(interval)
    
    # Lưu vào file để phân tích
    with open(f'statistics_{instance_id}.json', 'w') as f:
        json.dump(data_points, f, indent=2)
    
    return data_points
```

## Error Handling

### Instance Not Found (404)

Khi instance không tồn tại hoặc không đang chạy:

```json
{
  "error": "Not found",
  "message": "Instance not found or not running: {instanceId}"
}
```

**Giải pháp:**
- Kiểm tra instance ID có đúng không
- Đảm bảo instance đang chạy (sử dụng GET /v1/core/instances để kiểm tra)
- Start instance trước khi lấy statistics

### Internal Server Error (500)

Khi có lỗi server:

```json
{
  "error": "Internal server error",
  "message": "Error message details"
}
```

**Giải pháp:**
- Kiểm tra logs của server
- Đảm bảo instance registry đã được khởi tạo
- Kiểm tra pipeline có hợp lệ không

## Best Practices

### 1. Polling Frequency

- **Không nên polling quá thường xuyên:** Mỗi 1-2 giây là hợp lý cho monitoring
- **Polling quá nhanh:** Có thể gây overhead không cần thiết
- **Polling quá chậm:** Có thể bỏ lỡ các sự kiện quan trọng

### 2. Error Handling

Luôn kiểm tra status code trước khi xử lý response:

```python
response = requests.get(url)
if response.status_code == 200:
    stats = response.json()
    # Process statistics
elif response.status_code == 404:
    print("Instance not found or not running")
else:
    print(f"Error: {response.status_code}")
```

### 3. Timeout

Đặt timeout hợp lý cho requests:

```python
response = requests.get(url, timeout=5)  # 5 seconds timeout
```

### 4. Caching

Nếu không cần real-time tuyệt đối, có thể cache kết quả trong vài giây:

```python
from functools import lru_cache
from time import time

_cache_time = {}
_cache_data = {}

def get_cached_statistics(instance_id, cache_seconds=2):
    now = time()
    if instance_id in _cache_time and (now - _cache_time[instance_id]) < cache_seconds:
        return _cache_data[instance_id]
    
    # Fetch fresh data
    stats = get_instance_statistics(instance_id)
    _cache_time[instance_id] = now
    _cache_data[instance_id] = stats
    return stats
```

## Troubleshooting

### Vấn đề: Tất cả giá trị đều là 0

**Nguyên nhân có thể:**
- Instance vừa mới start, chưa xử lý frame nào
- Instance không nhận được frames từ source
- Source node không hoạt động đúng

**Giải pháp:**
- Kiểm tra source node có đang stream không
- Kiểm tra logs của instance
- Đợi vài giây rồi thử lại

### Vấn đề: source_framerate và source_resolution là 0

**Nguyên nhân có thể:**
- CVEDIX SDK không cung cấp API để lấy thông tin này từ source node
- Source node chưa khởi tạo xong
- Source type không hỗ trợ (ví dụ: App source)

**Giải pháp:**
- Đây là hành vi bình thường nếu SDK không cung cấp
- Sử dụng `current_framerate` và `resolution` thay thế

### Vấn đề: Statistics reset về 0 sau khi restart

**Nguyên nhân:**
- Đây là hành vi đúng theo thiết kế
- Statistics được reset khi instance restart

**Giải pháp:**
- Đây không phải là bug, mà là feature
- Nếu cần lưu trữ lâu dài, cần implement persistence layer riêng

## Related Documentation

- [CREATE_INSTANCE_GUIDE.md](CREATE_INSTANCE_GUIDE.md) - Hướng dẫn tạo instance
- [UPDATE_INSTANCE_GUIDE.md](UPDATE_INSTANCE_GUIDE.md) - Hướng dẫn cập nhật instance
- [GETTING_STARTED.md](GETTING_STARTED.md) - Hướng dẫn bắt đầu sử dụng API
- [OpenAPI Specification](../openapi.yaml) - OpenAPI specification đầy đủ

## API Reference

Chi tiết đầy đủ về API endpoint này có thể xem trong OpenAPI specification:

- Swagger UI: `http://localhost:8080/swagger-ui` (khi server đang chạy)
- OpenAPI YAML: `/openapi.yaml` hoặc `/v1/openapi.yaml`

