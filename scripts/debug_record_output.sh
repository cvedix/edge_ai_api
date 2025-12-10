#!/bin/bash
# Debug script for record output

INSTANCE_ID="${1:-8dc40922-aefa-4026-97d0-b61e155b3438}"
API_URL="http://localhost:3546"

echo "=========================================="
echo "Debug Record Output Configuration"
echo "=========================================="
echo "Instance ID: $INSTANCE_ID"
echo ""

# 1. Check instance status
echo "1. Checking instance status..."
INSTANCE_INFO=$(curl -s "$API_URL/v1/core/instance/$INSTANCE_ID" 2>/dev/null)
if echo "$INSTANCE_INFO" | grep -q "404\|Not Found"; then
    echo "   ❌ Instance not found!"
    echo "   Response: $INSTANCE_INFO"
    exit 1
fi

RUNNING=$(echo "$INSTANCE_INFO" | jq -r '.running // false' 2>/dev/null || echo "false")
LOADED=$(echo "$INSTANCE_INFO" | jq -r '.loaded // false' 2>/dev/null || echo "false")
echo "   Running: $RUNNING"
echo "   Loaded: $LOADED"
echo ""

# 2. Check output stream config
echo "2. Checking output stream configuration..."
OUTPUT_CONFIG=$(curl -s "$API_URL/v1/core/instance/$INSTANCE_ID/output/stream")
echo "$OUTPUT_CONFIG" | jq '.' 2>/dev/null || echo "$OUTPUT_CONFIG"
echo ""

# 3. Check RECORD_PATH in instance config
echo "3. Checking RECORD_PATH in instance config..."
INSTANCE_CONFIG=$(curl -s "$API_URL/v1/core/instance/$INSTANCE_ID/config")
RECORD_PATH=$(echo "$INSTANCE_CONFIG" | jq -r '.AdditionalParams.RECORD_PATH // empty' 2>/dev/null)
if [ -n "$RECORD_PATH" ] && [ "$RECORD_PATH" != "null" ] && [ "$RECORD_PATH" != "" ]; then
    echo "   ✓ RECORD_PATH found: $RECORD_PATH"
else
    echo "   ❌ RECORD_PATH not found in config!"
    echo "   Full AdditionalParams:"
    echo "$INSTANCE_CONFIG" | jq '.AdditionalParams' 2>/dev/null || echo "   (Cannot parse JSON)"
fi
echo ""

# 4. Check directory
echo "4. Checking save directory..."
SAVE_DIR="${RECORD_PATH:-/mnt/sb1/data}"
if [ -d "$SAVE_DIR" ]; then
    echo "   ✓ Directory exists: $SAVE_DIR"
    echo "   Permissions: $(ls -ld "$SAVE_DIR" | awk '{print $1, $3, $4}')"
    
    # Check write permission
    if [ -w "$SAVE_DIR" ]; then
        echo "   ✓ Directory is writable"
    else
        echo "   ❌ Directory is NOT writable!"
    fi
    
    # List files
    FILE_COUNT=$(ls -1 "$SAVE_DIR" 2>/dev/null | wc -l)
    echo "   Files in directory: $FILE_COUNT"
    if [ "$FILE_COUNT" -gt 0 ]; then
        echo "   Recent files:"
        ls -lht "$SAVE_DIR" | head -5
    fi
else
    echo "   ❌ Directory does not exist: $SAVE_DIR"
fi
echo ""

# 5. Check if process is running
echo "5. Checking edge_ai_api process..."
if pgrep -f "edge_ai_api" > /dev/null; then
    echo "   ✓ Process is running"
    PID=$(pgrep -f "edge_ai_api" | head -1)
    echo "   PID: $PID"
else
    echo "   ❌ Process is NOT running!"
fi
echo ""

# 6. Recommendations
echo "=========================================="
echo "Recommendations:"
echo "=========================================="

if [ "$RUNNING" != "true" ]; then
    echo "⚠️  Instance is not running. Start it with:"
    echo "   curl -X POST $API_URL/v1/core/instance/$INSTANCE_ID/start"
    echo ""
fi

if [ -z "$RECORD_PATH" ] || [ "$RECORD_PATH" = "null" ] || [ "$RECORD_PATH" = "" ]; then
    echo "⚠️  RECORD_PATH not configured. Configure it with:"
    echo "   curl -X POST $API_URL/v1/core/instance/$INSTANCE_ID/output/stream \\"
    echo "     -H 'Content-Type: application/json' \\"
    echo "     -d '{\"enabled\": true, \"path\": \"/mnt/sb1/data\"}'"
    echo ""
    echo "   Then restart the instance:"
    echo "   curl -X POST $API_URL/v1/core/instance/$INSTANCE_ID/stop"
    echo "   curl -X POST $API_URL/v1/core/instance/$INSTANCE_ID/start"
    echo ""
fi

if [ "$FILE_COUNT" -eq 0 ] && [ "$RUNNING" = "true" ]; then
    echo "⚠️  No files found but instance is running."
    echo "   Possible reasons:"
    echo "   1. No video input (check input source)"
    echo "   2. file_des node not added to pipeline (check logs for 'RECORD_PATH detected')"
    echo "   3. file_des node failed to create (check logs for errors)"
    echo ""
    echo "   Check logs:"
    echo "   journalctl -u edge-ai-api -n 100 | grep -i 'record_path\|file_des'"
    echo "   or"
    echo "   tail -f /opt/edge_ai_api/logs/*.log | grep -i 'record_path\|file_des'"
    echo ""
fi

