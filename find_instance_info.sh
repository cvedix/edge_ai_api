#!/bin/bash

# Script helper để tìm thông tin instance từ logs
# Usage: ./find_instance_info.sh <instance_id>

INSTANCE_ID="${1:-275a6cc9-5b84-42af-b157-10e27901e920}"

echo "Tìm thông tin cho instance: $INSTANCE_ID"
echo "=========================================="
echo ""

# Tìm service name
SERVICE_NAME=""
for name in "edge-ai-api" "edge_ai_api" "edgeai-api"; do
    if systemctl list-units --type=service --all 2>/dev/null | grep -q "$name"; then
        SERVICE_NAME="$name"
        break
    fi
done

if [ -z "$SERVICE_NAME" ]; then
    echo "Không tìm thấy service. Tìm trong tất cả logs..."
    LOGS_SOURCE="journalctl --since '24 hours ago'"
else
    echo "Service: $SERVICE_NAME"
    LOGS_SOURCE="journalctl -u $SERVICE_NAME --since '24 hours ago'"
fi

echo ""

# 1. Tìm port
echo "1. Tìm port API..."
echo "-------------------"
PORT=$(eval "$LOGS_SOURCE --no-pager 2>/dev/null" | \
    grep -iE "(listening|port|started.*port|server.*port)" | \
    grep -oE "port[:\s]+([0-9]+)" | \
    grep -oE "[0-9]+" | \
    head -1)

if [ ! -z "$PORT" ]; then
    echo "   Port: $PORT"
else
    # Thử tìm từ process
    PID=$(pgrep -f "edge_ai_api\|edge-ai-api" | head -1)
    if [ ! -z "$PID" ]; then
        PORT=$(ss -tlnp 2>/dev/null | grep "pid=$PID" | grep LISTEN | \
            grep -oE ":[0-9]+" | tr -d ':' | head -1)
        if [ ! -z "$PORT" ]; then
            echo "   Port (từ process): $PORT"
        fi
    fi
    
    if [ -z "$PORT" ]; then
        echo "   Không tìm thấy port"
    fi
fi

echo ""

# 2. Tìm file path
echo "2. Tìm file path..."
echo "-------------------"
FILE_PATH=$(eval "$LOGS_SOURCE --no-pager 2>/dev/null" | \
    grep -i "$INSTANCE_ID" | \
    grep -iE "(FILE_PATH|filePath|File path:)" | \
    grep -oE "(FILE_PATH|filePath)\s*[:=]\s*['\"]?([^'\"]+)['\"]?" | \
    grep -oE "['\"]?([^'\"]+)['\"]?$" | \
    head -1 | tr -d "'\"")

if [ -z "$FILE_PATH" ]; then
    # Thử pattern khác
    FILE_PATH=$(eval "$LOGS_SOURCE --no-pager 2>/dev/null" | \
        grep -i "$INSTANCE_ID" | \
        grep -oE "(/|\./)[^[:space:]]+\.(mp4|avi|mov|mkv|flv)" | \
        head -1)
fi

if [ ! -z "$FILE_PATH" ]; then
    echo "   File path: $FILE_PATH"
    
    # Kiểm tra file có tồn tại không
    if [ -f "$FILE_PATH" ]; then
        echo "   ✓ File tồn tại"
        ls -lh "$FILE_PATH" | awk '{print "   Size: " $5 ", Permissions: " $1}'
    else
        echo "   ✗ File KHÔNG tồn tại"
    fi
else
    echo "   Không tìm thấy file path"
fi

echo ""

# 3. Hiển thị logs liên quan
echo "3. Logs liên quan đến instance..."
echo "-----------------------------------"
eval "$LOGS_SOURCE --no-pager 2>/dev/null" | \
    grep -i "$INSTANCE_ID" | \
    tail -20

echo ""
echo "=========================================="

# Tóm tắt
echo ""
echo "TÓM TẮT:"
if [ ! -z "$PORT" ]; then
    echo "  API URL: http://localhost:$PORT/api/v1/instances/$INSTANCE_ID"
fi
if [ ! -z "$FILE_PATH" ]; then
    echo "  File path: $FILE_PATH"
fi

