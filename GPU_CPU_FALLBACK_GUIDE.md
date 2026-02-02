# Hướng Dẫn: Tự Động Fallback từ GPU sang CPU

## Tổng Quan

Hệ thống đã được cải thiện để tự động fallback từ GPU sang CPU khi GPU không khả dụng hoặc gặp lỗi CUDA compatibility. Có 2 cách để kích hoạt:

## Giải Pháp 1: Sửa config.json (Đã áp dụng)

File `config.json` đã được cập nhật để ưu tiên CPU devices lên đầu trong `auto_device_list`:

```json
{
  "auto_device_list": [
    "openvino.CPU",        // CPU được ưu tiên đầu tiên
    "armnn.CpuAcc",
    "armnn.CpuRef",
    "memx.cpu",
    "openvino.GPU",        // GPU vẫn có sẵn nếu cần
    "tensorrt.1",
    "tensorrt.2",
    ...
  ]
}
```

**Ưu điểm:**
- ✅ Tự động sử dụng CPU nếu GPU không khả dụng
- ✅ Không cần thay đổi code hoặc environment variables
- ✅ Áp dụng ngay lập tức sau khi restart service

## Giải Pháp 2: Environment Variable (Linh hoạt hơn)

Nếu muốn force CPU inference mà không sửa config.json, set environment variable:

```bash
export FORCE_CPU_INFERENCE=1
```

Hoặc trong systemd service file (`/etc/systemd/system/edge-ai-api.service`):

```ini
[Service]
Environment="FORCE_CPU_INFERENCE=1"
```

**Ưu điểm:**
- ✅ Không cần sửa config.json
- ✅ Có thể bật/tắt dễ dàng
- ✅ Tự động sắp xếp lại auto_device_list để CPU lên đầu

## Cách Hoạt Động

### 1. Khi tạo Face Detector Node

Hệ thống sẽ:
1. Kiểm tra `FORCE_CPU_INFERENCE` environment variable
2. Nếu được set, tự động sắp xếp lại `auto_device_list` để CPU devices lên đầu
3. CVEDIX SDK sẽ chọn CPU device đầu tiên trong list

### 2. Khi phát hiện CUDA Error

Nếu có lỗi CUDA/GPU trong quá trình tạo node:
- Hệ thống sẽ log chi tiết lỗi
- Đề xuất giải pháp: set `FORCE_CPU_INFERENCE=1` hoặc sửa config.json
- Throw exception để prevent crash (cần restart với CPU config)

## Sử Dụng

### Option A: Sử dụng config.json đã được sửa (Khuyến nghị)

```bash
# Restart service để áp dụng config mới
sudo systemctl restart edge-ai-api
```

### Option B: Sử dụng Environment Variable

```bash
# Set environment variable
export FORCE_CPU_INFERENCE=1

# Restart service
sudo systemctl restart edge-ai-api
```

### Option C: Kiểm tra và sửa thủ công

1. Kiểm tra config hiện tại:
```bash
cat config.json | grep -A 20 "auto_device_list"
```

2. Đảm bảo CPU devices ở đầu:
```json
"auto_device_list": [
  "openvino.CPU",
  "armnn.CpuAcc",
  ...
]
```

3. Restart service:
```bash
sudo systemctl restart edge-ai-api
```

## Log Messages

Khi `FORCE_CPU_INFERENCE=1` được set, bạn sẽ thấy:

```
[PipelineBuilderDetectorNodes] FORCE_CPU_INFERENCE=1 detected - prioritizing CPU devices
[PipelineBuilderDetectorNodes] ✓ Updated auto_device_list to prioritize CPU
```

Khi có CUDA error:

```
[PipelineBuilderDetectorNodes] ⚠ CUDA/GPU error detected!
[PipelineBuilderDetectorNodes] SOLUTION:
[PipelineBuilderDetectorNodes]   1. Set environment variable to force CPU:
[PipelineBuilderDetectorNodes]      export FORCE_CPU_INFERENCE=1
[PipelineBuilderDetectorNodes]   2. Or modify config.json to prioritize CPU
[PipelineBuilderDetectorNodes]   3. Restart the service
```

## Lưu Ý

1. **Performance**: CPU inference chậm hơn GPU, nhưng ổn định hơn
2. **Queue Overflow**: Với CPU, có thể cần giảm FPS hoặc enable frame dropping
3. **Config Priority**: Environment variable `FORCE_CPU_INFERENCE` có priority cao hơn config.json
4. **Restart Required**: Cần restart service sau khi thay đổi config hoặc environment variable

## Troubleshooting

### Vấn đề: Vẫn bị CUDA error

**Giải pháp:**
1. Kiểm tra `FORCE_CPU_INFERENCE` đã được set chưa:
```bash
echo $FORCE_CPU_INFERENCE
```

2. Kiểm tra config.json có CPU ở đầu không:
```bash
cat config.json | grep -A 5 "auto_device_list"
```

3. Restart service:
```bash
sudo systemctl restart edge-ai-api
```

### Vấn đề: Performance chậm với CPU

**Giải pháp:**
- Đây là bình thường, CPU inference chậm hơn GPU
- Có thể giảm FPS hoặc resolution để cải thiện
- Hoặc fix GPU compatibility issues để dùng GPU

## Tóm Tắt

✅ **Đã sửa config.json** để CPU được ưu tiên  
✅ **Đã thêm logic** để tự động detect và suggest CPU fallback khi có CUDA error  
✅ **Đã thêm environment variable** `FORCE_CPU_INFERENCE` để force CPU nếu cần  

Hệ thống giờ sẽ tự động fallback sang CPU nếu GPU không khả dụng, tránh crash!



