#!/bin/bash
# Record Output Helper Script
# Gộp các chức năng: check, debug, restart cho record output

INSTANCE_ID="${1}"
ACTION="${2:-check}"  # check, debug, restart
API_URL="${API_URL:-http://localhost:8080}"

if [ -z "$INSTANCE_ID" ] && [ "$ACTION" != "help" ]; then
    echo "Usage: $0 <instanceId> [action]"
    echo "Actions: check, debug, restart"
    echo ""
    echo "Examples:"
    echo "  $0 <instanceId> check    # Quick status check"
    echo "  $0 <instanceId> debug    # Detailed debugging"
    echo "  $0 <instanceId> restart  # Restart instance for record"
    exit 1
fi

case "$ACTION" in
    check)
        echo "=========================================="
        echo "Record Output Status Check"
        echo "=========================================="
        echo "Instance ID: $INSTANCE_ID"
        echo ""
        
        # Check output stream config
        echo "1. Output Stream Config:"
        OUTPUT=$(curl -s "$API_URL/v1/core/instance/$INSTANCE_ID/output/stream" 2>/dev/null)
        echo "$OUTPUT" | jq '.' 2>/dev/null || echo "$OUTPUT"
        echo ""
        
        # Check RECORD_PATH in config
        echo "2. RECORD_PATH in Config:"
        RECORD_PATH=$(curl -s "$API_URL/v1/core/instance/$INSTANCE_ID/config" 2>/dev/null | jq -r '.AdditionalParams.RECORD_PATH // empty' 2>/dev/null)
        if [ -n "$RECORD_PATH" ] && [ "$RECORD_PATH" != "null" ] && [ "$RECORD_PATH" != "" ]; then
            echo "   ✓ RECORD_PATH: $RECORD_PATH"
        else
            echo "   ❌ RECORD_PATH not found!"
        fi
        echo ""
        
        # Check instance status
        echo "3. Instance Status:"
        STATUS=$(curl -s "$API_URL/v1/core/instances" 2>/dev/null | jq ".instances[] | select(.instanceId == \"$INSTANCE_ID\")" 2>/dev/null)
        RUNNING=$(echo "$STATUS" | jq -r '.running // false' 2>/dev/null)
        FPS=$(echo "$STATUS" | jq -r '.fps // 0' 2>/dev/null)
        echo "   Running: $RUNNING"
        echo "   FPS: $FPS"
        if [ "$FPS" = "0" ] || [ "$FPS" = "0.0" ]; then
            echo "   ⚠️  WARNING: FPS = 0 - No frames being processed!"
        fi
        echo ""
        
        # Check directory
        echo "4. Save Directory:"
        SAVE_DIR="${RECORD_PATH:-/mnt/sb1/data}"
        if [ -d "$SAVE_DIR" ]; then
            echo "   ✓ Directory exists: $SAVE_DIR"
            FILE_COUNT=$(ls -1 "$SAVE_DIR" 2>/dev/null | wc -l)
            echo "   Files: $FILE_COUNT"
            if [ "$FILE_COUNT" -gt 0 ]; then
                echo "   Recent files:"
                ls -lht "$SAVE_DIR" 2>/dev/null | head -5 | awk '{print "     " $0}'
            fi
        else
            echo "   ❌ Directory does not exist: $SAVE_DIR"
        fi
        ;;
        
    debug)
        echo "=========================================="
        echo "Debug Record Output Configuration"
        echo "=========================================="
        echo "Instance ID: $INSTANCE_ID"
        echo ""
        
        # Check instance status
        echo "1. Checking instance status..."
        INSTANCE_INFO=$(curl -s "$API_URL/v1/core/instance/$INSTANCE_ID" 2>/dev/null)
        if echo "$INSTANCE_INFO" | grep -q "404\|Not Found"; then
            echo "   ❌ Instance not found!"
            exit 1
        fi
        
        RUNNING=$(echo "$INSTANCE_INFO" | jq -r '.running // false' 2>/dev/null || echo "false")
        LOADED=$(echo "$INSTANCE_INFO" | jq -r '.loaded // false' 2>/dev/null || echo "false")
        FPS=$(echo "$INSTANCE_INFO" | jq -r '.fps // 0' 2>/dev/null || echo "0")
        echo "   Running: $RUNNING"
        echo "   Loaded: $LOADED"
        echo "   FPS: $FPS"
        echo ""
        
        # Check output stream config
        echo "2. Checking output stream configuration..."
        OUTPUT_CONFIG=$(curl -s "$API_URL/v1/core/instance/$INSTANCE_ID/output/stream" 2>/dev/null)
        echo "$OUTPUT_CONFIG" | jq '.' 2>/dev/null || echo "$OUTPUT_CONFIG"
        echo ""
        
        # Check RECORD_PATH
        echo "3. Checking RECORD_PATH in instance config..."
        INSTANCE_CONFIG=$(curl -s "$API_URL/v1/core/instance/$INSTANCE_ID/config" 2>/dev/null)
        RECORD_PATH=$(echo "$INSTANCE_CONFIG" | jq -r '.AdditionalParams.RECORD_PATH // empty' 2>/dev/null)
        if [ -n "$RECORD_PATH" ] && [ "$RECORD_PATH" != "null" ] && [ "$RECORD_PATH" != "" ]; then
            echo "   ✓ RECORD_PATH found: $RECORD_PATH"
        else
            echo "   ❌ RECORD_PATH not found in config!"
        fi
        echo ""
        
        # Check directory
        echo "4. Checking save directory..."
        SAVE_DIR="${RECORD_PATH:-/mnt/sb1/data}"
        if [ -d "$SAVE_DIR" ]; then
            echo "   ✓ Directory exists: $SAVE_DIR"
            echo "   Permissions: $(ls -ld "$SAVE_DIR" 2>/dev/null | awk '{print $1, $3, $4}')"
            if [ -w "$SAVE_DIR" ]; then
                echo "   ✓ Directory is writable"
            else
                echo "   ❌ Directory is NOT writable!"
            fi
            FILE_COUNT=$(ls -1 "$SAVE_DIR" 2>/dev/null | wc -l)
            echo "   Files in directory: $FILE_COUNT"
        else
            echo "   ❌ Directory does not exist: $SAVE_DIR"
        fi
        echo ""
        
        # Check process
        echo "5. Checking edge_ai_api process..."
        if pgrep -f "edge_ai_api" > /dev/null; then
            echo "   ✓ Process is running"
        else
            echo "   ❌ Process is NOT running!"
        fi
        echo ""
        
        # Recommendations
        echo "=========================================="
        echo "Recommendations:"
        echo "=========================================="
        if [ "$RUNNING" != "true" ]; then
            echo "⚠️  Instance is not running. Start it with:"
            echo "   curl -X POST $API_URL/v1/core/instance/$INSTANCE_ID/start"
            echo ""
        fi
        if [ -z "$RECORD_PATH" ] || [ "$RECORD_PATH" = "null" ]; then
            echo "⚠️  RECORD_PATH not configured. Configure it with:"
            echo "   curl -X POST $API_URL/v1/core/instance/$INSTANCE_ID/output/stream \\"
            echo "     -H 'Content-Type: application/json' \\"
            echo "     -d '{\"enabled\": true, \"path\": \"/mnt/sb1/data\"}'"
            echo ""
        fi
        if [ "$FPS" = "0" ] || [ "$FPS" = "0.0" ]; then
            echo "⚠️  FPS = 0 - No frames being processed"
            echo "   - Check video input source"
            echo "   - Check if input file/stream is accessible"
            echo ""
        fi
        ;;
        
    restart)
        echo "=========================================="
        echo "Restart Instance for Record Output"
        echo "=========================================="
        echo "Instance ID: $INSTANCE_ID"
        echo ""
        
        # Check RECORD_PATH
        echo "1. Checking RECORD_PATH:"
        RECORD_PATH=$(curl -s "$API_URL/v1/core/instance/$INSTANCE_ID/config" 2>/dev/null | jq -r '.AdditionalParams.RECORD_PATH // empty' 2>/dev/null)
        if [ -z "$RECORD_PATH" ] || [ "$RECORD_PATH" = "null" ] || [ "$RECORD_PATH" = "" ]; then
            echo "   ❌ RECORD_PATH not configured!"
            echo "   Configure it first with:"
            echo "   curl -X POST $API_URL/v1/core/instance/$INSTANCE_ID/output/stream \\"
            echo "     -H 'Content-Type: application/json' \\"
            echo "     -d '{\"enabled\": true, \"path\": \"/mnt/sb1/data\"}'"
            exit 1
        fi
        echo "   ✓ RECORD_PATH: $RECORD_PATH"
        echo ""
        
        # Stop instance
        echo "2. Stopping instance..."
        curl -s -X POST "$API_URL/v1/core/instance/$INSTANCE_ID/stop" > /dev/null 2>&1
        sleep 2
        echo "   ✓ Instance stopped"
        echo ""
        
        # Start instance
        echo "3. Starting instance..."
        curl -s -X POST "$API_URL/v1/core/instance/$INSTANCE_ID/start" > /dev/null 2>&1
        sleep 3
        echo "   ✓ Instance started"
        echo ""
        
        # Check status
        echo "4. Verifying instance status..."
        STATUS=$(curl -s "$API_URL/v1/core/instances" 2>/dev/null | jq ".instances[] | select(.instanceId == \"$INSTANCE_ID\")" 2>/dev/null)
        RUNNING=$(echo "$STATUS" | jq -r '.running // false' 2>/dev/null)
        FPS=$(echo "$STATUS" | jq -r '.fps // 0' 2>/dev/null)
        echo "   Running: $RUNNING"
        echo "   FPS: $FPS"
        echo ""
        
        # Check directory
        echo "5. Checking save directory:"
        if [ -d "$RECORD_PATH" ]; then
            FILE_COUNT=$(ls -1 "$RECORD_PATH" 2>/dev/null | wc -l)
            echo "   Directory: $RECORD_PATH"
            echo "   Files: $FILE_COUNT"
        else
            echo "   ❌ Directory does not exist: $RECORD_PATH"
        fi
        echo ""
        
        echo "=========================================="
        echo "Next Steps:"
        echo "=========================================="
        if [ "$FPS" = "0" ] || [ "$FPS" = "0.0" ]; then
            echo "⚠️  FPS = 0 - Check video input source"
        fi
        echo "Monitor: watch -n 1 'curl -s $API_URL/v1/core/instances | jq \".instances[] | select(.instanceId == \\\"$INSTANCE_ID\\\") | {fps, running}\"'"
        ;;
        
    *)
        echo "Unknown action: $ACTION"
        echo "Usage: $0 <instanceId> [check|debug|restart]"
        exit 1
        ;;
esac

