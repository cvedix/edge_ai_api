# Hướng dẫn Test API Register Face Subject với File Upload

API hỗ trợ 2 cách để đăng ký face subject:

## 1. Upload File Trực Tiếp (Multipart/Form-Data) - Khuyến nghị

### Sử dụng curl:

```bash
curl -X POST "http://localhost:8080/v1/recognition/faces?subject=subject_name&det_prob_threshold=0.5" \
  -F "file=@/path/to/image.jpg"
```

### Sử dụng script test:

```bash
cd /home/cvedix/project/edge_ai_api
./scripts/test_register_face.sh /path/to/image.jpg "subject_name" 0.5
```

### Ví dụ:

```bash
# Test với file từ thư mục test_images
./scripts/test_register_face.sh \
  /home/cvedix/project/cvedix_data/test_images/faces/0.jpg \
  "test_subject" \
  0.5
```

## 2. Upload Base64 (JSON)

### Sử dụng curl:

```bash
# Encode image to base64
BASE64_DATA=$(base64 -w 0 /path/to/image.jpg)

# Send request
curl -X POST "http://localhost:8080/v1/recognition/faces?subject=subject_name" \
  -H "Content-Type: application/json" \
  -d "{\"file\": \"$BASE64_DATA\"}"
```

## Định dạng File Được Hỗ Trợ

- **jpeg, jpg**: JPEG images
- **png**: PNG images
- **bmp**: Bitmap images
- **gif**: GIF images
- **tif, tiff**: TIFF images
- **ico**: Icon files
- **webp**: WebP images

**Giới hạn kích thước**: Tối đa 5MB

## Parameters

### Query Parameters:

- **subject** (required): Tên subject để đăng ký
- **det_prob_threshold** (optional): Ngưỡng confidence cho face detection (0.0-1.0, mặc định: 0.5)

### Request Body:

#### Multipart/Form-Data:
- **file** (required): File ảnh cần upload

#### JSON:
- **file** (required): Base64 encoded image string

## Response

### Success (200 OK):

```json
{
  "image_id": "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx",
  "subject": "subject_name"
}
```

### Error (400 Bad Request):

```json
{
  "error": "Error message",
  "message": "Detailed error description"
}
```

## Ví dụ với Postman/Insomnia

1. **Method**: POST
2. **URL**: `http://localhost:8080/v1/recognition/faces?subject=test&det_prob_threshold=0.5`
3. **Body**:
   - Chọn `form-data`
   - Key: `file` (type: File)
   - Value: Chọn file ảnh từ máy tính
4. **Send**

## Kiểm tra Kết Quả

### List tất cả subjects:

```bash
curl -s "http://localhost:8080/v1/recognition/faces?page=0&size=10" | python3 -m json.tool
```

### Kiểm tra file database:

```bash
cat /opt/edge_ai_api/data/face_database.txt | cut -d'|' -f1
```

## Troubleshooting

### Lỗi "Failed to open database file for writing":

Cần thiết lập quyền ghi cho file database:

```bash
sudo ./deploy/setup_face_database.sh --full-permissions
```

### Lỗi "No face detected":

- Kiểm tra ảnh có chứa khuôn mặt rõ ràng không
- Thử giảm `det_prob_threshold` (ví dụ: 0.3)
- Kiểm tra định dạng file có được hỗ trợ không

### Lỗi "File too large":

- File phải nhỏ hơn 5MB
- Nén ảnh hoặc resize nếu cần
