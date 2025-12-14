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

8. **[LOGGING.md](LOGGING.md)** - Logging Documentation
    - Hướng dẫn sử dụng các tính năng logging
    - API logging (`--log-api`)
    - Instance execution logging (`--log-instance`)
    - SDK output logging (`--log-sdk-output`)
    - Cấu hình và best practices
    - Troubleshooting logging issues

9. **[CONFIG_API_GUIDE.md](CONFIG_API_GUIDE.md)** - Hướng dẫn Config API
    - Tất cả các endpoint Config API
    - Cấu trúc config.json chi tiết
    - Cách config ảnh hưởng đến instance
    - Ví dụ sử dụng và best practices
    - Xử lý lỗi

### 🔧 Troubleshooting & Guides

11. **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** - Hướng dẫn khắc phục sự cố
    - Crash analysis - Queue deadlock
    - Crash analysis - RTSP + MQTT deadlock
    - MQTT vs RTMP queue crash
    - MQTT debug guide
    - RTSP connection issues
    - RTSP decoder issues

12. **[QUEUE_MONITORING.md](QUEUE_MONITORING.md)** - Queue Monitoring và Auto-Clear
    - QueueMonitor class
    - Queue monitoring thread
    - Cơ chế phát hiện queue issues
    - Auto-restart instance
    - Configuration và tuning

13. **[MQTT_GUIDE.md](MQTT_GUIDE.md)** - MQTT Implementation Guide
    - Non-blocking MQTT publisher
    - Background thread implementation
    - MQTT debug guide
    - CVEDIX JSON MQTT Broker Node API
    - Troubleshooting

14. **[RTSP_TROUBLESHOOTING.md](RTSP_TROUBLESHOOTING.md)** - RTSP Troubleshooting Guide
    - RTSP connection timeout
    - RTSP decoder issues
    - RTSP error analysis
    - Giải pháp và workarounds

15. **[RESIZE_RATIO_GUIDE.md](RESIZE_RATIO_GUIDE.md)** - Hướng dẫn RESIZE_RATIO
    - Bảng so sánh RESIZE_RATIO
    - Tối ưu cho MQTT vs RTMP
    - Test strategy
    - Khuyến nghị cho từng use case

### 🛠️ Development Tools & Analysis

16. **[SCRIPTS_ANALYSIS.md](SCRIPTS_ANALYSIS.md)** - Phân tích và tối ưu Scripts
    - Phân tích tất cả scripts trong project
    - Đề xuất scripts nào cần giữ lại
    - Hướng dẫn sử dụng setup.sh và fix_all_symlinks.sh
    - Cấu trúc scripts đề xuất

17. **[CMAKE_FIXES_APPLIED.md](CMAKE_FIXES_APPLIED.md)** - Các lỗi CMake đã được fix
    - Danh sách các lỗi CMake đã được giải quyết
    - Các thay đổi trong CMakeLists.txt
    - Hướng dẫn sử dụng fix_all_symlinks.sh

18. **[CMAKE_ISSUES_ANALYSIS.md](CMAKE_ISSUES_ANALYSIS.md)** - Phân tích chi tiết các vấn đề CMake
    - Phân tích nguyên nhân các lỗi CMake
    - Giải pháp và workarounds
    - Best practices cho CMake configuration

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

