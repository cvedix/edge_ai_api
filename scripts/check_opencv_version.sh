#!/bin/bash
# Script to check OpenCV version and provide installation guidance

echo "=========================================="
echo "Checking OpenCV installation..."
echo "=========================================="
echo ""

# Check pkg-config
if command -v pkg-config &> /dev/null; then
    VERSION=$(pkg-config --modversion opencv4 2>/dev/null || echo "not found")
    echo "OpenCV version (pkg-config): $VERSION"
else
    echo "pkg-config not found"
fi

echo ""
echo "Checking for OpenCV libraries..."

# Check system OpenCV
SYSTEM_LIBS=$(find /usr/lib/x86_64-linux-gnu -name "libopencv*.so.*" 2>/dev/null | head -5)
if [ -n "$SYSTEM_LIBS" ]; then
    echo "System OpenCV libraries found:"
    echo "$SYSTEM_LIBS" | while read lib; do
        echo "  $lib"
    done
else
    echo "No system OpenCV libraries found"
fi

# Check local OpenCV
LOCAL_LIBS=$(find /usr/local/lib -name "libopencv*.so.*" 2>/dev/null | head -5)
if [ -n "$LOCAL_LIBS" ]; then
    echo ""
    echo "Local OpenCV libraries found (/usr/local/lib):"
    echo "$LOCAL_LIBS" | while read lib; do
        echo "  $lib"
    done
fi

# Check for OpenCV 4.10 specifically
echo ""
echo "Checking for OpenCV 4.10..."
OCV_410=$(find /usr /usr/local -name "libopencv*.so.410" 2>/dev/null | head -1)
if [ -n "$OCV_410" ]; then
    echo "✓ OpenCV 4.10 found: $OCV_410"
    echo ""
    echo "You can proceed with building the project."
else
    echo "✗ OpenCV 4.10 not found"
    echo ""
    echo "CVEDIX SDK requires OpenCV 4.10. You need to install it."
    echo ""
    echo "To install OpenCV 4.10 from source, run:"
    echo "  bash ~/project/edge_ai_api/scripts/install_opencv_4.10.sh"
    echo ""
    echo "Note: Building from source may take 30-60 minutes."
fi

echo ""
echo "=========================================="



