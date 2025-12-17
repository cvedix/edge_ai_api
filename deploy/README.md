# Deploy Directory

Thư mục chứa các script và file cấu hình cho production deployment.

## Files Quan Trọng

### Scripts

- **`deploy.sh`** - Script chính để build và deploy production
  - Cài đặt dependencies
  - Build project
  - Tạo user và directories
  - Cài đặt executable và libraries
  - Cài đặt systemd service
  - Usage: `sudo ./deploy/deploy.sh [options]`

- **`create_directories.sh`** - Helper script tạo thư mục từ `directories.conf`
  - Được dùng bởi `deploy.sh`, `debian/rules`, `debian/postinst`

### Configuration Files

- **`directories.conf`** - Định nghĩa tất cả thư mục cần tạo
  - Single source of truth cho directory structure
  - Format: `["directory_name"]="permissions"`

- **`edge-ai-api.service`** - Systemd service file

## Quick Start

### Production Deployment

```bash
# Full deployment (recommended)
sudo ./deploy/deploy.sh

# Skip dependencies (if already installed)
sudo ./deploy/deploy.sh --skip-deps

# Skip build (use existing build)
sudo ./deploy/deploy.sh --skip-build

# Full permissions (777) - development only
sudo ./deploy/deploy.sh --full-permissions
```

## Deploy Options

- `--skip-deps` - Skip installing system dependencies
- `--skip-build` - Skip building project
- `--skip-fixes` - Skip fixing libraries/uploads/watchdog
- `--no-start` - Don't auto-start service
- `--full-permissions` - Use 777 permissions (development)
- `--standard-permissions` - Use 755 permissions (production, default)

---

## Directory Management System

Hệ thống quản lý thư mục tập trung, đảm bảo đồng bộ giữa development và production.

### Tổng quan

Tất cả các thư mục được định nghĩa trong một file duy nhất: `deploy/directories.conf`

File này được sử dụng bởi:
- ✅ `deploy/deploy.sh` - Production deployment
- ✅ `debian/rules` - Debian package build
- ✅ `debian/postinst` - Debian package installation
- ✅ `deploy/create_directories.sh` - Helper script

### Cấu trúc `directories.conf`

File định nghĩa tất cả thư mục cần tạo:

```bash
declare -A APP_DIRECTORIES=(
    ["instances"]="750"      # Instance configurations
    ["solutions"]="750"      # Custom solutions
    ["groups"]="750"         # Group configurations
    ["nodes"]="750"          # Pre-configured nodes
    ["models"]="750"         # Uploaded model files
    ["videos"]="750"        # Uploaded video files
    ["logs"]="750"          # Application logs
    ["data"]="750"          # Application data
    ["config"]="750"        # Configuration files
    ["fonts"]="750"         # Font files
    ["uploads"]="755"       # Uploaded files (public read)
    ["lib"]="755"           # Bundled libraries
)
```

**Format:** `["directory_name"]="permissions"`

**Permissions:**
- `750` = Restricted (chỉ user/group có quyền truy cập)
- `755` = Public read (mọi người có thể đọc, chỉ user/group có thể ghi)

### Helper Script `create_directories.sh`

Cung cấp 2 functions:

1. **`create_app_directories INSTALL_DIR [PROJECT_ROOT]`**
   - Tạo tất cả thư mục từ `directories.conf`
   - Áp dụng permissions tương ứng

2. **`get_directory_list [PROJECT_ROOT]`**
   - Trả về danh sách tên thư mục (dùng cho Makefile)

### Cách sử dụng

#### Thêm thư mục mới

1. Mở `deploy/directories.conf`
2. Thêm dòng mới:
   ```bash
   ["new_directory"]="750"
   ```
3. Tất cả script sẽ tự động sử dụng thư mục mới!

#### Sử dụng trong script mới

```bash
#!/bin/bash
source deploy/create_directories.sh
create_app_directories "/opt/edge_ai_api" "$(pwd)"
```

#### Sử dụng trong Makefile

```makefile
DIRS := $(shell bash -c 'source deploy/create_directories.sh; get_directory_list')
```

### Lợi ích

✅ **Single Source of Truth** - Chỉ cần sửa một file
✅ **Đồng bộ tự động** - Dev và production luôn giống nhau
✅ **Dễ bảo trì** - Không cần sửa nhiều file
✅ **Tránh lỗi** - Không còn thư mục lạ do typo
✅ **Consistent permissions** - Quyền được quản lý tập trung

### Migration

Nếu bạn đang có script cũ với danh sách thư mục hardcoded:

**Trước:**
```bash
mkdir -p "$INSTALL_DIR"/instances
mkdir -p "$INSTALL_DIR"/solutions
# ... nhiều dòng khác
```

**Sau:**
```bash
source deploy/create_directories.sh
create_app_directories "$INSTALL_DIR" "$PROJECT_ROOT"
```

### Troubleshooting

#### Script không tìm thấy directories.conf

Script sẽ tự động tìm file theo thứ tự:
1. `$PROJECT_ROOT/deploy/directories.conf`
2. `deploy/directories.conf` (relative to current dir)
3. Fallback to default directories

#### Permissions không đúng

Kiểm tra:
1. File `directories.conf` có format đúng không?
2. Script có quyền chạy không? (có thể cần `sudo`)
3. User/group có tồn tại không?
