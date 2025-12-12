# Phân tích lỗi RTSP Connection Timeout

## 📋 Tóm tắt lỗi

**Lỗi:** RTSP không thể kết nối đến server  
**RTSP URL:** `rtsp://100.76.5.84:8554/mystream`  
**Thông báo lỗi:** `Could not connect to server. (Timeout while waiting for server response)`

---

## 🔍 Chi tiết lỗi từ log

### Lỗi GStreamer RTSP:
```
ERROR rtspsrc gstrtspsrc.c:5492:gst_rtsp_conninfo_connect:<rtspsrc0> 
Could not connect to server. (Timeout while waiting for server response)

WARN rtspsrc gstrtspsrc.c:8442:gst_rtspsrc_retrieve_sdp:<rtspsrc0> 
error: Failed to connect. (Timeout while waiting for server response)

WARN rtspsrc gstrtspsrc.c:8528:gst_rtspsrc_open:<rtspsrc0> can't get sdp
```

### Retry attempts:
- Attempt 1: Failed sau ~20 giây
- Attempt 2: Failed sau ~40 giây  
- Attempt 3: Failed sau ~60 giây
- Attempt 4: Failed sau ~80 giây
- ... (tiếp tục retry đến 10 lần)

---

## ✅ Kết quả kiểm tra

### 1. Ping test:
```bash
$ ping -c 3 100.76.5.84
PING 100.76.5.84 (100.76.5.84) 56(84) bytes of data.
--- 100.76.5.84 ping statistics ---
3 packets transmitted, 0 received, 100% packet loss
```
**Kết luận:** Server không thể truy cập từ mạng này

### 2. Port test:
```bash
$ timeout 5 bash -c 'echo > /dev/tcp/100.76.5.84/8554'
Port 8554 is closed or unreachable
```
**Kết luận:** Port RTSP (8554) đóng hoặc bị firewall chặn

---

## 🎯 Nguyên nhân có thể

### 1. **RTSP Server không chạy**
- Server tại `100.76.5.84:8554` không đang chạy
- Service RTSP đã bị dừng

### 2. **Vấn đề mạng**
- Server không thể truy cập từ máy hiện tại
- Routing issue giữa các mạng
- Server ở mạng khác (VPN cần thiết?)

### 3. **Firewall chặn**
- Firewall trên server chặn port 8554
- Firewall trên client chặn kết nối ra ngoài
- Network security group rules

### 4. **IP Address sai**
- IP `100.76.5.84` có thể đã thay đổi
- IP là private IP và không route được

### 5. **RTSP Stream không tồn tại**
- Stream path `/mystream` không tồn tại
- Stream đã bị xóa hoặc đổi tên

---

## 🔧 Giải pháp

### Giải pháp 1: Kiểm tra RTSP Server

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

### Giải pháp 2: Kiểm tra từ client

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

### Giải pháp 3: Sửa firewall

**Nếu server ở cùng mạng:**
```bash
# Trên server
sudo ufw allow 8554/tcp
sudo ufw allow 8554/udp
```

**Nếu server ở mạng khác:**
- Kiểm tra security group rules (AWS, Azure, GCP)
- Mở port 8554 (TCP và UDP) trong firewall rules

### Giải pháp 4: Kiểm tra VPN/Network

**Nếu server ở mạng riêng:**
```bash
# Kiểm tra VPN connection
ip addr show
route -n

# Kiểm tra có thể ping được gateway không
ping <gateway_ip>
```

### Giải pháp 5: Thử RTSP URL khác

**Nếu có RTSP server khác để test:**
```json
{
  "additionalParams": {
    "RTSP_SRC_URL": "rtsp://<other_server>:<port>/<stream>",
    ...
  }
}
```

### Giải pháp 6: Sử dụng file source tạm thời

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

---

## 📊 Timeline lỗi

```
00:00:00 - Pipeline start
00:00:20 - RTSP connection attempt 1: Timeout
00:00:40 - RTSP connection attempt 2: Timeout  
00:01:00 - RTSP connection attempt 3: Timeout
00:01:20 - RTSP connection attempt 4: Timeout
...
```

**Mỗi retry cách nhau ~20 giây** (timeout của GStreamer RTSP)

---

## 🎯 Kết luận

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



