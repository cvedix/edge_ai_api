#!/bin/bash
# ============================================
# Fix cpp-base64 symlink for CVEDIX SDK
# ============================================
# 
# CVEDIX SDK headers include base64.h from relative path:
#   ../../third_party/cpp_base64/base64.h
# 
# This script creates the symlink structure so CVEDIX SDK can find base64.h
#
# Usage:
#   sudo ./scripts/fix_cpp_base64_symlink.sh [BASE64_PATH]
#
# If BASE64_PATH is not provided, it will try to find it in build directory
# ============================================

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo -e "${RED}Error: Script này cần chạy với quyền sudo${NC}"
    echo "Usage: sudo ./scripts/fix_cpp_base64_symlink.sh [BASE64_PATH]"
    exit 1
fi

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Determine base64.h path
if [ -n "$1" ]; then
    BASE64_PATH="$1"
else
    # Try to find in build directory
    if [ -f "$PROJECT_ROOT/build/_deps/cpp-base64-src/base64.h" ]; then
        BASE64_PATH="$PROJECT_ROOT/build/_deps/cpp-base64-src/base64.h"
    else
        echo -e "${RED}Error: Không tìm thấy base64.h${NC}"
        echo "Vui lòng build project trước hoặc cung cấp đường dẫn:"
        echo "  sudo ./scripts/fix_cpp_base64_symlink.sh /path/to/base64.h"
        exit 1
    fi
fi

if [ ! -f "$BASE64_PATH" ]; then
    echo -e "${RED}Error: File không tồn tại: $BASE64_PATH${NC}"
    exit 1
fi

echo "=========================================="
echo "Fix cpp-base64 symlink for CVEDIX SDK"
echo "=========================================="
echo "Base64.h path: $BASE64_PATH"
echo ""

# Check CVEDIX SDK installation location
CVEDIX_INCLUDE_DIR=""
if [ -d "/opt/cvedix/include/cvedix" ]; then
    CVEDIX_INCLUDE_DIR="/opt/cvedix/include/cvedix"
    echo "Found CVEDIX SDK in /opt/cvedix"
elif [ -d "/usr/include/cvedix" ]; then
    CVEDIX_INCLUDE_DIR="/usr/include/cvedix"
    echo "Found CVEDIX SDK in /usr/include"
else
    echo -e "${RED}Error: Không tìm thấy CVEDIX SDK${NC}"
    echo "CVEDIX SDK should be installed in /opt/cvedix or /usr/include"
    exit 1
fi

# Create directory structure
THIRD_PARTY_DIR="$CVEDIX_INCLUDE_DIR/third_party/cpp_base64"
echo "Creating directory: $THIRD_PARTY_DIR"
mkdir -p "$THIRD_PARTY_DIR"

# Create symlink
SYMLINK_PATH="$THIRD_PARTY_DIR/base64.h"
if [ -L "$SYMLINK_PATH" ]; then
    echo "Removing existing symlink..."
    rm "$SYMLINK_PATH"
fi

echo "Creating symlink: $SYMLINK_PATH -> $BASE64_PATH"
ln -sf "$BASE64_PATH" "$SYMLINK_PATH"

# Verify symlink
if [ -L "$SYMLINK_PATH" ] && [ -f "$SYMLINK_PATH" ]; then
    echo -e "${GREEN}✓${NC} Symlink created successfully!"
    echo ""
    echo "Symlink details:"
    ls -la "$SYMLINK_PATH"
    echo ""
    echo -e "${GREEN}=========================================="
    echo "cpp-base64 symlink fixed!"
    echo "==========================================${NC}"
else
    echo -e "${RED}Error: Không thể tạo symlink${NC}"
    exit 1
fi

