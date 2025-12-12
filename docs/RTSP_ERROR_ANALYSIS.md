# Phân tích chi tiết lỗi RTSP Stream (Lines 951-1018)

## 📋 Tóm tắt lỗi

**Trạng thái:** RTSP kết nối thành công nhưng không nhận được video frames  
**Decoder đang dùng:** `vulkanh264dec`  
**Instance ID:** `8467fbc5-a989-4e57-b659-2be6171ade8a`

---

## 🔍 Phân tích từng bước

### ✅ Bước 1: Pipeline khởi tạo thành công (Dòng 951-996)
```
[PipelineBuilder] ✓ RTMP destination node created successfully
[PipelineBuilder] Successfully built pipeline with 6 nodes
[InstanceRegistry] ✓ Instance started successfully
```
- Pipeline được build thành công với 6 nodes
- RTMP destination node tạo thành công
- Instance đã start

### ✅ Bước 2: RTSP Connection thành công (Dòng 998-1002)
```
INFO rtspsrc gstrtspsrc.c:8337:gst_rtspsrc_retrieve_sdp: Now using version: 1.0
INFO rtspsrc gstrtspsrc.c:4284:gst_rtspsrc_stream_configure_manager: configure bandwidth
INFO rtspsrc gstrtspsrc.c:4289:gst_rtspsrc_stream_configure_manager: setting AS: 2500000.000000
INFO rtspsrc gstrtspsrc.c:4289:gst_rtspsrc_stream_configure_manager: setting AS: 160000.000000
```
- ✅ RTSP handshake thành công
- ✅ SDP negotiation thành công
- ✅ Bandwidth được configure (2.5Mbps cho video, 160Kbps cho audio)
- ✅ Stream đã được setup

### ❌ Bước 3: GStreamer CRITICAL Errors (Dòng 1005-1009)
```
GStreamer-CRITICAL **: gst_caps_get_structure: assertion 'GST_IS_CAPS (caps)' failed
GStreamer-CRITICAL **: gst_structure_get_int: assertion 'structure != NULL' failed
GStreamer-CRITICAL **: gst_structure_get_fraction: assertion 'structure != NULL' failed
```

**Nguyên nhân:**
- SDK CVEDIX cố gắng lấy **caps** (capabilities) từ `appsink` nhưng nhận được **NULL**
- Caps negotiation giữa decoder và appsink **thất bại**
- SDK không thể xác định format của video frames

**Vị trí lỗi:** 
- File: `/home/cvedix/core_ai_runtime/nodes/src/cvedix_rtsp_src_node.cpp`
- SDK đang cố gắng lấy width, height, framerate từ caps nhưng caps = NULL

### ⚠️ Bước 4: RTSP Connection Opened nhưng không có data (Dòng 1010)
```
[rtsp_src_8467fbc5-a989-4e57-b659-2be6171ade8a] RTSP connection opened successfully
```
- ✅ RTSP connection mở thành công
- ❌ Nhưng không có frame nào được nhận từ stream

### 🔄 Bước 5: Instance Retry liên tục (Dòng 1003, 1011)
```
[InstanceRegistry] Instance retry detected: count=1/10, running=70s, no_data=yes, inactive=70s
[InstanceRegistry] Instance retry detected: count=2/10, running=100s, no_data=yes, inactive=100s
```
- Instance retry vì `no_data=yes` (không có data)
- Instance inactive trong 70s, 100s...
- Sẽ retry đến 10 lần

---

## 🎯 Nguyên nhân gốc rễ

### 1. **Caps Negotiation Failure**
```
Decoder (vulkanh264dec) → videoconvert → appsink
                          ↑
                    Caps negotiation fails here
```
- Decoder decode được frames nhưng không thể negotiate caps với appsink
- SDK không biết format của frames (NV12? RGB? BGR?)

### 2. **SDK Code Issue**
- SDK code trong `cvedix_rtsp_src_node.cpp:152` cố gắng lấy caps từ sample
- Nhưng sample không có caps hoặc caps = NULL
- SDK không handle được trường hợp này

### 3. **Pipeline thiếu Caps Filter**
Pipeline hiện tại:
```
rtspsrc ! rtph264depay ! h264parse ! vulkanh264dec ! videoconvert ! appsink
```

Pipeline cần có:
```
rtspsrc ! rtph264depay ! h264parse ! vulkanh264dec ! videoconvert ! video/x-raw,format=NV12 ! appsink
                                                                    ↑
                                                          Thiếu caps filter này
```

---

## 🔧 Giải pháp đã thử

### ❌ Đã thử các decoder:
1. `avdec_h264` - ❌ Lỗi tương tự
2. `openh264dec` - ❌ Lỗi tương tự  
3. `vulkanh264dec` - ❌ Lỗi tương tự

### ⚠️ Vấn đề:
- SDK CVEDIX **hardcode pipeline**, không thể thêm caps filter
- Tất cả decoder đều gặp lỗi tương tự → **không phải vấn đề decoder**

---

## 💡 Giải pháp đề xuất

### 1. **Kiểm tra SDK CVEDIX Source Code**
- File: `/home/cvedix/core_ai_runtime/nodes/src/cvedix_rtsp_src_node.cpp:152`
- Xem cách SDK lấy caps từ appsink
- Có thể cần fix SDK để handle NULL caps

### 2. **Thử với decodebin (auto-detect)**
- SDK có thể không hỗ trợ decodebin trực tiếp
- Nhưng có thể thử trong config

### 3. **Bật GStreamer Debug**
```bash
export GST_DEBUG=rtspsrc:4,vulkanh264dec:4,appsink:4,videoconvert:4
./bin/edge_ai_api
```
- Xem chi tiết caps negotiation
- Xem decoder output format

### 4. **Liên hệ CVEDIX SDK Team**
- Đây có thể là **bug trong SDK**
- SDK không handle được trường hợp caps = NULL
- Cần fix trong SDK code

---

## 📊 Timeline lỗi

```
00:00:00 - Pipeline start
00:00:02 - RTSP connection opened ✅
00:00:02 - Bandwidth configured ✅
00:01:10 - GStreamer CRITICAL errors ❌ (caps = NULL)
00:01:10 - RTSP connection opened successfully ✅ (nhưng no data)
00:01:10 - Instance retry #1 (no_data=yes)
00:01:40 - Instance retry #2 (no_data=yes)
...
```

---

## 🎯 Kết luận

**Vấn đề chính:** SDK CVEDIX không thể lấy caps từ appsink, dẫn đến không thể xử lý video frames.

**Nguyên nhân:** 
- Caps negotiation failure giữa decoder và appsink
- SDK code không handle được trường hợp caps = NULL

**Giải pháp:** 
- Cần fix SDK code để handle NULL caps
- Hoặc thêm caps filter vào pipeline (nhưng SDK hardcode pipeline)

**Khuyến nghị:** Liên hệ CVEDIX SDK team để fix bug này.

