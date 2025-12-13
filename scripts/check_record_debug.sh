#!/bin/bash
# Script to debug why record files are not being created

INSTANCE_ID="${1:-ba33c372-eb4e-40f7-80c9-4d14e103bc45}"
API_URL="http://localhost:3546"
RECORD_PATH="/mnt/sb1/data"

echo "=========================================="
echo "Debug Record Output Issue"
echo "=========================================="
echo "Instance ID: $INSTANCE_ID"
echo ""

# 1. Check instance status
echo "1. Checking instance status:"
INSTANCE_INFO=$(curl -s "$API_URL/v1/core/instance/$INSTANCE_ID" 2>/dev/null)
if [ -z "$INSTANCE_INFO" ] || [ "$INSTANCE_INFO" = "null" ]; then
    echo "   ❌ Instance not found!"
    exit 1
fi

RUNNING=$(echo "$INSTANCE_INFO" | jq -r '.running // false' 2>/dev/null)
LOADED=$(echo "$INSTANCE_INFO" | jq -r '.loaded // false' 2>/dev/null)
FPS=$(echo "$INSTANCE_INFO" | jq -r '.fps // 0' 2>/dev/null)
echo "   Running: $RUNNING"
echo "   Loaded: $LOADED"
echo "   FPS: $FPS"
echo ""

# 2. Check RECORD_PATH in config
echo "2. Checking RECORD_PATH in config:"
CONFIG=$(curl -s "$API_URL/v1/core/instance/$INSTANCE_ID/config" 2>/dev/null)
RECORD_PATH_CONFIG=$(echo "$CONFIG" | jq -r '.AdditionalParams.RECORD_PATH // empty' 2>/dev/null)
if [ -n "$RECORD_PATH_CONFIG" ] && [ "$RECORD_PATH_CONFIG" != "null" ] && [ "$RECORD_PATH_CONFIG" != "" ]; then
    echo "   ✓ RECORD_PATH: $RECORD_PATH_CONFIG"
else
    echo "   ❌ RECORD_PATH not found in config!"
    echo "   Please configure it first with:"
    echo "   curl -X POST $API_URL/v1/core/instance/$INSTANCE_ID/output/stream \\"
    echo "     -H 'Content-Type: application/json' \\"
    echo "     -d '{\"enabled\": true, \"path\": \"$RECORD_PATH\"}'"
    exit 1
fi
echo ""

# 3. Check output stream config
echo "3. Checking output stream config:"
OUTPUT_CONFIG=$(curl -s "$API_URL/v1/core/instance/$INSTANCE_ID/output/stream" 2>/dev/null)
OUTPUT_ENABLED=$(echo "$OUTPUT_CONFIG" | jq -r '.enabled // false' 2>/dev/null)
OUTPUT_PATH=$(echo "$OUTPUT_CONFIG" | jq -r '.path // empty' 2>/dev/null)
echo "   Enabled: $OUTPUT_ENABLED"
echo "   Path: $OUTPUT_PATH"
echo ""

# 4. Check directory
echo "4. Checking directory:"
if [ -d "$RECORD_PATH" ]; then
    echo "   ✓ Directory exists: $RECORD_PATH"
    if [ -w "$RECORD_PATH" ]; then
        echo "   ✓ Directory is writable"
    else
        echo "   ❌ Directory is NOT writable!"
        echo "   Fix with: sudo chmod 755 $RECORD_PATH"
    fi
    echo "   Files in directory:"
    ls -lh "$RECORD_PATH" 2>/dev/null | head -10 || echo "   (empty or cannot list)"
else
    echo "   ❌ Directory does not exist: $RECORD_PATH"
    echo "   Create with: sudo mkdir -p $RECORD_PATH && sudo chmod 755 $RECORD_PATH"
fi
echo ""

# 5. Check if instance needs restart
echo "5. Checking if instance needs restart:"
if [ "$RUNNING" = "true" ]; then
    echo "   Instance is running"
    echo "   If RECORD_PATH was just configured, instance should have been auto-restarted"
    echo "   If files still not created, try manual restart:"
    echo "   curl -X POST $API_URL/v1/core/instance/$INSTANCE_ID/stop"
    echo "   curl -X POST $API_URL/v1/core/instance/$INSTANCE_ID/start"
else
    echo "   ⚠ Instance is NOT running"
    echo "   Start instance with:"
    echo "   curl -X POST $API_URL/v1/core/instance/$INSTANCE_ID/start"
fi
echo ""

# 6. Check logs for RECORD_PATH and file_des
echo "6. Checking logs (last 100 lines for RECORD_PATH/file_des):"
echo "   Looking for:"
echo "   - [PipelineBuilder] RECORD_PATH detected"
echo "   - [PipelineBuilder] Auto-adding file_des node"
echo "   - [PipelineBuilder] ✓ Auto-added file_des node"
echo ""
echo "   Run this command to check logs:"
echo "   journalctl -u edge-ai-api -n 500 | grep -i \"$INSTANCE_ID\\|RECORD_PATH\\|file_des\" | tail -20"
echo "   OR if running directly:"
echo "   tail -500 /path/to/log | grep -i \"$INSTANCE_ID\\|RECORD_PATH\\|file_des\" | tail -20"
echo ""

# 7. Summary and recommendations
echo "=========================================="
echo "Summary and Recommendations:"
echo "=========================================="
if [ "$RUNNING" = "true" ] && [ -n "$RECORD_PATH_CONFIG" ] && [ -w "$RECORD_PATH" ]; then
    echo "✓ Instance is running"
    echo "✓ RECORD_PATH is configured"
    echo "✓ Directory is writable"
    echo ""
    echo "If files are still not created, check:"
    echo "1. Logs for errors when creating file_des node"
    echo "2. FPS > 0 (instance is processing frames)"
    echo "3. Input source is working (RTSP/file)"
    echo "4. Try manual restart:"
    echo "   curl -X POST $API_URL/v1/core/instance/$INSTANCE_ID/stop"
    echo "   sleep 2"
    echo "   curl -X POST $API_URL/v1/core/instance/$INSTANCE_ID/start"
else
    echo "⚠ Some checks failed - see above for details"
fi
echo ""

