#!/bin/bash

# RTSP Connection Diagnostic Script
# Usage: ./scripts/diagnose_rtsp.sh <RTSP_URL>

RTSP_URL="${1:-rtsp://localhost:8554/live/camera_demo_sang_vehicle}"

echo "=========================================="
echo "RTSP Connection Diagnostic"
echo "=========================================="
echo "RTSP URL: $RTSP_URL"
echo ""

# Extract host and port
HOST=$(echo $RTSP_URL | sed -n 's|rtsp://\([^:/]*\).*|\1|p')
PORT=$(echo $RTSP_URL | sed -n 's|rtsp://[^:]*:\([0-9]*\).*|\1|p')
if [ -z "$PORT" ]; then
    PORT=554  # Default RTSP port
fi

echo "1. Testing network connectivity..."
echo "   Host: $HOST"
echo "   Port: $PORT"
echo ""

# Test ping
echo "   Testing ping..."
if ping -c 2 -W 2 "$HOST" > /dev/null 2>&1; then
    echo "   ✓ Ping successful"
else
    echo "   ✗ Ping failed - host may be unreachable"
fi
echo ""

# Test port
echo "   Testing port $PORT..."
if timeout 3 bash -c "cat < /dev/null > /dev/tcp/$HOST/$PORT" 2>/dev/null; then
    echo "   ✓ Port $PORT is open"
else
    echo "   ✗ Port $PORT is closed or unreachable"
    echo "   → Check firewall rules"
fi
echo ""

# Test RTSP with ffmpeg
echo "2. Testing RTSP stream with ffmpeg..."
if command -v ffmpeg > /dev/null 2>&1; then
    echo "   Attempting to connect (5 second timeout)..."
    timeout 5 ffmpeg -i "$RTSP_URL" -t 1 -f null - 2>&1 | head -20
    FFMPEG_EXIT=$?
    if [ $FFMPEG_EXIT -eq 0 ]; then
        echo "   ✓ RTSP stream is accessible"
    elif [ $FFMPEG_EXIT -eq 124 ]; then
        echo "   ⚠ Timeout - stream may be slow or not responding"
    else
        echo "   ✗ RTSP stream connection failed"
        echo "   → Check RTSP URL format and stream availability"
    fi
else
    echo "   ⚠ ffmpeg not installed - skipping RTSP test"
fi
echo ""

# Test RTSP with gst-launch
echo "3. Testing RTSP stream with GStreamer..."
if command -v gst-launch-1.0 > /dev/null 2>&1; then
    echo "   Attempting to connect (5 second timeout)..."
    timeout 5 gst-launch-1.0 rtspsrc location="$RTSP_URL" ! fakesink 2>&1 | head -20
    GST_EXIT=$?
    if [ $GST_EXIT -eq 0 ]; then
        echo "   ✓ GStreamer can connect to RTSP stream"
    elif [ $GST_EXIT -eq 124 ]; then
        echo "   ⚠ Timeout - stream may be slow or not responding"
    else
        echo "   ✗ GStreamer RTSP connection failed"
        echo "   → This is the same library used by CVEDIX SDK"
    fi
else
    echo "   ⚠ gst-launch-1.0 not installed - skipping GStreamer test"
fi
echo ""

# Check environment variables
echo "4. Checking GStreamer environment..."
if [ -n "$GST_RTSP_PROTOCOLS" ]; then
    echo "   GST_RTSP_PROTOCOLS=$GST_RTSP_PROTOCOLS"
else
    echo "   GST_RTSP_PROTOCOLS not set (default: tcp+udp)"
fi
echo ""

# Recommendations
echo "=========================================="
echo "Recommendations:"
echo "=========================================="
echo "1. If ping fails: Check network connectivity"
echo "2. If port is closed: Check firewall rules"
echo "3. If ffmpeg/gst-launch fails:"
echo "   - Verify RTSP stream is running"
echo "   - Check RTSP URL format"
echo "   - Try with authentication: rtsp://user:pass@host:port/path"
echo "4. Enable GStreamer debug:"
echo "   export GST_DEBUG=rtspsrc:4"
echo "   export GST_DEBUG_FILE=/tmp/gst_debug.log"
echo "5. Check CVEDIX SDK logs for connection errors"
echo ""

