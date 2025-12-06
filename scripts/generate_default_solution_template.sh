#!/bin/bash

# Script để generate template code cho default solution mới

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Màu sắc
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  GENERATE DEFAULT SOLUTION TEMPLATE${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Nhập thông tin
read -p "Solution ID (ví dụ: face_detection_webcam): " SOLUTION_ID
read -p "Solution Name (ví dụ: Face Detection with Webcam): " SOLUTION_NAME
read -p "Solution Type (ví dụ: face_detection): " SOLUTION_TYPE

# Validate
if [ -z "$SOLUTION_ID" ] || [ -z "$SOLUTION_NAME" ] || [ -z "$SOLUTION_TYPE" ]; then
    echo -e "${YELLOW}❌ Tất cả fields đều bắt buộc${NC}"
    exit 1
fi

# Generate function name
FUNCTION_NAME=$(echo "$SOLUTION_ID" | sed 's/_\([a-z]\)/\U\1/g' | sed 's/^\([a-z]\)/\U\1/')
FUNCTION_NAME="register${FUNCTION_NAME}Solution"

echo ""
echo -e "${GREEN}✓ Thông tin đã nhập:${NC}"
echo "  Solution ID: $SOLUTION_ID"
echo "  Solution Name: $SOLUTION_NAME"
echo "  Solution Type: $SOLUTION_TYPE"
echo "  Function Name: $FUNCTION_NAME"
echo ""

# Tạo template code
TEMPLATE_CPP=$(cat <<EOF
void SolutionRegistry::${FUNCTION_NAME}() {
    SolutionConfig config;
    config.solutionId = "${SOLUTION_ID}";
    config.solutionName = "${SOLUTION_NAME}";
    config.solutionType = "${SOLUTION_TYPE}";
    config.isDefault = true;  // QUAN TRỌNG: Phải set = true
    
    // ===== SOURCE NODE =====
    SolutionConfig::NodeConfig sourceNode;
    sourceNode.nodeType = "rtsp_src";  // Thay đổi theo nhu cầu: rtsp_src, file_src, etc.
    sourceNode.nodeName = "source_{instanceId}";
    sourceNode.parameters["rtsp_url"] = "\${RTSP_URL}";
    sourceNode.parameters["channel"] = "0";
    sourceNode.parameters["resize_ratio"] = "1.0";
    config.pipeline.push_back(sourceNode);
    
    // ===== PROCESSOR NODE =====
    SolutionConfig::NodeConfig processorNode;
    processorNode.nodeType = "yunet_face_detector";  // Thay đổi theo nhu cầu
    processorNode.nodeName = "processor_{instanceId}";
    processorNode.parameters["model_path"] = "\${MODEL_PATH}";
    processorNode.parameters["score_threshold"] = "\${detectionSensitivity}";
    processorNode.parameters["nms_threshold"] = "0.5";
    processorNode.parameters["top_k"] = "50";
    config.pipeline.push_back(processorNode);
    
    // ===== DESTINATION NODE =====
    SolutionConfig::NodeConfig destNode;
    destNode.nodeType = "file_des";  // Thay đổi theo nhu cầu: file_des, rtmp_des, etc.
    destNode.nodeName = "destination_{instanceId}";
    destNode.parameters["save_dir"] = "./output/{instanceId}";
    destNode.parameters["name_prefix"] = "${SOLUTION_ID}";
    destNode.parameters["osd"] = "true";
    config.pipeline.push_back(destNode);
    
    // ===== DEFAULTS =====
    config.defaults["detectorMode"] = "SmartDetection";
    config.defaults["detectionSensitivity"] = "0.7";
    config.defaults["sensorModality"] = "RGB";
    
    registerSolution(config);
}
EOF
)

TEMPLATE_HEADER=$(cat <<EOF
    /**
     * @brief Register ${SOLUTION_NAME}
     */
    void ${FUNCTION_NAME}();
EOF
)

# Lưu vào file
TEMPLATE_FILE="$PROJECT_ROOT/docs/templates/${SOLUTION_ID}_template.cpp"
TEMPLATE_HEADER_FILE="$PROJECT_ROOT/docs/templates/${SOLUTION_ID}_template.h"

mkdir -p "$PROJECT_ROOT/docs/templates"

echo "$TEMPLATE_CPP" > "$TEMPLATE_FILE"
echo "$TEMPLATE_HEADER" > "$TEMPLATE_HEADER_FILE"

echo -e "${GREEN}✓ Đã tạo template files:${NC}"
echo "  - $TEMPLATE_FILE"
echo "  - $TEMPLATE_HEADER_FILE"
echo ""

# Hiển thị hướng dẫn
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  HƯỚNG DẪN SỬ DỤNG${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo "1. Copy code từ template file vào:"
echo "   - src/solutions/solution_registry.cpp (thêm hàm mới)"
echo ""
echo "2. Copy header declaration vào:"
echo "   - include/solutions/solution_registry.h (phần private)"
echo ""
echo "3. Thêm dòng gọi hàm vào initializeDefaultSolutions():"
echo "   register${FUNCTION_NAME}();"
echo ""
echo "4. Sửa đổi template theo nhu cầu:"
echo "   - Thay đổi node types"
echo "   - Thêm/bớt nodes"
echo "   - Điều chỉnh parameters"
echo ""
echo "5. Rebuild và test:"
echo "   cd build && make"
echo ""

# Hiển thị code để copy
echo -e "${YELLOW}Code để copy vào solution_registry.cpp:${NC}"
echo "----------------------------------------"
cat "$TEMPLATE_FILE"
echo "----------------------------------------"
echo ""
echo -e "${YELLOW}Header declaration để copy vào solution_registry.h:${NC}"
echo "----------------------------------------"
cat "$TEMPLATE_HEADER_FILE"
echo "----------------------------------------"
echo ""

