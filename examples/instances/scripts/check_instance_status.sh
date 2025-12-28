#!/bin/bash

# Script kiểm tra trạng thái và kết quả xử lý của instance
# Sử dụng: ./check_instance_status.sh [INSTANCE_ID] [BASE_URL]
# Mặc định: BASE_URL=http://localhost:8848

INSTANCE_ID="$1"
BASE_URL="${2:-http://localhost:8848}"
API_BASE="${BASE_URL}/v1/core"

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
CYAN='\033[0;36m'
NC='\033[0m'

# Function to print section header
print_section() {
    echo ""
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
}

# Function to print status
print_status() {
    local status=$1
    local message=$2
    if [ "$status" = "OK" ] || [ "$status" = "SUCCESS" ]; then
        echo -e "${GREEN}✓${NC} $message"
    elif [ "$status" = "WARNING" ]; then
        echo -e "${YELLOW}⚠${NC} $message"
    elif [ "$status" = "ERROR" ]; then
        echo -e "${RED}✗${NC} $message"
    else
        echo -e "${CYAN}ℹ${NC} $message"
    fi
}

# Check if instance ID is provided
if [ -z "$INSTANCE_ID" ]; then
    echo "Usage: $0 <INSTANCE_ID> [BASE_URL]"
    echo ""
    echo "Example:"
    echo "  $0 abc-123-def-456"
    echo "  $0 abc-123-def-456 http://localhost:8848"
    echo ""
    echo "Available instances:"
    curl -s "${API_BASE}/instances" | jq -r '.instances[] | "  - \(.instanceId) (\(.displayName)) [Running: \(.running)]"'
    exit 1
fi

print_section "Kiểm tra trạng thái Instance: $INSTANCE_ID"

# 1. Kiểm tra instance có tồn tại không
print_section "1. Kiểm tra Instance có tồn tại"

INSTANCE_INFO=$(curl -s "${API_BASE}/instances/${INSTANCE_ID}")
if [ $? -ne 0 ] || [ -z "$INSTANCE_INFO" ]; then
    print_status "ERROR" "Không thể kết nối đến API server"
    exit 1
fi

ERROR_MSG=$(echo "$INSTANCE_INFO" | jq -r '.error // empty')
if [ -n "$ERROR_MSG" ]; then
    print_status "ERROR" "Instance không tồn tại: $ERROR_MSG"
    exit 1
fi

print_status "OK" "Instance tồn tại"

# 2. Kiểm tra trạng thái cơ bản
print_section "2. Trạng thái cơ bản"

DISPLAY_NAME=$(echo "$INSTANCE_INFO" | jq -r '.displayName')
RUNNING=$(echo "$INSTANCE_INFO" | jq -r '.running')
LOADED=$(echo "$INSTANCE_INFO" | jq -r '.loaded')
FPS=$(echo "$INSTANCE_INFO" | jq -r '.fps')
SOLUTION=$(echo "$INSTANCE_INFO" | jq -r '.solutionName')

echo "  Display Name: $DISPLAY_NAME"
echo "  Solution: $SOLUTION"
echo "  Loaded: $LOADED"
echo "  Running: $RUNNING"
echo "  FPS: $FPS"

if [ "$RUNNING" = "true" ]; then
    print_status "OK" "Instance đang chạy"

    if (( $(echo "$FPS > 0" | bc -l) )); then
        print_status "SUCCESS" "Instance đang xử lý frames (FPS: $FPS)"
    else
        print_status "WARNING" "Instance đang chạy nhưng FPS = 0 (có thể đang khởi động hoặc không có input)"
    fi
else
    print_status "WARNING" "Instance không đang chạy"
    echo ""
    echo "Để start instance:"
    echo "  curl -X POST ${API_BASE}/instances/${INSTANCE_ID}/start"
fi

# 3. Kiểm tra output files (nếu có file_des node)
print_section "3. Kiểm tra Output Files"

OUTPUT_DIR="./output/${INSTANCE_ID}"
BUILD_OUTPUT_DIR="./build/output/${INSTANCE_ID}"

if [ -d "$OUTPUT_DIR" ]; then
    FILE_COUNT=$(find "$OUTPUT_DIR" -type f | wc -l)
    LATEST_FILE=$(find "$OUTPUT_DIR" -type f -printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-)

    if [ $FILE_COUNT -gt 0 ]; then
        print_status "SUCCESS" "Tìm thấy $FILE_COUNT file(s) trong $OUTPUT_DIR"

        if [ -n "$LATEST_FILE" ]; then
            FILE_TIME=$(stat -c %y "$LATEST_FILE" 2>/dev/null || stat -f "%Sm" "$LATEST_FILE" 2>/dev/null)
            FILE_SIZE=$(du -h "$LATEST_FILE" | cut -f1)
            echo "  File mới nhất: $(basename "$LATEST_FILE")"
            echo "  Thời gian: $FILE_TIME"
            echo "  Kích thước: $FILE_SIZE"

            # Kiểm tra file có được tạo gần đây không (trong 1 phút)
            FILE_AGE=$(find "$OUTPUT_DIR" -type f -mmin -1 | wc -l)
            if [ $FILE_AGE -gt 0 ]; then
                print_status "SUCCESS" "Có file mới được tạo trong 1 phút qua - Instance đang xử lý!"
            else
                print_status "WARNING" "Không có file mới trong 1 phút qua"
            fi
        fi

        echo ""
        echo "Xem các file gần đây:"
        echo "  ls -lht $OUTPUT_DIR | head -10"
    else
        print_status "WARNING" "Thư mục output tồn tại nhưng chưa có file"
    fi
elif [ -d "$BUILD_OUTPUT_DIR" ]; then
    FILE_COUNT=$(find "$BUILD_OUTPUT_DIR" -type f | wc -l)
    if [ $FILE_COUNT -gt 0 ]; then
        print_status "SUCCESS" "Tìm thấy $FILE_COUNT file(s) trong $BUILD_OUTPUT_DIR"
        echo "  Thư mục: $BUILD_OUTPUT_DIR"
    else
        print_status "WARNING" "Thư mục output tồn tại nhưng chưa có file"
    fi
else
    print_status "INFO" "Chưa có thư mục output (có thể instance không có file_des node)"
    echo "  Thư mục mong đợi: $OUTPUT_DIR hoặc $BUILD_OUTPUT_DIR"
fi

# 4. Kiểm tra RTMP stream (nếu có)
print_section "4. Kiểm tra RTMP Stream"

RTMP_URL=$(echo "$INSTANCE_INFO" | jq -r '.rtmpUrl // empty')
if [ -n "$RTMP_URL" ] && [ "$RTMP_URL" != "null" ]; then
    print_status "INFO" "Instance có RTMP stream: $RTMP_URL"
    echo ""
    echo "Để kiểm tra RTMP stream:"
    echo "  1. Sử dụng VLC hoặc ffplay:"
    echo "     ffplay $RTMP_URL"
    echo ""
    echo "  2. Hoặc kiểm tra RTMP server logs"
    echo ""
    echo "  3. Hoặc sử dụng RTMP client để test connection"
else
    print_status "INFO" "Instance không có RTMP stream"
fi

# 5. Kiểm tra RTSP stream (nếu có)
print_section "5. Kiểm tra RTSP Stream"

RTSP_URL=$(echo "$INSTANCE_INFO" | jq -r '.rtspUrl // empty')
if [ -n "$RTSP_URL" ] && [ "$RTSP_URL" != "null" ]; then
    print_status "INFO" "Instance có RTSP stream: $RTSP_URL"
    echo ""
    echo "Để kiểm tra RTSP stream:"
    echo "  1. Sử dụng VLC hoặc ffplay:"
    echo "     ffplay $RTSP_URL"
    echo ""
    echo "  2. Hoặc kiểm tra RTSP server logs"
else
    print_status "INFO" "Instance không có RTSP stream"
fi

# 6. Kiểm tra cấu hình
print_section "6. Cấu hình Instance"

METADATA_MODE=$(echo "$INSTANCE_INFO" | jq -r '.metadataMode')
STATISTICS_MODE=$(echo "$INSTANCE_INFO" | jq -r '.statisticsMode')
DEBUG_MODE=$(echo "$INSTANCE_INFO" | jq -r '.debugMode')
FRAME_RATE_LIMIT=$(echo "$INSTANCE_INFO" | jq -r '.frameRateLimit')

echo "  Metadata Mode: $METADATA_MODE"
echo "  Statistics Mode: $STATISTICS_MODE"
echo "  Debug Mode: $DEBUG_MODE"
echo "  Frame Rate Limit: $FRAME_RATE_LIMIT"

if [ "$METADATA_MODE" = "true" ]; then
    print_status "INFO" "Metadata mode đang bật - có thể nhận metadata qua WebSocket hoặc callback"
fi

if [ "$STATISTICS_MODE" = "true" ]; then
    print_status "INFO" "Statistics mode đang bật - có thể nhận statistics"
fi

# 7. Tổng kết và khuyến nghị
print_section "7. Tổng kết"

if [ "$RUNNING" = "true" ]; then
    if (( $(echo "$FPS > 0" | bc -l) )); then
        print_status "SUCCESS" "Instance đang hoạt động tốt!"
        echo ""
        echo "Các dấu hiệu thành công:"
        echo "  ✓ Instance đang chạy (running = true)"
        echo "  ✓ FPS > 0 (đang xử lý frames)"

        if [ -d "$OUTPUT_DIR" ] || [ -d "$BUILD_OUTPUT_DIR" ]; then
            FILE_COUNT=$(find "$OUTPUT_DIR" "$BUILD_OUTPUT_DIR" -type f 2>/dev/null | wc -l)
            if [ $FILE_COUNT -gt 0 ]; then
                echo "  ✓ Có output files được tạo"
            fi
        fi
    else
        print_status "WARNING" "Instance đang chạy nhưng chưa xử lý frames"
        echo ""
        echo "Khuyến nghị:"
        echo "  1. Kiểm tra input source (RTSP_URL, FILE_PATH) có hợp lệ không"
        echo "  2. Kiểm tra logs để xem lỗi kết nối"
        echo "  3. Đợi thêm một chút (có thể đang khởi động)"
        echo ""
        echo "Xem logs:"
        echo "  tail -f /var/log/edge_ai_api.log"
        echo "  hoặc: journalctl -u edge-ai-api -f"
    fi
else
    print_status "WARNING" "Instance không đang chạy"
    echo ""
    echo "Để start instance:"
    echo "  curl -X POST ${API_BASE}/instances/${INSTANCE_ID}/start"
fi

echo ""
print_section "Các lệnh hữu ích"

echo "Xem thông tin chi tiết instance:"
echo "  curl -s ${API_BASE}/instances/${INSTANCE_ID} | jq '.'"
echo ""

echo "Monitor output files (real-time):"
echo "  watch -n 1 'ls -lht ${OUTPUT_DIR} | head -10'"
echo ""

echo "Xem logs của instance:"
echo "  tail -f /var/log/edge_ai_api.log | grep ${INSTANCE_ID}"
echo ""

echo "Kiểm tra FPS định kỳ:"
echo "  watch -n 2 'curl -s ${API_BASE}/instances/${INSTANCE_ID} | jq \".fps\"'"
echo ""
