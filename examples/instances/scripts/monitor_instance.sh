#!/bin/bash

# Script monitor instance liên tục
# Sử dụng: ./monitor_instance.sh [INSTANCE_ID] [BASE_URL] [INTERVAL_SECONDS]
# Mặc định: BASE_URL=http://localhost:8848, INTERVAL=2

INSTANCE_ID="$1"
BASE_URL="${2:-http://localhost:8848}"
INTERVAL="${3:-2}"
API_BASE="${BASE_URL}/v1/core"

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
CYAN='\033[0;36m'
NC='\033[0m'

# Check if instance ID is provided
if [ -z "$INSTANCE_ID" ]; then
    echo "Usage: $0 <INSTANCE_ID> [BASE_URL] [INTERVAL_SECONDS]"
    echo ""
    echo "Example:"
    echo "  $0 abc-123-def-456"
    echo "  $0 abc-123-def-456 http://localhost:8848 5"
    exit 1
fi

# Function to clear line
clear_line() {
    echo -ne "\r\033[K"
}

# Function to print status
print_status() {
    local status=$1
    local message=$2
    if [ "$status" = "OK" ]; then
        echo -e "${GREEN}✓${NC} $message"
    elif [ "$status" = "WARNING" ]; then
        echo -e "${YELLOW}⚠${NC} $message"
    elif [ "$status" = "ERROR" ]; then
        echo -e "${RED}✗${NC} $message"
    else
        echo -e "${CYAN}ℹ${NC} $message"
    fi
}

echo "=========================================="
echo "Monitoring Instance: $INSTANCE_ID"
echo "Base URL: $BASE_URL"
echo "Interval: ${INTERVAL}s"
echo "Press Ctrl+C to stop"
echo "=========================================="
echo ""

# Track previous values
PREV_FPS=0
PREV_FILE_COUNT=0

while true; do
    # Get instance info
    INSTANCE_INFO=$(curl -s "${API_BASE}/instances/${INSTANCE_ID}" 2>/dev/null)

    if [ $? -ne 0 ] || [ -z "$INSTANCE_INFO" ]; then
        clear_line
        echo -e "${RED}✗${NC} Không thể kết nối đến API"
        sleep $INTERVAL
        continue
    fi

    ERROR_MSG=$(echo "$INSTANCE_INFO" | jq -r '.error // empty' 2>/dev/null)
    if [ -n "$ERROR_MSG" ]; then
        clear_line
        echo -e "${RED}✗${NC} Instance không tồn tại: $ERROR_MSG"
        sleep $INTERVAL
        continue
    fi

    # Extract values
    RUNNING=$(echo "$INSTANCE_INFO" | jq -r '.running' 2>/dev/null)
    FPS=$(echo "$INSTANCE_INFO" | jq -r '.fps' 2>/dev/null)
    DISPLAY_NAME=$(echo "$INSTANCE_INFO" | jq -r '.displayName' 2>/dev/null)

    # Check output files
    OUTPUT_DIR="./output/${INSTANCE_ID}"
    BUILD_OUTPUT_DIR="./build/output/${INSTANCE_ID}"
    FILE_COUNT=0

    if [ -d "$OUTPUT_DIR" ]; then
        FILE_COUNT=$(find "$OUTPUT_DIR" -type f 2>/dev/null | wc -l)
    elif [ -d "$BUILD_OUTPUT_DIR" ]; then
        FILE_COUNT=$(find "$BUILD_OUTPUT_DIR" -type f 2>/dev/null | wc -l)
    fi

    # Build status line
    TIMESTAMP=$(date '+%H:%M:%S')
    STATUS_LINE="[$TIMESTAMP] "

    if [ "$RUNNING" = "true" ]; then
        STATUS_LINE+="${GREEN}RUNNING${NC} | "
    else
        STATUS_LINE+="${RED}STOPPED${NC} | "
    fi

    STATUS_LINE+="FPS: "
    if (( $(echo "$FPS > 0" | bc -l 2>/dev/null) )); then
        STATUS_LINE+="${GREEN}${FPS}${NC} | "

        # Check if FPS increased
        if (( $(echo "$FPS > $PREV_FPS" | bc -l 2>/dev/null) )); then
            STATUS_LINE+="${GREEN}↑${NC} "
        elif (( $(echo "$FPS < $PREV_FPS" | bc -l 2>/dev/null) )); then
            STATUS_LINE+="${YELLOW}↓${NC} "
        else
            STATUS_LINE+="→ "
        fi
    else
        STATUS_LINE+="${RED}0.0${NC} | "
    fi

    STATUS_LINE+="Files: ${FILE_COUNT}"

    if [ $FILE_COUNT -gt $PREV_FILE_COUNT ]; then
        STATUS_LINE+=" ${GREEN}(+$((FILE_COUNT - PREV_FILE_COUNT)))${NC}"
    fi

    STATUS_LINE+=" | Name: $DISPLAY_NAME"

    # Print status
    clear_line
    echo -ne "$STATUS_LINE"

    # Update previous values
    PREV_FPS=$FPS
    PREV_FILE_COUNT=$FILE_COUNT

    sleep $INTERVAL
done
