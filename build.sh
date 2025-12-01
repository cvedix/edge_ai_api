#!/bin/bash
# ============================================
# Edge AI API - Complete Build & Deploy Script
# ============================================
# 
# Script này tổng hợp tất cả các bước:
# 1. Cài đặt dependencies hệ thống
# 2. Build project
# 3. Deploy production (user, directories, executable, libraries)
# 4. Fix uploads directory permissions
# 5. Fix watchdog configuration
# 6. Cài đặt và khởi động systemd service
#
# Usage:
#   sudo ./deploy/build.sh [options]
#   hoặc từ thư mục deploy:
#   sudo ./build.sh [options]
#
# Options:
#   --skip-deps      Skip cài đặt dependencies
#   --skip-build     Skip build (dùng build có sẵn)
#   --skip-fixes     Skip các bước fix (libraries, uploads, watchdog)
#   --no-start       Không tự động start service sau khi deploy
#
# ============================================

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Get script directory (script is in deploy/)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Configuration
SERVICE_USER="edgeai"
SERVICE_GROUP="edgeai"
INSTALL_DIR="/opt/edge_ai_api"
BIN_DIR="/usr/local/bin"
LIB_DIR="/usr/local/lib"
SERVICE_NAME="edge-ai-api"
SERVICE_FILE="${SERVICE_NAME}.service"

# Flags
SKIP_DEPS=false
SKIP_BUILD=false
SKIP_FIXES=false
NO_START=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --skip-deps)
            SKIP_DEPS=true
            shift
            ;;
        --skip-build)
            SKIP_BUILD=true
            shift
            ;;
        --skip-fixes)
            SKIP_FIXES=true
            shift
            ;;
        --no-start)
            NO_START=true
            shift
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            echo "Usage: sudo ./build.sh [--skip-deps] [--skip-build] [--skip-fixes] [--no-start]"
            exit 1
            ;;
    esac
done

echo "=========================================="
echo "Edge AI API - Complete Build & Deploy"
echo "=========================================="
echo ""
echo "Options:"
echo "  Skip dependencies: $SKIP_DEPS"
echo "  Skip build:        $SKIP_BUILD"
echo "  Skip fixes:        $SKIP_FIXES"
echo "  No auto-start:     $NO_START"
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo -e "${RED}Error: Script này cần chạy với quyền sudo${NC}"
    echo "Usage: sudo ./build.sh"
    exit 1
fi

# ============================================
# Step 1: Install System Dependencies
# ============================================
if [ "$SKIP_DEPS" = false ]; then
    echo -e "${BLUE}[1/6]${NC} Cài đặt system dependencies..."
    
    # Detect OS
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        OS=$ID
    else
        echo "Cannot detect OS. Assuming Ubuntu/Debian"
        OS="ubuntu"
    fi
    
    echo "Detected OS: $OS"
    echo ""
    
    # Install dependencies based on OS
    if [ "$OS" = "ubuntu" ] || [ "$OS" = "debian" ]; then
        echo "Installing dependencies for Ubuntu/Debian..."
        apt-get update
        apt-get install -y \
            build-essential \
            cmake \
            git \
            libssl-dev \
            zlib1g-dev \
            libjsoncpp-dev \
            uuid-dev \
            pkg-config
        
        echo -e "${GREEN}✓${NC} Dependencies installed successfully!"
        
    elif [ "$OS" = "centos" ] || [ "$OS" = "rhel" ] || [ "$OS" = "fedora" ]; then
        echo "Installing dependencies for CentOS/RHEL/Fedora..."
        
        if command -v dnf &> /dev/null; then
            dnf install -y \
                gcc-c++ \
                cmake \
                git \
                openssl-devel \
                zlib-devel \
                jsoncpp-devel \
                libuuid-devel \
                pkgconfig
        else
            yum install -y \
                gcc-c++ \
                cmake \
                git \
                openssl-devel \
                zlib-devel \
                jsoncpp-devel \
                libuuid-devel \
                pkgconfig
        fi
        
        echo -e "${GREEN}✓${NC} Dependencies installed successfully!"
    else
        echo -e "${YELLOW}⚠${NC}  Unknown OS. Please install dependencies manually"
    fi
    echo ""
else
    echo -e "${YELLOW}[1/6]${NC} Skip cài đặt dependencies"
    echo ""
fi

# ============================================
# Step 2: Build Project
# ============================================
if [ "$SKIP_BUILD" = false ]; then
    echo -e "${BLUE}[2/6]${NC} Build project..."
    cd "$PROJECT_ROOT" || exit 1
    
    if [ ! -d "build" ]; then
        echo "Tạo thư mục build..."
        mkdir -p build
    fi
    
    cd build
    
    if [ ! -f "CMakeCache.txt" ]; then
        echo "Chạy CMake..."
        cmake ..
    else
        echo "CMake đã được cấu hình, chỉ build..."
    fi
    
    echo "Build project (sử dụng tất cả CPU cores)..."
    make -j$(nproc)
    
    cd ..
    echo -e "${GREEN}✓${NC} Build hoàn tất!"
    echo ""
else
    echo -e "${YELLOW}[2/6]${NC} Skip build (dùng build có sẵn)"
    echo ""
fi

# ============================================
# Step 3: Create User and Directories
# ============================================
echo -e "${BLUE}[3/6]${NC} Tạo user và thư mục..."
cd "$PROJECT_ROOT" || exit 1

# Create user and group
if ! id "$SERVICE_USER" &>/dev/null; then
    useradd -r -s /bin/false -d "$INSTALL_DIR" "$SERVICE_USER"
    echo -e "${GREEN}✓${NC} Đã tạo user: $SERVICE_USER"
else
    echo -e "${GREEN}✓${NC} User $SERVICE_USER đã tồn tại"
fi

# Create installation directory
mkdir -p "$INSTALL_DIR"/{logs,data,config,uploads}
chown -R "$SERVICE_USER:$SERVICE_GROUP" "$INSTALL_DIR"
chmod 755 "$INSTALL_DIR"
chmod 750 "$INSTALL_DIR"/{logs,data,config}
chmod 755 "$INSTALL_DIR"/uploads
echo -e "${GREEN}✓${NC} Đã tạo thư mục: $INSTALL_DIR"
echo ""

# ============================================
# Step 4: Install Executable and Libraries
# ============================================
echo -e "${BLUE}[4/6]${NC} Cài đặt executable và libraries..."

# Find executable
EXECUTABLE=""
if [ -f "build/bin/edge_ai_api" ]; then
    EXECUTABLE="build/bin/edge_ai_api"
elif [ -f "build/edge_ai_api" ]; then
    EXECUTABLE="build/edge_ai_api"
else
    echo -e "${RED}Error: Không tìm thấy executable${NC}"
    echo "Vui lòng build project trước hoặc bỏ --skip-build"
    exit 1
fi

echo "Executable: $EXECUTABLE"

# Stop service if running (to avoid "Text file busy" error)
if systemctl is-active --quiet "${SERVICE_NAME}.service" 2>/dev/null; then
    echo "Dừng service để copy file mới..."
    systemctl stop "${SERVICE_NAME}.service" || true
    sleep 1
fi

# Copy executable
cp "$PROJECT_ROOT/$EXECUTABLE" "$BIN_DIR/edge_ai_api"
chmod +x "$BIN_DIR/edge_ai_api"
chown root:root "$BIN_DIR/edge_ai_api"
echo -e "${GREEN}✓${NC} Đã cài đặt: $BIN_DIR/edge_ai_api"

# Copy shared libraries
mkdir -p "$LIB_DIR"
LIB_SOURCE="$PROJECT_ROOT/build/lib"

if [ -d "$LIB_SOURCE" ]; then
    echo "Copy shared libraries..."
    cd "$LIB_SOURCE"
    
    # Copy all drogon, trantor, and jsoncpp libraries (including symlinks)
    for lib in libdrogon.so* libtrantor.so* libjsoncpp.so*; do
        if [ -e "$lib" ]; then
            cp -P "$lib" "$LIB_DIR/" 2>/dev/null || true
        fi
    done
    
    # Update library cache
    ldconfig
    echo -e "${GREEN}✓${NC} Đã cài đặt shared libraries vào $LIB_DIR"
else
    echo -e "${YELLOW}⚠${NC}  Thư mục build/lib không tồn tại. Bỏ qua copy libraries."
fi

cd "$PROJECT_ROOT"

# Copy configuration files
if [ -f "$PROJECT_ROOT/.env" ]; then
    cp "$PROJECT_ROOT/.env" "$INSTALL_DIR/config/.env"
    chown "$SERVICE_USER:$SERVICE_GROUP" "$INSTALL_DIR/config/.env"
    chmod 640 "$INSTALL_DIR/config/.env"
    echo -e "${GREEN}✓${NC} Đã copy .env"
else
    echo -e "${YELLOW}⚠${NC}  File .env không tồn tại. Bạn có thể tạo sau tại $INSTALL_DIR/config/.env"
fi

# Copy openapi.yaml file (required for Swagger UI)
if [ -f "$PROJECT_ROOT/openapi.yaml" ]; then
    cp "$PROJECT_ROOT/openapi.yaml" "$INSTALL_DIR/openapi.yaml"
    chown "$SERVICE_USER:$SERVICE_GROUP" "$INSTALL_DIR/openapi.yaml"
    chmod 644 "$INSTALL_DIR/openapi.yaml"
    echo -e "${GREEN}✓${NC} Đã copy openapi.yaml"
else
    echo -e "${YELLOW}⚠${NC}  File openapi.yaml không tồn tại. Swagger UI sẽ không hoạt động."
fi

echo ""

# ============================================
# Step 5: Fix Issues (Libraries, Uploads, Watchdog)
# ============================================
if [ "$SKIP_FIXES" = false ]; then
    echo -e "${BLUE}[5/6]${NC} Fix các vấn đề cấu hình..."
    
    # Fix uploads directory (already done in step 3, but ensure permissions)
    echo "Đảm bảo uploads directory có quyền đúng..."
    mkdir -p "$INSTALL_DIR/uploads"
    chown "$SERVICE_USER:$SERVICE_USER" "$INSTALL_DIR/uploads"
    chmod 755 "$INSTALL_DIR/uploads"
    echo -e "${GREEN}✓${NC} Uploads directory OK"
    
    # Fix watchdog in service file (if service file exists)
    if [ -f "/etc/systemd/system/${SERVICE_NAME}.service" ]; then
        echo "Fix watchdog configuration..."
        SERVICE_FILE="/etc/systemd/system/${SERVICE_NAME}.service"
        
        # Backup service file
        if [ ! -f "${SERVICE_FILE}.backup" ]; then
            cp "$SERVICE_FILE" "${SERVICE_FILE}.backup"
        fi
        
        # Comment out WatchdogSec and NotifyAccess if not already commented
        sed -i 's/^WatchdogSec=/#WatchdogSec=/' "$SERVICE_FILE" 2>/dev/null || true
        sed -i 's/^NotifyAccess=/#NotifyAccess=/' "$SERVICE_FILE" 2>/dev/null || true
        
        # Ensure ReadWritePaths includes uploads
        if ! grep -q "ReadWritePaths.*uploads" "$SERVICE_FILE"; then
            sed -i 's|ReadWritePaths=\([^ ]*\)|ReadWritePaths=\1 '"$INSTALL_DIR"'/uploads|g' "$SERVICE_FILE" 2>/dev/null || true
        fi
        
        echo -e "${GREEN}✓${NC} Service file đã được cập nhật"
    fi
    
    echo ""
else
    echo -e "${YELLOW}[5/6]${NC} Skip các bước fix"
    echo ""
fi

# ============================================
# Step 6: Install and Start Systemd Service
# ============================================
echo -e "${BLUE}[6/6]${NC} Cài đặt systemd service..."

if [ ! -f "$PROJECT_ROOT/$SERVICE_FILE" ]; then
    echo -e "${RED}Error: Service file không tồn tại: $SERVICE_FILE${NC}"
    exit 1
fi

# Update service file paths if needed
SERVICE_TEMP=$(mktemp)
sed "s|/opt/edge_ai_api|$INSTALL_DIR|g" "$PROJECT_ROOT/$SERVICE_FILE" > "$SERVICE_TEMP"

# Copy service file
cp "$SERVICE_TEMP" "/etc/systemd/system/${SERVICE_NAME}.service"
rm "$SERVICE_TEMP"
chmod 644 "/etc/systemd/system/${SERVICE_NAME}.service"

# Reload systemd
systemctl daemon-reload
echo -e "${GREEN}✓${NC} Đã cài đặt service: ${SERVICE_NAME}.service"

# Enable service (auto-start on boot)
systemctl enable "${SERVICE_NAME}.service"
echo -e "${GREEN}✓${NC} Đã enable service (sẽ tự động chạy khi khởi động)"

# Start or restart service
if [ "$NO_START" = false ]; then
    echo ""
    echo "Khởi động service..."
    if systemctl is-active --quiet "${SERVICE_NAME}.service" 2>/dev/null; then
        systemctl restart "${SERVICE_NAME}.service"
        echo -e "${GREEN}✓${NC} Đã restart service"
    else
        systemctl start "${SERVICE_NAME}.service"
        echo -e "${GREEN}✓${NC} Đã khởi động service"
    fi
    
    # Wait a moment for service to start
    sleep 2
    
    # Check service status
    echo ""
    echo "=========================================="
    echo "Kết quả triển khai:"
    echo "=========================================="
    if systemctl is-active --quiet "${SERVICE_NAME}.service"; then
        echo -e "${GREEN}✓ Service đang chạy thành công!${NC}"
        echo ""
        echo "Thông tin service:"
        systemctl status "${SERVICE_NAME}.service" --no-pager -l | head -n 10
    else
        echo -e "${RED}✗ Service không chạy. Kiểm tra log:${NC}"
        echo "  sudo journalctl -u ${SERVICE_NAME}.service -n 50"
        exit 1
    fi
else
    echo -e "${YELLOW}⚠${NC}  Service không được tự động khởi động (--no-start)"
    echo "  Để khởi động thủ công: sudo systemctl start ${SERVICE_NAME}.service"
fi

echo ""
echo "=========================================="
echo "Các lệnh hữu ích:"
echo "=========================================="
echo "  Xem trạng thái:    sudo systemctl status ${SERVICE_NAME}"
echo "  Xem log:           sudo journalctl -u ${SERVICE_NAME} -f"
echo "  Khởi động lại:     sudo systemctl restart ${SERVICE_NAME}"
echo "  Dừng service:      sudo systemctl stop ${SERVICE_NAME}"
echo "  Bắt đầu service:   sudo systemctl start ${SERVICE_NAME}"
echo ""
echo -e "${GREEN}=========================================="
echo "Build & Deploy hoàn tất!"
echo "==========================================${NC}"

