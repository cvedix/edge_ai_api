# Giải Pháp: Queue Monitoring và Auto-Clear

## 🎯 Mục Tiêu

Tự động phát hiện và xử lý queue đầy **trước khi** gây ra deadlock và crash chương trình.

## 🔧 Giải Pháp Đã Implement

### 1. QueueMonitor Class

**File:** `src/instances/queue_monitor.h` và `src/instances/queue_monitor.cpp`

**Chức năng:**
- Track queue full warnings cho mỗi instance
- Tính toán warning rate (warnings per second)
- Phát hiện khi queue đầy quá nhiều
- Recommend restart khi cần

### 2. Queue Monitoring Thread

**File:** `src/main.cpp`

**Chức năng:**
- Monitor instance FPS và queue status mỗi 10 giây
- Phát hiện queue issues qua:
  - **FPS = 0** trong 30+ giây → Queue có thể đầy
  - **Excessive warnings** (>100 warnings) → Queue đầy
- **Tự động restart instance** khi detect queue issues

### 3. Cơ Chế Phát Hiện

#### A. FPS Monitoring
```cpp
// Nếu FPS = 0 trong 3 lần check liên tiếp (30 giây)
if (current_fps == 0.0 && zero_fps_count >= 3) {
    // Queue có thể đầy → Restart instance
}
```

#### B. Warning Count Monitoring
```cpp
// Nếu có >100 queue full warnings
if (warning_count >= 100) {
    // Queue đầy → Restart instance
}
```

## 📊 Cách Hoạt Động

1. **Monitoring Thread** chạy mỗi 10 giây
2. Check tất cả running instances:
   - FPS = 0 trong 30+ giây?
   - Queue full warnings > 100?
3. Nếu detect issue:
   - Stop instance
   - Wait 1 second
   - Start instance lại
   - Clear stats

## ⚙️ Configuration

Có thể config trong code:

```cpp
queueMonitor.setAutoClearThreshold(50.0);  // 50 warnings per second
queueMonitor.setMonitoringWindow(5);      // 5 seconds window
```

## ✅ Lợi Ích

1. **Proactive Prevention**: Phát hiện queue issues trước khi deadlock
2. **Auto-Recovery**: Tự động restart instance để clear queue
3. **No Manual Intervention**: Không cần can thiệp thủ công
4. **Prevent Crash**: Tránh crash do deadlock

## 🔍 Monitoring

Log sẽ hiển thị:
```
[QueueMonitor] Instance xxx needs restart: FPS = 0 for 30+ seconds
[QueueMonitor] Restarting instance xxx to clear queue
[QueueMonitor] Instance xxx restarted successfully
```

## 📝 Lưu Ý

1. **Restart sẽ mất data**: Khi restart, pipeline sẽ reset
2. **FPS = 0 có thể do nguyên nhân khác**: Không chỉ queue đầy
3. **Cần test**: Cần test với các scenarios khác nhau

## 🎯 Next Steps

1. Test với video có FPS cao
2. Monitor logs để verify hoạt động
3. Adjust thresholds nếu cần
4. Có thể thêm log parsing để detect "queue full" warnings trực tiếp

