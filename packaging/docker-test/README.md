# Docker Test cho Debian Package

Thư mục này chứa các file Docker để test cài đặt Debian package (`.deb`) trong môi trường container.

## 📁 Cấu Trúc

```
packaging/docker-test/
├── README.md                    # File này
├── DOCKER_TEST_README.md        # Hướng dẫn chi tiết
├── Dockerfile.test              # Dockerfile với systemd support
├── Dockerfile.test.simple       # Dockerfile đơn giản (không systemd)
├── docker-compose.test.yml      # Docker Compose configuration
└── test_deb_docker.sh           # Script tự động để test
```

## 🚀 Quick Start

```bash
# Từ project root
./packaging/docker-test/test_deb_docker.sh

# Hoặc từ thư mục này
cd packaging/docker-test
./test_deb_docker.sh
```

## 📚 Tài Liệu

Xem [DOCKER_TEST_README.md](DOCKER_TEST_README.md) để biết hướng dẫn chi tiết.

## 📝 Lưu Ý

- File `.deb` cần có sẵn ở project root: `edge-ai-api-with-sdk-2026.0.1.21-amd64.deb`
- Docker build context là project root để có thể copy file `.deb`
- Container cần `--privileged` flag để systemd hoạt động (nếu dùng `Dockerfile.test`)





