#!/bin/bash
# ============================================
# Edge AI API - Fix All Symlinks Script
# ============================================
# 
# Script này tự động fix tất cả symlinks cần thiết cho CVEDIX SDK:
# 1. CVEDIX SDK libraries (libtinyexpr.so, libcvedix_instance_sdk.so)
# 2. Cereal library symlink
# 3. cpp-base64 library symlink
# 4. OpenCV 4.10 symlinks (from OpenCV 4.6.0 if available)
#
# Usage:
#   sudo ./scripts/fix_all_symlinks.sh
#
# ============================================

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo -e "${RED}Error: Script này cần chạy với quyền sudo${NC}"
    echo "Usage: sudo ./scripts/fix_all_symlinks.sh"
    exit 1
fi

echo "=========================================="
echo "Edge AI API - Fix All Symlinks"
echo "=========================================="
echo ""

# Track results
FIXED_LIBS=0
FIXED_CEREAL=0
FIXED_BASE64=0
FIXED_OPENCV=0
SKIPPED=0

# ============================================
# Step 1: Fix CVEDIX SDK Libraries
# ============================================
echo -e "${BLUE}[1/4]${NC} Fixing CVEDIX SDK library symlinks..."

CVEDIX_LIB_DIR="/opt/cvedix/lib"
TARGET_LIB_DIR="/usr/lib"

if [ -d "$CVEDIX_LIB_DIR" ]; then
    LIBS=("libtinyexpr.so" "libcvedix_instance_sdk.so")
    
    for lib in "${LIBS[@]}"; do
        SOURCE="${CVEDIX_LIB_DIR}/${lib}"
        TARGET="${TARGET_LIB_DIR}/${lib}"
        
        if [ ! -f "$SOURCE" ]; then
            echo -e "${YELLOW}⚠${NC}  Source not found: $SOURCE"
            continue
        fi
        
        if [ -L "$TARGET" ]; then
            CURRENT_LINK=$(readlink -f "$TARGET")
            if [ "$CURRENT_LINK" = "$SOURCE" ]; then
                echo -e "${GREEN}✓${NC} Already correct: $lib"
                SKIPPED=$((SKIPPED + 1))
            else
                echo "Fixing symlink: $lib"
                rm -f "$TARGET"
                ln -sf "$SOURCE" "$TARGET"
                echo -e "${GREEN}✓${NC} Fixed: $lib"
                FIXED_LIBS=$((FIXED_LIBS + 1))
            fi
        elif [ -f "$TARGET" ]; then
            echo "Backing up and creating symlink: $lib"
            mv "$TARGET" "${TARGET}.backup"
            ln -sf "$SOURCE" "$TARGET"
            echo -e "${GREEN}✓${NC} Fixed: $lib (backup saved)"
            FIXED_LIBS=$((FIXED_LIBS + 1))
        else
            echo "Creating symlink: $lib"
            ln -sf "$SOURCE" "$TARGET"
            echo -e "${GREEN}✓${NC} Created: $lib"
            FIXED_LIBS=$((FIXED_LIBS + 1))
        fi
    done
else
    echo -e "${YELLOW}⚠${NC}  CVEDIX library directory not found: $CVEDIX_LIB_DIR"
    echo "  Skipping library symlinks..."
fi

echo ""

# ============================================
# Step 2: Fix Cereal Symlink
# ============================================
echo -e "${BLUE}[2/4]${NC} Fixing cereal symlink..."

BUILD_DIR="$PROJECT_ROOT/build"
CEREAL_SRC="${BUILD_DIR}/_deps/cereal-src/include/cereal"
CEREAL_DEST="/usr/include/cvedix/third_party/cereal"

# Check if cereal source exists
if [ -d "$CEREAL_SRC" ]; then
    # Create directory if needed
    if [ ! -d "$(dirname "$CEREAL_DEST")" ]; then
        mkdir -p "$(dirname "$CEREAL_DEST")"
    fi
    
    if [ ! -e "$CEREAL_DEST" ]; then
        echo "Creating cereal symlink..."
        ln -sf "$CEREAL_SRC" "$CEREAL_DEST"
        echo -e "${GREEN}✓${NC} Created cereal symlink"
        FIXED_CEREAL=1
    else
        CURRENT_LINK=$(readlink -f "$CEREAL_DEST" 2>/dev/null || echo "")
        if [ "$CURRENT_LINK" = "$CEREAL_SRC" ]; then
            echo -e "${GREEN}✓${NC} Cereal symlink already correct"
            SKIPPED=$((SKIPPED + 1))
        else
            echo "Updating cereal symlink..."
            rm -f "$CEREAL_DEST"
            ln -sf "$CEREAL_SRC" "$CEREAL_DEST"
            echo -e "${GREEN}✓${NC} Updated cereal symlink"
            FIXED_CEREAL=1
        fi
    fi
else
    echo -e "${YELLOW}⚠${NC}  Cereal source not found: $CEREAL_SRC"
    echo "  Run 'cmake ..' first to download cereal"
fi

echo ""

# ============================================
# Step 3: Fix cpp-base64 Symlink
# ============================================
echo -e "${BLUE}[3/4]${NC} Fixing cpp-base64 symlink..."

BASE64_PATH=""
if [ -f "$PROJECT_ROOT/build/_deps/cpp-base64-src/base64.h" ]; then
    BASE64_PATH="$PROJECT_ROOT/build/_deps/cpp-base64-src/base64.h"
fi

if [ -n "$BASE64_PATH" ] && [ -f "$BASE64_PATH" ]; then
    # Check CVEDIX SDK installation location
    CVEDIX_INCLUDE_DIR=""
    if [ -d "/opt/cvedix/include/cvedix" ]; then
        CVEDIX_INCLUDE_DIR="/opt/cvedix/include/cvedix"
    elif [ -d "/usr/include/cvedix" ]; then
        CVEDIX_INCLUDE_DIR="/usr/include/cvedix"
    fi
    
    if [ -n "$CVEDIX_INCLUDE_DIR" ]; then
        THIRD_PARTY_DIR="$CVEDIX_INCLUDE_DIR/third_party/cpp_base64"
        SYMLINK_PATH="$THIRD_PARTY_DIR/base64.h"
        
        mkdir -p "$THIRD_PARTY_DIR"
        
        if [ -L "$SYMLINK_PATH" ]; then
            CURRENT_LINK=$(readlink -f "$SYMLINK_PATH" 2>/dev/null || echo "")
            if [ "$CURRENT_LINK" = "$BASE64_PATH" ]; then
                echo -e "${GREEN}✓${NC} cpp-base64 symlink already correct"
                SKIPPED=$((SKIPPED + 1))
            else
                echo "Updating cpp-base64 symlink..."
                rm -f "$SYMLINK_PATH"
                ln -sf "$BASE64_PATH" "$SYMLINK_PATH"
                echo -e "${GREEN}✓${NC} Updated cpp-base64 symlink"
                FIXED_BASE64=1
            fi
        else
            echo "Creating cpp-base64 symlink..."
            ln -sf "$BASE64_PATH" "$SYMLINK_PATH"
            echo -e "${GREEN}✓${NC} Created cpp-base64 symlink"
            FIXED_BASE64=1
        fi
    else
        echo -e "${YELLOW}⚠${NC}  CVEDIX SDK not found in /opt/cvedix or /usr/include"
    fi
else
    echo -e "${YELLOW}⚠${NC}  cpp-base64 source not found"
    echo "  Run 'cmake ..' first to download cpp-base64"
fi

echo ""

# ============================================
# Step 4: Fix OpenCV 4.10 Symlinks
# ============================================
echo -e "${BLUE}[4/4]${NC} Fixing OpenCV 4.10 symlinks..."

OPENCV_LIBS=(
    "libopencv_dnn.so"
    "libopencv_core.so"
    "libopencv_imgproc.so"
    "libopencv_imgcodecs.so"
    "libopencv_videoio.so"
    "libopencv_highgui.so"
    "libopencv_video.so"
    "libopencv_objdetect.so"
    "libopencv_freetype.so"
)

OPENCV_LIB_DIR="/usr/lib/x86_64-linux-gnu"
OPENCV_FIXED_COUNT=0
OPENCV_SKIPPED_COUNT=0

echo -e "${YELLOW}Note:${NC} This creates symlinks from OpenCV 4.6.0 to 4.10 (workaround)"
echo -e "${YELLOW}Warning:${NC} This may not work due to ABI differences between versions"
echo ""

for lib in "${OPENCV_LIBS[@]}"; do
    SOURCE="${OPENCV_LIB_DIR}/${lib}.406"
    TARGET="${OPENCV_LIB_DIR}/${lib}.410"
    
    if [ ! -f "$SOURCE" ]; then
        echo -e "${YELLOW}⚠${NC}  Source not found: $SOURCE (skipping $lib)"
        continue
    fi
    
    if [ -L "$TARGET" ]; then
        CURRENT_LINK=$(readlink -f "$TARGET" 2>/dev/null || echo "")
        if [ "$CURRENT_LINK" = "$SOURCE" ]; then
            echo -e "${GREEN}✓${NC} Already correct: ${lib}.410"
            OPENCV_SKIPPED_COUNT=$((OPENCV_SKIPPED_COUNT + 1))
        else
            echo "Fixing symlink: ${lib}.410"
            rm -f "$TARGET"
            ln -sf "$SOURCE" "$TARGET"
            echo -e "${GREEN}✓${NC} Fixed: ${lib}.410"
            OPENCV_FIXED_COUNT=$((OPENCV_FIXED_COUNT + 1))
        fi
    elif [ -f "$TARGET" ]; then
        echo "Backing up and creating symlink: ${lib}.410"
        mv "$TARGET" "${TARGET}.backup"
        ln -sf "$SOURCE" "$TARGET"
        echo -e "${GREEN}✓${NC} Fixed: ${lib}.410 (backup saved)"
        OPENCV_FIXED_COUNT=$((OPENCV_FIXED_COUNT + 1))
    else
        echo "Creating symlink: ${lib}.410"
        ln -sf "$SOURCE" "$TARGET"
        echo -e "${GREEN}✓${NC} Created: ${lib}.410"
        OPENCV_FIXED_COUNT=$((OPENCV_FIXED_COUNT + 1))
    fi
done

if [ $OPENCV_FIXED_COUNT -gt 0 ] || [ $OPENCV_SKIPPED_COUNT -gt 0 ]; then
    FIXED_OPENCV=1
    SKIPPED=$((SKIPPED + OPENCV_SKIPPED_COUNT))
    echo ""
    echo -e "${GREEN}✓${NC} OpenCV symlinks: $OPENCV_FIXED_COUNT fixed, $OPENCV_SKIPPED_COUNT already correct"
else
    echo -e "${YELLOW}⚠${NC}  No OpenCV 4.6.0 libraries found. OpenCV 4.10 may need to be installed separately."
fi

echo ""

# ============================================
# Summary
# ============================================
echo "=========================================="
echo "Summary:"
echo "  CVEDIX libraries fixed: $FIXED_LIBS"
echo "  Cereal fixed:            $FIXED_CEREAL"
echo "  cpp-base64 fixed:        $FIXED_BASE64"
echo "  OpenCV symlinks fixed:   $FIXED_OPENCV"
echo "  Already correct:         $SKIPPED"
echo "=========================================="
echo ""

TOTAL_FIXED=$((FIXED_LIBS + FIXED_CEREAL + FIXED_BASE64 + FIXED_OPENCV))

if [ $TOTAL_FIXED -gt 0 ] || [ $SKIPPED -gt 0 ]; then
    echo -e "${GREEN}✓${NC} Symlink fix completed!"
    echo ""
    echo "You can now run 'cmake ..' in your build directory."
else
    echo -e "${YELLOW}⚠${NC}  No symlinks were created or fixed."
    echo "  Please check if CVEDIX SDK is installed correctly."
    echo "  Or run 'cmake ..' first to download dependencies."
fi

echo ""

