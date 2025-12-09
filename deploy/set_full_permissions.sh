#!/bin/bash
# ============================================
# Edge AI API - Set Full Permissions Script
# ============================================
# 
# Script này cấp quyền 777 (drwxrwxrwx) cho thư mục /opt/edge_ai_api
# Tương tự như cvedix-rt: drwxrwxrwx 15 root root 4096 Dec  8 11:02 cvedix-rt
#
# Usage:
#   sudo ./deploy/set_full_permissions.sh
#
# Lưu ý: Quyền 777 cho phép mọi người đọc/ghi, không an toàn cho production!
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
INSTALL_DIR="/opt/edge_ai_api"

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo -e "${RED}Error: Script này cần chạy với quyền root (sudo)${NC}"
    echo "Usage: sudo $0"
    exit 1
fi

echo -e "${BLUE}===========================================${NC}"
echo -e "${BLUE}Edge AI API - Set Full Permissions (777)${NC}"
echo -e "${BLUE}===========================================${NC}"
echo ""

# Check if directory exists
if [ ! -d "$INSTALL_DIR" ]; then
    echo -e "${RED}Error: Thư mục $INSTALL_DIR không tồn tại${NC}"
    echo "Hãy chạy script cài đặt trước: sudo ./deploy/install_directories.sh"
    exit 1
fi

echo -e "${YELLOW}⚠ CẢNH BÁO:${NC}"
echo "  - Quyền 777 (drwxrwxrwx) cho phép MỌI NGƯỜI đọc/ghi vào thư mục"
echo "  - Không an toàn cho production environment"
echo "  - Chỉ nên dùng cho development hoặc môi trường nội bộ"
echo ""
read -p "Bạn có chắc chắn muốn tiếp tục? (yes/no): " confirm

if [ "$confirm" != "yes" ]; then
    echo "Đã hủy."
    exit 0
fi

echo ""
echo -e "${BLUE}Đang cấp quyền 777 cho $INSTALL_DIR...${NC}"

# Set full permissions recursively
chmod -R 777 "$INSTALL_DIR"

echo -e "${GREEN}✓${NC} Đã cấp quyền 777 cho tất cả thư mục và file trong: $INSTALL_DIR"
echo ""

# Show current permissions
echo -e "${BLUE}Kiểm tra quyền hiện tại:${NC}"
ls -ld "$INSTALL_DIR"
echo ""
echo "Các thư mục con:"
ls -la "$INSTALL_DIR" | head -10
echo ""

echo -e "${GREEN}===========================================${NC}"
echo -e "${GREEN}✓ Hoàn tất!${NC}"
echo -e "${GREEN}===========================================${NC}"
echo ""
echo -e "${YELLOW}Lưu ý:${NC}"
echo "  - Quyền hiện tại: 777 (drwxrwxrwx) - tương tự cvedix-rt"
echo "  - Để quay lại quyền chuẩn (755), chạy:"
echo "    sudo ./deploy/install_directories.sh --standard-permissions"
echo ""

