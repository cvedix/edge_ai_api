#!/bin/bash

# Check RTSP Instance Status
# Usage: ./scripts/check_rtsp_instance.sh [INSTANCE_ID]

INSTANCE_ID="${1:-7d18bcaa-c2d9-4ad2-9ad1-6b626edd3377}"
API_URL="http://localhost:8080/v1/core/instances"

echo "=========================================="
echo "Checking RTSP Instance Status"
echo "=========================================="
echo "Instance ID: $INSTANCE_ID"
echo ""

# Get instance info
RESPONSE=$(curl -s "$API_URL/$INSTANCE_ID")

if [ $? -ne 0 ]; then
    echo "❌ Failed to connect to API"
    exit 1
fi

# Check if instance exists
if echo "$RESPONSE" | grep -q "not found"; then
    echo "❌ Instance not found"
    exit 1
fi

# Extract key info using python for better JSON parsing
FPS=$(echo "$RESPONSE" | python3 -c "import sys, json; d=json.load(sys.stdin); print(d.get('metrics', {}).get('fps', 0))" 2>/dev/null || echo "0")
RUNNING=$(echo "$RESPONSE" | python3 -c "import sys, json; d=json.load(sys.stdin); print(d.get('running', False))" 2>/dev/null || echo "false")
INPUT_TYPE=$(echo "$RESPONSE" | python3 -c "import sys, json; d=json.load(sys.stdin); print(d.get('input', {}).get('type', 'N/A'))" 2>/dev/null || echo "N/A")
INPUT_URL=$(echo "$RESPONSE" | python3 -c "import sys, json; d=json.load(sys.stdin); print(d.get('input', {}).get('url', 'N/A'))" 2>/dev/null || echo "N/A")

echo "Status: $RUNNING"
echo "Input Type: $INPUT_TYPE"
if [ -n "$INPUT_URL" ]; then
    echo "Input URL: $INPUT_URL"
fi
echo "FPS: $FPS"
echo ""

# Analysis
if [ "$RUNNING" = "true" ]; then
    if [ -n "$FPS" ] && [ "$(echo "$FPS > 0" | bc 2>/dev/null || echo 0)" = "1" ]; then
        echo "✅ Instance is running and receiving frames"
        echo "   → RTSP connection successful"
        echo "   → Pipeline is processing frames"
    else
        echo "⚠️  Instance is running but FPS = 0"
        echo "   → RTSP connection may be established but no frames received yet"
        echo "   → Wait 10-30 seconds for stream to stabilize"
        echo "   → Check RTSP stream is actually sending data"
    fi
else
    echo "❌ Instance is not running"
    echo "   → Check if instance was started"
    echo "   → Check logs for errors"
fi

echo ""
echo "Full response:"
echo "$RESPONSE" | jq '.' 2>/dev/null || echo "$RESPONSE"

