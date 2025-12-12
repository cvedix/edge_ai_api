# Giải Pháp: Auto-Clear Queue Trước Khi Đầy

## 🎯 Mục Tiêu

Tự động phát hiện và **clear queue trước khi đầy** để tránh deadlock và crash chương trình.

## ✅ Giải Pháp Đã Implement

### 1. QueueMonitor Class

**Files:**
- `src/instances/queue_monitor.h`
- `src/instances/queue_monitor.cpp`

**Chức năng:**
- Track queue full warnings cho mỗi instance
- Tính toán warning rate (warnings per second)
- Phát hiện khi queue đầy quá nhiều
- Log parsing để detect "queue full" warnings từ CVEDIX SDK

### 2. Queue Monitoring Thread trong main.cpp

**Chức năng:**
- Monitor instance **FPS** và queue status mỗi 10 giây
- Phát hiện queue issues qua:
  - **FPS = 0** trong 30+ giây → Queue có thể đầy
  - **Excessive warnings** (>100 warnings) → Queue đầy
- **Tự động restart instance** khi detect queue issues

### 3. Cơ Chế Phát Hiện

#### A. FPS Monitoring (Primary)
```cpp
// Nếu FPS = 0 trong 3 lần check liên tiếp (30 giây)
if (current_fps == 0.0 && zero_fps_count >= 3) {
    // Queue có thể đầy → Restart instance để clear queue
    instanceRegistry.stopInstance(instanceId);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    instanceRegistry.startInstance(instanceId);
}
```

#### B. Warning Count Monitoring (Secondary)
```cpp
// Nếu có >100 queue full warnings
if (warning_count >= 100) {
    // Queue đầy → Restart instance
}
```

## 📊 Flow Diagram

```
┌─────────────────┐
│  Monitoring     │
│  Thread (10s)   │
└────────┬────────┘
         │
         ├─→ Check FPS for all instances
         │   ├─→ FPS = 0 for 30+ seconds?
         │   │   └─→ YES → Restart instance
         │   │
         │   └─→ Warning count > 100?
         │       └─→ YES → Restart instance
         │
         └─→ Parse log file (if enabled)
             └─→ Detect "queue full" warnings
                 └─→ Record to QueueMonitor
```

## 🔧 Configuration

### Environment Variables

```bash
# Set CVEDIX log level to INFO to see queue full warnings
export CVEDIX_LOG_LEVEL=INFO
```

### Code Configuration

```cpp
queueMonitor.setAutoClearThreshold(50.0);  // 50 warnings per second
queueMonitor.setMonitoringWindow(5);      // 5 seconds window
```

## 📝 Cách Hoạt Động

1. **Monitoring Thread** chạy mỗi 10 giây
2. Check tất cả running instances:
   - **FPS = 0** trong 30+ giây? → Queue có thể đầy
   - **Queue full warnings > 100?** → Queue đầy
3. Nếu detect issue:
   - **Stop instance** (clear queue)
   - **Wait 1 second** (để cleanup hoàn tất)
   - **Start instance lại** (fresh pipeline, empty queue)
   - **Clear stats** (reset monitoring)

## ✅ Lợi Ích

1. **Proactive Prevention**: Phát hiện queue issues **trước khi** deadlock
2. **Auto-Recovery**: Tự động restart instance để clear queue
3. **No Manual Intervention**: Không cần can thiệp thủ công
4. **Prevent Crash**: Tránh crash do deadlock khi queue đầy
5. **Continuous Operation**: Instance tự động recover và tiếp tục chạy

## 🔍 Monitoring Logs

Khi queue issues được detect:

```
[QueueMonitor] Instance xxx needs restart: FPS = 0 for 30+ seconds (possible queue full)
[QueueMonitor] Restarting instance xxx to clear queue and prevent deadlock
[QueueMonitor] Instance xxx restarted successfully
```

Khi queue warnings được track:

```
[QueueMonitor] Instance xxx queue full warnings: 100 in 5s (rate: 20 warnings/s)
[QueueMonitor] WARNING: Queue full rate (50 warnings/s) exceeds threshold (50)
```

## ⚙️ Tuning

### Nếu Restart Quá Nhiều

Giảm sensitivity:
```cpp
// Tăng threshold để ít restart hơn
zero_fps_count[instanceId] >= 5;  // 50 seconds instead of 30
```

### Nếu Không Phát Hiện Kịp

Tăng sensitivity:
```cpp
// Giảm threshold để phát hiện sớm hơn
zero_fps_count[instanceId] >= 2;  // 20 seconds instead of 30
```

## 🎯 Next Steps

1. **Test với video có FPS cao** để verify hoạt động
2. **Monitor logs** để xem có restart quá nhiều không
3. **Adjust thresholds** nếu cần
4. **Enable log parsing** nếu muốn detect warnings trực tiếp từ log file

## 📌 Lưu Ý

1. **Restart sẽ mất data**: Khi restart, pipeline sẽ reset → mất frames đang xử lý
2. **FPS = 0 có thể do nguyên nhân khác**: Không chỉ queue đầy (có thể video hết, RTSP disconnect, etc.)
3. **Cần test**: Cần test với các scenarios khác nhau để verify hoạt động đúng

## 🚀 Quick Start

Giải pháp đã được tích hợp tự động. Chỉ cần:

1. **Rebuild project**:
```bash
cd build && cmake .. && make -j$(nproc)
```

2. **Run server**:
```bash
./bin/edge_ai_api
```

3. **Monitor logs** để xem queue monitoring hoạt động:
```bash
./bin/edge_ai_api 2>&1 | grep -i "QueueMonitor"
```

## 🔬 Testing

Test với video có FPS cao:

```bash
# Create instance với video có FPS cao
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d @examples/instances/example_ba_crossline_file_mqtt_test.json

# Start instance
curl -X POST http://localhost:8080/v1/core/instances/{instanceId}/start

# Monitor logs để xem queue monitoring
tail -f logs/general/*.log | grep -i "QueueMonitor"
```

