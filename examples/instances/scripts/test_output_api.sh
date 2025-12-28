#!/bin/bash

# Script để test API endpoint /v1/core/instance/{instanceId}/output
# Sử dụng instance face_detection_file_source để test

API_BASE="http://localhost:8848/v1/core"
INSTANCE_FILE="examples/instances/create_face_detection_file_source.json"

echo "=========================================="
echo "Test API: GET /instances/{instanceId}/output"
echo "=========================================="
echo ""

# Màu sắc cho output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# 1. Tạo instance
echo -e "${YELLOW}1. Tạo instance từ file: ${INSTANCE_FILE}${NC}"
echo ""

RESPONSE=$(curl -s -X POST "${API_BASE}/instance" \
  -H 'Content-Type: application/json' \
  -d @"${INSTANCE_FILE}")

INSTANCE_ID=$(echo "$RESPONSE" | jq -r '.instanceId // empty')

if [ -z "$INSTANCE_ID" ] || [ "$INSTANCE_ID" = "null" ]; then
    echo -e "${RED}✗ Lỗi: Không thể tạo instance${NC}"
    echo "Response:"
    echo "$RESPONSE" | jq '.'
    exit 1
fi

echo -e "${GREEN}✓ Instance đã được tạo${NC}"
echo "Instance ID: $INSTANCE_ID"
echo ""

# 2. Đợi instance khởi động (nếu autoStart=true)
echo -e "${YELLOW}2. Đợi instance khởi động...${NC}"
echo "Đợi 5 giây để pipeline khởi động..."
sleep 5
echo ""

# 3. Test endpoint /output
echo -e "${YELLOW}3. Test endpoint: GET /instances/${INSTANCE_ID}/output${NC}"
echo ""

OUTPUT_RESPONSE=$(curl -s -X GET "${API_BASE}/instances/${INSTANCE_ID}/output")

# Kiểm tra response
ERROR=$(echo "$OUTPUT_RESPONSE" | jq -r '.error // empty')

if [ -n "$ERROR" ]; then
    echo -e "${RED}✗ Lỗi: $ERROR${NC}"
    echo "Response:"
    echo "$OUTPUT_RESPONSE" | jq '.'
    exit 1
fi

echo -e "${GREEN}✓ API trả về thành công!${NC}"
echo ""
echo "=========================================="
echo "Kết quả:"
echo "=========================================="
echo ""

# Hiển thị thông tin chính
echo "📊 Thông tin cơ bản:"
echo "$OUTPUT_RESPONSE" | jq '{
    timestamp,
    instanceId,
    displayName,
    solutionName,
    running,
    loaded
}'
echo ""

echo "⚡ Metrics:"
echo "$OUTPUT_RESPONSE" | jq '.metrics'
echo ""

echo "📥 Input:"
echo "$OUTPUT_RESPONSE" | jq '.input'
echo ""

echo "📤 Output:"
OUTPUT_TYPE=$(echo "$OUTPUT_RESPONSE" | jq -r '.output.type')
echo "Type: $OUTPUT_TYPE"
echo ""

if [ "$OUTPUT_TYPE" = "FILE" ]; then
    echo "📁 File Output Details:"
    echo "$OUTPUT_RESPONSE" | jq '.output.files'
    echo ""

    FILE_COUNT=$(echo "$OUTPUT_RESPONSE" | jq -r '.output.files.fileCount // 0')
    IS_ACTIVE=$(echo "$OUTPUT_RESPONSE" | jq -r '.output.files.isActive // false')

    if [ "$FILE_COUNT" -gt 0 ]; then
        echo -e "${GREEN}✓ Có $FILE_COUNT file(s) trong output directory${NC}"
    else
        echo -e "${YELLOW}⚠ Chưa có file output (có thể đang xử lý)${NC}"
    fi

    if [ "$IS_ACTIVE" = "true" ]; then
        echo -e "${GREEN}✓ Instance đang tạo file mới (active)${NC}"
    else
        echo -e "${YELLOW}⚠ Không có file mới trong 1 phút qua${NC}"
    fi
elif [ "$OUTPUT_TYPE" = "RTMP_STREAM" ]; then
    echo "📺 RTMP Stream Details:"
    echo "$OUTPUT_RESPONSE" | jq '.output | {rtmpUrl, rtspUrl}'
fi

echo ""
echo "🎯 Detection Settings:"
echo "$OUTPUT_RESPONSE" | jq '.detection'
echo ""

echo "⚙️ Processing Modes:"
echo "$OUTPUT_RESPONSE" | jq '.modes'
echo ""

echo "📈 Status:"
echo "$OUTPUT_RESPONSE" | jq '.status'
echo ""

# 4. Test lại sau vài giây để xem có thay đổi không
echo "=========================================="
echo "4. Test lại sau 5 giây để xem thay đổi..."
echo "=========================================="
sleep 5

OUTPUT_RESPONSE2=$(curl -s -X GET "${API_BASE}/instances/${INSTANCE_ID}/output")
TIMESTAMP1=$(echo "$OUTPUT_RESPONSE" | jq -r '.timestamp')
TIMESTAMP2=$(echo "$OUTPUT_RESPONSE2" | jq -r '.timestamp')
FPS1=$(echo "$OUTPUT_RESPONSE" | jq -r '.metrics.fps')
FPS2=$(echo "$OUTPUT_RESPONSE2" | jq -r '.metrics.fps')

echo "Timestamp lần 1: $TIMESTAMP1"
echo "Timestamp lần 2: $TIMESTAMP2"
echo "FPS lần 1: $FPS1"
echo "FPS lần 2: $FPS2"
echo ""

if [ "$OUTPUT_TYPE" = "FILE" ]; then
    FILE_COUNT1=$(echo "$OUTPUT_RESPONSE" | jq -r '.output.files.fileCount // 0')
    FILE_COUNT2=$(echo "$OUTPUT_RESPONSE2" | jq -r '.output.files.fileCount // 0')
    echo "File count lần 1: $FILE_COUNT1"
    echo "File count lần 2: $FILE_COUNT2"

    if [ "$FILE_COUNT2" -gt "$FILE_COUNT1" ]; then
        echo -e "${GREEN}✓ File count tăng - Instance đang tạo file mới!${NC}"
    fi
fi

echo ""
echo "=========================================="
echo "✅ Test hoàn tất!"
echo "=========================================="
echo ""
echo "Để xem full response:"
echo "  curl -s ${API_BASE}/instances/${INSTANCE_ID}/output | jq '.'"
echo ""
echo "Để xóa instance sau khi test:"
echo "  curl -X DELETE ${API_BASE}/instances/${INSTANCE_ID}"
echo ""
