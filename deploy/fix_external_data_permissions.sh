#!/bin/bash
# ============================================
# Fix External Data Directory Permissions
# ============================================
# 
# Script này fix quyền cho thư mục external data directory
# để phù hợp với user đang chạy ứng dụng
#
# Usage:
#   sudo ./deploy/fix_external_data_permissions.sh [--user USER] [--path PATH]
#
# ============================================

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
EXTERNAL_DATA_DIR="${EXTERNAL_DATA_DIR:-/mnt/sb1/data}"
TARGET_USER="${TARGET_USER:-}"
PERMISSION_MODE="${PERMISSION_MODE:-standard}"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --user)
            TARGET_USER="$2"
            shift 2
            ;;
        --path)
            EXTERNAL_DATA_DIR="$2"
            shift 2
            ;;
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
            echo "Usage: sudo $0 [--user USER] [--path PATH] [--full-permissions|--standard-permissions]"
            exit 1
            ;;
    esac
done

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo -e "${RED}Error: Script này cần chạy với quyền root (sudo)${NC}"
    echo "Usage: sudo $0 [--user USER] [--path PATH]"
    exit 1
fi

# Auto-detect user if not specified
if [ -z "$TARGET_USER" ]; then
    # Try to detect from running process
    RUNNING_USER=$(ps aux | grep -E "edge_ai_api|build/bin/edge_ai_api" | grep -v grep | awk '{print $1}' | head -1)
    if [ -n "$RUNNING_USER" ]; then
        TARGET_USER="$RUNNING_USER"
        echo -e "${BLUE}Auto-detected user from running process: $TARGET_USER${NC}"
    else
        # Fallback to current user (if running with sudo -u)
        TARGET_USER="${SUDO_USER:-$USER}"
        if [ -z "$TARGET_USER" ] || [ "$TARGET_USER" = "root" ]; then
            echo -e "${YELLOW}Warning: Không thể tự động phát hiện user${NC}"
            echo "Vui lòng chỉ định user với --user option"
            echo "Usage: sudo $0 --user cvedix --path /mnt/sb1/data"
            exit 1
        fi
        echo -e "${BLUE}Using user: $TARGET_USER${NC}"
    fi
fi

# Verify user exists
if ! id "$TARGET_USER" &>/dev/null; then
    echo -e "${RED}Error: User $TARGET_USER không tồn tại${NC}"
    exit 1
fi

echo -e "${BLUE}===========================================${NC}"
echo -e "${BLUE}Fix External Data Directory Permissions${NC}"
echo -e "${BLUE}===========================================${NC}"
echo ""
echo "Thư mục: $EXTERNAL_DATA_DIR"
echo "User: $TARGET_USER"
echo "Permission mode: $PERMISSION_MODE"
echo ""

# Check if directory exists
if [ ! -d "$EXTERNAL_DATA_DIR" ]; then
    echo -e "${YELLOW}Thư mục không tồn tại, đang tạo...${NC}"
    EXTERNAL_DATA_PARENT=$(dirname "$EXTERNAL_DATA_DIR")
    if [ ! -d "$EXTERNAL_DATA_PARENT" ]; then
        mkdir -p "$EXTERNAL_DATA_PARENT"
        echo -e "${GREEN}✓${NC} Đã tạo thư mục cha: $EXTERNAL_DATA_PARENT"
    fi
    mkdir -p "$EXTERNAL_DATA_DIR"
    echo -e "${GREEN}✓${NC} Đã tạo thư mục: $EXTERNAL_DATA_DIR"
fi

# Set ownership
echo -e "${BLUE}Thiết lập quyền sở hữu...${NC}"
chown -R "$TARGET_USER:$TARGET_USER" "$EXTERNAL_DATA_DIR"
echo -e "${GREEN}✓${NC} Đã set owner: $TARGET_USER:$TARGET_USER"

# Set permissions based on mode
if [ "$PERMISSION_MODE" = "full" ]; then
    chmod 777 "$EXTERNAL_DATA_DIR"
    echo -e "${GREEN}✓${NC} Đã cấp quyền 777 (drwxrwxrwx)"
    echo -e "${YELLOW}⚠${NC} Cảnh báo: Quyền 777 cho phép mọi người đọc/ghi"
else
    chmod 755 "$EXTERNAL_DATA_DIR"
    echo -e "${GREEN}✓${NC} Đã cấp quyền 755 (drwxr-xr-x)"
fi

# Verify write permission
echo ""
echo -e "${BLUE}Kiểm tra quyền ghi...${NC}"
if sudo -u "$TARGET_USER" test -w "$EXTERNAL_DATA_DIR"; then
    echo -e "${GREEN}✓${NC} User $TARGET_USER có quyền ghi vào $EXTERNAL_DATA_DIR"
else
    echo -e "${RED}✗${NC} User $TARGET_USER KHÔNG có quyền ghi vào $EXTERNAL_DATA_DIR"
    exit 1
fi

# Test write
TEST_FILE="$EXTERNAL_DATA_DIR/.write_test_$(date +%s)"
if sudo -u "$TARGET_USER" touch "$TEST_FILE" 2>/dev/null; then
    sudo -u "$TARGET_USER" rm -f "$TEST_FILE"
    echo -e "${GREEN}✓${NC} Test ghi file thành công"
else
    echo -e "${RED}✗${NC} Test ghi file thất bại"
    exit 1
fi

echo ""
echo -e "${GREEN}===========================================${NC}"
echo -e "${GREEN}✓ Hoàn tất!${NC}"
echo -e "${GREEN}===========================================${NC}"
echo ""
echo "Thông tin thư mục:"
ls -ld "$EXTERNAL_DATA_DIR"
echo ""
echo "Bây giờ bạn có thể sử dụng API với path: $EXTERNAL_DATA_DIR"

