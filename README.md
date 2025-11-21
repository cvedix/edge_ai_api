# EDGE AI Workflow

The new Edge AI Workflow architecture is designed to accelerate AI application development for edge platforms and accelerator cards, focusing on simplicity, efficiency, and integration via APIs. This modern workflow divides the process into practical stages, making it easy for developers to get started and deploy robust AI solutions with minimal friction.

![Edge AI Workflow](docs/image.png)

# AI System (Updating)
| Vendor | Device |  SOC | Edge AI Workflow | Model Conversion & Optimization | Deploy Application |
| -------- | -------- | -------- | ---- | ---- | ---- |
| Qualcomm | DK2721  | QCS6490 | [How-To](ai_system/qualcomm/dk2721/README.md) | [Convert & Optimize](ai_system/qualcomm/dk2721/object_detection_demo-using-qc_snpe.md#Open_AI_Model) | [App Guide](ai_system/qualcomm/dk2721/object_detection_demo-using-qc_snpe.md#Application) |
| Intel | R360    | Core Ultra | [How-To](ai_system/intel/r360/README.md)  | [Convert & Optimize](ai_system/intel/r360/object_detection_demo-using-intel_openvino.md#Covert_Optimize) |[App Guide](ai_system/intel/r360/object_detection_demo-using-intel_openvino.md#Deploy) |
| NVIDIA | 030     | Jetson AGX Orin | [How-To](ai_system/jetson/030/README.md)  | [Convert & Optimize](ai_system/jetson/030/object_detection_demo-using-ds7.0.md#convert-ai-model) |[App Guide](ai_system/jetson/030/object_detection_demo-using-ds7.0.md#application) |
| NVIDIA | R7300   | Jetson Orin Nano   | [How-To](ai_system/jetson/r7300/README.md)  | [Convert & Optimize](ai_system/jetson/r7300/object_detection_demo-using-ds7.1.md#convert-ai-model) | [App Guide](ai_system/jetson/r7300/object_detection_demo-using-ds7.1.md#application) |
| AMD | 2210   | Ryzen 8000 Series | [How-To](ai_system/amd/2210/README.md)  | [Convert & Optimize](ai_system/amd/2210/object_detection_demo-using-amd_ryzenaisdk.md#download-ai-files) | [App Guide](ai_system/amd/2210/object_detection_demo-using-amd_ryzenaisdk.md#application) |

# AI Accelerator (Updating)
| Vendor | Model |  SOC | AI Workflow | Model Conversion & Optimization | Deploy Application |
| -------- | -------- | -------- | ---- | ---- | ---- |
| Hailo | 1200 <br/> EAI-3300   | Hailo-8 | [How-To](ai_accelerator/hailo/1200_3300/README.md) | [Convert & Optimize](ai_accelerator/hailo/1200_3300/object_detection_demo-using-hailo.md#Model) | [App Guide](ai_accelerator/hailo/1200_3300/object_detection_demo-using-hailo.md#App) |
| Rockchip | OPI5-Plus  | RK3588 | [How-To](ai_system/rockchip/opi5-plus/README.md) | [Convert & Optimize](ai_system/rockchip/opi5-plus/object_detection_demo-using-rknpu.md#convert-ai-model) | [App Guide](ai_system/rockchip/opi5-plus/object_detection_demo-using-rknpu.md#application) |

# Hệ thống REST Instance

Kho mã này mô tả cách phơi bày CVEDIX Edge AI SDK thông qua một control plane RESTful.  
Mục tiêu là giúp backend dịch vụ hoặc người vận hành từ xa có thể cấu hình, khởi chạy và giám sát
các instance thị giác máy tính thời gian thực trên thiết bị biên mà không cần truy cập trực tiếp.

## Tổng quan hệ thống

1. **Client RESTful API Backend**  
   Backend sản phẩm hoặc cổng vận hành gửi các lệnh REST để điều khiển instance trên thiết bị biên.
2. **RESTful API Backend (Edge node)**  
   Dịch vụ HTTP nhẹ chạy cùng SDK, chuyển đổi request thành hành động trên instance.
3. **instance Manager**  
   Quản lý vòng đời node, kiểm tra đồ thị kết nối và lưu trữ cấu hình instance.
4. **Các khối AI Node**  
   Tập hợp node CVEDIX (nguồn, suy luận, tracker, phân tích hành vi, OSD...) xử lý luồng dữ liệu thời gian thực.
5. **Data Broker**  
   Trung chuyển metadata khung hình và sự kiện giữa các node, đồng thời công bố phân tích cho hệ thống thượng tầng.
6. **Output Display Nodes**  
   Xuất ra màn hình cục bộ, đẩy RTMP/RTSP hoặc ghi file tùy nhu cầu triển khai.

### Chu trình vòng đời

1. **Create**: API kiểm tra schema, lưu đồ thị và cấp ID.
2. **Start**: instance Manager khởi tạo node qua Edge AI SDK và kết nối phụ thuộc.
3. **Monitor**: Data Broker phát số liệu (kèm luồng WebSocket nếu bật).
4. **Stop**: instance Manager tháo node, xả buffer và lưu bộ đếm.

## Lưu ý triển khai

- Đóng gói REST API và SDK trong container hoặc dịch vụ systemd.
- Sử dụng lưu trữ bền vững cho cấu hình instance và mô hình AI (`/opt/cvedix_data`).
- Giám sát mức sử dụng CPU/GPU, lập kế hoạch tài nguyên cho từng node (source/infer/tracker/BA).
- Bảo vệ REST API bằng mTLS hoặc token, đồng thời ghi log mọi thay đổi instance.

## Lộ trình phát triển

- Bổ sung RBAC đa tenant để kiểm soát truy cập theo instance.
- Hiện thực luồng sự kiện WebSocket cho cảnh báo thời gian thực.
- Hỗ trợ thay nóng mô hình và chỉnh ROI tức thời.
- Tích hợp cơ sở dữ liệu chuỗi thời gian (InfluxDB, Prometheus) cho phân tích dài hạn.

---

Để xem chi tiết giao diện các node của CVEDIX SDK, tham khảo tài liệu của nhà cung cấp hoặc các
header dưới `/usr/include/cvedix`. Bạn có thể mở rộng ví dụ này với đồ thị node riêng, dashboard,
hoặc script tự động triển khai.

