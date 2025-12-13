#!/bin/bash
# Script to fix dependency issues before installing OpenCV

set -e

echo "=========================================="
echo "Fixing dependency issues..."
echo "=========================================="
echo ""

# Check for cvedix version mismatch
echo "Checking cvedix package versions..."
CVEDIX_RUNTIME=$(dpkg -l | grep "cvedix-ai-runtime " | awk '{print $3}' | head -1)
CVEDIX_DEV=$(dpkg -l | grep "cvedix-ai-runtime-dev" | awk '{print $3}' | head -1)

if [ -n "$CVEDIX_RUNTIME" ] && [ -n "$CVEDIX_DEV" ]; then
    echo "Found: cvedix-ai-runtime=$CVEDIX_RUNTIME"
    echo "Found: cvedix-ai-runtime-dev=$CVEDIX_DEV"
    
    # Extract version numbers (remove -1, -2 suffixes)
    RUNTIME_VER=$(echo "$CVEDIX_RUNTIME" | cut -d'-' -f1)
    DEV_VER=$(echo "$CVEDIX_DEV" | cut -d'-' -f1)
    
    if [ "$RUNTIME_VER" != "$DEV_VER" ]; then
        echo ""
        echo "⚠ Version mismatch detected!"
        echo "  Runtime: $CVEDIX_RUNTIME"
        echo "  Dev: $CVEDIX_DEV"
        echo ""
        echo "Options:"
        echo "  1. Remove dev package (recommended if not needed for OpenCV build):"
        echo "     sudo apt remove cvedix-ai-runtime-dev"
        echo ""
        echo "  2. Or try to fix broken dependencies:"
        echo "     sudo apt --fix-broken install"
        echo ""
        read -p "Do you want to remove cvedix-ai-runtime-dev? (y/N): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            sudo apt remove -y cvedix-ai-runtime-dev || echo "Could not remove, continuing..."
        fi
    else
        echo "✓ Versions match"
    fi
fi

echo ""
echo "Fixing broken dependencies..."
sudo apt --fix-broken install -y || {
    echo ""
    echo "⚠ Some dependencies could not be fixed automatically."
    echo "You may need to resolve conflicts manually."
    echo ""
    echo "Common solutions:"
    echo "  1. Remove conflicting packages"
    echo "  2. Update package lists: sudo apt update"
    echo "  3. Try: sudo apt autoremove && sudo apt autoclean"
    exit 1
}

echo ""
echo "=========================================="
echo "Dependencies fixed!"
echo "=========================================="
echo ""
echo "You can now run:"
echo "  bash ~/project/edge_ai_api/scripts/install_opencv_4.10.sh"



