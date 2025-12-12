# RTSP Troubleshooting Guide

Tài liệu này tổng hợp các vấn đề thường gặp với RTSP và cách khắc phục.

## 📋 Mục Lục

1. [RTSP Connection Timeout](#rtsp-connection-timeout)
2. [RTSP Decoder Issues](#rtsp-decoder-issues)
3. [RTSP Error Analysis](#rtsp-error-analysis)

---

## RTSP Connection Timeout

### 📋 Tóm tắt lỗi

**Lỗi:** RTSP không thể kết nối đến server  
**RTSP URL:** `rtsp://100.76.5.84:8554/mystream`  
**Thông báo lỗi:** `Could not connect to server. (Timeout while waiting for server response)`

### 🔍 Chi tiết lỗi từ log

#### Lỗi GStreamer RTSP:
```
ERROR rtspsrc gstrtspsrc.c:5492:gst_rtsp_conninfo_connect:<rtspsrc0> 
Could not connect to server. (Timeout while waiting for server response)

WARN rtspsrc gstrtspsrc.c:8442:gst_rtspsrc_retrieve_sdp:<rtspsrc0> 
error: Failed to connect. (Timeout while waiting for server response)
```

#### Retry attempts:
- Attempt 1: Failed sau ~20 giây
- Attempt 2: Failed sau ~40 giây  
- Attempt 3: Failed sau ~60 giây
- Attempt 4: Failed sau ~80 giây
- ... (tiếp tục retry đến 10 lần)

### ✅ Kết quả kiểm tra

#### 1. Ping test:
```bash
$ ping -c 3 100.76.5.84
PING 100.76.5.84 (100.76.5.84) 56(84) bytes of data.
--- 100.76.5.84 ping statistics ---
3 packets transmitted, 0 received, 100% packet loss
```
**Kết luận:** Server không thể truy cập từ mạng này

#### 2. Port test:
```bash
$ timeout 5 bash -c 'echo > /dev/tcp/100.76.5.84/8554'
Port 8554 is closed or unreachable
```
**Kết luận:** Port RTSP (8554) đóng hoặc bị firewall chặn

### 🎯 Nguyên nhân có thể

1. **RTSP Server không chạy**
   - Server tại `100.76.5.84:8554` không đang chạy
   - Service RTSP đã bị dừng

2. **Vấn đề mạng**
   - Server không thể truy cập từ máy hiện tại
   - Routing issue giữa các mạng
   - Server ở mạng khác (VPN cần thiết?)

3. **Firewall chặn**
   - Firewall trên server chặn port 8554
   - Firewall trên client chặn kết nối ra ngoài
   - Network security group rules

4. **IP Address sai**
   - IP `100.76.5.84` có thể đã thay đổi
   - IP là private IP và không route được

5. **RTSP Stream không tồn tại**
   - Stream path `/mystream` không tồn tại
   - Stream đã bị xóa hoặc đổi tên

### 🔧 Giải pháp

#### Giải pháp 1: Kiểm tra RTSP Server

**Trên server RTSP (`100.76.5.84`):**
```bash
# Kiểm tra RTSP service có chạy không
sudo systemctl status mediamtx  # hoặc service khác
sudo netstat -tlnp | grep 8554
sudo ss -tlnp | grep 8554

# Kiểm tra firewall
sudo ufw status
sudo iptables -L -n | grep 8554

# Test RTSP stream locally
ffprobe rtsp://localhost:8554/mystream
```

#### Giải pháp 2: Kiểm tra từ client

**Sử dụng script diagnostic:**
```bash
./scripts/test_rtsp_connection.sh rtsp://100.76.5.84:8554/mystream
```

**Kiểm tra thủ công:**
```bash
# Test với ffprobe
ffprobe -v error -rtsp_transport tcp rtsp://100.76.5.84:8554/mystream

# Test với GStreamer
gst-launch-1.0 -v rtspsrc location=rtsp://100.76.5.84:8554/mystream protocols=tcp latency=0 ! fakesink

# Test với VLC (GUI)
vlc rtsp://100.76.5.84:8554/mystream
```

#### Giải pháp 3: Sửa firewall

**Nếu server ở cùng mạng:**
```bash
# Trên server
sudo ufw allow 8554/tcp
sudo ufw allow 8554/udp
```

**Nếu server ở mạng khác:**
- Kiểm tra security group rules (AWS, Azure, GCP)
- Mở port 8554 (TCP và UDP) trong firewall rules

#### Giải pháp 4: Kiểm tra VPN/Network

**Nếu server ở mạng riêng:**
```bash
# Kiểm tra VPN connection
ip addr show
route -n

# Kiểm tra có thể ping được gateway không
ping <gateway_ip>
```

#### Giải pháp 5: Thử RTSP URL khác

**Nếu có RTSP server khác để test:**
```json
{
  "additionalParams": {
    "RTSP_SRC_URL": "rtsp://<other_server>:<port>/<stream>",
    ...
  }
}
```

#### Giải pháp 6: Sử dụng file source tạm thời

**Để test pipeline hoạt động:**
```json
{
  "additionalParams": {
    "FILE_PATH": "/path/to/test/video.mp4",
    // Xóa RTSP_SRC_URL để dùng file source
    ...
  }
}
```

### 🎯 Kết luận

**Vấn đề chính:** RTSP server tại `100.76.5.84:8554` không thể truy cập từ máy hiện tại.

**Nguyên nhân:** 
- Server không chạy hoặc không thể truy cập (100% packet loss)
- Port 8554 đóng hoặc bị firewall chặn

**Hành động cần thiết:**
1. ✅ Kiểm tra RTSP server có đang chạy không
2. ✅ Kiểm tra firewall rules trên server
3. ✅ Kiểm tra network connectivity (ping, routing)
4. ✅ Xác nhận RTSP URL đúng và stream tồn tại
5. ✅ Test RTSP stream với ffprobe/VLC trước khi dùng trong API

**Khuyến nghị:** 
- Sửa vấn đề network/server trước khi tiếp tục
- Hoặc sử dụng file source để test pipeline trong khi chờ RTSP server sẵn sàng

---

## RTSP Decoder Issues

### Vấn đề: GStreamer CRITICAL errors với RTSP stream

#### Lỗi gặp phải:
```
GStreamer-CRITICAL **: gst_caps_get_structure: assertion 'GST_IS_CAPS (caps)' failed
GStreamer-CRITICAL **: gst_sample_get_caps: assertion 'GST_IS_SAMPLE (sample)' failed
retrieveVideoFrame GStreamer: gst_sample_get_caps() returns NULL
```

#### Nguyên nhân:
- Decoder không tương thích với stream format
- Caps negotiation giữa decoder và appsink thất bại
- SDK không lấy được sample từ appsink

#### Giải pháp đã thử:
1. ✅ Đổi từ `avdec_h264` → `openh264dec` (vẫn lỗi)
2. ⏳ Cần thử các decoder khác

### Các decoder có thể thử:

#### 1. Kiểm tra decoder có sẵn:
```bash
gst-inspect-1.0 | grep -E "h264.*dec|dec.*h264"
```

#### 2. Test decoder với GStreamer:
```bash
# Test openh264dec
gst-launch-1.0 rtspsrc location=rtsp://anhoidong.datacenter.cvedix.com:8554/live/camera_demo_sang_vehicle protocols=tcp latency=0 ! application/x-rtp,media=video ! rtph264depay ! h264parse ! openh264dec ! videoconvert ! fakesink

# Test avdec_h264
gst-launch-1.0 rtspsrc location=rtsp://anhoidong.datacenter.cvedix.com:8554/live/camera_demo_sang_vehicle protocols=tcp latency=0 ! application/x-rtp,media=video ! rtph264depay ! h264parse ! avdec_h264 ! videoconvert ! fakesink

# Test với decodebin (auto-detect)
gst-launch-1.0 rtspsrc location=rtsp://anhoidong.datacenter.cvedix.com:8554/live/camera_demo_sang_vehicle protocols=tcp latency=0 ! application/x-rtp,media=video ! rtph264depay ! h264parse ! decodebin ! videoconvert ! fakesink
```

#### 3. Bật GStreamer debug để xem chi tiết:
```bash
export GST_DEBUG=rtspsrc:4,openh264dec:4,appsink:4
./bin/edge_ai_api
```

### Decoder có sẵn trên hệ thống:
- `avdec_h264` (libav H.264 decoder) - ❌ Không hoạt động
- `openh264dec` (OpenH264 decoder) - ❌ Không hoạt động  
- `vulkanh264dec` (Vulkan H.264 decoder) - ⏳ Chưa thử

### Cập nhật config để thử decoder khác:
Trong `example_ba_crossline_in_rtsp_out_rtmp.json`, thay đổi:
```json
"GST_DECODER_NAME": "vulkanh264dec"
```

### Lưu ý:
- SDK CVEDIX hardcode pipeline, không thể thêm caps filter trực tiếp
- Vấn đề có thể nằm ở cách SDK lấy sample từ appsink
- Cần kiểm tra với CVEDIX SDK team về vấn đề này

---

## RTSP Error Analysis

### 📋 Tóm tắt lỗi

**Trạng thái:** RTSP kết nối thành công nhưng không nhận được video frames  
**Decoder đang dùng:** `vulkanh264dec`  
**Instance ID:** `8467fbc5-a989-4e57-b659-2be6171ade8a`

### 🔍 Phân tích từng bước

#### ✅ Bước 1: Pipeline khởi tạo thành công
- Pipeline được build thành công với 6 nodes
- RTMP destination node tạo thành công
- Instance đã start

#### ✅ Bước 2: RTSP Connection thành công
- RTSP handshake thành công
- SDP negotiation thành công
- Bandwidth được configure (2.5Mbps cho video, 160Kbps cho audio)
- Stream đã được setup

#### ❌ Bước 3: GStreamer CRITICAL Errors
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

#### ⚠️ Bước 4: RTSP Connection Opened nhưng không có data
- ✅ RTSP connection mở thành công
- ❌ Nhưng không có frame nào được nhận từ stream

#### 🔄 Bước 5: Instance Retry liên tục
- Instance retry vì `no_data=yes` (không có data)
- Instance inactive trong 70s, 100s...
- Sẽ retry đến 10 lần

### 🎯 Nguyên nhân gốc rễ

#### 1. **Caps Negotiation Failure**
```
Decoder (vulkanh264dec) → videoconvert → appsink
                          ↑
                    Caps negotiation fails here
```
- Decoder decode được frames nhưng không thể negotiate caps với appsink
- SDK không biết format của frames (NV12? RGB? BGR?)

#### 2. **SDK Code Issue**
- SDK code trong `cvedix_rtsp_src_node.cpp:152` cố gắng lấy caps từ sample
- Nhưng sample không có caps hoặc caps = NULL
- SDK không handle được trường hợp này

#### 3. **Pipeline thiếu Caps Filter**
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

### 🔧 Giải pháp đã thử

#### ❌ Đã thử các decoder:
1. `avdec_h264` - ❌ Lỗi tương tự
2. `openh264dec` - ❌ Lỗi tương tự  
3. `vulkanh264dec` - ❌ Lỗi tương tự

#### ⚠️ Vấn đề:
- SDK CVEDIX **hardcode pipeline**, không thể thêm caps filter
- Tất cả decoder đều gặp lỗi tương tự → **không phải vấn đề decoder**

### 💡 Giải pháp đề xuất

#### 1. **Kiểm tra SDK CVEDIX Source Code**
- File: `/home/cvedix/core_ai_runtime/nodes/src/cvedix_rtsp_src_node.cpp:152`
- Xem cách SDK lấy caps từ appsink
- Có thể cần fix SDK để handle NULL caps

#### 2. **Thử với decodebin (auto-detect)**
- SDK có thể không hỗ trợ decodebin trực tiếp
- Nhưng có thể thử trong config

#### 3. **Bật GStreamer Debug**
```bash
export GST_DEBUG=rtspsrc:4,vulkanh264dec:4,appsink:4,videoconvert:4
./bin/edge_ai_api
```
- Xem chi tiết caps negotiation
- Xem decoder output format

#### 4. **Liên hệ CVEDIX SDK Team**
- Đây có thể là **bug trong SDK**
- SDK không handle được trường hợp caps = NULL
- Cần fix trong SDK code

### 📊 Timeline lỗi

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

### 🎯 Kết luận

**Vấn đề chính:** SDK CVEDIX không thể lấy caps từ appsink, dẫn đến không thể xử lý video frames.

**Nguyên nhân:** 
- Caps negotiation failure giữa decoder và appsink
- SDK code không handle được trường hợp caps = NULL

**Giải pháp:** 
- Cần fix SDK code để handle NULL caps
- Hoặc thêm caps filter vào pipeline (nhưng SDK hardcode pipeline)

**Khuyến nghị:** Liên hệ CVEDIX SDK team để fix bug này.

---

## 📚 Tài Liệu Liên Quan

- [Troubleshooting Guide](./TROUBLESHOOTING.md) - Phân tích các vấn đề crash và deadlock
- [Development Setup](./DEVELOPMENT_SETUP.md) - Setup môi trường phát triển

