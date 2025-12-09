# Quick Start - Cài Đặt Thư Mục

## Cách Chạy Script

### Từ thư mục gốc project (`/home/cvedix/project/edge_ai_api`):

```bash
# Cấp quyền 777 (như cvedix-rt)
sudo ./deploy/install_directories.sh --full-permissions

# Cấp quyền 755 (như Tabby, google, nvidia) - mặc định
sudo ./deploy/install_directories.sh --standard-permissions
# hoặc đơn giản
sudo ./deploy/install_directories.sh
```

### Từ thư mục deploy:

```bash
cd deploy
sudo ./install_directories.sh --full-permissions
```

### Sử dụng đường dẫn tuyệt đối:

```bash
sudo /home/cvedix/project/edge_ai_api/deploy/install_directories.sh --full-permissions
```

## Kiểm Tra File Có Tồn Tại

```bash
# Kiểm tra file
ls -la deploy/install_directories.sh

# Kết quả mong đợi:
# -rwxrwxr-x 1 cvedix cvedix 6132 ... deploy/install_directories.sh
# (phải có 'x' - quyền thực thi)
```

## Nếu File Không Có Quyền Thực Thi

```bash
chmod +x deploy/install_directories.sh
```

## Các Script Có Sẵn

```bash
# 1. Cài đặt thư mục với tùy chọn quyền
sudo ./deploy/install_directories.sh [--full-permissions|--standard-permissions]

# 2. Cấp quyền 777 cho thư mục đã tồn tại
sudo ./deploy/set_full_permissions.sh

# 3. Build và deploy hoàn chỉnh
sudo ./deploy/build.sh [--full-permissions|--standard-permissions]
```

## Troubleshooting

### Lỗi: "Permission denied"
```bash
# Cấp quyền thực thi
chmod +x deploy/install_directories.sh
```

### Lỗi: "No such file or directory"
```bash
# Kiểm tra bạn đang ở đúng thư mục
pwd
# Phải là: /home/cvedix/project/edge_ai_api

# Hoặc dùng đường dẫn tuyệt đối
sudo /home/cvedix/project/edge_ai_api/deploy/install_directories.sh --full-permissions
```

### Lỗi: "Script này cần chạy với quyền root"
```bash
# Phải dùng sudo
sudo ./deploy/install_directories.sh --full-permissions
```

