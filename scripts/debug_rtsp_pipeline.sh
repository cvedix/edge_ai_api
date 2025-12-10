#!/bin/bash

# Debug RTSP Pipeline - Check why pipeline is stuck after start
# Usage: ./scripts/debug_rtsp_pipeline.sh [INSTANCE_ID]

INSTANCE_ID="${1:-7d18bcaa-c2d9-4ad2-9ad1-6b626edd3377}"

echo "=========================================="
echo "Debugging RTSP Pipeline"
echo "=========================================="
echo "Instance ID: $INSTANCE_ID"
echo ""

# 1. Check instance status
echo "1. Checking instance status..."
STATUS=$(curl -s "http://localhost:8080/v1/core/instances" | python3 -c "
import sys, json
d = json.load(sys.stdin)
inst = next((i for i in d.get('instances', []) if i.get('instanceId') == '$INSTANCE_ID'), {})
print(f\"Running: {inst.get('running', False)}\")
print(f\"FPS: {inst.get('fps', 0)}\")
print(f\"Loaded: {inst.get('loaded', False)}\")
" 2>/dev/null)
echo "$STATUS"
echo ""

# 2. Check RTSP stream accessibility
echo "2. Testing RTSP stream accessibility..."
RTSP_URL="rtsp://anhoidong.datacenter.cvedix.com:8554/live/camera_demo_sang_vehicle"
timeout 5 ffmpeg -i "$RTSP_URL" -t 1 -f null - 2>&1 | grep -E "(Input|Stream|Video|Could not)" | head -5
echo ""

# 3. Check GStreamer processes
echo "3. Checking GStreamer processes..."
ps aux | grep -E "gst|rtspsrc|rtmpsink" | grep -v grep | head -5
echo ""

# 4. Check network connectivity
echo "4. Checking network connectivity..."
ping -c 2 -W 2 anhoidong.datacenter.cvedix.com > /dev/null 2>&1 && echo "✓ Server is reachable" || echo "✗ Server is not reachable"
nc -zv -w 2 anhoidong.datacenter.cvedix.com 8554 > /dev/null 2>&1 && echo "✓ RTSP port 8554 is open" || echo "✗ RTSP port 8554 is closed"
nc -zv -w 2 anhoidong.datacenter.cvedix.com 1935 > /dev/null 2>&1 && echo "✓ RTMP port 1935 is open" || echo "✗ RTMP port 1935 is closed"
echo ""

# 5. Recommendations
echo "=========================================="
echo "Recommendations:"
echo "=========================================="
echo "1. Enable GStreamer debug:"
echo "   export GST_DEBUG=rtspsrc:4,rtmpsink:4"
echo "   # Restart service"
echo ""
echo "2. Check CVEDIX SDK logs for RTSP connection:"
echo "   # Look for 'rtspsrc' messages in terminal running edge_ai_api"
echo ""
echo "3. Wait 30-60 seconds for RTSP connection to establish"
echo ""
echo "4. Check if RTSP stream is actually streaming:"
echo "   ffplay $RTSP_URL"
echo ""

