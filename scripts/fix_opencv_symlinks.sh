#!/bin/bash
# Script to create OpenCV 4.10 symlinks from OpenCV 4.6.0
# WARNING: This is a workaround and may not work due to ABI differences
# CVEDIX SDK requires OpenCV 4.10 but system has 4.6.0

set -e

echo "Creating OpenCV 4.10 symlinks from 4.6.0..."
echo "WARNING: This may not work due to ABI differences between versions"

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

for lib in "${OPENCV_LIBS[@]}"; do
    SOURCE="/usr/lib/x86_64-linux-gnu/${lib}.406"
    TARGET="/usr/lib/x86_64-linux-gnu/${lib}.410"
    
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
for lib in "${OPENCV_LIBS[@]}"; do
    TARGET="/usr/lib/x86_64-linux-gnu/${lib}.410"
    if [ -L "$TARGET" ]; then
        echo "✓ $TARGET -> $(readlink -f $TARGET)"
    else
        echo "✗ $TARGET not found!"
    fi
done

echo ""
echo "Done! Try building again. If it still fails, you may need to install OpenCV 4.10."



