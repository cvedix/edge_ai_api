#!/bin/bash
# Script to fix CVEDIX SDK library symlinks
# This script creates symlinks from /usr/lib/ to /opt/cvedix/lib/ for libraries
# that CMake expects to find in /usr/lib/

set -e

echo "Fixing CVEDIX SDK library symlinks..."

# Check if /opt/cvedix/lib exists
if [ ! -d "/opt/cvedix/lib" ]; then
    echo "Error: /opt/cvedix/lib does not exist!"
    echo "Please ensure cvedix-ai-runtime package is installed."
    exit 1
fi

# Create symlinks for required libraries
LIBRARIES=(
    "libtinyexpr.so"
    "libcvedix_instance_sdk.so"
)

for lib in "${LIBRARIES[@]}"; do
    SOURCE="/opt/cvedix/lib/$lib"
    TARGET="/usr/lib/$lib"
    
    if [ -f "$SOURCE" ]; then
        if [ -L "$TARGET" ] || [ -f "$TARGET" ]; then
            echo "Removing existing $TARGET..."
            sudo rm -f "$TARGET"
        fi
        echo "Creating symlink: $TARGET -> $SOURCE"
        sudo ln -sf "$SOURCE" "$TARGET"
    else
        echo "Warning: $SOURCE does not exist, skipping..."
    fi
done

echo ""
echo "Verifying symlinks..."
for lib in "${LIBRARIES[@]}"; do
    TARGET="/usr/lib/$lib"
    if [ -L "$TARGET" ]; then
        echo "✓ $TARGET -> $(readlink -f $TARGET)"
    else
        echo "✗ $TARGET not found!"
    fi
done

echo ""
echo "Done! You can now run cmake .. in your build directory."



