#!/bin/bash

# Script để restore default solutions về trạng thái mặc định
# Default solutions được hardcode trong code, không được lưu vào storage
# Script này chỉ reset storage file về trạng thái rỗng

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DOCS_DIR="$PROJECT_ROOT/docs"

# Màu sắc cho output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  RESTORE DEFAULT SOLUTIONS${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Tìm solutions.json file
if [ -n "$SOLUTIONS_DIR" ]; then
    SOLUTIONS_FILE="$SOLUTIONS_DIR/solutions.json"
    SOLUTIONS_DIR_PATH="$SOLUTIONS_DIR"
elif [ -d "/var/lib/edge_ai_api/solutions" ]; then
    SOLUTIONS_FILE="/var/lib/edge_ai_api/solutions/solutions.json"
    SOLUTIONS_DIR_PATH="/var/lib/edge_ai_api/solutions"
elif [ -d "$HOME/.local/share/edge_ai_api/solutions" ]; then
    SOLUTIONS_FILE="$HOME/.local/share/edge_ai_api/solutions/solutions.json"
    SOLUTIONS_DIR_PATH="$HOME/.local/share/edge_ai_api/solutions"
else
    echo -e "${YELLOW}⚠️  Không tìm thấy solutions directory${NC}"
    echo "Tạo directory mặc định: $HOME/.local/share/edge_ai_api/solutions"
    mkdir -p "$HOME/.local/share/edge_ai_api/solutions"
    SOLUTIONS_FILE="$HOME/.local/share/edge_ai_api/solutions/solutions.json"
    SOLUTIONS_DIR_PATH="$HOME/.local/share/edge_ai_api/solutions"
fi

echo -e "${BLUE}Solutions file:${NC} $SOLUTIONS_FILE"
echo ""

# Backup file hiện tại nếu tồn tại
if [ -f "$SOLUTIONS_FILE" ]; then
    BACKUP_FILE="${SOLUTIONS_FILE}.backup.$(date +%Y%m%d_%H%M%S)"
    echo -e "${YELLOW}Đang backup file hiện tại...${NC}"
    cp "$SOLUTIONS_FILE" "$BACKUP_FILE"
    echo -e "${GREEN}✓ Backup đã lưu tại: $BACKUP_FILE${NC}"
    echo ""
    
    # Hiển thị thông tin file hiện tại
    if command -v python3 &> /dev/null; then
        SOL_COUNT=$(python3 -c "import json, sys; data=json.load(open('$SOLUTIONS_FILE')); print(len(data))" 2>/dev/null || echo "0")
        echo -e "${BLUE}File hiện tại có ${SOL_COUNT} solution(s)${NC}"
    fi
else
    echo -e "${YELLOW}File solutions.json chưa tồn tại${NC}"
fi

echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  THÔNG TIN DEFAULT SOLUTIONS${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

if [ -f "$DOCS_DIR/default_solutions_backup.json" ]; then
    echo -e "${GREEN}✓ Tìm thấy file backup default solutions${NC}"
    echo ""
    echo "Các default solutions được hỗ trợ:"
    if command -v python3 &> /dev/null; then
        python3 << EOF
import json
import sys

try:
    with open("$DOCS_DIR/default_solutions_backup.json", 'r') as f:
        data = json.load(f)
    
    if "defaultSolutions" in data:
        solutions = data["defaultSolutions"]
        for i, (sid, sol) in enumerate(solutions.items(), 1):
            print(f"  {i}. {sid}")
            print(f"     Name: {sol.get('solutionName', 'N/A')}")
            print(f"     Type: {sol.get('solutionType', 'N/A')}")
            print(f"     Description: {sol.get('description', 'N/A')}")
            print()
    
    if "summary" in data:
        summary = data["summary"]
        print(f"Tổng số: {summary.get('totalDefaultSolutions', 0)} default solutions")
except Exception as e:
    print(f"Lỗi đọc file: {e}")
EOF
    else
        echo "  (Cần python3 để hiển thị chi tiết)"
    fi
else
    echo -e "${YELLOW}⚠️  Không tìm thấy file backup default solutions${NC}"
    echo "  File backup nên ở: $DOCS_DIR/default_solutions_backup.json"
fi

echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  THỰC HIỆN RESTORE${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Xác nhận
read -p "Bạn có chắc muốn reset solutions.json về trạng thái mặc định (rỗng)? [y/N]: " -n 1 -r
echo ""

if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo -e "${YELLOW}Đã hủy restore${NC}"
    exit 0
fi

# Reset file về trạng thái rỗng
echo -e "${YELLOW}Đang reset file...${NC}"
echo '{}' > "$SOLUTIONS_FILE"
echo -e "${GREEN}✓ Đã reset file solutions.json về trạng thái mặc định${NC}"
echo ""

# Verify
if command -v python3 &> /dev/null; then
    SOL_COUNT=$(python3 -c "import json; data=json.load(open('$SOLUTIONS_FILE')); print(len(data))" 2>/dev/null || echo "0")
    if [ "$SOL_COUNT" -eq 0 ]; then
        echo -e "${GREEN}✓ Xác nhận: File đã được reset (0 solutions)${NC}"
    else
        echo -e "${RED}❌ Lỗi: File vẫn có $SOL_COUNT solution(s)${NC}"
        exit 1
    fi
fi

echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${GREEN}  HOÀN TẤT${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo -e "${GREEN}✓ Default solutions được hardcode trong code và sẽ tự động load khi khởi động ứng dụng${NC}"
echo -e "${GREEN}✓ Storage file đã được reset về trạng thái rỗng${NC}"
echo ""
echo "Các default solutions sẽ có sẵn khi khởi động lại ứng dụng:"
echo "  1. face_detection"
echo "  2. face_detection_file"
echo "  3. object_detection"
echo "  4. face_detection_rtmp"
echo ""

