#!/bin/bash
# Fix cereal symlink for CVEDIX SDK
# CVEDIX SDK expects cereal at /usr/include/cvedix/third_party/cereal/cereal.hpp
# This script creates the required symlink

# Get the build directory (default to ./build)
BUILD_DIR="${1:-./build}"
CEREAL_SRC="${BUILD_DIR}/_deps/cereal-src/include/cereal"
CEREAL_DEST="/usr/include/cvedix/third_party/cereal"

if [ ! -d "$(dirname "$CEREAL_DEST")" ]; then
    echo "Creating directory: $(dirname "$CEREAL_DEST")"
    sudo mkdir -p "$(dirname "$CEREAL_DEST")"
fi

if [ ! -e "$CEREAL_DEST" ]; then
    if [ -d "$CEREAL_SRC" ]; then
        echo "Creating symlink: $CEREAL_DEST -> $CEREAL_SRC"
        sudo ln -sf "$CEREAL_SRC" "$CEREAL_DEST"
        echo "✓ Symlink created successfully"
    else
        echo "Error: Cereal source not found at $CEREAL_SRC"
        echo "Please run 'cmake ..' first to download cereal"
        exit 1
    fi
else
    echo "Symlink already exists: $CEREAL_DEST"
    ls -la "$CEREAL_DEST"
fi

