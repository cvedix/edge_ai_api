# Frame API Documentation

Tài liệu này hướng dẫn sử dụng API endpoint để lấy khung hình cuối cùng (last frame) từ các instance đang chạy trong Edge AI API Server.

## Tổng Quan

Frame API cung cấp endpoint để lấy khung hình cuối cùng đã được xử lý từ instance đang chạy:

- **GET /v1/core/instances/{instanceId}/frame** - Lấy khung hình cuối cùng từ instance

**Tính năng:**
- Lấy frame đã qua xử lý (có OSD/detection overlays)
- Frame được encode thành JPEG base64 format
- Frame được cache tự động khi pipeline xử lý
- Thread-safe và hiệu suất cao

**Lưu ý quan trọng:**
- Frame capture chỉ hoạt động nếu pipeline có `app_des_node`
- Nếu pipeline không có `app_des_node`, frame sẽ trống (empty string)
- Frame được cache tự động mỗi khi pipeline xử lý frame mới
- Frame cache được cleanup tự động khi instance stop hoặc delete
- Instance phải đang chạy (running) để có frame

## Endpoints

### 1. Get Last Frame

**Endpoint:** `GET /v1/core/instances/{instanceId}/frame`

**Mô tả:** Lấy khung hình cuối cùng có sẵn từ một phiên bản đang chạy. Frame được encode thành JPEG base64 format.

**Path Parameters:**
- `instanceId` (required): Unique identifier của instance (UUID format)

**Request Headers:**
```
Accept: application/json
```

**Request Example:**
```bash
curl --request GET \
  --url http://192.168.1.188:8080/v1/core/instances/a5204fc9-9a59-f80f-fb9f-bf3b42214943/frame \
  --header 'Accept: application/json'
```

**Response (200 OK):**
```json
{
  "frame": "/9j/4AAQSkZJRgABAQEAYABgAAD/2wBDAAYEBQYFBAYGBQYHBwYIChAKCgkJChQODwwQFxQYGBcUFhYaHSUfGhsjHBYWICwgIyYnKSopGR8tMC0oMCUoKSj/2wBDAQcHBwoIChMKChMoGhYaKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCj/wAARCAABAAEDASIAAhEBAxEB/8QAFQABAQAAAAAAAAAAAAAAAAAAAAv/xAAUEAEAAAAAAAAAAAAAAAAAAAAA/8QAFQEBAQAAAAAAAAAAAAAAAAAAAAX/xAAUEQEAAAAAAAAAAAAAAAAAAAAA/9oADAMBAAIRAxEAPwCdABmX/9k=",
  "running": true
}
```

**Response khi instance không tồn tại (404 Not Found):**
```json
{
  "error": "Not found",
  "message": "Instance ID not found: a5204fc9-9a59-f80f-fb9f-bf3b42214943"
}
```

**Response khi instance đang chạy nhưng chưa có frame cached:**
```json
{
  "frame": "",
  "running": true
}
```

**Response khi instance không chạy:**
```json
{
  "frame": "",
  "running": false
}
```

**Chi tiết các trường:**

#### frame
- **Type:** `string`
- **Mô tả:** Khung hình cuối cùng được encode thành JPEG base64 format
- **Format:** Base64-encoded JPEG (không có prefix `data:image/jpeg;base64,`)
- **Ví dụ:** `"/9j/4AAQSkZJRgABAQEAYABgAAD..."`
- **Lưu ý:** 
  - Chuỗi rỗng nếu chưa có frame cached hoặc instance không chạy
  - Frame được cache tự động mỗi khi pipeline xử lý frame mới
  - Frame được encode với JPEG quality 85%

#### running
- **Type:** `boolean`
- **Mô tả:** Trạng thái instance (đang chạy hay không)
- **Ví dụ:** `true`
- **Lưu ý:** 
  - `true` nếu instance đang chạy
  - `false` nếu instance đã dừng

## Yêu Cầu Pipeline

### App DES Node Requirement

**Quan trọng:** Frame capture chỉ hoạt động nếu pipeline có `app_des_node`. 

`app_des_node` là một destination node trong CVEDIX SDK cho phép capture frames từ pipeline thông qua callback hook. Nếu pipeline không có `app_des_node`, frame sẽ trống.

**Cách kiểm tra pipeline có app_des_node:**

1. Kiểm tra solution configuration:
   ```bash
   GET /v1/core/solutions/{solutionId}
   ```
   
2. Tìm trong `pipeline` array xem có node với `nodeType: "app_des"` không

**Ví dụ solution có app_des_node:**
```json
{
  "solutionId": "face_detection",
  "pipeline": [
    {
      "nodeType": "rtsp_src",
      "nodeName": "rtsp_src_0"
    },
    {
      "nodeType": "yunet_face_detector",
      "nodeName": "yunet_face_detector_0"
    },
    {
      "nodeType": "face_osd_v2",
      "nodeName": "osd_0"
    },
    {
      "nodeType": "app_des",
      "nodeName": "app_des_0"
    }
  ]
}
```

**Nếu solution không có app_des_node:**

Bạn có thể thêm `app_des_node` vào solution configuration:

1. Lấy solution hiện tại:
   ```bash
   GET /v1/core/solutions/{solutionId}
   ```

2. Thêm app_des_node vào cuối pipeline:
   ```json
   {
     "nodeType": "app_des",
     "nodeName": "app_des_0",
     "parameters": {
       "channel": "0"
     }
   }
   ```

3. Update solution:
   ```bash
   PUT /v1/core/solutions/{solutionId}
   ```

4. Restart instance để áp dụng thay đổi:
   ```bash
   POST /v1/core/instances/{instanceId}/restart
   ```

## Use Cases

### 1. Live Preview

Lấy frame cuối cùng để hiển thị preview trong web dashboard:

```javascript
async function getLastFrame(instanceId) {
  const response = await fetch(
    `http://localhost:8080/v1/core/instances/${instanceId}/frame`
  );
  const data = await response.json();
  
  if (data.frame && data.running) {
    // Decode base64 và hiển thị
    const imageUrl = `data:image/jpeg;base64,${data.frame}`;
    document.getElementById('preview').src = imageUrl;
  }
}

// Poll mỗi 1 giây để update preview
setInterval(() => {
  getLastFrame('a5204fc9-9a59-f80f-fb9f-bf3b42214943');
}, 1000);
```

### 2. Thumbnail Generation

Lấy frame để tạo thumbnail:

```python
import requests
import base64
from PIL import Image
from io import BytesIO

def get_frame_thumbnail(instance_id):
    url = f"http://localhost:8080/v1/core/instances/{instance_id}/frame"
    response = requests.get(url)
    data = response.json()
    
    if data['frame'] and data['running']:
        # Decode base64
        image_data = base64.b64decode(data['frame'])
        image = Image.open(BytesIO(image_data))
        
        # Resize để tạo thumbnail
        thumbnail = image.resize((320, 240))
        thumbnail.save(f"thumbnail_{instance_id}.jpg")
        return thumbnail
    
    return None
```

### 3. Monitoring và Debugging

Kiểm tra instance có đang xử lý frames không:

```bash
# Lấy frame để verify instance đang hoạt động
curl http://localhost:8080/v1/core/instances/{instanceId}/frame | jq

# Nếu frame rỗng nhưng running=true, có thể:
# 1. Pipeline chưa có app_des_node
# 2. Instance mới start, chưa có frame nào được xử lý
# 3. Source không có data (RTSP stream down, file không tồn tại)
```

## Troubleshooting

### Frame luôn trống (empty string)

**Nguyên nhân có thể:**

1. **Pipeline không có app_des_node**
   - **Giải pháp:** Thêm `app_des_node` vào solution configuration và restart instance

2. **Instance chưa xử lý frame nào**
   - **Giải pháp:** Đợi vài giây sau khi start instance, sau đó thử lại

3. **Source không có data**
   - **RTSP:** Kiểm tra RTSP stream có đang chạy không
   - **File:** Kiểm tra file path có đúng không, file có tồn tại không

4. **Instance đã stop**
   - **Giải pháp:** Start instance trước khi lấy frame

**Cách kiểm tra:**

```bash
# 1. Kiểm tra instance status
curl http://localhost:8080/v1/core/instances/{instanceId} | jq '.running'

# 2. Kiểm tra solution có app_des_node không
curl http://localhost:8080/v1/core/solutions/{solutionId} | jq '.pipeline[] | select(.nodeType == "app_des")'

# 3. Kiểm tra instance statistics (xem có frames processed không)
curl http://localhost:8080/v1/core/instance/{instanceId}/statistics | jq '.frames_processed'
```

### Frame không update

**Nguyên nhân:**
- Frame cache được update mỗi khi pipeline xử lý frame mới
- Nếu source không có data mới, frame sẽ không thay đổi

**Giải pháp:**
- Kiểm tra source (RTSP stream hoặc file) có đang cung cấp frames mới không
- Kiểm tra instance statistics để xem `frames_processed` có tăng không

### Frame có kích thước lớn

**Nguyên nhân:**
- Base64 encoding làm tăng kích thước ~33%
- JPEG quality 85% có thể tạo file lớn với resolution cao

**Giải pháp:**
- Sử dụng resolution thấp hơn trong pipeline (thông qua `RESIZE_RATIO` parameter)
- Decode và resize frame ở client side nếu cần

## Performance Considerations

### Frame Cache

- Frame được cache trong memory với thread-safe mechanism
- Cache được update mỗi khi pipeline xử lý frame mới
- Cache được cleanup tự động khi instance stop hoặc delete
- Memory usage: ~1-5MB per instance (tùy resolution)

### API Response Time

- Response time thường < 50ms (không bao gồm network)
- Frame encoding (JPEG + base64) mất ~10-30ms tùy resolution
- Không block pipeline processing (cache được update async)

### Rate Limiting

- Không có rate limiting built-in
- Có thể poll frame nhiều lần mà không ảnh hưởng đến pipeline
- Khuyến nghị: Poll mỗi 1-2 giây cho live preview

## Examples

### cURL

```bash
# Lấy frame từ instance
curl -X GET \
  http://localhost:8080/v1/core/instances/a5204fc9-9a59-f80f-fb9f-bf3b42214943/frame \
  -H "Accept: application/json" | jq

# Lưu frame vào file
curl -X GET \
  http://localhost:8080/v1/core/instances/a5204fc9-9a59-f80f-fb9f-bf3b42214943/frame \
  -H "Accept: application/json" | jq -r '.frame' | base64 -d > frame.jpg
```

### Python

```python
import requests
import base64

def get_frame(instance_id, base_url="http://localhost:8080"):
    url = f"{base_url}/v1/core/instances/{instance_id}/frame"
    response = requests.get(url, headers={"Accept": "application/json"})
    
    if response.status_code == 200:
        data = response.json()
        if data['frame'] and data['running']:
            # Decode base64
            image_data = base64.b64decode(data['frame'])
            return image_data
        else:
            print("No frame available or instance not running")
            return None
    elif response.status_code == 404:
        print(f"Instance not found: {instance_id}")
        return None
    else:
        print(f"Error: {response.status_code}")
        return None

# Usage
frame_data = get_frame("a5204fc9-9a59-f80f-fb9f-bf3b42214943")
if frame_data:
    with open("last_frame.jpg", "wb") as f:
        f.write(frame_data)
```

### JavaScript/Node.js

```javascript
const axios = require('axios');
const fs = require('fs');

async function getFrame(instanceId) {
  try {
    const response = await axios.get(
      `http://localhost:8080/v1/core/instances/${instanceId}/frame`,
      {
        headers: { 'Accept': 'application/json' }
      }
    );
    
    if (response.data.frame && response.data.running) {
      // Decode base64
      const imageBuffer = Buffer.from(response.data.frame, 'base64');
      fs.writeFileSync('last_frame.jpg', imageBuffer);
      console.log('Frame saved to last_frame.jpg');
    } else {
      console.log('No frame available or instance not running');
    }
  } catch (error) {
    if (error.response && error.response.status === 404) {
      console.log('Instance not found');
    } else {
      console.error('Error:', error.message);
    }
  }
}

// Usage
getFrame('a5204fc9-9a59-f80f-fb9f-bf3b42214943');
```

### HTML/JavaScript (Browser)

```html
<!DOCTYPE html>
<html>
<head>
    <title>Frame Preview</title>
</head>
<body>
    <h1>Instance Frame Preview</h1>
    <img id="preview" src="" alt="Frame preview" style="max-width: 800px;">
    
    <script>
        const instanceId = 'a5204fc9-9a59-f80f-fb9f-bf3b42214943';
        const apiUrl = 'http://localhost:8080';
        
        async function updateFrame() {
            try {
                const response = await fetch(
                    `${apiUrl}/v1/core/instances/${instanceId}/frame`,
                    {
                        headers: { 'Accept': 'application/json' }
                    }
                );
                
                const data = await response.json();
                
                if (data.frame && data.running) {
                    // Update image source
                    document.getElementById('preview').src = 
                        `data:image/jpeg;base64,${data.frame}`;
                } else {
                    document.getElementById('preview').src = '';
                    console.log('No frame available');
                }
            } catch (error) {
                console.error('Error fetching frame:', error);
            }
        }
        
        // Update frame every second
        setInterval(updateFrame, 1000);
        
        // Initial load
        updateFrame();
    </script>
</body>
</html>
```

## Integration với Swagger UI

Bạn có thể test API endpoint trực tiếp từ Swagger UI:

1. Mở Swagger UI: `http://localhost:8080/swagger` hoặc `http://localhost:8080/v1/swagger`
2. Tìm endpoint `GET /v1/core/instances/{instanceId}/frame`
3. Click "Try it out"
4. Nhập `instanceId` vào parameter
5. Click "Execute"
6. Xem response với frame base64 string

**Lưu ý:** Frame base64 string có thể rất dài, Swagger UI có thể hiển thị không đầy đủ. Sử dụng cURL hoặc code để decode và hiển thị frame.

## Best Practices

1. **Polling Frequency**
   - Khuyến nghị: Poll mỗi 1-2 giây cho live preview
   - Không cần poll quá nhanh (frame cache chỉ update khi có frame mới)

2. **Error Handling**
   - Luôn kiểm tra `running` status trước khi sử dụng frame
   - Handle 404 error khi instance không tồn tại
   - Handle empty frame khi instance chưa có frame cached

3. **Memory Management**
   - Frame cache được cleanup tự động, không cần manual cleanup
   - Nếu cần, có thể stop instance để cleanup cache

4. **Performance**
   - Frame encoding mất ~10-30ms, không đáng kể
   - API response time thường < 50ms
   - Có thể poll nhiều instances cùng lúc mà không ảnh hưởng performance

## Related APIs

- **GET /v1/core/instances/{instanceId}** - Lấy thông tin chi tiết instance
- **GET /v1/core/instance/{instanceId}/statistics** - Lấy thống kê thời gian thực
- **GET /v1/core/instances/{instanceId}/output** - Lấy thông tin output của instance
- **GET /v1/core/solutions/{solutionId}** - Kiểm tra solution configuration

## Changelog

- **2025-01-XX**: Thêm API endpoint GET /v1/core/instances/{instanceId}/frame
  - Frame capture thông qua app_des_node hook
  - JPEG base64 encoding với quality 85%
  - Thread-safe frame cache mechanism
  - Automatic cache cleanup khi instance stop/delete

