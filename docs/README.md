# Edge AI API - Documentation

Tài liệu hướng dẫn đầy đủ cho Edge AI API project.

## 📚 Tài Liệu

### 🚀 Bắt Đầu

1. **[DEVELOPMENT_SETUP.md](DEVELOPMENT_SETUP.md)** - Hướng dẫn setup môi trường phát triển
   - Cài đặt dependencies
   - Build project
   - Cấu hình môi trường
   - Troubleshooting

2. **[GETTING_STARTED.md](GETTING_STARTED.md)** - Hướng dẫn khởi động và sử dụng
   - Khởi động server
   - Sử dụng API endpoints
   - Testing APIs
   - Monitoring và logs

3. **[DEVELOPMENT_GUIDE.md](DEVELOPMENT_GUIDE.md)** - Hướng dẫn phát triển
   - Cấu trúc codebase
   - Tạo API handler mới
   - Viết unit tests
   - Cập nhật Swagger/OpenAPI
   - Best practices
   - Ví dụ hoàn chỉnh

### 📖 Tài Liệu Kỹ Thuật

4. **[DROGON_SETUP.md](DROGON_SETUP.md)** - Hướng dẫn Drogon Framework
   - Tự động download và build Drogon
   - Cấu hình dependencies
   - Troubleshooting

5. **[architecture.md](architecture.md)** - Kiến trúc hệ thống
   - System architecture
   - Request flow
   - Component structure
   - API endpoints diagram

6. **[FLOW_DIAGRAM.md](FLOW_DIAGRAM.md)** - Flow Diagram Tổng Quan
   - Flow tổng quan hệ thống
   - Flow xử lý request chi tiết
   - Flow khởi động server
   - Background services flow
   - Mô tả các component

7. **[HARDWARE_INFO_API.md](HARDWARE_INFO_API.md)** - Hardware Information API
   - Hướng dẫn sử dụng API lấy thông tin phần cứng
   - Endpoints: `/v1/core/system/info` và `/v1/core/system/status`
   - Ví dụ sử dụng với curl, Python, JavaScript
   - Troubleshooting và best practices
   - Chi tiết các thông số có thể lấy được

## 🎯 Quick Start

### Cho Người Mới

1. Đọc [DEVELOPMENT_SETUP.md](DEVELOPMENT_SETUP.md) để setup môi trường
2. Đọc [GETTING_STARTED.md](GETTING_STARTED.md) để khởi động và test
3. Đọc [DEVELOPMENT_GUIDE.md](DEVELOPMENT_GUIDE.md) để bắt đầu phát triển

### Cho Developer

1. Đã setup môi trường? → Đọc [DEVELOPMENT_GUIDE.md](DEVELOPMENT_GUIDE.md)
2. Cần thêm API mới? → Xem phần "Tạo API Handler Mới" trong [DEVELOPMENT_GUIDE.md](DEVELOPMENT_GUIDE.md)
3. Cần viết tests? → Xem phần "Viết Unit Tests" trong [DEVELOPMENT_GUIDE.md](DEVELOPMENT_GUIDE.md)

## 📋 Checklist Phát Triển Feature Mới

- [ ] Đọc [DEVELOPMENT_GUIDE.md](DEVELOPMENT_GUIDE.md)
- [ ] Tạo handler header (`include/api/xxx_handler.h`)
- [ ] Implement handler (`src/api/xxx_handler.cpp`)
- [ ] Đăng ký handler trong `main.cpp`
- [ ] Thêm source vào `CMakeLists.txt`
- [ ] Viết unit tests (`tests/test_xxx_handler.cpp`)
- [ ] Thêm tests vào `tests/CMakeLists.txt`
- [ ] Cập nhật `openapi.yaml`
- [ ] Build và chạy tests
- [ ] Test API với curl/Postman
- [ ] Verify Swagger UI
- [ ] Commit code

## 🔗 Liên Kết Nhanh

- [Project README](../README.md)
- [Quick Start Guide](../QUICK_START.md)
- [OpenAPI Specification](../openapi.yaml)
- [Tests README](../tests/README.md)

## 📝 Ghi Chú

- Tất cả tài liệu đều bằng tiếng Việt để dễ hiểu
- Code examples sử dụng C++17 và Drogon Framework
- Tests sử dụng Google Test framework
- API documentation sử dụng OpenAPI 3.0

