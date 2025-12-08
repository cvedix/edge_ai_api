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
chmod 755 "$INSTALL_DIR"  # Base directory should be accessible

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

