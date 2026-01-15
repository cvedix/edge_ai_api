#!/bin/bash
# ============================================
# Fix All Symlinks for CVEDIX SDK
# ============================================
#
# Script này fix tất cả các symlink issues liên quan đến CVEDIX SDK:
# 1. libtinyexpr.so symlink
# 2. Cereal symlink
# 3. cpp-base64 symlink
# 4. OpenCV freetype compatibility
#
# Usage:
#   sudo ./scripts/fix_all_symlinks.sh
#
# ============================================

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}✗${NC} This script must be run as root (use sudo)"
    exit 1
fi

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Fix All Symlinks for CVEDIX SDK${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# ============================================
# 1. Fix libtinyexpr.so symlink
# ============================================
echo -e "${BLUE}[1/4]${NC} Fixing libtinyexpr.so symlink..."

CVEDIX_LIB_DIR="/opt/cvedix/lib"
TINYEXPR_SOURCE="$CVEDIX_LIB_DIR/libtinyexpr.so"
TINYEXPR_TARGET="/usr/lib/libtinyexpr.so"

if [ -f "$TINYEXPR_SOURCE" ]; then
    if [ -L "$TINYEXPR_TARGET" ] || [ -f "$TINYEXPR_TARGET" ]; then
        rm -f "$TINYEXPR_TARGET"
    fi
    ln -sf "$TINYEXPR_SOURCE" "$TINYEXPR_TARGET"
    echo -e "${GREEN}✓${NC} Created symlink: $TINYEXPR_TARGET -> $TINYEXPR_SOURCE"
else
    echo -e "${YELLOW}⚠${NC}  libtinyexpr.so not found at $TINYEXPR_SOURCE"
fi
echo ""

# ============================================
# 2. Fix Cereal symlink
# ============================================
echo -e "${BLUE}[2/4]${NC} Fixing Cereal symlink..."

# Try to find cereal in build directory first
CEREAL_SOURCE=""
if [ -d "$PROJECT_ROOT/build/_deps/cereal-src/include/cereal" ]; then
    CEREAL_SOURCE="$PROJECT_ROOT/build/_deps/cereal-src/include/cereal"
elif [ -d "/usr/include/cereal" ]; then
    CEREAL_SOURCE="/usr/include/cereal"
fi

if [ -n "$CEREAL_SOURCE" ]; then
    # Create symlink in /opt/cvedix/include/cvedix/third_party/cereal
    CVEDIX_CEREAL_DIR="/opt/cvedix/include/cvedix/third_party"
    mkdir -p "$CVEDIX_CEREAL_DIR"
    
    if [ -L "$CVEDIX_CEREAL_DIR/cereal" ] || [ -d "$CVEDIX_CEREAL_DIR/cereal" ]; then
        rm -rf "$CVEDIX_CEREAL_DIR/cereal"
    fi
    ln -sf "$CEREAL_SOURCE" "$CVEDIX_CEREAL_DIR/cereal"
    echo -e "${GREEN}✓${NC} Created symlink: $CVEDIX_CEREAL_DIR/cereal -> $CEREAL_SOURCE"
    
    # Also create in /usr/include/cvedix/third_party/cereal (for compatibility)
    USR_CEREAL_DIR="/usr/include/cvedix/third_party"
    mkdir -p "$USR_CEREAL_DIR"
    if [ -L "$USR_CEREAL_DIR/cereal" ] || [ -d "$USR_CEREAL_DIR/cereal" ]; then
        rm -rf "$USR_CEREAL_DIR/cereal"
    fi
    ln -sf "$CEREAL_SOURCE" "$USR_CEREAL_DIR/cereal"
    echo -e "${GREEN}✓${NC} Created symlink: $USR_CEREAL_DIR/cereal -> $CEREAL_SOURCE"
else
    echo -e "${YELLOW}⚠${NC}  Cereal not found. Run cmake to download dependencies first."
fi
echo ""

# ============================================
# 3. Fix cpp-base64 symlink
# ============================================
echo -e "${BLUE}[3/4]${NC} Fixing cpp-base64 symlink..."

# Try to find base64.h in build directory first
BASE64_SOURCE=""
if [ -f "$PROJECT_ROOT/build/_deps/cpp-base64-src/base64.h" ]; then
    BASE64_SOURCE="$PROJECT_ROOT/build/_deps/cpp-base64-src/base64.h"
elif [ -f "/usr/include/base64.h" ]; then
    BASE64_SOURCE="/usr/include/base64.h"
fi

if [ -n "$BASE64_SOURCE" ]; then
    # Create symlink in /opt/cvedix/include/cvedix/third_party/cpp_base64
    CVEDIX_BASE64_DIR="/opt/cvedix/include/cvedix/third_party/cpp_base64"
    mkdir -p "$CVEDIX_BASE64_DIR"
    
    if [ -L "$CVEDIX_BASE64_DIR/base64.h" ] || [ -f "$CVEDIX_BASE64_DIR/base64.h" ]; then
        rm -f "$CVEDIX_BASE64_DIR/base64.h"
    fi
    ln -sf "$BASE64_SOURCE" "$CVEDIX_BASE64_DIR/base64.h"
    echo -e "${GREEN}✓${NC} Created symlink: $CVEDIX_BASE64_DIR/base64.h -> $BASE64_SOURCE"
    
    # Also create in /usr/include/cvedix/third_party/cpp_base64 (for compatibility)
    USR_BASE64_DIR="/usr/include/cvedix/third_party/cpp_base64"
    mkdir -p "$USR_BASE64_DIR"
    if [ -L "$USR_BASE64_DIR/base64.h" ] || [ -f "$USR_BASE64_DIR/base64.h" ]; then
        rm -f "$USR_BASE64_DIR/base64.h"
    fi
    ln -sf "$BASE64_SOURCE" "$USR_BASE64_DIR/base64.h"
    echo -e "${GREEN}✓${NC} Created symlink: $USR_BASE64_DIR/base64.h -> $BASE64_SOURCE"
else
    echo -e "${YELLOW}⚠${NC}  base64.h not found. Run cmake to download dependencies first."
fi
echo ""

# ============================================
# 4. Fix OpenCV freetype compatibility
# ============================================
echo -e "${BLUE}[4/4]${NC} Fixing OpenCV freetype compatibility..."

FREETYPE_406="/usr/lib/x86_64-linux-gnu/libopencv_freetype.so.4.6.0"
FREETYPE_410_BUILD="$PROJECT_ROOT/build/lib/libopencv_freetype.so.410"

if [ -f "$FREETYPE_406" ]; then
    mkdir -p "$PROJECT_ROOT/build/lib"
    if [ -f "$FREETYPE_410_BUILD" ]; then
        rm -f "$FREETYPE_410_BUILD"
    fi
    cp "$FREETYPE_406" "$FREETYPE_410_BUILD"
    echo -e "${GREEN}✓${NC} Copied OpenCV freetype 4.6.0 to build/lib/libopencv_freetype.so.410"
else
    echo -e "${YELLOW}⚠${NC}  OpenCV freetype 4.6.0 not found at $FREETYPE_406"
fi
echo ""

# ============================================
# Summary
# ============================================
echo -e "${BLUE}========================================${NC}"
echo -e "${GREEN}✓${NC} All symlinks fixed!"
echo -e "${BLUE}========================================${NC}"
echo ""
echo "Next steps:"
echo "  1. Run: sudo ldconfig (to update library cache)"
echo "  2. Rebuild project if needed: cd build && cmake .. && make"
echo "  3. Test the application"
echo ""

