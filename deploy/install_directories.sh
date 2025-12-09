#!/bin/bash
# ============================================
# Edge AI API - Install Directories Script
# ============================================
# 
# Script này tạo các thư mục cần thiết cho Edge AI API
# với quyền phù hợp cho production deployment.
#
# Usage:
#   sudo ./deploy/install_directories.sh
#
# ============================================

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
SERVICE_USER="edgeai"
SERVICE_GROUP="edgeai"
INSTALL_DIR="/opt/edge_ai_api"

# Permission mode: "standard" (755) or "full" (777)
# Standard: drwxr-xr-x (như Tabby, google, nvidia)
# Full: drwxrwxrwx (như cvedix-rt) - cho phép mọi người đọc/ghi
PERMISSION_MODE="${PERMISSION_MODE:-standard}"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --full-permissions|--full)
            PERMISSION_MODE="full"
            shift
            ;;
        --standard-permissions|--standard)
            PERMISSION_MODE="standard"
            shift
            ;;
        *)
            echo -e "${YELLOW}Unknown option: $1${NC}"
            echo "Usage: sudo $0 [--full-permissions|--standard-permissions]"
            exit 1
            ;;
    esac
done

# Load directory configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DIRS_CONF="$SCRIPT_DIR/directories.conf"
if [ -f "$DIRS_CONF" ]; then
    source "$DIRS_CONF"
else
    # Fallback: Default directory configuration
    declare -A APP_DIRECTORIES=(
        ["instances"]="750"      # Instance configurations (instances.json)
        ["solutions"]="750"     # Custom solutions
        ["groups"]="750"        # Group configurations
        ["models"]="750"        # Uploaded model files
        ["logs"]="750"          # Application logs
        ["data"]="750"          # Application data
        ["config"]="750"        # Configuration files
        ["uploads"]="755"       # Uploaded files (may need public access)
    )
fi

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo -e "${RED}Error: Script này cần chạy với quyền root (sudo)${NC}"
    echo "Usage: sudo $0"
    exit 1
fi

echo -e "${BLUE}===========================================${NC}"
echo -e "${BLUE}Edge AI API - Install Directories${NC}"
echo -e "${BLUE}===========================================${NC}"
echo ""

# Check if user exists
if ! id "$SERVICE_USER" &>/dev/null; then
    echo -e "${YELLOW}Warning: User $SERVICE_USER chưa tồn tại${NC}"
    echo "Tạo user $SERVICE_USER..."
    if useradd -r -s /bin/false -d "$INSTALL_DIR" "$SERVICE_USER" 2>/dev/null; then
        echo -e "${GREEN}✓${NC} Đã tạo user: $SERVICE_USER"
    else
        echo -e "${RED}Error: Không thể tạo user $SERVICE_USER${NC}"
        exit 1
    fi
else
    echo -e "${GREEN}✓${NC} User $SERVICE_USER đã tồn tại"
fi

# Ensure group exists
if ! getent group "$SERVICE_GROUP" > /dev/null 2>&1; then
    if groupadd -r "$SERVICE_GROUP" 2>/dev/null; then
        echo -e "${GREEN}✓${NC} Đã tạo group: $SERVICE_GROUP"
    fi
    usermod -a -G "$SERVICE_GROUP" "$SERVICE_USER" 2>/dev/null || true
fi

# Create all required directories
echo ""
echo -e "${BLUE}Tạo thư mục...${NC}"
mkdir -p "$INSTALL_DIR"

# Create all directories from configuration
for dir_name in "${!APP_DIRECTORIES[@]}"; do
    dir_path="$INSTALL_DIR/$dir_name"
    dir_perms="${APP_DIRECTORIES[$dir_name]}"
    mkdir -p "$dir_path"
    chmod "$dir_perms" "$dir_path"
    echo -e "${GREEN}✓${NC} Đã tạo: $dir_path (permissions: $dir_perms)"
done

# Set ownership
echo -e "${BLUE}Thiết lập quyền sở hữu...${NC}"
chown -R "$SERVICE_USER:$SERVICE_GROUP" "$INSTALL_DIR"

# Set base directory permissions based on mode
if [ "$PERMISSION_MODE" = "full" ]; then
    chmod 777 "$INSTALL_DIR"  # Full permissions: drwxrwxrwx (như cvedix-rt)
    echo -e "${YELLOW}⚠${NC} Đang sử dụng FULL PERMISSIONS (777) - mọi người có thể đọc/ghi"
    echo -e "${YELLOW}⚠${NC} Cảnh báo: Quyền 777 không an toàn cho production!"
else
    chmod 755 "$INSTALL_DIR"  # Standard permissions: drwxr-xr-x (như Tabby, google, nvidia)
    echo -e "${GREEN}✓${NC} Đang sử dụng STANDARD PERMISSIONS (755)"
fi

# Apply permissions to subdirectories based on mode
if [ "$PERMISSION_MODE" = "full" ]; then
    echo -e "${BLUE}Áp dụng quyền 777 cho tất cả thư mục con...${NC}"
    chmod -R 777 "$INSTALL_DIR"
else
    # Keep individual directory permissions from configuration
    for dir_name in "${!APP_DIRECTORIES[@]}"; do
        dir_path="$INSTALL_DIR/$dir_name"
        dir_perms="${APP_DIRECTORIES[$dir_name]}"
        chmod "$dir_perms" "$dir_path"
    done
fi

echo ""
echo -e "${GREEN}===========================================${NC}"
echo -e "${GREEN}✓ Hoàn tất!${NC}"
echo -e "${GREEN}===========================================${NC}"
echo ""
echo "Các thư mục đã được tạo:"
for dir_name in "${!APP_DIRECTORIES[@]}"; do
    echo "  - $INSTALL_DIR/$dir_name (${APP_DIRECTORIES[$dir_name]})"
done
echo ""
echo "Quyền sở hữu: $SERVICE_USER:$SERVICE_GROUP"
echo ""
echo -e "${YELLOW}Lưu ý:${NC}"
echo "  - Tất cả thư mục được tạo từ cấu hình tập trung"
echo "  - Để thêm thư mục mới, chỉ cần cập nhật APP_DIRECTORIES"
echo "  - Service sẽ tự động có quyền ghi vào các thư mục này"
echo ""
if [ "$PERMISSION_MODE" = "full" ]; then
    echo -e "${YELLOW}Quyền hiện tại: FULL (777)${NC}"
    echo "  - Tương tự như: cvedix-rt (drwxrwxrwx)"
    echo "  - Mọi người có thể đọc/ghi vào thư mục"
else
    echo -e "${GREEN}Quyền hiện tại: STANDARD (755)${NC}"
    echo "  - Tương tự như: Tabby, google, nvidia (drwxr-xr-x)"
    echo "  - Chỉ owner/group có quyền ghi"
fi
echo ""
echo "Để thay đổi quyền sau khi cài đặt:"
echo "  sudo $0 --full-permissions    # Cấp quyền 777"
echo "  sudo $0 --standard-permissions # Cấp quyền 755 (mặc định)"
echo ""

