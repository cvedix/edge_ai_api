# Recognition API Test Script

Script Python để test các chức năng của Recognition API.

## Cài Đặt

```bash
pip install requests pillow
```

## Sử Dụng

### Chạy Interactive Menu

```bash
python examples/test_recognition_api.py
```

### Sử Dụng Như Module

```python
from examples.test_recognition_api import (
    register_face_multipart,
    recognize_face,
    list_faces,
    search_face
)

# Đăng ký khuôn mặt
result = register_face_multipart("john_doe", "face.jpg")

# Nhận diện khuôn mặt
result = recognize_face("test_image.jpg")

# Liệt kê subjects
result = list_faces(page=0, size=20)

# Tìm kiếm khuôn mặt
result = search_face("search_image.jpg", threshold=0.5)
```

## Các Chức Năng

1. **Đăng ký khuôn mặt** (Multipart & Base64)
2. **Nhận diện khuôn mặt**
3. **Liệt kê subjects** (có pagination)
4. **Xóa subject** (theo image_id hoặc subject name)
5. **Xóa nhiều subjects**
6. **Đổi tên subject**
7. **Tìm kiếm khuôn mặt**
8. **Cấu hình database**
9. **Test Face Detection Validation**

## Test Face Detection Validation

Script có chức năng test đặc biệt để kiểm tra tính năng **Face Detection Node**:

- Test với ảnh **có khuôn mặt** → Phải đăng ký thành công
- Test với ảnh **không có khuôn mặt** → Phải bị từ chối với lỗi 400

Chọn option **11** trong menu để chạy test này.

## Xem Thêm

- [Recognition API Guide](../docs/RECOGNITION_API_GUIDE.md) - Hướng dẫn chi tiết về API
- [API Documentation](../docs/API.md) - Tài liệu API tổng quát

