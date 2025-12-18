# Scripts Documentation

Tài liệu tổng hợp về tất cả các script trong project Edge AI API.

## 📋 Tổng Quan

Project có 3 loại script chính:

1. **Development Setup** - Setup môi trường phát triển
2. **Production Setup** - Deploy production với systemd service
3. **Build Package** - Build Debian package (.deb)

## 🚀 Scripts Chính

### 1. Development Setup

**Script:** `scripts/dev_setup.sh`

**Mục đích:** Setup môi trường development từ đầu đến cuối

**Chức năng:**
- Cài đặt system dependencies
- Fix symlinks (CVEDIX SDK, Cereal, cpp-base64, OpenCV)
- Build project
- Setup face database (optional, với flag `--setup-face-db`)

**Usage:**
```bash
# Full setup (mặc định - setup tất cả)
./scripts/dev_setup.sh

# Full setup với face database (cần sudo)
sudo ./scripts/dev_setup.sh --all

# Skip dependencies
./scripts/dev_setup.sh --skip-deps

# Only build
./scripts/dev_setup.sh --build-only
```

**Options:**
- `--all, --full-setup` - Setup everything including face database (requires sudo) - mặc định
- `--skip-deps` - Skip installing dependencies
- `--skip-symlinks` - Skip fixing symlinks
- `--skip-build` - Skip building
- `--build-only` - Only build, skip other steps
- `--setup-face-db` - Setup face database permissions (requires sudo)

**Sau khi setup, chạy server:**
```bash
./scripts/load_env.sh
```

---

### 2. Production Setup

**Script:** `scripts/prod_setup.sh` (symlink đến `deploy/deploy.sh`)

**Mục đích:** Deploy production với systemd service

**Chức năng:**
- Cài đặt system dependencies
- Build project
- Tạo user và directories
- Cài đặt executable và libraries
- Setup face database permissions
- Cài đặt và khởi động systemd service

**Usage:**
```bash
# Full deployment (mặc định - setup tất cả, cần sudo)
sudo ./scripts/prod_setup.sh

# Hoặc sử dụng deploy script trực tiếp
sudo ./deploy/deploy.sh

# Explicit full setup
sudo ./scripts/prod_setup.sh --all

# Skip dependencies
sudo ./scripts/prod_setup.sh --skip-deps

# Skip build
sudo ./scripts/prod_setup.sh --skip-build
```

**Options:**
- `--all, --full-setup` - Setup everything (deps, build, fixes, service, face database) - mặc định
- `--skip-deps` - Skip installing system dependencies
- `--skip-build` - Skip building project
- `--skip-fixes` - Skip fixing libraries/uploads/watchdog
- `--no-start` - Don't auto-start service
- `--full-permissions` - Use 777 permissions (development only)
- `--standard-permissions` - Use 755 permissions (production, default)

**Sau khi deploy:**
```bash
# Kiểm tra service
sudo systemctl status edge-ai-api

# Xem log
sudo journalctl -u edge-ai-api -f
```

---

### 3. Build Debian Package

**Script:** `packaging/scripts/build_deb.sh`

**Mục đích:** Build file .deb package tự chứa tất cả dependencies

**Chức năng:**
- Kiểm tra build dependencies
- Build project
- Bundle libraries
- Tạo file .deb

**Usage:**
```bash
# Build package
./packaging/scripts/build_deb.sh

# Clean build
./packaging/scripts/build_deb.sh --clean

# Skip build (use existing)
./packaging/scripts/build_deb.sh --no-build

# Custom version
./packaging/scripts/build_deb.sh --version 1.0.0
```

**Options:**
- `--clean` - Clean build directory before building
- `--no-build` - Skip build (use existing build)
- `--version VER` - Set package version

**Sau khi build:**
```bash
# Cài đặt package
sudo dpkg -i edge-ai-api-*.deb

# Khởi động service
sudo systemctl start edge-ai-api
```

Xem chi tiết: [packaging/docs/BUILD_DEB.md](../packaging/docs/BUILD_DEB.md)

---

## 🔧 Utility Scripts

### `scripts/load_env.sh`

Load environment variables từ `.env` và chạy server.

```bash
# Load và chạy server
./scripts/load_env.sh

# Chỉ load env (cho current shell)
source ./scripts/load_env.sh --load-only
```

### `scripts/create_directories.sh`

Tạo thư mục từ `deploy/directories.conf`.

```bash
# Tạo thư mục với permissions từ config
./scripts/create_directories.sh /opt/edge_ai_api

# Tạo với full permissions
./scripts/create_directories.sh /opt/edge_ai_api --full-permissions
```

### `scripts/utils.sh setup-face-db`

Setup face database permissions.

```bash
# Standard permissions (644)
sudo ./scripts/utils.sh setup-face-db --standard-permissions

# Full permissions (666)
sudo ./scripts/utils.sh setup-face-db --full-permissions
```

### `scripts/utils.sh`

Utility commands.

```bash
# Run tests
./scripts/utils.sh test

# Generate solution template
./scripts/utils.sh generate-solution

# Restore default solutions
./scripts/utils.sh restore-solutions

# Setup face database (requires sudo)
sudo ./scripts/utils.sh setup-face-db
sudo ./scripts/utils.sh setup-face-db --full-permissions
```

---

## 📊 So Sánh Scripts

| Script | Mục đích | Cần sudo? | Chạy server? | Setup service? |
|--------|----------|-----------|--------------|---------------|
| `dev_setup.sh` | Development setup | Một phần (deps) | ❌ | ❌ |
| `prod_setup.sh` | Production deploy | ✅ | ✅ | ✅ |
| `build_deb.sh` | Build package | ❌ | ❌ | ❌ |

---

## 🗂️ Cấu Trúc Thư Mục

```
edge_ai_api/
├── scripts/              # Development scripts
│   ├── dev_setup.sh      # Development setup
│   ├── prod_setup.sh     # Production setup (symlink)
│   ├── load_env.sh       # Load env và chạy server
│   ├── create_directories.sh
│   └── utils.sh
├── deploy/               # Production deployment
│   ├── deploy.sh         # Production deploy script
│   ├── directories.conf  # Directory configuration
│   └── edge-ai-api.service
└── packaging/            # Package building
    └── scripts/
        └── build_deb.sh  # Build .deb package
```

---

## 🔄 Workflow Đề Xuất

### Development

```bash
# 1. Setup môi trường
./scripts/dev_setup.sh

# 2. Chạy server
./scripts/load_env.sh
```

### Production

```bash
# 1. Deploy
sudo ./scripts/prod_setup.sh

# 2. Kiểm tra
sudo systemctl status edge-ai-api
```

### Build Package

```bash
# 1. Build package
./packaging/scripts/build_deb.sh

# 2. Cài đặt
sudo dpkg -i edge-ai-api-*.deb
```

---

## 📚 Xem Thêm

- [deploy/README.md](../deploy/README.md) - Production deployment guide
- [packaging/docs/BUILD_DEB.md](../packaging/docs/BUILD_DEB.md) - Build package guide
- [docs/DEVELOPMENT.md](DEVELOPMENT.md) - Development guide
- [README.md](../README.md) - Project overview

