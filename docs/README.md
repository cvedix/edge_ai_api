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

3. **[CREATE_INSTANCE_GUIDE.md](CREATE_INSTANCE_GUIDE.md)** - Hướng dẫn chi tiết tạo instance
   - 16 cases cụ thể với examples đầy đủ
   - Inference nodes (Detector): TensorRT, RKNN, YOLO, etc.
   - Source nodes (Input): RTSP, File, App, Image, RTMP, UDP
   - Broker nodes (Output): MQTT, Kafka, Socket, Console, XML
   - Pipeline hoàn chỉnh
   - Kiểm tra và testing
   - Troubleshooting

4. **[DEVELOPMENT_GUIDE.md](DEVELOPMENT_GUIDE.md)** - Hướng dẫn phát triển
   - Cấu trúc codebase
   - Tạo API handler mới
   - Viết unit tests
   - Cập nhật Swagger/OpenAPI
   - Best practices
   - Ví dụ hoàn chỉnh

### 📖 Tài Liệu Kỹ Thuật

5. **[DROGON_SETUP.md](DROGON_SETUP.md)** - Hướng dẫn Drogon Framework
   - Tự động download và build Drogon
   - Cấu hình dependencies
   - Troubleshooting

6. **[architecture.md](architecture.md)** - Kiến trúc hệ thống
   - System architecture
   - Request flow
   - Component structure
   - API endpoints diagram

7. **[FLOW_DIAGRAM.md](FLOW_DIAGRAM.md)** - Flow Diagram Tổng Quan
   - Flow tổng quan hệ thống
   - Flow xử lý request chi tiết
   - Flow khởi động server
   - Background services flow
   - Mô tả các component

8. **[HARDWARE_INFO_API.md](HARDWARE_INFO_API.md)** - Hardware Information API
   - Hướng dẫn sử dụng API lấy thông tin phần cứng
   - Endpoints: `/v1/core/system/info` và `/v1/core/system/status`
   - Ví dụ sử dụng với curl, Python, JavaScript
   - Troubleshooting và best practices
   - Chi tiết các thông số có thể lấy được

9. **[LOGGING.md](LOGGING.md)** - Logging Documentation

10. **[CONFIG_API_GUIDE.md](CONFIG_API_GUIDE.md)** - Hướng dẫn Config API
    - Tất cả các endpoint Config API
    - Cấu trúc config.json chi tiết
    - Cách config ảnh hưởng đến instance
    - Ví dụ sử dụng và best practices
    - Xử lý lỗi

11. **[CONFIG_IMPACT_ON_INSTANCES.md](CONFIG_IMPACT_ON_INSTANCES.md)** - Tác động của Config với Instance
    - max_running_instances và cách hoạt động
    - Các config khác và tác động của chúng
    - Luồng hoạt động khi tạo instance
    - Bảng tóm tắt tác động

### Node Support & Implementation

12. **[NODE_SUPPORT_STATUS.md](NODE_SUPPORT_STATUS.md)** - Trạng thái hỗ trợ các node types
    - Inference nodes: 23 nodes (100%)
    - Source nodes: 6 nodes (100%)
    - Broker nodes: 12 nodes (100%)

13. **[REQUIREMENT_CHECKLIST.md](REQUIREMENT_CHECKLIST.md)** - Checklist đáp ứng yêu cầu
    - Detector (Inference): ✅ 100%
    - Input (Source): ✅ 100%
    - Output (Broker): ✅ 100%

14. **[IMPLEMENTATION_SUMMARY.md](IMPLEMENTATION_SUMMARY.md)** - Tổng kết implementation
    - Tổng số nodes đã hỗ trợ: 41 nodes
    - Example files: 26 files
    - Files đã tạo/cập nhật
   - Hướng dẫn sử dụng các tính năng logging
   - API logging (`--log-api`)
   - Instance execution logging (`--log-instance`)
   - SDK output logging (`--log-sdk-output`)
   - Cấu hình và best practices
   - Troubleshooting logging issues

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

