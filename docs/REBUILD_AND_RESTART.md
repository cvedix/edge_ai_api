# Hướng Dẫn Rebuild và Restart Ứng Dụng

## 🔄 Rebuild Code Sau Khi Cập Nhật

### Bước 1: Dừng Ứng Dụng Đang Chạy

```bash
# Tìm process ID
ps aux | grep edge_ai_api | grep -v grep

# Dừng ứng dụng (thay PID bằng process ID thực tế)
kill <PID>

# Hoặc nếu chạy trong terminal, dùng Ctrl+C
```

### Bước 2: Rebuild

```bash
cd /home/cvedix/project/edge_ai_api

# Nếu có build directory
cd build
cmake ..
make -j$(nproc)

# Hoặc rebuild từ đầu
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

### Bước 3: Restart Ứng Dụng

```bash
# Chạy lại ứng dụng
./build/bin/edge_ai_api

# Hoặc nếu dùng systemd
sudo systemctl restart edge-ai-api
```

### Bước 4: Kiểm Tra

```bash
# Test API
curl -X GET http://localhost:8080/v1/core/instance/a4d54476-475e-4790-a3c4-805e5c41fd9b/output/stream

# Response sẽ có field "path" nếu đã rebuild đúng
```

