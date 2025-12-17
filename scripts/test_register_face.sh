#!/bin/bash
# ============================================
# Test Script: Register Face Subject với File Upload
# ============================================
#
# Script này test API POST register face subject với file upload trực tiếp
# Hỗ trợ các định dạng: jpeg, jpg, ico, png, bmp, gif, tif, tiff, webp
#
# Usage:
#   ./scripts/test_register_face.sh <image_file> <subject_name> [det_prob_threshold]
#
# Example:
#   ./scripts/test_register_face.sh /path/to/image.jpg "subject1" 0.5
#
# ============================================

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
API_URL="${API_URL:-http://localhost:8080}"
ENDPOINT="/v1/recognition/faces"

# Check arguments
if [ $# -lt 2 ]; then
    echo -e "${RED}Error: Thiếu tham số${NC}"
    echo ""
    echo "Usage: $0 <image_file> <subject_name> [det_prob_threshold]"
    echo ""
    echo "Parameters:"
    echo "  image_file          : Đường dẫn đến file ảnh"
    echo "  subject_name        : Tên subject để đăng ký"
    echo "  det_prob_threshold  : Ngưỡng confidence (0.0-1.0, mặc định: 0.5)"
    echo ""
    echo "Example:"
    echo "  $0 /home/cvedix/project/cvedix_data/test_images/faces/0.jpg subject1 0.5"
    echo ""
    exit 1
fi

IMAGE_FILE="$1"
SUBJECT_NAME="$2"
DET_PROB_THRESHOLD="${3:-0.5}"

# Validate image file
if [ ! -f "$IMAGE_FILE" ]; then
    echo -e "${RED}Error: File không tồn tại: $IMAGE_FILE${NC}"
    exit 1
fi

# Check file size (max 5MB)
FILE_SIZE=$(stat -f%z "$IMAGE_FILE" 2>/dev/null || stat -c%s "$IMAGE_FILE" 2>/dev/null || echo "0")
MAX_SIZE=$((5 * 1024 * 1024)) # 5MB in bytes

if [ "$FILE_SIZE" -gt "$MAX_SIZE" ]; then
    echo -e "${RED}Error: File quá lớn (max 5MB). File size: $((FILE_SIZE / 1024 / 1024))MB${NC}"
    exit 1
fi

# Check file extension
FILE_EXT="${IMAGE_FILE##*.}"
FILE_EXT_LOWER=$(echo "$FILE_EXT" | tr '[:upper:]' '[:lower:]')

ALLOWED_EXTENSIONS=("jpeg" "jpg" "ico" "png" "bmp" "gif" "tif" "tiff" "webp")
IS_VALID=false

for ext in "${ALLOWED_EXTENSIONS[@]}"; do
    if [ "$FILE_EXT_LOWER" = "$ext" ]; then
        IS_VALID=true
        break
    fi
done

if [ "$IS_VALID" = false ]; then
    echo -e "${YELLOW}Warning: Định dạng file '$FILE_EXT' có thể không được hỗ trợ${NC}"
    echo "Định dạng được hỗ trợ: ${ALLOWED_EXTENSIONS[*]}"
    echo ""
fi

# Display information
echo -e "${BLUE}===========================================${NC}"
echo -e "${BLUE}Test Register Face Subject${NC}"
echo -e "${BLUE}===========================================${NC}"
echo ""
echo "Image file: $IMAGE_FILE"
echo "File size: $((FILE_SIZE / 1024))KB"
echo "Subject name: $SUBJECT_NAME"
echo "Detection threshold: $DET_PROB_THRESHOLD"
echo "API URL: $API_URL$ENDPOINT"
echo ""

# Make API request
echo -e "${BLUE}Sending request...${NC}"
RESPONSE=$(curl -s -w "\nHTTP_STATUS:%{http_code}" \
    -X POST \
    "$API_URL$ENDPOINT?subject=$SUBJECT_NAME&det_prob_threshold=$DET_PROB_THRESHOLD" \
    -F "file=@$IMAGE_FILE" \
    2>&1)

# Extract HTTP status and body
HTTP_STATUS=$(echo "$RESPONSE" | grep "HTTP_STATUS:" | cut -d: -f2)
BODY=$(echo "$RESPONSE" | sed '/HTTP_STATUS:/d')

# Check response
if [ -z "$HTTP_STATUS" ]; then
    echo -e "${RED}Error: Không nhận được phản hồi từ server${NC}"
    echo "Response: $RESPONSE"
    exit 1
fi

echo ""
echo -e "${BLUE}Response:${NC}"
echo "HTTP Status: $HTTP_STATUS"
echo ""

# Parse and display JSON response
if command -v python3 &> /dev/null; then
    echo "$BODY" | python3 -m json.tool 2>/dev/null || echo "$BODY"
else
    echo "$BODY"
fi

echo ""

# Check if successful
if [ "$HTTP_STATUS" = "200" ]; then
    echo -e "${GREEN}✓ Success! Face subject đã được đăng ký${NC}"

    # Extract image_id if available
    if command -v python3 &> /dev/null; then
        IMAGE_ID=$(echo "$BODY" | python3 -c "import sys, json; data=json.load(sys.stdin); print(data.get('image_id', ''))" 2>/dev/null)
        if [ -n "$IMAGE_ID" ]; then
            echo "Image ID: $IMAGE_ID"
        fi
    fi

    echo ""
    echo -e "${BLUE}Kiểm tra database:${NC}"
    echo "curl -s \"$API_URL/v1/recognition/faces?page=0&size=10\" | python3 -m json.tool"
    exit 0
else
    echo -e "${RED}✗ Failed! HTTP Status: $HTTP_STATUS${NC}"
    exit 1
fi
