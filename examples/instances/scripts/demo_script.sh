#!/bin/bash

# Demo script cho các thao tác CRUD với Instances
# Sử dụng: ./demo_script.sh [BASE_URL]
# Mặc định: BASE_URL=http://localhost:8848

BASE_URL="${1:-http://localhost:8848}"
API_BASE="${BASE_URL}/v1/core"

echo "=========================================="
echo "Edge AI API - Instance Management Demo"
echo "=========================================="
echo "Base URL: ${BASE_URL}"
echo ""

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Function to print section header
print_section() {
    echo ""
    echo -e "${BLUE}=== $1 ===${NC}"
    echo ""
}

# Function to print command
print_cmd() {
    echo -e "${YELLOW}Command:${NC} $1"
    echo ""
}

# Function to execute and show response
execute_cmd() {
    print_cmd "$1"
    eval "$1"
    echo ""
    echo "---"
    sleep 1
}

# ==========================================
# 1. CREATE - Tạo instance mới
# ==========================================
print_section "1. CREATE - Tạo Instance Mới"

echo "1.1. Tạo face detection instance cơ bản:"
execute_cmd "curl -X POST ${API_BASE}/instance \\
  -H 'Content-Type: application/json' \\
  -d @examples/instances/create_face_detection_basic.json \\
  | jq '.'"

echo ""
echo "Lưu instanceId từ response trên để sử dụng cho các thao tác tiếp theo"
read -p "Nhập instanceId (hoặc Enter để tiếp tục với demo): " INSTANCE_ID

if [ -z "$INSTANCE_ID" ]; then
    echo "Sử dụng instanceId mẫu: demo-instance-123"
    INSTANCE_ID="demo-instance-123"
fi

# ==========================================
# 2. READ - Đọc thông tin instance
# ==========================================
print_section "2. READ - Đọc Thông Tin Instance"

echo "2.1. Liệt kê tất cả instances:"
execute_cmd "curl -X GET ${API_BASE}/instances | jq '.'"

echo ""
echo "2.2. Lấy thông tin chi tiết của một instance:"
execute_cmd "curl -X GET ${API_BASE}/instances/${INSTANCE_ID} | jq '.'"

# ==========================================
# 3. UPDATE - Cập nhật instance
# ==========================================
print_section "3. UPDATE - Cập Nhật Instance"

echo "3.1. Cập nhật tên và group:"
execute_cmd "curl -X PUT ${API_BASE}/instances/${INSTANCE_ID} \\
  -H 'Content-Type: application/json' \\
  -d @examples/instances/update_change_name_group.json \\
  | jq '.'"

echo ""
echo "3.2. Cập nhật các settings:"
execute_cmd "curl -X PUT ${API_BASE}/instances/${INSTANCE_ID} \\
  -H 'Content-Type: application/json' \\
  -d @examples/instances/update_change_settings.json \\
  | jq '.'"

echo ""
echo "3.3. Cập nhật RTSP URL:"
execute_cmd "curl -X PUT ${API_BASE}/instances/${INSTANCE_ID} \\
  -H 'Content-Type: application/json' \\
  -d @examples/instances/update_change_rtsp_url.json \\
  | jq '.'"

# ==========================================
# 4. START - Khởi động instance
# ==========================================
print_section "4. START - Khởi Động Instance"

echo "4.1. Start instance:"
execute_cmd "curl -X POST ${API_BASE}/instances/${INSTANCE_ID}/start \\
  -H 'Content-Type: application/json' \\
  | jq '.'"

# Kiểm tra trạng thái sau khi start
echo ""
echo "Kiểm tra trạng thái sau khi start:"
execute_cmd "curl -X GET ${API_BASE}/instances/${INSTANCE_ID} | jq '.running'"

# ==========================================
# 5. STOP - Dừng instance
# ==========================================
print_section "5. STOP - Dừng Instance"

echo "5.1. Stop instance:"
execute_cmd "curl -X POST ${API_BASE}/instances/${INSTANCE_ID}/stop \\
  -H 'Content-Type: application/json' \\
  | jq '.'"

# Kiểm tra trạng thái sau khi stop
echo ""
echo "Kiểm tra trạng thái sau khi stop:"
execute_cmd "curl -X GET ${API_BASE}/instances/${INSTANCE_ID} | jq '.running'"

# ==========================================
# 6. RESTART - Khởi động lại instance
# ==========================================
print_section "6. RESTART - Khởi Động Lại Instance"

echo "6.1. Restart instance:"
execute_cmd "curl -X POST ${API_BASE}/instances/${INSTANCE_ID}/restart \\
  -H 'Content-Type: application/json' \\
  | jq '.'"

# ==========================================
# 7. DELETE - Xóa instance
# ==========================================
print_section "7. DELETE - Xóa Instance"

echo "7.1. Delete instance (chỉ chạy nếu muốn xóa thật):"
read -p "Bạn có muốn xóa instance ${INSTANCE_ID}? (y/N): " CONFIRM_DELETE

if [ "$CONFIRM_DELETE" = "y" ] || [ "$CONFIRM_DELETE" = "Y" ]; then
    execute_cmd "curl -X DELETE ${API_BASE}/instances/${INSTANCE_ID} | jq '.'"

    echo ""
    echo "Kiểm tra xem instance đã bị xóa chưa:"
    execute_cmd "curl -X GET ${API_BASE}/instances/${INSTANCE_ID} | jq '.'"
else
    echo "Bỏ qua việc xóa instance."
fi

# ==========================================
# Tổng kết
# ==========================================
print_section "Tổng Kết"

echo "Đã hoàn thành demo các thao tác CRUD với instances!"
echo ""
echo "Các thao tác đã thực hiện:"
echo "  1. CREATE - Tạo instance mới"
echo "  2. READ - Đọc thông tin instance"
echo "  3. UPDATE - Cập nhật instance"
echo "  4. START - Khởi động instance"
echo "  5. STOP - Dừng instance"
echo "  6. RESTART - Khởi động lại instance"
echo "  7. DELETE - Xóa instance"
echo ""
echo "Các file JSON mẫu có sẵn trong thư mục examples/instances/"
echo ""
