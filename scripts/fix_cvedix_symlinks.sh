#!/bin/bash
# Script to fix CVEDIX SDK library symlinks
# This script creates symlinks from /usr/lib/ to /opt/cvedix/lib/ for libraries
# that CMake expects to find in /usr/lib/

set -e

CVEDIX_LIB_DIR="/opt/cvedix/lib"
TARGET_LIB_DIR="/usr/lib"

echo "=========================================="
echo "CVEDIX SDK Symlink Fix Script"
echo "=========================================="
echo ""

# Check if CVEDIX directory exists
if [ ! -d "$CVEDIX_LIB_DIR" ]; then
    echo "❌ Error: CVEDIX library directory not found: $CVEDIX_LIB_DIR"
    echo "Please ensure cvedix-ai-runtime package is installed."
    exit 1
fi

# List of libraries to symlink
LIBS=(
    "libtinyexpr.so"
    "libcvedix_instance_sdk.so"
)

echo "Checking CVEDIX libraries in $CVEDIX_LIB_DIR..."
echo ""

FIXED=0
MISSING=0
EXISTS=0

for lib in "${LIBS[@]}"; do
    SOURCE="${CVEDIX_LIB_DIR}/${lib}"
    TARGET="${TARGET_LIB_DIR}/${lib}"
    
    if [ ! -f "$SOURCE" ]; then
        echo "⚠️  Warning: Source file not found: $SOURCE"
        MISSING=$((MISSING + 1))
        continue
    fi
    
    if [ -L "$TARGET" ]; then
        CURRENT_LINK=$(readlink -f "$TARGET")
        if [ "$CURRENT_LINK" = "$SOURCE" ]; then
            echo "✓ Symlink already correct: $lib"
            EXISTS=$((EXISTS + 1))
        else
            echo "⚠️  Symlink exists but points to wrong location: $lib"
            echo "   Current: $CURRENT_LINK"
            echo "   Expected: $SOURCE"
            echo "   Fixing..."
            sudo rm -f "$TARGET"
            sudo ln -sf "$SOURCE" "$TARGET"
            echo "✓ Fixed: $lib"
            FIXED=$((FIXED + 1))
        fi
    elif [ -f "$TARGET" ]; then
        echo "⚠️  File exists (not symlink): $TARGET"
        echo "   Backing up and creating symlink..."
        sudo mv "$TARGET" "${TARGET}.backup"
        sudo ln -sf "$SOURCE" "$TARGET"
        echo "✓ Fixed: $lib (backup saved as ${TARGET}.backup)"
        FIXED=$((FIXED + 1))
    else
        echo "Creating symlink: $lib"
        sudo ln -sf "$SOURCE" "$TARGET"
        if [ $? -eq 0 ]; then
            echo "✓ Created: $lib"
            FIXED=$((FIXED + 1))
        else
            echo "❌ Failed to create symlink: $lib"
            exit 1
        fi
    fi
done

echo ""
echo "=========================================="
echo "Summary:"
echo "  Fixed:   $FIXED"
echo "  Exists:  $EXISTS"
echo "  Missing: $MISSING"
echo "=========================================="
echo ""

if [ $FIXED -gt 0 ] || [ $EXISTS -gt 0 ]; then
    echo "Verifying symlinks..."
    echo ""
    for lib in "${LIBS[@]}"; do
        TARGET="${TARGET_LIB_DIR}/${lib}"
        if [ -L "$TARGET" ]; then
            echo "✓ $lib -> $(readlink -f $TARGET)"
        else
            echo "✗ $TARGET not found!"
        fi
    done
    echo ""
    echo "✅ Done! You can now run 'cmake ..' in your build directory."
else
    echo "⚠️  No symlinks were created. Please check if CVEDIX SDK is installed correctly."
fi
