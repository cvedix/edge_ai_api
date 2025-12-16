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

3. **[INSTANCE_GUIDE.md](INSTANCE_GUIDE.md)** - Hướng dẫn tạo và cập nhật instance
   - Tổng quan về pipeline và instance
   - Tạo instance với các loại nodes khác nhau
   - Cập nhật instance (camelCase và PascalCase)
   - Inference nodes (Detector): TensorRT, RKNN, YOLO, etc.
   - Source nodes (Input): RTSP, File, App, Image, RTMP, UDP
   - Broker nodes (Output): MQTT, Kafka, Socket, Console, XML
   - Ví dụ pipeline hoàn chỉnh
   - Troubleshooting

4. **[DEVELOPMENT_GUIDE.md](DEVELOPMENT_GUIDE.md)** - Hướng dẫn phát triển
   - Cấu trúc codebase
   - Tạo API handler mới
   - Viết unit tests
   - Cập nhật Swagger/OpenAPI
   - Best practices
   - Ví dụ hoàn chỉnh

### 📖 Tài Liệu Kỹ Thuật

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

7. **[API_REFERENCE.md](API_REFERENCE.md)** - Tài Liệu Tham Khảo API (Hợp Nhất)
   - **Frame API**: Lấy khung hình cuối cùng từ instance
   - **Statistics API**: Lấy thống kê thời gian thực
   - **Logs API**: Truy cập và quản lý logs
   - **Hardware Info API**: Thông tin phần cứng và trạng thái hệ thống
   - **Config API**: Quản lý cấu hình hệ thống
   - Tất cả endpoints, ví dụ sử dụng, troubleshooting

8. **[LOGGING.md](LOGGING.md)** - Logging Documentation
    - Hướng dẫn sử dụng các tính năng logging
    - API logging (`--log-api`)
    - Instance execution logging (`--log-instance`)
    - SDK output logging (`--log-sdk-output`)
    - Cấu hình và best practices
    - Troubleshooting logging issues

### 🔧 Troubleshooting & Guides

13. **[QUEUE_MONITORING.md](QUEUE_MONITORING.md)** - Queue Monitoring và Auto-Clear
    - QueueMonitor class
    - Queue monitoring thread
    - Cơ chế phát hiện queue issues
    - Auto-restart instance
    - Configuration và tuning

14. **[MQTT_GUIDE.md](MQTT_GUIDE.md)** - MQTT Implementation Guide
    - Non-blocking MQTT publisher
    - Background thread implementation
    - MQTT debug guide
    - CVEDIX JSON MQTT Broker Node API
    - Troubleshooting

15. **[ENVIRONMENT_VARIABLES.md](ENVIRONMENT_VARIABLES.md)** - Environment Variables Documentation
    - Danh sách đầy đủ các biến môi trường
    - Cách sử dụng .env file
    - Cấu hình server, logging, storage
    - Performance tuning

16. **[DIRECTORY_CREATION_GUIDE.md](DIRECTORY_CREATION_GUIDE.md)** - Directory Creation Guide
    - Hướng dẫn tạo thư mục tự động với fallback
    - Cấu trúc thư mục production
    - Xử lý quyền truy cập

### ⚡ Performance Optimization

17. **[OPTIMIZATION_COMPLETE_REPORT.md](OPTIMIZATION_COMPLETE_REPORT.md)** - Báo Cáo Tổng Hợp Optimization
    - Tổng quan và kết quả tóm tắt
    - Tổng hợp tất cả 3 phases optimization
    - Kết quả chi tiết và metrics
    - Code changes và impact
    - Files đã thay đổi và testing status

### 🛠️ Development Tools & Analysis

19. **[SCRIPTS_ANALYSIS.md](SCRIPTS_ANALYSIS.md)** - Phân tích và tối ưu Scripts
    - Phân tích tất cả scripts trong project
    - Đề xuất scripts nào cần giữ lại
    - Hướng dẫn sử dụng setup.sh và fix_all_symlinks.sh
    - Cấu trúc scripts đề xuất

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

