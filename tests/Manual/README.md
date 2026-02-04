# Manual Tests - Edge AI API

Thư mục này chứa các tài liệu hướng dẫn test thủ công cho từng tính năng lớn của Edge AI API.

## Cấu Trúc

```
Manual/
├── ONVIF/              # Manual tests cho tính năng ONVIF
│   ├── ONVIF_MANUAL_TEST_GUIDE.md
│   ├── ONVIF_QUICK_TEST.md
│   └── ONVIF_TESTING_GUIDE.md
│
├── Recognition/        # Manual tests cho tính năng Recognition
│   └── RECOGNITION_API_GUIDE.md
│
├── Instance_Management/ # Manual tests cho Instance Management
│
├── Core_API/           # Manual tests cho Core API
│
├── Solutions/          # Manual tests cho Solutions
│
├── Groups/             # Manual tests cho Groups
│
├── Nodes/              # Manual tests cho Nodes
│
├── Analytics/          # Manual tests cho Analytics (Lines, Jams, Stops, Area, SecuRT)
│
└── Config/             # Manual tests cho Config
```

## Mục Đích

Manual tests được thiết kế để:
- Hướng dẫn tester thực hiện test thủ công các tính năng
- Cung cấp các test cases chi tiết với expected results
- Hướng dẫn troubleshooting khi gặp vấn đề
- Document các edge cases và scenarios phức tạp

## Cách Sử Dụng

1. Chọn tính năng cần test từ danh sách trên
2. Đọc các file markdown trong thư mục tính năng đó
3. Làm theo hướng dẫn từng bước
4. Ghi lại kết quả và báo cáo nếu có vấn đề

## Tính Năng Hiện Có

### ONVIF
- **ONVIF_MANUAL_TEST_GUIDE.md**: Hướng dẫn test chi tiết với camera Tapo
- **ONVIF_QUICK_TEST.md**: Các lệnh nhanh để test ONVIF
- **ONVIF_TESTING_GUIDE.md**: Hướng dẫn test với camera thật

### Recognition
- **RECOGNITION_API_GUIDE.md**: Hướng dẫn đầy đủ về Recognition API, bao gồm:
  - Đăng ký khuôn mặt
  - Nhận diện khuôn mặt
  - Tìm kiếm khuôn mặt
  - Quản lý subjects
  - Cấu hình database

## Thêm Manual Test Mới

Khi thêm manual test mới:

1. Xác định tính năng lớn mà test thuộc về
2. Tạo file markdown trong thư mục tính năng tương ứng
3. Bao gồm:
   - Mục đích test
   - Prerequisites
   - Các bước test chi tiết
   - Expected results
   - Troubleshooting guide
   - Test cases examples

## Best Practices

1. **Rõ ràng và chi tiết**: Mỗi bước test nên có hướng dẫn rõ ràng
2. **Có ví dụ**: Cung cấp ví dụ cụ thể với curl commands hoặc code
3. **Expected results**: Luôn mô tả kết quả mong đợi
4. **Troubleshooting**: Bao gồm phần troubleshooting cho các vấn đề thường gặp
5. **Cập nhật**: Giữ tài liệu cập nhật khi API thay đổi

