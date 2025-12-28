#!/bin/bash

# Script để phân tích log của instance

INSTANCE_ID="${1:-a309c06d-48af-4a32-b8e3-e85cfe23a4cb}"
API_BASE="http://localhost:8848/v1/core"

echo "=========================================="
echo "Phân tích Log cho Instance: $INSTANCE_ID"
echo "=========================================="
echo ""

# Màu sắc
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

# 1. Kiểm tra instance status
echo -e "${YELLOW}1. Instance Status:${NC}"
INSTANCE_INFO=$(curl -s "${API_BASE}/instances/${INSTANCE_ID}")
if [ $? -ne 0 ]; then
    echo -e "${RED}✗ Không thể kết nối đến API${NC}"
    exit 1
fi

echo "$INSTANCE_INFO" | jq '{
    instanceId,
    displayName,
    solutionName,
    running,
    fps,
    loaded
}'
echo ""

# 2. Kiểm tra output
echo -e "${YELLOW}2. Output Information:${NC}"
OUTPUT_INFO=$(curl -s "${API_BASE}/instances/${INSTANCE_ID}/output")
if [ $? -eq 0 ]; then
    echo "$OUTPUT_INFO" | jq '{
        timestamp,
        running: .status.running,
        processing: .status.processing,
        fps: .metrics.fps,
        outputType: .output.type,
        message: .status.message
    }'

    OUTPUT_TYPE=$(echo "$OUTPUT_INFO" | jq -r '.output.type')
    if [ "$OUTPUT_TYPE" = "FILE" ]; then
        echo ""
        echo "File Output Details:"
        echo "$OUTPUT_INFO" | jq '.output.files'
    fi
else
    echo -e "${RED}✗ Không thể lấy output information${NC}"
fi
echo ""

# 3. Phân tích warnings trong log
echo -e "${YELLOW}3. Phân tích Warnings:${NC}"
echo ""

# Đếm số lượng "queue full" warnings
QUEUE_FULL_COUNT=$(grep -c "queue full, dropping meta" /dev/stdin 2>/dev/null || echo "0")

if [ "$QUEUE_FULL_COUNT" -gt 0 ]; then
    echo -e "${RED}⚠ Phát hiện $QUEUE_FULL_COUNT cảnh báo 'queue full, dropping meta'${NC}"
    echo ""
    echo "Nguyên nhân có thể:"
    echo "  1. File source đọc quá nhanh so với tốc độ xử lý của face detector"
    echo "  2. Face detector xử lý chậm hơn tốc độ đọc file"
    echo "  3. File destination node không ghi kịp"
    echo "  4. Queue giữa các nodes bị đầy"
    echo ""
    echo "Giải pháp:"
    echo "  - Thêm frameRateLimit để giới hạn tốc độ đọc"
    echo "  - Sử dụng resize_ratio nhỏ hơn để giảm kích thước frame"
    echo "  - Kiểm tra xem file_des node có đang ghi file không"
    echo ""
else
    echo -e "${GREEN}✓ Không có queue full warnings${NC}"
fi

# 4. Kiểm tra output files
echo -e "${YELLOW}4. Kiểm tra Output Files:${NC}"
OUTPUT_DIRS=(
    "./output/${INSTANCE_ID}"
    "./build/output/${INSTANCE_ID}"
    "output/${INSTANCE_ID}"
    "build/output/${INSTANCE_ID}"
)

FOUND_DIR=""
for dir in "${OUTPUT_DIRS[@]}"; do
    if [ -d "$dir" ]; then
        FOUND_DIR="$dir"
        break
    fi
done

if [ -n "$FOUND_DIR" ]; then
    echo -e "${GREEN}✓ Tìm thấy output directory: $FOUND_DIR${NC}"
    FILE_COUNT=$(find "$FOUND_DIR" -type f 2>/dev/null | wc -l)
    echo "  Số lượng files: $FILE_COUNT"

    if [ "$FILE_COUNT" -gt 0 ]; then
        echo ""
        echo "  5 files mới nhất:"
        ls -lht "$FOUND_DIR" | head -6 | tail -5
        echo ""

        LATEST_FILE=$(ls -t "$FOUND_DIR" | head -1)
        if [ -n "$LATEST_FILE" ]; then
            FILE_SIZE=$(du -h "$FOUND_DIR/$LATEST_FILE" | cut -f1)
            FILE_AGE=$(find "$FOUND_DIR/$LATEST_FILE" -mmin -1 2>/dev/null | wc -l)
            echo "  File mới nhất: $LATEST_FILE"
            echo "  Kích thước: $FILE_SIZE"
            if [ "$FILE_AGE" -gt 0 ]; then
                echo -e "  ${GREEN}✓ File được tạo trong 1 phút qua${NC}"
            else
                echo -e "  ${YELLOW}⚠ File không được tạo gần đây${NC}"
            fi
        fi
    else
        echo -e "  ${YELLOW}⚠ Thư mục tồn tại nhưng chưa có file${NC}"
    fi
else
    echo -e "${YELLOW}⚠ Không tìm thấy output directory${NC}"
    echo "  Đã kiểm tra:"
    for dir in "${OUTPUT_DIRS[@]}"; do
        echo "    - $dir"
    done
fi
echo ""

# 5. Đề xuất giải pháp
echo -e "${YELLOW}5. Đề xuất:${NC}"
echo ""

FPS=$(echo "$INSTANCE_INFO" | jq -r '.fps // 0')
RUNNING=$(echo "$INSTANCE_INFO" | jq -r '.running // false')

if [ "$RUNNING" = "true" ] && (( $(echo "$FPS == 0" | bc -l 2>/dev/null || echo "1") )); then
    echo -e "${YELLOW}⚠ Instance đang chạy nhưng FPS = 0${NC}"
    echo "  Có thể đang khởi động hoặc có vấn đề với pipeline"
    echo ""
fi

if [ "$QUEUE_FULL_COUNT" -gt 100 ]; then
    echo -e "${RED}⚠ Có quá nhiều queue full warnings (>100)${NC}"
    echo ""
    echo "Khuyến nghị:"
    echo "  1. Thêm frameRateLimit vào instance config:"
    echo "     \"frameRateLimit\": 15"
    echo ""
    echo "  2. Hoặc sử dụng resize_ratio nhỏ hơn:"
    echo "     \"RESIZE_RATIO\": \"0.5\""
    echo ""
    echo "  3. Kiểm tra xem file_des node có đang ghi file không"
    echo ""
fi

echo "=========================================="
echo "Kết thúc phân tích"
echo "=========================================="
