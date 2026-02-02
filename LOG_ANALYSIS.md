# Phân tích Log - Lỗi CUDA GPU Compatibility

## 🔴 Lỗi Nghiêm Trọng (CRITICAL)

### 1. CUDA Forward Compatibility Error (Dòng 719-722)
```
ERROR: OpenCV(4.10.0) /home/cvedix/opencv-4.10.0/modules/dnn/src/cuda4dnn/csl/memory.hpp:54: 
error: (-217:Gpu API call) forward compatibility was attempted on non supported HW 
in function 'ManagedPtr'
```

**Nguyên nhân:**
- OpenCV đang cố sử dụng CUDA/GPU để chạy inference
- GPU hardware không hỗ trợ forward compatibility mode
- Lỗi xảy ra khi OpenCV cố gắng allocate CUDA memory

**Thông tin từ log:**
- Dòng 618-619: NVIDIA GPU được phát hiện
- Dòng 624: `auto_device_list` có `tensorrt.1`, `tensorrt.2` (CUDA devices)
- CVEDIX SDK tự động chọn GPU dựa trên `auto_device_list` trong `config.json`

**Giải pháp:**
1. **Tạm thời: Force CPU usage**
   - Sửa `config.json` để đặt CPU lên đầu trong `auto_device_list`
   - Hoặc set environment variable để disable CUDA

2. **Kiểm tra GPU compatibility:**
   ```bash
   nvidia-smi
   nvcc --version
   ```

3. **Kiểm tra OpenCV CUDA build:**
   ```bash
   python3 -c "import cv2; print(cv2.getBuildInformation())" | grep -i cuda
   ```

---

## ⚠️ Cảnh Báo (WARNINGS)

### 2. CVEDIX SDK Dependencies Missing (Dòng 449-458, 597-606)
```
[CVEDIXValidator] Warning: Some SDK dependencies may be missing
[CVEDIXValidator] CVEDIX SDK dependencies not available
```

**Giải pháp được đề xuất:**
```bash
sudo /opt/edge_ai_api/scripts/dev_setup.sh --skip-deps --skip-build
sudo ldconfig
sudo systemctl restart edge-ai-api
```

---

### 3. Queue Overflow - Face Detector Queue Growing (Dòng 708-820)
- Queue size tăng từ 0 → 18 frames trong vài giây
- Face detector không xử lý frames đủ nhanh
- Có thể do GPU đang bị crash hoặc không hoạt động đúng

**Log cho thấy:**
```
[face_detector_...] before meta flow, in_queue.size()==>0
[face_detector_...] after meta flow, in_queue.size()==>1
...
[face_detector_...] after meta flow, in_queue.size()==>18
```

**Nguyên nhân có thể:**
- GPU crash khiến inference không chạy
- Frames bị queue lại vì không được xử lý

---

### 4. app_des_node Attached to Non-OSD Node (Dòng 509-514, 657-662)
```
⚠ WARNING: app_des_node attached to non-OSD node. Frame may not be processed (no overlays).
⚠ CRITICAL WARNING: app_des_node is NOT attached to OSD node!
```

**Ảnh hưởng:**
- `getLastFrame` API sẽ trả về frames chưa được xử lý (không có overlays)
- Không phải lỗi nghiêm trọng, chỉ là warning về functionality

---

### 5. Timeout Stopping Instance (Dòng 801)
```
[CRITICAL] Timeout stopping instance ... (500ms) - skipping to avoid deadlock
```

**Nguyên nhân:**
- Instance đang crash do CUDA error
- Cleanup timeout vì pipeline đang trong trạng thái không ổn định

---

## 📊 Tóm Tắt Vấn Đề

| Vấn đề | Mức độ | Trạng thái |
|--------|--------|------------|
| CUDA Forward Compatibility Error | 🔴 CRITICAL | Gây crash application |
| CVEDIX SDK Dependencies | ⚠️ WARNING | Có thể ảnh hưởng functionality |
| Queue Overflow | ⚠️ WARNING | Hệ quả của CUDA error |
| app_des_node Warning | ⚠️ INFO | Không ảnh hưởng core functionality |
| Stop Timeout | ⚠️ WARNING | Hệ quả của crash |

---

## 🔧 Giải Pháp Đề Xuất

### Bước 1: Tạm thời disable CUDA để test
Sửa `config.json` để force CPU:
```json
{
  "auto_device_list": [
    "cpu.auto",
    "openvino.CPU",
    ...
  ]
}
```

### Bước 2: Kiểm tra GPU và CUDA
```bash
# Kiểm tra GPU
nvidia-smi

# Kiểm tra CUDA version
nvcc --version

# Kiểm tra OpenCV CUDA support
python3 -c "import cv2; print(cv2.getBuildInformation())" | grep -i cuda
```

### Bước 3: Fix CVEDIX SDK dependencies
```bash
sudo /opt/edge_ai_api/scripts/dev_setup.sh --skip-deps --skip-build
sudo ldconfig
sudo systemctl restart edge-ai-api
```

### Bước 4: Nếu cần dùng GPU
- Kiểm tra GPU compute capability
- Có thể cần rebuild OpenCV với CUDA support phù hợp
- Hoặc downgrade OpenCV version nếu có vấn đề compatibility

---

## 📝 Ghi Chú

- Lỗi xảy ra ngay sau khi pipeline start (dòng 704-719)
- Queue overflow là hệ quả của CUDA crash, không phải nguyên nhân gốc
- Application tự động cleanup và stop instance sau khi crash



