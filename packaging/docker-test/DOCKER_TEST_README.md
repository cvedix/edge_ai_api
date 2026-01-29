# Hướng Dẫn Test Debian Package trong Docker

Tài liệu này hướng dẫn cách test cài đặt file `.deb` trong môi trường Docker container.

## 📋 Yêu Cầu

- Docker đã được cài đặt
- File `.deb` có sẵn: `edge-ai-api-with-sdk-2026.0.1.21-amd64.deb`

## 🚀 Cách Sử Dụng

**Lưu ý:** Tất cả các lệnh dưới đây cần chạy từ thư mục `packaging/docker-test/` hoặc từ project root với đường dẫn đầy đủ.

### Phương Pháp 1: Sử Dụng Script Tự Động (Khuyến Nghị)

```bash
# Từ project root
./packaging/docker-test/test_deb_docker.sh

# Hoặc từ thư mục docker-test
cd packaging/docker-test
./test_deb_docker.sh
```

Script sẽ hướng dẫn bạn qua các bước:
1. Build Docker image
2. Chạy container
3. Test cài đặt

### Phương Pháp 2: Sử Dụng Docker Compose

```bash
# Từ thư mục docker-test
cd packaging/docker-test

# Build image
docker-compose -f docker-compose.test.yml build

# Chạy container ở background
docker-compose -f docker-compose.test.yml up -d

# Vào container để test
docker-compose -f docker-compose.test.yml exec edge-ai-api-test bash

# Xem logs
docker-compose -f docker-compose.test.yml logs -f

# Dừng container
docker-compose -f docker-compose.test.yml down
```

### Phương Pháp 3: Sử Dụng Docker Run

```bash
# Từ project root
docker build -f packaging/docker-test/Dockerfile.test -t edge-ai-api-test:latest .

# Chạy container (interactive)
docker run -it --privileged --name edge-ai-api-test \
  -v /sys/fs/cgroup:/sys/fs/cgroup:ro \
  -p 8080:8080 \
  edge-ai-api-test:latest

# Hoặc chạy ở background
docker run -d --privileged --name edge-ai-api-test \
  -v /sys/fs/cgroup:/sys/fs/cgroup:ro \
  -p 8080:8080 \
  edge-ai-api-test:latest

# Vào container
docker exec -it edge-ai-api-test bash
```

## 🧪 Test Trong Container

Sau khi vào container, bạn có thể test:

### 1. Kiểm Tra Package Đã Cài Đặt

```bash
dpkg -l | grep edge-ai-api
```

### 2. Kiểm Tra Executable

```bash
ls -la /usr/local/bin/edge_ai_api
/usr/local/bin/edge_ai_api --help
```

### 3. Kiểm Tra Service File

```bash
cat /etc/systemd/system/edge-ai-api.service
ls -la /opt/edge_ai_api/
```

### 4. Test Chạy Trực Tiếp (Không Dùng Systemd)

```bash
# Chạy với các options
/usr/local/bin/edge_ai_api --help
/usr/local/bin/edge_ai_api --log-api

# Test API (từ container)
curl http://localhost:8080/v1/core/health
curl http://localhost:8080/v1/core/version
```

### 5. Test Với Systemd (Trong Container)

```bash
# Khởi động systemd (cần --privileged flag)
systemctl daemon-reload
systemctl start edge-ai-api
systemctl status edge-ai-api

# Xem logs
journalctl -u edge-ai-api -f
```

### 6. Test API Từ Host Machine

```bash
# Từ máy host (không phải trong container)
curl http://localhost:8080/v1/core/health
curl http://localhost:8080/v1/core/version
```

## 📝 Lưu Ý

### Systemd trong Docker

- Container cần chạy với `--privileged` flag để systemd hoạt động
- Cần mount `/sys/fs/cgroup` để systemd có thể quản lý services
- Nếu không cần test systemd, có thể chạy executable trực tiếp

### Dependencies

Dockerfile đã cài đặt tất cả runtime dependencies cần thiết:
- OpenCV
- GStreamer và plugins
- Mosquitto
- SSL, JSON, cURL libraries
- FFmpeg libraries

### Port Mapping

- Container expose port `8080` cho API
- Map sang host: `-p 8080:8080`
- Có thể đổi port nếu cần: `-p 9090:8080`

## 🧹 Dọn Dẹp

```bash
# Dừng và xóa container (từ thư mục docker-test)
cd packaging/docker-test
docker-compose -f docker-compose.test.yml down

# Hoặc với docker run
docker stop edge-ai-api-test
docker rm edge-ai-api-test

# Xóa image (nếu cần)
docker rmi edge-ai-api-test:latest
```

## 🔍 Troubleshooting

### Lỗi: "Cannot connect to Docker daemon"

```bash
# Kiểm tra Docker service
sudo systemctl status docker

# Khởi động Docker
sudo systemctl start docker

# Thêm user vào docker group (không cần sudo)
sudo usermod -aG docker $USER
# Logout và login lại
```

### Lỗi: "dpkg: error processing package"

```bash
# Trong container, fix dependencies
apt-get update
apt-get install -f -y
dpkg -i /tmp/edge-ai-api-with-sdk-2026.0.1.21-amd64.deb
```

### Lỗi: "systemd not running"

- Đảm bảo container chạy với `--privileged` flag
- Đảm bảo mount `/sys/fs/cgroup`
- Hoặc chạy executable trực tiếp thay vì dùng systemd

### Container không start

```bash
# Xem logs
docker logs edge-ai-api-test

# Hoặc với docker-compose
docker-compose -f docker-compose.test.yml logs
```

## 📚 Tài Liệu Thêm

- [Docker Documentation](https://docs.docker.com/)
- [Docker Compose Documentation](https://docs.docker.com/compose/)
- [Systemd in Docker](https://github.com/gdraheim/docker-systemd-replacement)

