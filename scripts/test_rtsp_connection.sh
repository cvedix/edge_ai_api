#!/bin/bash
# Script kiểm tra kết nối RTSP

RTSP_URL="${1:-rtsp://100.76.5.84:8554/mystream}"

echo "=========================================="
echo "Kiểm tra kết nối RTSP"
echo "=========================================="
echo "RTSP URL: $RTSP_URL"
echo ""

# 1. Kiểm tra ping
echo "1. Kiểm tra ping đến server..."
if ping -c 3 -W 2 $(echo $RTSP_URL | sed -E 's|rtsp://([^:/]+).*|\1|') > /dev/null 2>&1; then
    echo "   ✓ Ping thành công"
else
    echo "   ✗ Ping thất bại - Server không thể truy cập"
fi
echo ""

# 2. Kiểm tra port
HOST=$(echo $RTSP_URL | sed -E 's|rtsp://([^:/]+).*|\1|')
PORT=$(echo $RTSP_URL | sed -E 's|rtsp://[^:]+:([0-9]+).*|\1|')
if [ -z "$PORT" ]; then
    PORT=554  # Default RTSP port
fi

echo "2. Kiểm tra port $PORT..."
if timeout 3 bash -c "echo > /dev/tcp/$HOST/$PORT" 2>/dev/null; then
    echo "   ✓ Port $PORT mở"
else
    echo "   ✗ Port $PORT đóng hoặc không thể truy cập"
fi
echo ""

# 3. Kiểm tra với ffprobe (nếu có)
if command -v ffprobe &> /dev/null; then
    echo "3. Kiểm tra với ffprobe..."
    timeout 10 ffprobe -v error -rtsp_transport tcp "$RTSP_URL" 2>&1 | head -20
    echo ""
fi

# 4. Kiểm tra với gst-launch-1.0 (nếu có)
if command -v gst-launch-1.0 &> /dev/null; then
    echo "4. Kiểm tra với GStreamer..."
    timeout 10 gst-launch-1.0 -v rtspsrc location="$RTSP_URL" protocols=tcp latency=0 ! fakesink 2>&1 | grep -E "(ERROR|WARN|connecting|connected)" | head -10
    echo ""
fi

echo "=========================================="
echo "Kết luận:"
echo "- Nếu ping thất bại: Server không thể truy cập từ mạng này"
echo "- Nếu port đóng: RTSP service không chạy hoặc firewall chặn"
echo "- Kiểm tra RTSP server có đang chạy không"
echo "=========================================="



