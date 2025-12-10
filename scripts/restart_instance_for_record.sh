#!/bin/bash
# Script to restart instance after configuring record output

INSTANCE_ID="${1}"
API_URL="http://localhost:3546"

if [ -z "$INSTANCE_ID" ]; then
    echo "Usage: $0 <instanceId>"
    echo "Example: $0 82d819ba-9f4c-4fc5-b44c-28c23d9f6ca2"
    exit 1
fi

echo "=========================================="
echo "Restart Instance for Record Output"
echo "=========================================="
echo "Instance ID: $INSTANCE_ID"
echo ""

# 1. Check current status
echo "1. Current status:"
STATUS=$(curl -s "$API_URL/v1/core/instances" | jq ".instances[] | select(.instanceId == \"$INSTANCE_ID\")" 2>/dev/null)
if [ -z "$STATUS" ]; then
    echo "   ❌ Instance not found!"
    exit 1
fi

RUNNING=$(echo "$STATUS" | jq -r '.running // false')
FPS=$(echo "$STATUS" | jq -r '.fps // 0')
echo "   Running: $RUNNING"
echo "   FPS: $FPS"
echo ""

# 2. Check RECORD_PATH
echo "2. Checking RECORD_PATH:"
RECORD_PATH=$(curl -s "$API_URL/v1/core/instance/$INSTANCE_ID/config" | jq -r '.AdditionalParams.RECORD_PATH // empty' 2>/dev/null)
if [ -n "$RECORD_PATH" ] && [ "$RECORD_PATH" != "null" ] && [ "$RECORD_PATH" != "" ]; then
    echo "   ✓ RECORD_PATH: $RECORD_PATH"
else
    echo "   ❌ RECORD_PATH not configured!"
    echo "   Configure it first with:"
    echo "   curl -X POST $API_URL/v1/core/instance/$INSTANCE_ID/output/stream \\"
    echo "     -H 'Content-Type: application/json' \\"
    echo "     -d '{\"enabled\": true, \"path\": \"/mnt/sb1/data\"}'"
    exit 1
fi
echo ""

# 3. Stop instance
echo "3. Stopping instance..."
STOP_RESPONSE=$(curl -s -w "\nHTTP_CODE:%{http_code}" -X POST "$API_URL/v1/core/instances/$INSTANCE_ID/stop")
HTTP_CODE=$(echo "$STOP_RESPONSE" | grep "HTTP_CODE" | cut -d: -f2)
BODY=$(echo "$STOP_RESPONSE" | grep -v "HTTP_CODE")
if [ "$HTTP_CODE" = "204" ] || [ "$HTTP_CODE" = "200" ]; then
    echo "   ✓ Instance stopped successfully"
else
    echo "   Response (HTTP $HTTP_CODE): $BODY"
fi
sleep 2
echo ""

# 4. Check if stopped
echo "4. Verifying instance stopped..."
STATUS_AFTER_STOP=$(curl -s "$API_URL/v1/core/instances" | jq ".instances[] | select(.instanceId == \"$INSTANCE_ID\") | .running" 2>/dev/null)
if [ "$STATUS_AFTER_STOP" = "true" ]; then
    echo "   ⚠️  Instance still running, waiting..."
    sleep 3
    STATUS_AFTER_STOP=$(curl -s "$API_URL/v1/core/instances" | jq ".instances[] | select(.instanceId == \"$INSTANCE_ID\") | .running" 2>/dev/null)
fi
echo "   Stopped: $([ "$STATUS_AFTER_STOP" = "false" ] && echo "Yes ✓" || echo "No (still running)")"
echo ""

# 5. Start instance
echo "5. Starting instance..."
START_RESPONSE=$(curl -s -w "\nHTTP_CODE:%{http_code}" -X POST "$API_URL/v1/core/instances/$INSTANCE_ID/start")
HTTP_CODE=$(echo "$START_RESPONSE" | grep "HTTP_CODE" | cut -d: -f2)
BODY=$(echo "$START_RESPONSE" | grep -v "HTTP_CODE")
if [ "$HTTP_CODE" = "204" ] || [ "$HTTP_CODE" = "200" ]; then
    echo "   ✓ Instance started successfully"
else
    echo "   Response (HTTP $HTTP_CODE): $BODY"
fi
sleep 3
echo ""

# 6. Check status after start
echo "6. Verifying instance started..."
STATUS_AFTER_START=$(curl -s "$API_URL/v1/core/instances" | jq ".instances[] | select(.instanceId == \"$INSTANCE_ID\")" 2>/dev/null)
RUNNING_AFTER=$(echo "$STATUS_AFTER_START" | jq -r '.running // false')
FPS_AFTER=$(echo "$STATUS_AFTER_START" | jq -r '.fps // 0')
echo "   Running: $RUNNING_AFTER"
echo "   FPS: $FPS_AFTER"
echo ""

# 7. Check directory
echo "7. Checking save directory:"
if [ -d "$RECORD_PATH" ]; then
    FILE_COUNT=$(ls -1 "$RECORD_PATH" 2>/dev/null | wc -l)
    echo "   Directory: $RECORD_PATH"
    echo "   Files: $FILE_COUNT"
    if [ "$FILE_COUNT" -gt 0 ]; then
        echo "   Recent files:"
        ls -lht "$RECORD_PATH" | head -5 | awk '{print "     " $0}'
    fi
else
    echo "   ❌ Directory does not exist: $RECORD_PATH"
fi
echo ""

# 8. Recommendations
echo "=========================================="
echo "Next Steps:"
echo "=========================================="

if [ "$FPS_AFTER" = "0" ] || [ "$FPS_AFTER" = "0.0" ]; then
    echo "⚠️  FPS = 0 - No frames being processed"
    echo "   - Check video input source"
    echo "   - Check if input file/stream is accessible"
    echo "   - Wait a few seconds and check FPS again"
    echo ""
fi

if [ "$FILE_COUNT" -eq 0 ]; then
    echo "⚠️  No files yet"
    echo "   - Wait for video to start processing (FPS > 0)"
    echo "   - Files will appear when frames are processed"
    echo "   - Check logs for 'RECORD_PATH detected' and 'Auto-added file_des node'"
    echo ""
fi

echo "Monitor FPS:"
echo "  watch -n 1 'curl -s $API_URL/v1/core/instances | jq \".instances[] | select(.instanceId == \\\"$INSTANCE_ID\\\") | {fps, running}\"'"
echo ""
echo "Check files:"
echo "  watch -n 1 'ls -lht $RECORD_PATH | head -10'"
echo ""

