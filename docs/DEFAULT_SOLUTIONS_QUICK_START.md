# Default Solutions - Quick Start

## ✅ Xác nhận: 4 Default Solutions Tự động Có sẵn

Khi bạn **chạy project**, **4 default solutions** sẽ **TỰ ĐỘNG có sẵn** ngay lập tức:

1. ✅ `face_detection` - Face Detection với RTSP source
2. ✅ `face_detection_file` - Face Detection với File source  
3. ✅ `object_detection` - Object Detection (YOLO)
4. ✅ `face_detection_rtmp` - Face Detection với RTMP Streaming

**Không cần cấu hình gì thêm** - chỉ cần chạy project và sử dụng!

---

## 🚀 Sử dụng Ngay

### 1. Khởi động project

```bash
cd /home/pnsang/project/edge_ai_api
cd build
./edge_ai_api
```

### 2. Kiểm tra solutions có sẵn

```bash
# List tất cả solutions (sẽ thấy 4 default solutions)
curl http://localhost:8080/v1/core/solutions | jq
```

Kết quả sẽ có:
```json
{
  "solutions": [
    {
      "solutionId": "face_detection",
      "solutionName": "Face Detection",
      "isDefault": true,
      ...
    },
    {
      "solutionId": "face_detection_file",
      "solutionName": "Face Detection with File Source",
      "isDefault": true,
      ...
    },
    {
      "solutionId": "object_detection",
      "solutionName": "Object Detection (YOLO)",
      "isDefault": true,
      ...
    },
    {
      "solutionId": "face_detection_rtmp",
      "solutionName": "Face Detection with RTMP Streaming",
      "isDefault": true,
      ...
    }
  ],
  "total": 4,
  "default": 4,
  "custom": 0
}
```

### 3. Sử dụng default solution

```bash
# Tạo instance với default solution
curl -X POST http://localhost:8080/v1/core/instances \
  -H "Content-Type: application/json" \
  -d '{
    "instanceId": "my_instance",
    "solutionId": "face_detection",
    "additionalParams": {
      "RTSP_URL": "rtsp://localhost/stream",
      "MODEL_PATH": "/path/to/yunet.onnx"
    }
  }'
```

---

## ➕ Thêm Default Solution Mới

Khi cần thêm default solution mới, có 2 cách:

### Cách 1: Sử dụng Script Helper (Khuyến nghị)

```bash
# Generate template code tự động
./scripts/generate_default_solution_template.sh

# Script sẽ hỏi:
# - Solution ID
# - Solution Name  
# - Solution Type
# → Tạo template code sẵn để copy vào project
```

### Cách 2: Làm thủ công

Tóm tắt:
1. Tạo hàm `register[Name]Solution()` trong `src/solutions/solution_registry.cpp`
2. Khai báo hàm trong `include/solutions/solution_registry.h`
3. Gọi hàm trong `initializeDefaultSolutions()`
4. Rebuild project

---

## 🔄 Cập nhật Default Solution

Để cập nhật default solution hiện có:

1. Mở `src/solutions/solution_registry.cpp`
2. Tìm hàm `register[SolutionName]Solution()`
3. Sửa đổi pipeline, parameters, hoặc defaults
4. Rebuild project

**Ví dụ**: Cập nhật `detectionSensitivity` mặc định của `face_detection`:

```cpp
void SolutionRegistry::registerFaceDetectionSolution() {
    // ... existing code ...
    
    // Thay đổi default
    config.defaults["detectionSensitivity"] = "0.8";  // Từ 0.7 → 0.8
    
    registerSolution(config);
}
```

Sau đó rebuild:
```bash
cd build && make
```

---

## 📋 Checklist Khi Thêm/Cập nhật

- [ ] Tạo hàm register mới (hoặc sửa hàm cũ)
- [ ] Khai báo trong header file
- [ ] Gọi hàm trong `initializeDefaultSolutions()`
- [ ] Set `config.isDefault = true`
- [ ] Đảm bảo `solutionId` unique
- [ ] Rebuild project
- [ ] Test solution bằng cách tạo instance
- [ ] Cập nhật `docs/default_solutions_backup.json` (nếu cần)
- [ ] Cập nhật tài liệu

---

## 📚 Tài liệu Liên quan

- **[DEFAULT_SOLUTIONS_REFERENCE.md](./DEFAULT_SOLUTIONS_REFERENCE.md)** - Tham khảo chi tiết 4 default solutions
- **[DEVELOPMENT_GUIDE.md](./DEVELOPMENT_GUIDE.md)** - Hướng dẫn phát triển và thêm features mới

---

## 🔍 Kiểm tra Default Solutions

### Kiểm tra trong code:

```bash
# Xem các hàm register
grep "register.*Solution()" src/solutions/solution_registry.cpp

# Xem initialization
grep -A 5 "initializeDefaultSolutions" src/main.cpp
```

### Kiểm tra khi chạy:

```bash
# List solutions
curl http://localhost:8080/v1/core/solutions | jq '.solutions[] | select(.isDefault == true)'

# Get chi tiết từng solution
curl http://localhost:8080/v1/core/solutions/face_detection | jq
```

---

## ⚠️ Lưu ý Quan trọng

1. **Default solutions tự động load**: Không cần cấu hình, tự động có sẵn khi chạy project
2. **Không lưu vào storage**: Default solutions không được lưu vào `solutions.json`
3. **Không thể xóa qua API**: Default solutions được bảo vệ, chỉ có thể sửa trong code
4. **Phải rebuild**: Sau khi sửa code, phải rebuild để thay đổi có hiệu lực
5. **isDefault = true**: Luôn nhớ set flag này cho default solutions

---

## 🎯 Tóm tắt

✅ **4 default solutions tự động có sẵn khi chạy project**  
✅ **Không cần cấu hình gì thêm**  
✅ **Có thể thêm/cập nhật bằng cách sửa code**  
✅ **Có script helper để tạo template nhanh**  

**Bắt đầu sử dụng ngay bây giờ!** 🚀

