# Báo Cáo Nghẽn Cổ Chai FPS

## Tổng Quan

Tài liệu này liệt kê các điểm nghẽn cổ chai (bottlenecks) có thể giới hạn tốc độ xử lý FPS trong hệ thống.

## 1. BackpressureController - Giới Hạn FPS Cứng

### Vị trí: `src/core/backpressure_controller.h` và `src/core/backpressure_controller.cpp`

**Các giới hạn:**
- **MAX_FPS = 60.0** (dòng 167): Giới hạn cứng tối đa 60 FPS
- **MIN_FPS = 12.0** (dòng 166): Giới hạn cứng tối thiểu 12 FPS
- **FPS mặc định = 30.0** (dòng 60, 133): FPS mặc định khi không cấu hình
- **FPS cho slow models = 10.0** (dòng 2705): FPS tự động cho Mask RCNN/OpenPose

**Vấn đề:**
- FPS bị giới hạn cứng ở 60 FPS, không thể vượt quá
- Trong `instance_registry.cpp` có clamp FPS từ 5-60 (dòng 2716), nhưng MIN_FPS thực tế trong controller là 12.0

**Giải pháp đề xuất:**
- Tăng MAX_FPS nếu cần xử lý > 60 FPS
- Đồng bộ MIN_FPS giữa instance_registry (5.0) và backpressure_controller (12.0)

## 2. AIProcessor::processingLoop() - Sleep 1ms Mỗi Frame

### Vị trí: `src/core/ai_processor.cpp` dòng 133

**Vấn đề:**
```cpp
std::this_thread::sleep_for(std::chrono::milliseconds(1));
```

- Sleep 1ms mỗi frame giới hạn lý thuyết ~1000 FPS
- Nhưng nếu processing nhanh, sleep này có thể là nghẽn cổ chai không cần thiết
- Sleep này được thiết kế để "prevent 100% CPU usage" nhưng có thể giới hạn throughput

**Giải pháp đề xuất:**
- Chỉ sleep khi processing quá nhanh (dựa trên target FPS)
- Hoặc loại bỏ sleep nếu không cần thiết (để OS scheduler quản lý)

## 3. WorkerHandler::updateFrameCache() - Frame Clone Không Cần Thiết

### Vị trí: `src/worker/worker_handler.cpp` dòng 531

**Vấn đề:**
```cpp
last_frame_ = frame.clone();
```

- `frame.clone()` tạo một bản copy đầy đủ của frame mỗi lần update
- Với frame 1920x1080 BGR (~6MB), clone mỗi frame tốn nhiều CPU và memory bandwidth
- InstanceRegistry đã tối ưu bằng cách dùng `shared_ptr` để tránh copy (dòng 5096)

**Giải pháp đề xuất:**
- Sử dụng `shared_ptr<cv::Mat>` thay vì clone (giống như InstanceRegistry)
- Hoặc chỉ clone khi thực sự cần (lazy copy)

## 4. Queue Warning Threshold - Giới Hạn Queue Size

### Vị trí: `src/instances/instance_registry.cpp` dòng 5315

**Vấn đề:**
```cpp
const size_t queue_warning_threshold = 8; // Warn at 8 frames
```

- Khi queue size >= 8 frames, hệ thống trigger backpressure và giảm FPS
- `max_queue_size` mặc định = 10 (dòng 2723)
- Queue nhỏ có thể gây drop frame sớm

**Giải pháp đề xuất:**
- Tăng `max_queue_size` nếu cần buffer nhiều frame hơn
- Điều chỉnh `queue_warning_threshold` dựa trên use case

## 5. FPS Clamp Range Không Nhất Quán

### Vị trí: `src/instances/instance_registry.cpp` dòng 2716

**Vấn đề:**
```cpp
maxFPS = std::max(5.0, std::min(60.0, maxFPS));
```

- Clamp FPS từ 5-60 trong instance_registry
- Nhưng BackpressureController có MIN_FPS = 12.0
- Không nhất quán có thể gây confusion

**Giải pháp đề xuất:**
- Đồng bộ MIN_FPS giữa 2 nơi (dùng constant chung)
- Hoặc loại bỏ clamp trong instance_registry, để BackpressureController quản lý

## 6. Adaptive FPS Update Frequency

### Vị trí: `src/core/backpressure_controller.cpp` dòng 112

**Vấn đề:**
```cpp
if (counter >= 30) {  // Only update every 30 frames
    counter = 0;
    updateAdaptiveFPS(instanceId);
}
```

- Adaptive FPS chỉ update mỗi 30 frames (~1 giây ở 30 FPS)
- Có thể phản ứng chậm với thay đổi workload

**Giải pháp đề xuất:**
- Giảm số frames giữa các lần update nếu cần phản ứng nhanh hơn
- Hoặc dùng time-based update thay vì frame-based

## 7. Frame Drop Policy - DROP_NEWEST

### Vị trí: `src/instances/instance_registry.cpp` dòng 2721

**Vấn đề:**
- Policy mặc định là `DROP_NEWEST` - drop frame mới nhất khi queue đầy
- Điều này có thể gây mất frame mới nhất (có thể quan trọng)

**Giải pháp đề xuất:**
- Xem xét sử dụng `DROP_OLDEST` nếu cần giữ frame mới nhất
- Hoặc `ADAPTIVE_FPS` để tự động điều chỉnh FPS thay vì drop frame

## Tóm Tắt Các Nghẽn Cổ Chai Chính

| # | Nghẽn Cổ Chai | Vị Trí | Tác Động | Mức Độ |
|---|---------------|--------|----------|--------|
| 1 | MAX_FPS = 60.0 | backpressure_controller.h:167 | Giới hạn cứng 60 FPS | ⚠️ Cao |
| 2 | Sleep 1ms mỗi frame | ai_processor.cpp:133 | Giới hạn ~1000 FPS lý thuyết | ⚠️ Trung bình |
| 3 | Frame clone() | worker_handler.cpp:531 | Tốn CPU/memory bandwidth | ⚠️ Trung bình |
| 4 | Queue size = 10 | instance_registry.cpp:2723 | Buffer nhỏ, dễ drop frame | ⚠️ Thấp |
| 5 | MIN_FPS không nhất quán | Nhiều nơi | Confusion, có thể gây lỗi | ⚠️ Thấp |

## Khuyến Nghị Ưu Tiên

1. ✅ **Cao:** Tăng MAX_FPS nếu cần xử lý > 60 FPS → **ĐÃ TRIỂN KHAI: 60→120 FPS**
2. ✅ **Trung bình:** Loại bỏ hoặc điều chỉnh sleep trong AIProcessor → **ĐÃ TRIỂN KHAI: Adaptive sleep**
3. ✅ **Trung bình:** Tối ưu frame cache trong WorkerHandler (dùng shared_ptr) → **ĐÃ TRIỂN KHAI: shared_ptr thay clone()**
4. ✅ **Thấp:** Đồng bộ MIN_FPS giữa các module → **ĐÃ TRIỂN KHAI: Đồng bộ 12.0-120.0**
5. ✅ **Thấp:** Tăng queue size nếu cần buffer nhiều hơn → **ĐÃ TRIỂN KHAI: 10→20, threshold 8→16**

**Xem chi tiết triển khai tại:** `FPS_OPTIMIZATION_IMPLEMENTED.md`
