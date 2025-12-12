# Hướng Dẫn RESIZE_RATIO

## 📊 Bảng So Sánh RESIZE_RATIO

| RESIZE_RATIO | Kích Thước Frame | Tốc Độ Xử Lý | Chất Lượng Detection | Nguy Cơ Crash | Khuyến Nghị |
|--------------|------------------|--------------|---------------------|---------------|-------------|
| **0.01** | 1% (cực nhỏ) | ⚡⚡⚡⚡ Cực kỳ nhanh | ❌❌ Rất thấp (hầu như không detect) | ✅✅ Không bao giờ | ⚠️ **Quá nhỏ, không khuyến nghị** |
| **0.1** | 10% (rất nhỏ) | ⚡⚡⚡ Rất nhanh | ⚠️ Thấp (có thể miss nhỏ) | ✅ Thấp | ✅ **Khuyến nghị cho MQTT** |
| **0.2** | 20% (nhỏ) | ⚡⚡ Nhanh | ✅ Trung bình | ✅ Thấp | ✅ Tốt cho production |
| **0.3** | 30% (trung bình) | ⚡ Nhanh vừa | ✅✅ Tốt | ⚠️ Trung bình | ⚠️ Cẩn thận với MQTT |
| **0.5** | 50% (lớn) | 🐌 Chậm | ✅✅✅ Rất tốt | ⚠️⚠️ Cao | ❌ **Không khuyến nghị cho MQTT** |
| **0.7** | 70% (rất lớn) | 🐌🐌 Rất chậm | ✅✅✅✅ Xuất sắc | ❌❌ Rất cao | ❌ **Tránh với MQTT** |
| **1.0** | 100% (gốc) | 🐌🐌🐌 Cực chậm | ✅✅✅✅✅ Hoàn hảo | ❌❌❌ Cực cao | ❌ **Không dùng với MQTT** |

## 🔍 Giải Thích Chi Tiết

### RESIZE_RATIO = 0.01 (Cực nhỏ)
- **Frame size**: Nếu video gốc 1280x720 → resize thành ~12.8x7.2 pixels (rất nhỏ!)
- **Tốc độ**: Cực kỳ nhanh, gần như không tốn tài nguyên
- **Chất lượng**: ❌ **Rất thấp** - Frame quá nhỏ, YOLO detector có thể:
  - Không detect được objects (quá nhỏ để nhận diện)
  - Miss hầu hết các objects
  - Chỉ detect được objects rất lớn và gần camera
- **Queue**: Không bao giờ đầy (xử lý quá nhanh)
- **Phù hợp**: ❌ **Không khuyến nghị** - Frame quá nhỏ để detection hoạt động hiệu quả
- **Lưu ý**: Có thể không detect được gì cả!

### RESIZE_RATIO = 0.1 (Hiện tại)
- **Frame size**: Nếu video gốc 1280x720 → resize thành ~128x72 pixels
- **Tốc độ**: Xử lý rất nhanh, ít tải cho CPU/GPU
- **Chất lượng**: Có thể miss các object nhỏ hoặc xa
- **Queue**: Ít bị đầy, ít crash
- **Phù hợp**: MQTT với frame rate cao, real-time processing

### RESIZE_RATIO = 0.5
- **Frame size**: 1280x720 → ~640x360 pixels (lớn hơn 5x so với 0.1)
- **Tốc độ**: Chậm hơn 5-10 lần so với 0.1
- **Chất lượng**: Detection tốt hơn nhiều, ít miss objects
- **Queue**: Dễ đầy hơn, nguy cơ crash cao
- **Phù hợp**: RTMP output, không phải MQTT real-time

### RESIZE_RATIO = 0.7
- **Frame size**: 1280x720 → ~896x504 pixels (lớn hơn 7x so với 0.1)
- **Tốc độ**: Chậm hơn 10-20 lần so với 0.1
- **Chất lượng**: Detection xuất sắc, gần như không miss
- **Queue**: Rất dễ đầy, nguy cơ crash rất cao
- **Phù hợp**: Offline processing, không phải real-time

## ⚠️ Tại Sao Tăng RESIZE_RATIO Gây Crash?

1. **Frame lớn hơn** → YOLO detector xử lý chậm hơn nhiều
2. **Xử lý chậm** → Queue của node đầy nhanh hơn
3. **Queue đầy** → Callback MQTT không được gọi
4. **Callback không gọi** → Publisher queue không nhận data
5. **Deadlock** → Threads chờ nhau → Crash

## ✅ Khuyến Nghị

### Cho MQTT Real-time (như hiện tại):
- **RESIZE_RATIO = 0.1 - 0.2**: Tối ưu cho tốc độ và ổn định
- Nếu cần chất lượng hơn: Thử **0.15** hoặc **0.2** (tăng dần)

### Cho RTMP Output:
- **RESIZE_RATIO = 0.3 - 0.5**: Cân bằng giữa chất lượng và tốc độ
- RTMP không bị crash như MQTT vì không có callback blocking

### Cho Offline Processing:
- **RESIZE_RATIO = 0.7 - 1.0**: Chất lượng tối đa
- Không có vấn đề queue vì không real-time

## 🧪 Test Strategy

1. **Bắt đầu với 0.1** (hiện tại) - ổn định nhất
2. **Tăng dần lên 0.15** - nếu không crash, tiếp tục
3. **Tăng lên 0.2** - nếu vẫn ổn, đây là giá trị tốt
4. **Tránh 0.5+** - trừ khi hardware rất mạnh và network rất nhanh

## 📝 Ví Dụ Tính Toán

Giả sử video gốc: **1280x720 @ 30fps**

| RESIZE_RATIO | Resolution | Pixels/Frame | Tốc Độ Xử Lý (ước tính) | Chất Lượng Detection |
|--------------|------------|--------------|------------------------|---------------------|
| **0.01** | **12.8x7.2** | **~92** | **~500-1000 fps** | ❌❌ Hầu như không detect |
| 0.1 | 128x72 | 9,216 | ~100-150 fps | ⚠️ Thấp (có thể miss) |
| 0.2 | 256x144 | 36,864 | ~50-80 fps | ✅ Trung bình |
| 0.5 | 640x360 | 230,400 | ~15-25 fps | ✅✅✅ Rất tốt |
| 0.7 | 896x504 | 451,584 | ~8-15 fps | ✅✅✅✅ Xuất sắc |
| 1.0 | 1280x720 | 921,600 | ~5-10 fps | ✅✅✅✅✅ Hoàn hảo |

**Kết luận**: 
- **0.01**: Quá nhỏ → Không detect được gì (không khuyến nghị)
- **0.1-0.2**: Tốt cho MQTT real-time (cân bằng tốc độ và chất lượng)
- **0.5-0.7**: Tốc độ xử lý chậm hơn nhiều so với frame rate → Queue đầy → Crash

