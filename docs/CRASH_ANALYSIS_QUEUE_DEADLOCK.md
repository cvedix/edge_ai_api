# Phân tích Crash: Queue Full và Deadlock

## 🔴 Vấn đề

Server crash với lỗi **"Resource deadlock avoided"** khi đang chạy pipeline với file video.

## 📊 Phân tích Log

### 1. Queue Đầy Liên Tục

Từ log (line 738-1003), thấy rất nhiều warnings:
```
[Warn] [yolo_detector_...] queue full, dropping meta!
[Warn] [json_mqtt_broker_...] queue full, dropping meta!
```

**Tần suất:** Hàng trăm warnings trong vài giây → Queue đầy liên tục

### 2. BA Crossline Đang Hoạt Động

```
[Info] [ba_crossline_...] [channel 0] has found target cross line, total number of crossline: [1]
[Info] [ba_crossline_...] [channel 0] has found target cross line, total number of crossline: [2]
```

**Kết luận:** BA crossline đang phát hiện events, nhưng không thể gửi qua MQTT vì queue đầy.

### 3. MQTT Connection Thành Công

```
[PipelineBuilder] [MQTT] Connected successfully!
```

**Nhưng:** Không thấy log `[MQTT] Callback called` hoặc `[MQTT] Published successfully` → Callback không được gọi vì queue đầy.

### 4. Crash với Deadlock

```
2025-12-12 01:46:36.257 ERROR [898358] [terminateHandler@474] [CRITICAL] Uncaught exception: Resource deadlock avoided
[InstanceRegistry] WARNING: listInstances() timeout - mutex is locked, returning empty vector
```

## 🔍 Nguyên Nhân

### 1. **Queue Size Quá Nhỏ**

CVEDIX SDK nodes có queue size mặc định nhỏ (có thể 10-50 items). Khi:
- Frame rate cao (video file)
- YOLO detector chậm hơn frame rate
- MQTT publish chậm

→ Queue đầy nhanh chóng → Data bị drop

### 2. **MQTT Publish Blocking**

MQTT publish có thể blocking nếu:
- Network chậm
- Broker chậm
- QoS > 0 (waiting for ACK)

→ `json_mqtt_broker` node không thể consume queue nhanh → Queue đầy

### 3. **Deadlock Khi Cleanup**

Khi cleanup:
- Threads đang lock mutex để access queue
- Queue đầy → threads đang chờ nhau
- Cleanup thread cũng cần lock → Deadlock

## ✅ Giải Pháp

### Giải Pháp 1: Tăng SKIP_INTERVAL (Khuyến Nghị)

Giảm frame rate để giảm tải cho queue:

```json
{
  "additionalParams": {
    "SKIP_INTERVAL": "10",  // Skip 10 frames, process 1 frame
    // Hoặc
    "SKIP_INTERVAL": "20"   // Skip 20 frames, process 1 frame
  }
}
```

**Lưu ý:** File source không có SKIP_INTERVAL, nhưng có thể thêm vào code.

### Giải Pháp 2: Tăng RESIZE_RATIO

Giảm resolution để tăng tốc độ xử lý:

```json
{
  "additionalParams": {
    "RESIZE_RATIO": "0.2"  // Giảm từ 0.4 xuống 0.2
  }
}
```

### Giải Pháp 3: Sử Dụng Video Có FPS Thấp Hơn

Re-encode video với FPS thấp hơn:

```bash
ffmpeg -i input.mp4 -r 10 -c:v libx264 -preset fast -crf 23 output.mp4
```

### Giải Pháp 4: Tăng Queue Size (Cần Modify SDK)

Nếu có quyền truy cập SDK code, tăng queue size trong CVEDIX SDK nodes.

### Giải Pháp 5: Fix Deadlock trong Cleanup

Cải thiện cleanup code để tránh deadlock khi queue đầy.

## 🛠️ Implementation

### Bước 1: Thêm SKIP_INTERVAL cho File Source

Cần modify code để support SKIP_INTERVAL cho file source (hiện tại chỉ có cho RTSP).

### Bước 2: Tăng Timeout cho MQTT Publish

Đảm bảo MQTT publish không blocking quá lâu.

### Bước 3: Thêm Queue Monitoring

Log queue size để debug.

## 📝 Checklist

- [ ] Tăng SKIP_INTERVAL (nếu có thể)
- [ ] Giảm RESIZE_RATIO
- [ ] Re-encode video với FPS thấp hơn
- [ ] Fix deadlock trong cleanup
- [ ] Thêm queue monitoring
- [ ] Tối ưu MQTT publish (async, non-blocking)

## 🎯 Quick Fix

**Ngay lập tức:** Sử dụng video có FPS thấp hơn hoặc re-encode:

```bash
# Re-encode với FPS = 10
ffmpeg -i vehicle.mp4 -r 10 -c:v libx264 -preset fast -crf 23 vehicle_10fps.mp4
```

Sau đó update FILE_PATH trong config.

