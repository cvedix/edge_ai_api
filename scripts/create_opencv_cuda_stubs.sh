#!/bin/bash
# Create stub libraries for OpenCV CUDA modules to satisfy linker
# These are empty libraries that satisfy the linker but don't provide any functionality

set -e

echo "=========================================="
echo "Creating OpenCV CUDA stub libraries..."
echo "=========================================="
echo ""

CUDA_MODULES=(
    "opencv_cudaarithm"
    "opencv_cudabgsegm"
    "opencv_cudacodec"
    "opencv_cudafeatures2d"
    "opencv_cudafilters"
    "opencv_cudaimgproc"
    "opencv_cudalegacy"
    "opencv_cudaobjdetect"
    "opencv_cudaoptflow"
    "opencv_cudastereo"
    "opencv_cudawarping"
    "opencv_cudev"
)

# Create a valid stub source file
cat > /tmp/opencv_cuda_stub.c << 'STUBEOF'
#include <stdio.h>

// Minimal valid shared library for CUDA stubs
// These functions are never called, but satisfy the linker

void __attribute__((constructor)) init() {}
void __attribute__((destructor)) fini() {}

// Export a dummy symbol to make the library valid
void opencv_cuda_stub_dummy() {}
STUBEOF

for module in "${CUDA_MODULES[@]}"; do
    LIB_PATH="/usr/local/lib/lib${module}.so.410"
    SYMLINK="/usr/local/lib/lib${module}.so"
    
    if [ ! -f "$LIB_PATH" ] || [ ! -s "$LIB_PATH" ]; then
        echo "Creating stub for $module..."
        # Create a valid shared library using gcc
        if gcc -shared -fPIC -o "$LIB_PATH" /tmp/opencv_cuda_stub.c -Wl,-soname,lib${module}.so.410 2>/dev/null; then
            sudo chmod 644 "$LIB_PATH"
            # Create symlink
            if [ -L "$SYMLINK" ] || [ -f "$SYMLINK" ]; then
                sudo rm -f "$SYMLINK"
            fi
            sudo ln -sf lib${module}.so.410 "$SYMLINK"
            echo "  ✓ Created valid stub $LIB_PATH"
        else
            echo "  ✗ Failed to create stub for $module (gcc not available or error)"
            # Fallback: create empty file (may not work but better than nothing)
            sudo touch "$LIB_PATH"
            sudo chmod 644 "$LIB_PATH"
            if [ -L "$SYMLINK" ] || [ -f "$SYMLINK" ]; then
                sudo rm -f "$SYMLINK"
            fi
            sudo ln -sf lib${module}.so.410 "$SYMLINK"
            echo "  ⚠ Created empty stub $LIB_PATH (fallback)"
        fi
    else
        echo "  ⊙ $module already exists, skipping..."
    fi
done

# Cleanup
rm -f /tmp/opencv_cuda_stub.c

echo ""
echo "Updating library cache..."
sudo ldconfig

echo ""
echo "=========================================="
echo "Done! CUDA stub libraries created."
echo "=========================================="
echo ""
echo "You can now rebuild your project:"
echo "  cd ~/project/edge_ai_api/build"
echo "  cmake .."
echo "  make -j\$(nproc)"



