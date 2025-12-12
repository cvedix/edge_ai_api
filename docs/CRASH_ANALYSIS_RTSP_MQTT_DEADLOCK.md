# Phân tích Crash: RTSP + MQTT - Resource Deadlock Avoided

## 🔴 Vấn đề

Server crash với lỗi **"Resource deadlock avoided"** khi chạy pipeline RTSP với MQTT output.

## 📊 Phân tích Log (Lines 545-1026)

### 1. Placeholder Không Được Thay Thế

**Line 904 trong log:**
```
[PipelineBuilder] [MQTT] Broke for: ${BROKE_FOR}, Warn threshold: 1000, Ignore threshold: 10000
```

**Vấn đề:** Placeholder `${BROKE_FOR}` không được thay thế thành `"NORMAL"` từ `additionalParams`.

**Nguyên nhân:** Code chỉ xử lý các placeholder cụ thể (như `${WEIGHTS_PATH}`, `${CONFIG_PATH}`), không có handler tổng quát cho các placeholder khác như `${BROKE_FOR}`.

### 2. RTSP Connection Thành Công

**Line 981:**
```
[2025-12-12 03:39:50.364][Info] [rtsp_src_...] RTSP connection opened successfully
```

RTSP stream đã kết nối thành công.

### 3. Pipeline Đang Chạy

**Line 982:**
```
[2025-12-12 03:39:51.318][Info] [sort_tracker_...] initialize kalmantracker the first time for channel 0
```

Pipeline đã bắt đầu xử lý frames.

### 4. Crash với Deadlock

**Line 994-1020:**
```
2025-12-12 02:40:01.856 ERROR [948807] [terminateHandler@508] [CRITICAL] Uncaught exception: Resource deadlock avoided
[InstanceRegistry] WARNING: listInstances() timeout - mutex is locked, returning empty vector
[RECOVERY] Received SIGABRT signal - possible causes:
[RECOVERY]   1. OpenCV DNN shape mismatch (frames with inconsistent sizes)
[RECOVERY]   2. Queue full causing deadlock (MQTT/processing too slow)
[RECOVERY]   3. Resource deadlock (mutex locked by blocked threads)
```

## 🔍 Nguyên Nhân

### 1. **Placeholder Không Được Thay Thế**

- `${BROKE_FOR}` không được thay thế → Node nhận giá trị literal `${BROKE_FOR}` thay vì `"NORMAL"`
- Code fallback về `NORMAL` nhưng có thể gây confusion và log sai

### 2. **Queue Full → Deadlock**

Khi pipeline chạy:
- RTSP stream đang gửi frames liên tục
- YOLO detector xử lý chậm hơn frame rate
- MQTT publish có thể chậm (network latency, broker chậm)
- Queue đầy → Threads block trên mutex
- Cleanup thread cũng cần lock → **Deadlock**

### 3. **Mutex Lock Timeout**

Khi deadlock xảy ra:
- `listInstances()` timeout vì mutex bị lock
- Recovery handler không thể list instances để stop
- Application crash với SIGABRT

## ✅ Giải Pháp Đã Áp Dụng

### Fix 1: Generic Placeholder Substitution

**File:** `src/core/pipeline_builder.cpp`

**Thay đổi:** Thêm generic placeholder substitution handler để xử lý tất cả placeholders từ `additionalParams`:

```cpp
// Generic placeholder substitution: Replace ${VARIABLE_NAME} with values from additionalParams
// This handles placeholders that weren't explicitly handled above (e.g., ${BROKE_FOR})
std::regex placeholderPattern("\\$\\{([A-Za-z0-9_]+)\\}");
std::sregex_iterator iter(value.begin(), value.end(), placeholderPattern);
std::sregex_iterator end;
std::set<std::string> processedVars; // Track processed variables to avoid duplicate replacements

for (; iter != end; ++iter) {
    std::string varName = (*iter)[1].str();
    
    // Skip if already processed
    if (processedVars.find(varName) != processedVars.end()) {
        continue;
    }
    processedVars.insert(varName);
    
    auto it = req.additionalParams.find(varName);
    if (it != req.additionalParams.end() && !it->second.empty()) {
        // Replace all occurrences of this placeholder
        value = std::regex_replace(value, std::regex("\\$\\{" + varName + "\\}"), it->second);
        std::cerr << "[PipelineBuilder] Replaced ${" << varName << "} with: " << it->second << std::endl;
    } else {
        // Placeholder not found in additionalParams - leave as is
        std::cerr << "[PipelineBuilder] WARNING: Placeholder ${" << varName << "} not found in additionalParams, leaving as literal" << std::endl;
    }
}
```

**Kết quả:** Bây giờ `${BROKE_FOR}` sẽ được thay thế thành `"NORMAL"` từ `additionalParams`.

### Fix 2: Thêm Include `<regex>`

Thêm `#include <regex>` để support regex operations.

## 🛠️ Giải Pháp Bổ Sung (Để Tránh Deadlock)

### 1. Tăng SKIP_INTERVAL

Giảm frame rate để giảm tải cho queue:

```json
{
  "additionalParams": {
    "SKIP_INTERVAL": "9",  // Đã có trong config
    // Có thể tăng lên 15-20 nếu vẫn bị deadlock
  }
}
```

### 2. Giảm RESIZE_RATIO

Giảm resolution để tăng tốc độ xử lý:

```json
{
  "additionalParams": {
    "RESIZE_RATIO": "0.1",  // Đã có trong config
    // Có thể giảm xuống 0.05 nếu vẫn chậm
  }
}
```

### 3. Kiểm Tra MQTT Broker

Đảm bảo MQTT broker (`localhost:1883`) có:
- Network latency thấp
- Không bị rate limit
- QoS = 0 (nếu có thể) để tránh blocking

### 4. Monitor Queue Status

Sử dụng queue monitoring để detect queue full sớm:
- Check `/v1/core/watchdog` endpoint
- Monitor logs cho "queue full" warnings
- Auto-restart instance nếu queue full quá nhiều

## 📝 Checklist

- [x] Fix placeholder substitution cho `${BROKE_FOR}`
- [x] Thêm generic placeholder handler
- [ ] Test với RTSP stream thực tế
- [ ] Monitor queue status trong production
- [ ] Tối ưu MQTT publish (nếu vẫn chậm)
- [ ] Thêm timeout cho MQTT operations

## 🎯 Kết Luận

**Nguyên nhân chính:**
1. Placeholder `${BROKE_FOR}` không được thay thế (đã fix)
2. Queue full do MQTT publish chậm → Deadlock

**Đã fix:**
- Generic placeholder substitution handler
- Placeholder `${BROKE_FOR}` sẽ được thay thế đúng

**Cần theo dõi:**
- Queue status khi chạy với RTSP stream thực tế
- MQTT publish performance
- Deadlock có còn xảy ra không

## 🔗 Liên Quan

- [CRASH_ANALYSIS_QUEUE_DEADLOCK.md](./CRASH_ANALYSIS_QUEUE_DEADLOCK.md) - Phân tích queue deadlock với file source
- [QUEUE_MONITORING_SOLUTION.md](./QUEUE_MONITORING_SOLUTION.md) - Giải pháp monitoring queue
- [DEBUG_CRASH_AND_MQTT.md](./DEBUG_CRASH_AND_MQTT.md) - Debug MQTT issues

