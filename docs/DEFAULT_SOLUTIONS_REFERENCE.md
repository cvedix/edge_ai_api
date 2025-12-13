# Default Solutions Reference

## Tổng quan

Tài liệu này mô tả các **Default Solutions** được hardcode trong ứng dụng. Các solutions này được tự động load khi khởi động và **KHÔNG THỂ** bị thay đổi, xóa hoặc ghi đè bởi người dùng.

## Danh sách Default Solutions

### 1. `face_detection`

**Mô tả**: Face detection với RTSP source, YuNet face detector, và file destination

**Pipeline**:
- **RTSP Source** (`rtsp_src_{instanceId}`)
  - `rtsp_url`: `${RTSP_URL}`
  - `channel`: `0`
  - `resize_ratio`: `1.0`
- **YuNet Face Detector** (`face_detector_{instanceId}`)
  - `model_path`: `${MODEL_PATH}`
  - `score_threshold`: `${detectionSensitivity}`
  - `nms_threshold`: `0.5`
  - `top_k`: `50`
- **File Destination** (`file_des_{instanceId}`)
  - `save_dir`: `./output/{instanceId}`
  - `name_prefix`: `face_detection`
  - `osd`: `true`

**Defaults**:
- `detectorMode`: `SmartDetection`
- `detectionSensitivity`: `0.7`
- `sensorModality`: `RGB`

---

### 2. `face_detection_file`

**Mô tả**: Face detection với file source, YuNet face detector, và file destination

**Pipeline**:
- **File Source** (`file_src_{instanceId}`)
  - `file_path`: `${FILE_PATH}`
  - `channel`: `0`
  - `resize_ratio`: `1.0`
- **YuNet Face Detector** (`face_detector_{instanceId}`)
  - `model_path`: `${MODEL_PATH}`
  - `score_threshold`: `${detectionSensitivity}`
  - `nms_threshold`: `0.5`
  - `top_k`: `50`
- **File Destination** (`file_des_{instanceId}`)
  - `save_dir`: `./output/{instanceId}`
  - `name_prefix`: `face_detection`
  - `osd`: `true`

**Defaults**:
- `detectorMode`: `SmartDetection`
- `detectionSensitivity`: `0.7`
- `sensorModality`: `RGB`

---

### 3. `object_detection`

**Mô tả**: Object detection với RTSP source, YOLO detector (chưa implement), và file destination

**Pipeline**:
- **RTSP Source** (`rtsp_src_{instanceId}`)
  - `rtsp_url`: `${RTSP_URL}`
  - `channel`: `0`
  - `resize_ratio`: `1.0`
- **YOLO Detector** (`yolo_detector_{instanceId}`) - **CHƯA IMPLEMENT**
  - `weights_path`: `${MODEL_PATH}`
  - `config_path`: `${CONFIG_PATH}`
  - `labels_path`: `${LABELS_PATH}`
  - **Lưu ý**: Node này đang bị comment trong code. Để sử dụng cần:
    1. Thêm case `yolo_detector` trong `PipelineBuilder::createNode()`
    2. Implement `createYOLODetectorNode()` trong `PipelineBuilder`
    3. Uncomment code trong `registerObjectDetectionSolution()`
- **File Destination** (`file_des_{instanceId}`)
  - `save_dir`: `./output/{instanceId}`
  - `name_prefix`: `object_detection`
  - `osd`: `true`

**Defaults**:
- `detectorMode`: `SmartDetection`
- `detectionSensitivity`: `0.7`
- `sensorModality`: `RGB`

---

### 4. `face_detection_rtmp`

**Mô tả**: Face detection với file source, YuNet detector, SFace encoder, Face OSD v2, và RTMP streaming destination

**Pipeline**:
- **File Source** (`file_src_{instanceId}`)
  - `file_path`: `${FILE_PATH}`
  - `channel`: `0`
  - `resize_ratio`: `1.0`
  - **Lưu ý**: Sử dụng `resize_ratio = 1.0` nếu video đã có resolution cố định để tránh double-resizing
- **YuNet Face Detector** (`yunet_face_detector_{instanceId}`)
  - `model_path`: `${MODEL_PATH}`
  - `score_threshold`: `${detectionSensitivity}`
  - `nms_threshold`: `0.5`
  - `top_k`: `50`
  - **Lưu ý**: YuNet 2022mar có thể có vấn đề với dynamic input sizes. Nên dùng YuNet 2023mar
- **SFace Feature Encoder** (`sface_face_encoder_{instanceId}`)
  - `model_path`: `${SFACE_MODEL_PATH}`
- **Face OSD v2** (`osd_{instanceId}`)
  - Không có parameters
- **RTMP Destination** (`rtmp_des_{instanceId}`)
  - `rtmp_url`: `${RTMP_URL}`
  - `channel`: `0`

**Defaults**:
- `detectorMode`: `SmartDetection`
- `detectionSensitivity`: `Low`
- `sensorModality`: `RGB`

---

## Tóm tắt

| Solution ID | Solution Name | Type | Source | Detector | Destination |
|-------------|---------------|------|--------|-----------|-------------|
| `face_detection` | Face Detection | face_detection | RTSP | YuNet | File |
| `face_detection_file` | Face Detection with File Source | face_detection | File | YuNet | File |
| `object_detection` | Object Detection (YOLO) | object_detection | RTSP | YOLO* | File |
| `face_detection_rtmp` | Face Detection with RTMP Streaming | face_detection | File | YuNet + SFace | RTMP |

*YOLO detector chưa được implement

## Vị trí trong Code

Các default solutions được định nghĩa trong:
- **File**: `src/solutions/solution_registry.cpp`
- **Functions**:
  - `registerFaceDetectionSolution()` - line 100
  - `registerFaceDetectionFileSolution()` - line 145
  - `registerObjectDetectionSolution()` - line 190
  - `registerFaceDetectionRTMPSolution()` - line 238
- **Initialization**: `initializeDefaultSolutions()` - line 93

## Backup và Restore

### File Backup

File backup JSON chứa tất cả default solutions:
- **Location**: `docs/default_solutions_backup.json`
- **Mục đích**: Tham khảo và restore khi cần
- **Lưu ý**: File này chỉ để tham khảo, không được load vào storage

### Script Restore

Script để reset storage file về trạng thái mặc định:
- **Location**: `scripts/restore_default_solutions.sh`
- **Usage**: 
  ```bash
  ./scripts/restore_default_solutions.sh
  ```
- **Chức năng**: 
  - Backup file `solutions.json` hiện tại
  - Reset file về trạng thái rỗng `{}`
  - Default solutions sẽ tự động load khi khởi động lại ứng dụng

## Bảo mật

Các default solutions được bảo vệ bởi nhiều lớp:
- ✅ Không thể tạo/update/delete qua API
- ✅ Không được lưu vào storage file
- ✅ Không thể load từ storage (nếu có ai đó manually edit)
- ✅ Luôn được load từ code khi khởi động

**Lưu ý bảo mật:** Default solutions được load từ code khi khởi động, không thể bị thay đổi từ bên ngoài.

## Tự động Load Khi Khởi động

**4 default solutions này sẽ TỰ ĐỘNG có sẵn khi bạn chạy project**, không cần cấu hình thêm.

Khi ứng dụng khởi động:
1. Hàm `initializeDefaultSolutions()` được gọi trong `main.cpp` (line 952)
2. Tất cả 4 default solutions được register vào registry
3. Solutions có sẵn ngay lập tức để sử dụng

**Không cần làm gì thêm** - chỉ cần chạy project và 4 solutions này sẽ có sẵn!

## Thêm/Cập nhật Default Solutions

Khi cần thêm hoặc cập nhật default solutions:

Tóm tắt nhanh:
1. Sửa code trong `src/solutions/solution_registry.cpp`
2. Thêm hàm `register[Name]Solution()` mới
3. Gọi hàm trong `initializeDefaultSolutions()`
4. Rebuild project

## Sử dụng

Default solutions luôn có sẵn và có thể được sử dụng ngay:

```bash
# List tất cả solutions (bao gồm default)
curl http://localhost:8080/v1/core/solutions

# Get chi tiết default solution
curl http://localhost:8080/v1/core/solutions/face_detection

# Tạo instance với default solution
curl -X POST http://localhost:8080/v1/core/instances \
  -H "Content-Type: application/json" \
  -d '{
    "instanceId": "my_instance",
    "solutionId": "face_detection",
    ...
  }'
```

## Lưu ý

1. **Default solutions không thể thay đổi**: Nếu cần customize, hãy tạo custom solution mới với ID khác
2. **Storage file**: File `solutions.json` chỉ chứa custom solutions, không chứa default solutions
3. **Restore**: Nếu muốn reset về trạng thái mặc định, chỉ cần xóa tất cả custom solutions trong storage file

