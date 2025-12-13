#!/bin/bash
# RTSP Helper Script
# Gộp các chức năng: check, debug, diagnose, test cho RTSP

INSTANCE_ID="${1}"
RTSP_URL="${2}"
ACTION="${3:-check}"  # check, debug, diagnose, test
API_URL="${API_URL:-http://localhost:8080}"

case "$ACTION" in
    check)
        if [ -z "$INSTANCE_ID" ]; then
            echo "Usage: $0 <instanceId> <rtsp_url> check"
            exit 1
        fi
        echo "=========================================="
        echo "Checking RTSP Instance Status"
        echo "=========================================="
        echo "Instance ID: $INSTANCE_ID"
        echo ""
        
        RESPONSE=$(curl -s "$API_URL/v1/core/instance/$INSTANCE_ID" 2>/dev/null)
        if [ $? -ne 0 ] || echo "$RESPONSE" | grep -q "not found"; then
            echo "❌ Instance not found"
            exit 1
        fi
        
        FPS=$(echo "$RESPONSE" | jq -r '.fps // 0' 2>/dev/null || echo "0")
        RUNNING=$(echo "$RESPONSE" | jq -r '.running // false' 2>/dev/null || echo "false")
        INPUT_TYPE=$(echo "$RESPONSE" | jq -r '.input.type // "N/A"' 2>/dev/null || echo "N/A")
        INPUT_URL=$(echo "$RESPONSE" | jq -r '.input.url // "N/A"' 2>/dev/null || echo "N/A")
        
        echo "Status: $RUNNING"
        echo "Input Type: $INPUT_TYPE"
        echo "Input URL: $INPUT_URL"
        echo "FPS: $FPS"
        echo ""
        
        if [ "$RUNNING" = "true" ]; then
            if [ -n "$FPS" ] && [ "$(echo "$FPS > 0" | bc 2>/dev/null || echo 0)" = "1" ]; then
                echo "✅ Instance is running and receiving frames"
            else
                echo "⚠️  Instance is running but FPS = 0"
                echo "   → Wait 10-30 seconds for stream to stabilize"
            fi
        else
            echo "❌ Instance is not running"
        fi
        ;;
        
    debug)
        if [ -z "$INSTANCE_ID" ]; then
            echo "Usage: $0 <instanceId> <rtsp_url> debug"
            exit 1
        fi
        echo "=========================================="
        echo "Debugging RTSP Pipeline"
        echo "=========================================="
        echo "Instance ID: $INSTANCE_ID"
        echo ""
        
        # Check instance status
        echo "1. Checking instance status..."
        STATUS=$(curl -s "$API_URL/v1/core/instances" 2>/dev/null | jq ".instances[] | select(.instanceId == \"$INSTANCE_ID\")" 2>/dev/null)
        RUNNING=$(echo "$STATUS" | jq -r '.running // false' 2>/dev/null)
        FPS=$(echo "$STATUS" | jq -r '.fps // 0' 2>/dev/null)
        echo "   Running: $RUNNING"
        echo "   FPS: $FPS"
        echo ""
        
        # Test RTSP stream
        if [ -n "$RTSP_URL" ]; then
            echo "2. Testing RTSP stream accessibility..."
            timeout 5 ffmpeg -i "$RTSP_URL" -t 1 -f null - 2>&1 | grep -E "(Input|Stream|Video|Could not)" | head -5
            echo ""
        fi
        
        # Check GStreamer processes
        echo "3. Checking GStreamer processes..."
        ps aux | grep -E "gst|rtspsrc|rtmpsink" | grep -v grep | head -5
        echo ""
        
        # Check network
        if [ -n "$RTSP_URL" ]; then
            HOST=$(echo "$RTSP_URL" | sed -n 's|rtsp://\([^:/]*\).*|\1|p')
            echo "4. Checking network connectivity..."
            ping -c 2 -W 2 "$HOST" > /dev/null 2>&1 && echo "   ✓ Server is reachable" || echo "   ✗ Server is not reachable"
            nc -zv -w 2 "$HOST" 8554 > /dev/null 2>&1 && echo "   ✓ RTSP port 8554 is open" || echo "   ✗ RTSP port 8554 is closed"
        fi
        echo ""
        
        echo "Recommendations:"
        echo "1. Enable GStreamer debug: export GST_DEBUG=rtspsrc:4,rtmpsink:4"
        echo "2. Wait 30-60 seconds for RTSP connection to establish"
        ;;
        
    diagnose)
        if [ -z "$RTSP_URL" ]; then
            echo "Usage: $0 <instanceId> <rtsp_url> diagnose"
            exit 1
        fi
        echo "=========================================="
        echo "RTSP Connection Diagnostic"
        echo "=========================================="
        echo "RTSP URL: $RTSP_URL"
        echo ""
        
        HOST=$(echo "$RTSP_URL" | sed -n 's|rtsp://\([^:/]*\).*|\1|p')
        PORT=$(echo "$RTSP_URL" | sed -n 's|rtsp://[^:]*:\([0-9]*\).*|\1|p')
        if [ -z "$PORT" ]; then
            PORT=554
        fi
        
        echo "1. Testing network connectivity..."
        echo "   Host: $HOST"
        echo "   Port: $PORT"
        echo ""
        
        if ping -c 2 -W 2 "$HOST" > /dev/null 2>&1; then
            echo "   ✓ Ping successful"
        else
            echo "   ✗ Ping failed"
        fi
        
        if timeout 3 bash -c "cat < /dev/null > /dev/tcp/$HOST/$PORT" 2>/dev/null; then
            echo "   ✓ Port $PORT is open"
        else
            echo "   ✗ Port $PORT is closed or unreachable"
        fi
        echo ""
        
        echo "2. Testing RTSP stream with ffmpeg..."
        if command -v ffmpeg > /dev/null 2>&1; then
            timeout 5 ffmpeg -i "$RTSP_URL" -t 1 -f null - 2>&1 | head -20
        else
            echo "   ⚠ ffmpeg not installed"
        fi
        echo ""
        
        echo "3. Testing RTSP stream with GStreamer..."
        if command -v gst-launch-1.0 > /dev/null 2>&1; then
            timeout 5 gst-launch-1.0 rtspsrc location="$RTSP_URL" ! fakesink 2>&1 | head -20
        else
            echo "   ⚠ gst-launch-1.0 not installed"
        fi
        echo ""
        
        echo "Recommendations:"
        echo "1. If ping fails: Check network connectivity"
        echo "2. If port is closed: Check firewall rules"
        echo "3. Enable GStreamer debug: export GST_DEBUG=rtspsrc:4"
        ;;
        
    test)
        if [ -z "$RTSP_URL" ]; then
            echo "Usage: $0 <instanceId> <rtsp_url> test"
            exit 1
        fi
        echo "Testing RTSP connection: $RTSP_URL"
        if command -v ffprobe > /dev/null 2>&1; then
            ffprobe -v error -show_entries stream=codec_name "$RTSP_URL" 2>&1 | head -5
        else
            echo "⚠ ffprobe not installed"
        fi
        ;;
        
    *)
        echo "Usage: $0 <instanceId> [rtsp_url] [action]"
        echo "Actions: check, debug, diagnose, test"
        echo ""
        echo "Examples:"
        echo "  $0 <instanceId> <rtsp_url> check     # Check instance status"
        echo "  $0 <instanceId> <rtsp_url> debug     # Debug pipeline"
        echo "  $0 <instanceId> <rtsp_url> diagnose   # Diagnose RTSP connection"
        echo "  $0 <instanceId> <rtsp_url> test     # Test RTSP stream"
        exit 1
        ;;
esac

