# Face Recognition API Documentation

## 📋 Tổng Quan

API này cung cấp các endpoint để nhận diện khuôn mặt từ hình ảnh. Hệ thống sử dụng YuNet để detect faces và InsightFace để recognize faces.

## 🗄️ Vị Trí Lưu Face Database

Face database (`face_database.txt`) được lưu tự động theo thứ tự ưu tiên sau:

### 1. Environment Variable (Ưu tiên cao nhất)
```bash
export FACE_DATABASE_PATH=/custom/path/face_database.txt
```

### 2. Production Path (Nếu có quyền)
```
/opt/edge_ai_api/data/face_database.txt
```

### 3. User Directory (Fallback)
```
~/.local/share/edge_ai_api/face_database.txt
```

### 4. Current Directory (Last Resort)
```
./face_database.txt
```

### Kiểm Tra Vị Trí Database Hiện Tại

Sau khi khởi động service, kiểm tra log để xem database path:
```bash
sudo journalctl -u edge-ai-api | grep "FaceDatabase.*db_path"
```

Hoặc trong log file:
```bash
grep "FaceDatabase.*Initializing" /home/cvedix/project/edge_ai_api/log/*.txt
```

## 📤 POST /v1/recognition/recognize

### Mô Tả
Nhận diện khuôn mặt từ hình ảnh được upload. API sẽ detect tất cả faces trong image và so sánh với database để identify.

### Request

#### URL
```
POST http://localhost:3546/v1/recognition/recognize
```

#### Query Parameters

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `det_prob_threshold` | float | No | 0.5 | Detection probability threshold (0.0 - 1.0) |
| `limit` | int | No | 0 | Maximum number of faces to process (0 = no limit) |
| `prediction_count` | int | No | 1 | Number of top similar subjects to return |
| `detect_faces` | bool | No | true | Whether to detect faces |

#### Headers
```
Content-Type: multipart/form-data
accept: application/json
```

#### Body (Multipart Form Data)

**Cách 1: Upload file ảnh trực tiếp (Khuyến nghị)**

```bash
curl -X POST \
  'http://localhost:3546/v1/recognition/recognize?det_prob_threshold=0.5&limit=0&prediction_count=1&detect_faces=true' \
  -H 'accept: application/json' \
  -H 'Content-Type: multipart/form-data' \
  -F 'file=@/path/to/image.jpg;type=image/jpeg'
```

**Cách 2: Upload với base64 string trong multipart**

Nếu bạn có base64 string, tạo file tạm:
```bash
# Tạo file chứa base64 string
echo "iVBORw0KGgoAAAANSUhEUgAA..." > base64_image.txt

# Gửi request
curl -X POST \
  'http://localhost:3546/v1/recognition/recognize?det_prob_threshold=0.5' \
  -H 'accept: application/json' \
  -H 'Content-Type: multipart/form-data' \
  -F 'file=@base64_image.txt'
```

**Cách 3: Sử dụng JSON với base64 (Nếu API hỗ trợ)**

```bash
curl -X POST \
  'http://localhost:3546/v1/recognition/recognize?det_prob_threshold=0.5' \
  -H 'accept: application/json' \
  -H 'Content-Type: application/json' \
  -d '{
    "file": "iVBORw0KGgoAAAANSUhEUgAA..."
  }'
```

### Mẫu Request với cURL

#### Mẫu 1: Request đơn giản
```bash
curl -X POST \
  'http://localhost:3546/v1/recognition/recognize?det_prob_threshold=0.5' \
  -H 'Content-Type: multipart/form-data' \
  -F 'file=@image.jpg;type=image/jpeg'
```

#### Mẫu 2: Request với tất cả parameters
```bash
curl -X POST \
  'http://localhost:3546/v1/recognition/recognize?limit=0&prediction_count=3&det_prob_threshold=0.4&detect_faces=true' \
  -H 'accept: application/json' \
  -H 'Content-Type: multipart/form-data' \
  -F 'file=@1.jpg;type=image/jpeg'
```

#### Mẫu 3: Request với threshold thấp (detect nhiều faces hơn)
```bash
curl -X POST \
  'http://localhost:3546/v1/recognition/recognize?det_prob_threshold=0.2&prediction_count=5' \
  -H 'Content-Type: multipart/form-data' \
  -F 'file=@group_photo.jpg;type=image/jpeg'
```

### Mẫu Request với Postman

1. **Method**: POST
2. **URL**: `http://localhost:3546/v1/recognition/recognize?det_prob_threshold=0.5`
3. **Headers**:
   - `Content-Type`: `multipart/form-data` (tự động set khi chọn form-data)
   - `accept`: `application/json`
4. **Body** (chọn `form-data`):
   - Key: `file`
   - Type: `File`
   - Value: Chọn file ảnh từ máy tính

### Mẫu Request với Python

```python
import requests

url = "http://localhost:3546/v1/recognition/recognize"
params = {
    "det_prob_threshold": 0.5,
    "limit": 0,
    "prediction_count": 1,
    "detect_faces": "true"
}

# Cách 1: Upload file
with open("image.jpg", "rb") as f:
    files = {"file": ("image.jpg", f, "image/jpeg")}
    response = requests.post(url, params=params, files=files)

# Cách 2: Upload base64
import base64
with open("image.jpg", "rb") as f:
    image_base64 = base64.b64encode(f.read()).decode("utf-8")
    files = {"file": (None, image_base64)}
    response = requests.post(url, params=params, files=files)

print(response.json())
```

### Mẫu Request với JavaScript (Fetch API)

```javascript
const formData = new FormData();
formData.append('file', fileInput.files[0]); // fileInput là input element

fetch('http://localhost:3546/v1/recognition/recognize?det_prob_threshold=0.5', {
  method: 'POST',
  body: formData
})
.then(response => response.json())
.then(data => console.log(data))
.catch(error => console.error('Error:', error));
```

### Response (Success)

#### Status Code: 200 OK

```json
{
  "result": [
    {
      "box": {
        "probability": 1.0,
        "x_min": 548,
        "y_min": 295,
        "x_max": 1420,
        "y_max": 1368
      },
      "landmarks": [
        [814, 713],
        [1104, 829],
        [832, 937],
        [704, 1030],
        [1017, 1133]
      ],
      "subjects": [
        {
          "similarity": 0.97858,
          "subject": "subject1"
        }
      ],
      "execution_time": {
        "age": 0.0,
        "gender": 0.0,
        "detector": 117.0,
        "calculator": 45.0,
        "mask": 0.0
      }
    }
  ]
}
```

#### Response Elements

| Element | Type | Description |
|---------|------|-------------|
| `result` | array | Danh sách các faces được detect |
| `result[].box` | object | Bounding box của face |
| `result[].box.probability` | float | Confidence score của detection (0.0 - 1.0) |
| `result[].box.x_min` | int | Tọa độ X tối thiểu |
| `result[].box.y_min` | int | Tọa độ Y tối thiểu |
| `result[].box.x_max` | int | Tọa độ X tối đa |
| `result[].box.y_max` | int | Tọa độ Y tối đa |
| `result[].landmarks` | array | 5 điểm landmarks: [right_eye, left_eye, nose_tip, right_mouth_corner, left_mouth_corner] |
| `result[].subjects` | array | Danh sách subjects được nhận diện, sắp xếp theo similarity |
| `result[].subjects[].subject` | string | Tên subject |
| `result[].subjects[].similarity` | float | Độ tương đồng (0.0 - 1.0) |
| `result[].execution_time` | object | Thời gian thực thi (milliseconds) |
| `result[].execution_time.detector` | float | Thời gian face detection |
| `result[].execution_time.calculator` | float | Thời gian face recognition |
| `result[].execution_time.age` | float | Thời gian age estimation (chưa implement) |
| `result[].execution_time.gender` | float | Thời gian gender estimation (chưa implement) |
| `result[].execution_time.mask` | float | Thời gian mask detection (chưa implement) |

### Response (Error)

#### Status Code: 400 Bad Request

```json
{
  "error": "Invalid request",
  "message": "Content-Type must be multipart/form-data"
}
```

#### Status Code: 500 Internal Server Error

```json
{
  "error": "Internal server error",
  "message": "Error details..."
}
```

## 📝 Lưu Ý

1. **Image Format**: Hỗ trợ JPEG, PNG, BMP, GIF
2. **Base64**: Nếu gửi base64 trong multipart, API sẽ tự động decode
3. **Database**: Phải có ít nhất 1 face đã được register để có kết quả recognition
4. **Threshold**: Giảm `det_prob_threshold` (ví dụ: 0.2-0.3) nếu không detect được faces
5. **Multiple Faces**: API sẽ detect và recognize tất cả faces trong image

## 🔍 Troubleshooting

### Vấn đề: Response trả về `{"result": []}`

**Nguyên nhân có thể:**
1. Không detect được faces → Giảm `det_prob_threshold` xuống 0.2-0.3
2. Model không được tìm thấy → Kiểm tra log
3. Image không được decode → Kiểm tra format image

**Giải pháp:**
```bash
# Test với threshold thấp
curl -X POST \
  'http://localhost:3546/v1/recognition/recognize?det_prob_threshold=0.2' \
  -H 'Content-Type: multipart/form-data' \
  -F 'file=@image.jpg'

# Kiểm tra log
sudo journalctl -u edge-ai-api -f
```

### Vấn đề: Database không tìm thấy

**Kiểm tra database path:**
```bash
# Xem log để biết database path
grep "FaceDatabase.*db_path" /home/cvedix/project/edge_ai_api/log/*.txt

# Hoặc set custom path
export FACE_DATABASE_PATH=/custom/path/face_database.txt
```

## 📚 Related APIs

- **POST /v1/recognition/faces**: Register face subject
- **GET /v1/recognition/faces**: List registered face subjects
