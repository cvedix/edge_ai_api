#!/bin/bash
# Quick check script for record output

INSTANCE_ID="${1:-7ee356cd-109e-4a5a-b932-4130e2ea67f4}"
API_URL="http://localhost:3546"

echo "=========================================="
echo "Record Output Status Check"
echo "=========================================="
echo "Instance ID: $INSTANCE_ID"
echo ""

# 1. Check output stream config
echo "1. Output Stream Config:"
OUTPUT=$(curl -s "$API_URL/v1/core/instance/$INSTANCE_ID/output/stream")
echo "$OUTPUT" | jq '.' 2>/dev/null || echo "$OUTPUT"
echo ""

# 2. Check RECORD_PATH in config
echo "2. RECORD_PATH in Config:"
RECORD_PATH=$(curl -s "$API_URL/v1/core/instance/$INSTANCE_ID/config" | jq -r '.AdditionalParams.RECORD_PATH // empty' 2>/dev/null)
if [ -n "$RECORD_PATH" ] && [ "$RECORD_PATH" != "null" ] && [ "$RECORD_PATH" != "" ]; then
    echo "   ✓ RECORD_PATH: $RECORD_PATH"
else
    echo "   ❌ RECORD_PATH not found!"
fi
echo ""

# 3. Check instance status
echo "3. Instance Status:"
STATUS=$(curl -s "$API_URL/v1/core/instances" | jq ".instances[] | select(.instanceId == \"$INSTANCE_ID\")" 2>/dev/null)
RUNNING=$(echo "$STATUS" | jq -r '.running // false')
FPS=$(echo "$STATUS" | jq -r '.fps // 0')
echo "   Running: $RUNNING"
echo "   FPS: $FPS"
if [ "$FPS" = "0" ] || [ "$FPS" = "0.0" ]; then
    echo "   ⚠️  WARNING: FPS = 0 - No frames being processed!"
    echo "   This means no video input or pipeline not working"
fi
echo ""

# 4. Check directory
echo "4. Save Directory:"
if [ -n "$RECORD_PATH" ] && [ "$RECORD_PATH" != "null" ] && [ "$RECORD_PATH" != "" ]; then
    SAVE_DIR="$RECORD_PATH"
else
    SAVE_DIR="/mnt/sb1/data"
fi

if [ -d "$SAVE_DIR" ]; then
    echo "   ✓ Directory exists: $SAVE_DIR"
    FILE_COUNT=$(ls -1 "$SAVE_DIR" 2>/dev/null | wc -l)
    echo "   Files: $FILE_COUNT"
    if [ "$FILE_COUNT" -gt 0 ]; then
        echo "   Recent files:"
        ls -lht "$SAVE_DIR" | head -5 | awk '{print "     " $0}'
    else
        echo "   ⚠️  No files found"
    fi
else
    echo "   ❌ Directory does not exist: $SAVE_DIR"
fi
echo ""

# 5. Recommendations
echo "=========================================="
echo "Recommendations:"
echo "=========================================="

if [ "$FPS" = "0" ] || [ "$FPS" = "0.0" ]; then
    echo "⚠️  CRITICAL: FPS = 0"
    echo "   - Check video input source (RTSP URL, file path, etc.)"
    echo "   - Check if input source is accessible"
    echo "   - Check pipeline logs for errors"
    echo ""
fi

if [ -z "$RECORD_PATH" ] || [ "$RECORD_PATH" = "null" ] || [ "$RECORD_PATH" = "" ]; then
    echo "⚠️  RECORD_PATH not configured"
    echo "   Configure with:"
    echo "   curl -X POST $API_URL/v1/core/instance/$INSTANCE_ID/output/stream \\"
    echo "     -H 'Content-Type: application/json' \\"
    echo "     -d '{\"enabled\": true, \"path\": \"/mnt/sb1/data\"}'"
    echo ""
    echo "   Then restart instance:"
    echo "   curl -X POST $API_URL/v1/core/instance/$INSTANCE_ID/stop"
    echo "   curl -X POST $API_URL/v1/core/instance/$INSTANCE_ID/start"
    echo ""
fi

if [ "$FILE_COUNT" -eq 0 ] && [ "$RUNNING" = "true" ] && [ -n "$RECORD_PATH" ]; then
    echo "⚠️  No files but instance is running and RECORD_PATH is set"
    echo "   Possible reasons:"
    echo "   1. FPS = 0 (no video input) - MOST LIKELY"
    echo "   2. file_des node not added (check if code was rebuilt and instance restarted)"
    echo "   3. file_des node not receiving frames (check pipeline connection)"
    echo ""
    echo "   Check logs for:"
    echo "   - 'RECORD_PATH detected' (confirms code has new feature)"
    echo "   - 'Auto-adding file_des node' (confirms node creation attempt)"
    echo "   - 'Auto-added file_des node' (confirms success)"
    echo ""
fi

