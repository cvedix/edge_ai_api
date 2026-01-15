#!/bin/bash
# ============================================
# Fix All CVEDIX SDK Issues
# ============================================
#
# Script tổng hợp để fix tất cả các vấn đề liên quan đến CVEDIX SDK:
# 1. Fix symlinks (tinyexpr, cereal, cpp-base64, OpenCV freetype)
# 2. Fix permissions cho model files và data directories
# 3. Update library cache
#
# Usage:
#   sudo ./scripts/fix_cvedix_issues.sh
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
echo -e "${BLUE}Fix All CVEDIX SDK Issues${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# ============================================
# Step 1: Fix Symlinks
# ============================================
echo -e "${BLUE}[Step 1/3]${NC} Fixing symlinks..."
"$SCRIPT_DIR/fix_all_symlinks.sh"
echo ""

# ============================================
# Step 2: Fix Permissions
# ============================================
echo -e "${BLUE}[Step 2/3]${NC} Fixing permissions..."

# Find the user that owns the project (usually cvedix or the current user)
PROJECT_OWNER=$(stat -c '%U' "$PROJECT_ROOT" 2>/dev/null || echo "cvedix")
SERVICE_USER="edgeai"

echo "Project owner: $PROJECT_OWNER"
echo "Service user: $SERVICE_USER"
echo ""

# Fix permissions for cvedix_data directory
CVEDIX_DATA_DIR="$PROJECT_ROOT/cvedix_data"
if [ -d "$CVEDIX_DATA_DIR" ]; then
    echo "Fixing permissions for $CVEDIX_DATA_DIR..."
    
    # Set directory permissions to 755
    find "$CVEDIX_DATA_DIR" -type d -exec chmod 755 {} \;
    echo -e "${GREEN}✓${NC} Set directory permissions to 755"
    
    # Set file permissions to 644
    find "$CVEDIX_DATA_DIR" -type f -exec chmod 644 {} \;
    echo -e "${GREEN}✓${NC} Set file permissions to 644"
    
    # Change ownership to project owner
    chown -R "$PROJECT_OWNER:$PROJECT_OWNER" "$CVEDIX_DATA_DIR"
    echo -e "${GREEN}✓${NC} Changed ownership to $PROJECT_OWNER:$PROJECT_OWNER"
    
    # Add service user to project owner's group (if service user exists)
    if id "$SERVICE_USER" &>/dev/null; then
        # Get the primary group of project owner
        OWNER_GROUP=$(id -gn "$PROJECT_OWNER")
        usermod -a -G "$OWNER_GROUP" "$SERVICE_USER" 2>/dev/null || true
        echo -e "${GREEN}✓${NC} Added $SERVICE_USER to group $OWNER_GROUP"
    fi
else
    echo -e "${YELLOW}⚠${NC}  cvedix_data directory not found at $CVEDIX_DATA_DIR"
fi
echo ""

# Fix permissions for /opt/edge_ai_api (if exists)
OPT_DIR="/opt/edge_ai_api"
if [ -d "$OPT_DIR" ]; then
    echo "Fixing permissions for $OPT_DIR..."
    
    # Set directory permissions
    find "$OPT_DIR" -type d -exec chmod 755 {} \;
    find "$OPT_DIR" -type f -exec chmod 644 {} \;
    
    # Make scripts executable
    find "$OPT_DIR" -type f -name "*.sh" -exec chmod 755 {} \;
    
    # Change ownership to service user
    if id "$SERVICE_USER" &>/dev/null; then
        chown -R "$SERVICE_USER:$SERVICE_USER" "$OPT_DIR"
        echo -e "${GREEN}✓${NC} Changed ownership of $OPT_DIR to $SERVICE_USER:$SERVICE_USER"
    fi
    echo ""
fi

# Fix permissions for /home/cvedix directories (if service needs access)
if [ -d "/home/cvedix" ]; then
    echo "Fixing permissions for /home/cvedix/project/edge_ai_api/cvedix_data..."
    
    HOME_CVEDIX_DATA="/home/cvedix/project/edge_ai_api/cvedix_data"
    if [ -d "$HOME_CVEDIX_DATA" ]; then
        find "$HOME_CVEDIX_DATA" -type d -exec chmod 755 {} \;
        find "$HOME_CVEDIX_DATA" -type f -exec chmod 644 {} \;
        
        # Add read permissions for others as fallback
        chmod -R o+r "$HOME_CVEDIX_DATA" 2>/dev/null || true
        echo -e "${GREEN}✓${NC} Fixed permissions for $HOME_CVEDIX_DATA"
    fi
    echo ""
fi

echo -e "${GREEN}✓${NC} Permissions fixed"
echo ""

# ============================================
# Step 3: Update Library Cache
# ============================================
echo -e "${BLUE}[Step 3/3]${NC} Updating library cache..."
ldconfig
echo -e "${GREEN}✓${NC} Library cache updated"
echo ""

# ============================================
# Summary
# ============================================
echo -e "${BLUE}========================================${NC}"
echo -e "${GREEN}✓${NC} All CVEDIX SDK issues fixed!"
echo -e "${BLUE}========================================${NC}"
echo ""
echo "Summary:"
echo "  ✓ Symlinks fixed (tinyexpr, cereal, cpp-base64, OpenCV freetype)"
echo "  ✓ Permissions fixed for model files and data directories"
echo "  ✓ Library cache updated"
echo ""
echo "Next steps:"
echo "  1. Rebuild project if needed: cd build && cmake .. && make"
echo "  2. Restart service if running: sudo systemctl restart edge-ai-api"
echo "  3. Test creating an instance"
echo ""

