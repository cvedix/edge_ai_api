# Hướng Dẫn Triển Khai Production

Tài liệu này hướng dẫn cách triển khai Edge AI API lên thiết bị thật và cấu hình để tự động chạy khi khởi động.

## 🚀 Triển Khai Tự Động (Khuyến Nghị)

### Cách 1: Sử dụng setup.sh (Khuyến Nghị)

```bash
cd /home/ubuntu/project/edge_ai_api
sudo ./setup.sh --production
```

Script này sẽ tự động:
- ✅ Kiểm tra prerequisites
- ✅ Cài đặt system dependencies
- ✅ Build project
- ✅ Tạo user `edgeai` và group `edgeai`
- ✅ Cài đặt executable vào `/usr/local/bin/edge_ai_api`
- ✅ Tạo thư mục production tại `/opt/edge_ai_api`
- ✅ Cài đặt systemd service
- ✅ Kích hoạt service tự động chạy khi khởi động
- ✅ Khởi động service ngay lập tức

### Cách 2: Sử dụng deploy/build.sh (Production Script)

```bash
cd /home/ubuntu/project/edge_ai_api
sudo ./deploy/build.sh
```

Script này sẽ tự động:
- ✅ Cài đặt system dependencies (nếu chưa có)
- ✅ Build project
- ✅ Tạo user `edgeai` và group `edgeai`
- ✅ Cài đặt executable và libraries
- ✅ Tạo thư mục production với cấu trúc đầy đủ
- ✅ Cài đặt systemd service
- ✅ Kích hoạt và khởi động service

**Tùy chọn:**
```bash
# Bỏ qua cài đặt dependencies
sudo ./deploy/build.sh --skip-deps

# Bỏ qua build (dùng build có sẵn)
sudo ./deploy/build.sh --skip-build

# Không tự động start service
sudo ./deploy/build.sh --no-start

# Cấp quyền 777 (full permissions)
sudo ./deploy/build.sh --full-permissions

# Cấp quyền 755 (standard permissions - mặc định)
sudo ./deploy/build.sh --standard-permissions
```

### Bước 2: Kiểm Tra Service

```bash
# Xem trạng thái
sudo systemctl status edge-ai-api

# Xem log
sudo journalctl -u edge-ai-api -f

# Test API
curl http://localhost:8080/v1/core/health
```

## 📝 Cấu Hình

### Cấu Hình Biến Môi Trường

Tạo file `.env` tại `/opt/edge_ai_api/config/.env`:

```bash
sudo nano /opt/edge_ai_api/config/.env
```

Ví dụ nội dung:

```bash
API_HOST=0.0.0.0
API_PORT=8080
WATCHDOG_CHECK_INTERVAL_MS=5000
LOG_LEVEL=INFO
```

Sau khi chỉnh sửa, restart service:

```bash
sudo systemctl restart edge-ai-api
```

### Cấu Hình Service

File service nằm tại: `/etc/systemd/system/edge-ai-api.service`

Để chỉnh sửa:

```bash
sudo nano /etc/systemd/system/edge-ai-api.service
sudo systemctl daemon-reload
sudo systemctl restart edge-ai-api
```

## 🔧 Quản Lý Service

### Các Lệnh Thường Dùng

```bash
# Xem trạng thái
sudo systemctl status edge-ai-api

# Khởi động
sudo systemctl start edge-ai-api

# Dừng
sudo systemctl stop edge-ai-api

# Khởi động lại
sudo systemctl restart edge-ai-api

# Xem log real-time
sudo journalctl -u edge-ai-api -f

# Xem log gần đây (50 dòng)
sudo journalctl -u edge-ai-api -n 50

# Bật tự động chạy khi khởi động
sudo systemctl enable edge-ai-api

# Tắt tự động chạy khi khởi động
sudo systemctl disable edge-ai-api
```

## 🛠️ Triển Khai Thủ Công

Nếu bạn muốn triển khai thủ công thay vì dùng script:

### 1. Build Project

```bash
cd /home/ubuntu/project/edge_ai_api
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

### 2. Tạo User và Thư Mục

```bash
sudo useradd -r -s /bin/false -d /opt/edge_ai_api edgeai
sudo mkdir -p /opt/edge_ai_api/{logs,data,config}
sudo chown -R edgeai:edgeai /opt/edge_ai_api
```

### 3. Cài Đặt Executable

```bash
sudo cp build/bin/edge_ai_api /usr/local/bin/edge_ai_api
sudo chmod +x /usr/local/bin/edge_ai_api
```

### 4. Cài Đặt Service

```bash
sudo cp deploy/edge-ai-api.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable edge-ai-api
sudo systemctl start edge-ai-api
```

## 🔍 Troubleshooting

### Service Không Khởi Động

1. Kiểm tra log:
```bash
sudo journalctl -u edge-ai-api -n 100
```

2. Kiểm tra quyền:
```bash
ls -la /usr/local/bin/edge_ai_api
ls -la /opt/edge_ai_api
```

3. Kiểm tra user:
```bash
id edgeai
```

### Service Chạy Nhưng API Không Phản Hồi

1. Kiểm tra port có bị chiếm không:
```bash
sudo netstat -tlnp | grep 8080
# hoặc
sudo ss -tlnp | grep 8080
```

2. Kiểm tra firewall:
```bash
sudo ufw status
```

3. Test local:
```bash
curl http://localhost:8080/v1/core/health
```

### Service Tự Động Restart

1. Xem log để tìm lỗi:
```bash
sudo journalctl -u edge-ai-api -n 100 --no-pager
```

2. Kiểm tra resource limits trong service file

## 📂 Cấu Trúc Thư Mục Production

```
/opt/edge_ai_api/
├── config/
│   └── .env              # File cấu hình biến môi trường
├── logs/                 # Log files (nếu có)
├── data/                 # Data files (nếu có)
└── ...

/usr/local/bin/
└── edge_ai_api           # Executable

/etc/systemd/system/
└── edge-ai-api.service   # Service file
```

## 🔐 Bảo Mật

Service được cấu hình với các thiết lập bảo mật:
- Chạy với user riêng (`edgeai`) không có shell
- Giới hạn quyền truy cập file system
- Giới hạn tài nguyên (memory, CPU)
- Private tmp directory

## 📊 Monitoring

### Xem Resource Usage

```bash
# CPU và Memory
sudo systemctl status edge-ai-api

# Chi tiết hơn
top -p $(pgrep edge_ai_api)
```

### Health Check

```bash
# API health check
curl http://localhost:8080/v1/core/health

# Version info
curl http://localhost:8080/v1/core/version
```

## 🔄 Cập Nhật

Khi cần cập nhật phiên bản mới:

```bash
# 1. Dừng service
sudo systemctl stop edge-ai-api

# 2. Build lại
cd /home/ubuntu/project/edge_ai_api
cd build
cmake ..
make -j$(nproc)

# 3. Copy executable mới
sudo cp build/bin/edge_ai_api /usr/local/bin/edge_ai_api

# 4. Khởi động lại
sudo systemctl start edge-ai-api
```

Hoặc chạy lại script deploy:

```bash
sudo ./setup.sh --production
```

