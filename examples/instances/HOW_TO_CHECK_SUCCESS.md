# Làm sao biết Instance đã xử lý thành công?

## Cách nhanh nhất: Kiểm tra FPS

```bash
# Nếu FPS > 0 → Instance đang xử lý thành công!
curl -s http://localhost:8848/v1/core/instances/{instanceId} | jq '.fps'
```

## Các cách kiểm tra

### 1. Script tự động (Khuyến nghị)

```bash
# Kiểm tra một lần - hiển thị đầy đủ thông tin
./examples/instances/check_instance_status.sh {instanceId}

# Monitor real-time - theo dõi liên tục
./examples/instances/monitor_instance.sh {instanceId}
```

### 2. Kiểm tra qua API

```bash
# Xem thông tin chi tiết
curl -s http://localhost:8848/v1/core/instances/{instanceId} | jq '.'

# Kiểm tra nhanh các trường quan trọng
curl -s http://localhost:8848/v1/core/instances/{instanceId} | jq '{running, fps, loaded}'
```

**Dấu hiệu thành công:**
- `running: true` ✓
- `fps > 0` ✓ (quan trọng nhất!)
- `loaded: true` ✓

### 3. Kiểm tra output files

```bash
# Xem files trong thư mục output
ls -lht ./output/{instanceId}/

# Monitor files real-time
watch -n 1 'ls -lht ./output/{instanceId}/ | head -10'
```

**Dấu hiệu thành công:**
- Có file mới được tạo liên tục ✓
- File có timestamp gần đây ✓

### 4. Kiểm tra RTMP/RTSP stream

```bash
# Lấy RTMP URL
RTMP_URL=$(curl -s http://localhost:8848/v1/core/instances/{instanceId} | jq -r '.rtmpUrl')

# Test stream
ffplay $RTMP_URL
```

## Checklist nhanh

- [ ] Instance `running = true`
- [ ] `fps > 0` (quan trọng nhất!)
- [ ] Có output files (nếu có file_des node)
- [ ] RTMP/RTSP stream hoạt động (nếu có)
- [ ] Không có error trong logs

## Vấn đề thường gặp

### FPS = 0 nhưng running = true

**Nguyên nhân:**
- Input source không hợp lệ
- Đang khởi động (đợi thêm 10-30s)
- Model chưa load xong

**Giải pháp:**
```bash
# Kiểm tra input source
curl -s http://localhost:8848/v1/core/instances/{instanceId} | jq '.additionalParams.RTSP_URL'

# Xem logs
tail -f /var/log/edge_ai_api.log | grep -i error

# Restart nếu cần
curl -X POST http://localhost:8848/v1/core/instances/{instanceId}/restart
```

## Xem thêm

Chi tiết đầy đủ: [README.md](README.md#8-kiểm-tra-kết-quả-xử-lý---làm-sao-biết-instance-đã-xử-lý-thành-công)

