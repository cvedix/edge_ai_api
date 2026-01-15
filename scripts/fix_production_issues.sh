#!/bin/bash
# ============================================
# Fix Production Issues After .deb Installation
# ============================================
#
# Script này fix các vấn đề thường gặp sau khi cài .deb package:
# - Worker executable path
# - Socket directory permissions
# - Environment variables
#
# Usage:
#   sudo ./scripts/fix_production_issues.sh
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
    echo -e "${RED}Error: This script must be run as root (use sudo)${NC}"
    exit 1
fi

SERVICE_NAME="edge-ai-api"
INSTALL_DIR="/opt/edge_ai_api"
BIN_DIR="/usr/local/bin"
RUN_DIR="$INSTALL_DIR/run"
SERVICE_USER="edgeai"
SERVICE_GROUP="edgeai"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Fix Production Issues${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# ============================================
# 1. Fix Socket Directory Permissions
# ============================================
echo -e "${BLUE}[1/4]${NC} Fixing socket directory permissions..."

if [ ! -d "$RUN_DIR" ]; then
    echo "  Creating socket directory: $RUN_DIR"
    mkdir -p "$RUN_DIR"
fi

chown "$SERVICE_USER:$SERVICE_GROUP" "$RUN_DIR"
chmod 755 "$RUN_DIR"
echo -e "${GREEN}✓${NC} Socket directory permissions fixed: $RUN_DIR"
echo ""

# ============================================
# 2. Verify Worker Executable
# ============================================
echo -e "${BLUE}[2/4]${NC} Verifying worker executable..."

WORKER_EXECUTABLE="$BIN_DIR/edge_ai_worker"
if [ -f "$WORKER_EXECUTABLE" ] && [ -x "$WORKER_EXECUTABLE" ]; then
    echo -e "${GREEN}✓${NC} Worker executable found: $WORKER_EXECUTABLE"
    ls -la "$WORKER_EXECUTABLE"
else
    echo -e "${RED}✗${NC} Worker executable NOT found: $WORKER_EXECUTABLE"
    echo "  This is required for subprocess mode!"
    echo "  Please check if package was installed correctly."
fi
echo ""

# ============================================
# 3. Update Systemd Service Environment Variables
# ============================================
echo -e "${BLUE}[3/4]${NC} Updating systemd service environment variables..."

SERVICE_FILE="/etc/systemd/system/${SERVICE_NAME}.service"

if [ ! -f "$SERVICE_FILE" ]; then
    echo -e "${YELLOW}⚠${NC}  Service file not found: $SERVICE_FILE"
    echo "  Service may not be installed correctly."
    echo ""
else
    # Check if environment variables are already set
    if grep -q "EDGE_AI_WORKER_PATH" "$SERVICE_FILE"; then
        echo -e "${GREEN}✓${NC} EDGE_AI_WORKER_PATH already set in service file"
    else
        echo -e "${YELLOW}⚠${NC}  EDGE_AI_WORKER_PATH not found in service file"
        echo "  Adding environment variables..."
        
        # Backup original file
        cp "$SERVICE_FILE" "${SERVICE_FILE}.backup"
        
        # Add environment variables after EnvironmentFile line
        sed -i '/^EnvironmentFile=/a Environment="EDGE_AI_WORKER_PATH=/usr/local/bin/edge_ai_worker"\nEnvironment="EDGE_AI_SOCKET_DIR=/opt/edge_ai_api/run"\nEnvironment="EDGE_AI_EXECUTION_MODE=subprocess"' "$SERVICE_FILE"
        
        echo -e "${GREEN}✓${NC} Environment variables added"
    fi
    
    # Check if ReadWritePaths includes /run directory
    if grep -q "ReadWritePaths=.*/opt/edge_ai_api/run" "$SERVICE_FILE"; then
        echo -e "${GREEN}✓${NC} Socket directory already in ReadWritePaths"
    else
        echo -e "${YELLOW}⚠${NC}  Socket directory not in ReadWritePaths"
        echo "  Adding to ReadWritePaths..."
        
        # Backup original file
        cp "$SERVICE_FILE" "${SERVICE_FILE}.backup"
        
        # Append /run to ReadWritePaths
        sed -i 's|ReadWritePaths=\(.*\)|ReadWritePaths=\1 /opt/edge_ai_api/run|' "$SERVICE_FILE"
        
        echo -e "${GREEN}✓${NC} Socket directory added to ReadWritePaths"
    fi
fi
echo ""

# ============================================
# 4. Update .env File
# ============================================
echo -e "${BLUE}[4/4]${NC} Updating .env file..."

ENV_FILE="$INSTALL_DIR/config/.env"

if [ ! -f "$ENV_FILE" ]; then
    echo "  Creating .env file..."
    mkdir -p "$INSTALL_DIR/config"
    cat > "$ENV_FILE" <<EOF
API_HOST=0.0.0.0
API_PORT=8080
LOG_LEVEL=INFO
EDGE_AI_EXECUTION_MODE=subprocess
EOF
    chown "$SERVICE_USER:$SERVICE_GROUP" "$ENV_FILE"
    chmod 640 "$ENV_FILE"
    echo -e "${GREEN}✓${NC} .env file created"
else
    echo -e "${GREEN}✓${NC} .env file exists"
    
    # Add worker path if not exists
    if ! grep -q "^EDGE_AI_WORKER_PATH=" "$ENV_FILE"; then
        echo "  Adding EDGE_AI_WORKER_PATH..."
        echo "EDGE_AI_WORKER_PATH=/usr/local/bin/edge_ai_worker" >> "$ENV_FILE"
    fi
    
    # Add socket dir if not exists
    if ! grep -q "^EDGE_AI_SOCKET_DIR=" "$ENV_FILE"; then
        echo "  Adding EDGE_AI_SOCKET_DIR..."
        echo "EDGE_AI_SOCKET_DIR=/opt/edge_ai_api/run" >> "$ENV_FILE"
    fi
    
    # Add execution mode if not exists
    if ! grep -q "^EDGE_AI_EXECUTION_MODE=" "$ENV_FILE"; then
        echo "  Adding EDGE_AI_EXECUTION_MODE..."
        echo "EDGE_AI_EXECUTION_MODE=subprocess" >> "$ENV_FILE"
    fi
    
    chown "$SERVICE_USER:$SERVICE_GROUP" "$ENV_FILE"
    echo -e "${GREEN}✓${NC} .env file updated"
fi
echo ""

# ============================================
# Reload and Restart Service
# ============================================
echo -e "${BLUE}Reloading systemd...${NC}"
systemctl daemon-reload
echo -e "${GREEN}✓${NC} Systemd reloaded"
echo ""

echo -e "${BLUE}Restarting service...${NC}"
if systemctl restart "$SERVICE_NAME"; then
    echo -e "${GREEN}✓${NC} Service restarted"
    sleep 2
    
    if systemctl is-active --quiet "$SERVICE_NAME"; then
        echo -e "${GREEN}✓${NC} Service is running"
    else
        echo -e "${YELLOW}⚠${NC}  Service may not be running"
        echo "  Check status: sudo systemctl status $SERVICE_NAME"
    fi
else
    echo -e "${RED}✗${NC} Failed to restart service"
    echo "  Check logs: sudo journalctl -u $SERVICE_NAME -n 50"
fi
echo ""

# ============================================
# Summary
# ============================================
echo "=========================================="
echo -e "${GREEN}✓ Production issues fixed!${NC}"
echo "=========================================="
echo ""
echo "Fixed:"
echo "  1. Socket directory permissions: $RUN_DIR"
echo "  2. Worker executable verified: $WORKER_EXECUTABLE"
echo "  3. Systemd service environment variables updated"
echo "  4. .env file updated"
echo ""
echo "Next steps:"
echo "  - Check service status: sudo systemctl status $SERVICE_NAME"
echo "  - Check logs: sudo journalctl -u $SERVICE_NAME -f"
echo "  - Test API: curl http://localhost:8080/v1/core/health"
echo ""

exit 0

