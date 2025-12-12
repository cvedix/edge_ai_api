# Tóm Tắt: Giải Pháp Queue Monitoring và Auto-Clear

## ✅ Đã Implement

### 1. QueueMonitor Class
- **File:** `src/instances/queue_monitor.h` và `src/instances/queue_monitor.cpp`
- Track queue full warnings
- Tính toán warning rate
- Log parsing để detect warnings từ CVEDIX SDK

### 2. Queue Monitoring Thread
- **File:** `src/main.cpp`
- Monitor FPS mỗi 10 giây
- Auto-restart instance khi:
  - FPS = 0 trong 30+ giây
  - Queue warnings > 100

### 3. Cơ Chế Phát Hiện

#### Primary: FPS Monitoring
- Nếu FPS = 0 trong 3 lần check liên tiếp (30 giây) → Restart instance

#### Secondary: Warning Count
- Nếu có >100 queue full warnings → Restart instance

## 🎯 Kết Quả

1. **Proactive Detection**: Phát hiện queue issues trước khi deadlock
2. **Auto-Recovery**: Tự động restart để clear queue
3. **Prevent Crash**: Tránh crash do deadlock
4. **Continuous Operation**: Instance tự động recover

## 📝 Cách Sử Dụng

1. **Rebuild project**:
```bash
cd build && cmake .. && make -j$(nproc)
```

2. **Run server** - Queue monitoring tự động chạy

3. **Monitor logs**:
```bash
./bin/edge_ai_api 2>&1 | grep -i "QueueMonitor"
```

## ⚙️ Configuration

Có thể adjust trong code:
- `zero_fps_count >= 3` → Thay đổi số lần check (3 = 30 giây)
- `warning_count >= 100` → Thay đổi threshold
- `setAutoClearThreshold(50.0)` → Thay đổi warning rate threshold

## 🔍 Log Examples

Khi detect queue issue:
```
[QueueMonitor] Instance xxx needs restart: FPS = 0 for 30+ seconds (possible queue full)
[QueueMonitor] Restarting instance xxx to clear queue and prevent deadlock
[QueueMonitor] Instance xxx restarted successfully
```

Khi track warnings:
```
[QueueMonitor] Instance xxx queue full warnings: 100 in 5s (rate: 20 warnings/s)
```

