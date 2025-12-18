#!/bin/bash
# ============================================
# Edge AI API - Utility Scripts
# ============================================
#
# Gộp các utility scripts:
# - run_tests.sh
# - generate_default_solution_template.sh
# - restore_default_solutions.sh
#
# Usage:
#   ./scripts/utils.sh <command> [options]
#
# Commands:
#   test                    Run unit tests
#   generate-solution       Generate default solution template
#   restore-solutions       Restore default solutions
#   setup-face-db           Setup face database permissions (requires sudo)
#   help                    Show help
#
# ============================================

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

COMMAND="${1:-help}"

case "$COMMAND" in
    test)
        BUILD_DIR="${2:-build}"
        TEST_EXEC="${BUILD_DIR}/bin/edge_ai_api_tests"

        echo "========================================"
        echo "Running Edge AI API Unit Tests"
        echo "========================================"
        echo ""

        if [ ! -f "${TEST_EXEC}" ]; then
            echo -e "${RED}Error: Test executable not found at ${TEST_EXEC}${NC}"
            echo "Please build tests first:"
            echo "  cd ${BUILD_DIR}"
            echo "  cmake .. -DBUILD_TESTS=ON"
            echo "  make -j\$(nproc)"
            exit 1
        fi

        echo "Running tests..."
        "${TEST_EXEC}"
        EXIT_CODE=$?

        if [ $EXIT_CODE -eq 0 ]; then
            echo -e "${GREEN}✓ All tests PASSED!${NC}"
        else
            echo -e "${RED}✗ Some tests FAILED!${NC}"
        fi
        exit $EXIT_CODE
        ;;

    generate-solution)
        echo -e "${BLUE}Generate Default Solution Template${NC}"
        echo ""

        read -p "Solution ID (e.g., face_detection_webcam): " SOLUTION_ID
        read -p "Solution Name (e.g., Face Detection with Webcam): " SOLUTION_NAME
        read -p "Solution Type (e.g., face_detection): " SOLUTION_TYPE

        if [ -z "$SOLUTION_ID" ] || [ -z "$SOLUTION_NAME" ] || [ -z "$SOLUTION_TYPE" ]; then
            echo -e "${RED}All fields are required${NC}"
            exit 1
        fi

        FUNCTION_NAME=$(echo "$SOLUTION_ID" | sed 's/_\([a-z]\)/\U\1/g' | sed 's/^\([a-z]\)/\U\1/')
        FUNCTION_NAME="register${FUNCTION_NAME}Solution"

        echo ""
        echo -e "${GREEN}Template code:${NC}"
        echo "void SolutionRegistry::${FUNCTION_NAME}() {"
        echo "    SolutionConfig config;"
        echo "    config.solutionId = \"${SOLUTION_ID}\";"
        echo "    config.solutionName = \"${SOLUTION_NAME}\";"
        echo "    config.solutionType = \"${SOLUTION_TYPE}\";"
        echo "    // Add your solution configuration here"
        echo "    registerSolution(config);"
        echo "}"
        ;;

    restore-solutions)
        echo -e "${BLUE}Restore Default Solutions${NC}"
        echo ""
        echo "This would restore default solutions from backup"
        echo "Feature not yet implemented"
        ;;

    setup-face-db)
        # Configuration
        SERVICE_USER="edgeai"
        SERVICE_GROUP="edgeai"
        INSTALL_DIR="/opt/edge_ai_api"
        DB_FILENAME="face_database.txt"
        
        # Permission mode: "standard" (644) or "full" (666)
        PERMISSION_MODE="standard"
        
        # Parse options
        shift
        while [[ $# -gt 0 ]]; do
            case $1 in
                --full-permissions|--full)
                    PERMISSION_MODE="full"
                    shift
                    ;;
                --standard-permissions|--standard)
                    PERMISSION_MODE="standard"
                    shift
                    ;;
                *)
                    echo -e "${YELLOW}Unknown option: $1${NC}"
                    echo "Usage: $0 setup-face-db [--full-permissions|--standard-permissions]"
                    exit 1
                    ;;
            esac
        done
        
        # Check if running as root
        if [ "$EUID" -ne 0 ]; then
            echo -e "${RED}Error: Command này cần chạy với quyền root (sudo)${NC}"
            echo "Usage: sudo $0 setup-face-db"
            exit 1
        fi
        
        echo -e "${BLUE}===========================================${NC}"
        echo -e "${BLUE}Edge AI API - Setup Face Database${NC}"
        echo -e "${BLUE}===========================================${NC}"
        echo ""
        
        # Check if user exists
        if ! id "$SERVICE_USER" &>/dev/null; then
            echo -e "${YELLOW}Warning: User $SERVICE_USER chưa tồn tại${NC}"
            echo "Tạo user $SERVICE_USER..."
            if useradd -r -s /bin/false -d "$INSTALL_DIR" "$SERVICE_USER" 2>/dev/null; then
                echo -e "${GREEN}✓${NC} Đã tạo user: $SERVICE_USER"
            else
                echo -e "${RED}Error: Không thể tạo user $SERVICE_USER${NC}"
                exit 1
            fi
        fi
        
        # Ensure group exists
        if ! getent group "$SERVICE_GROUP" > /dev/null 2>&1; then
            if groupadd -r "$SERVICE_GROUP" 2>/dev/null; then
                echo -e "${GREEN}✓${NC} Đã tạo group: $SERVICE_GROUP"
            fi
            usermod -a -G "$SERVICE_GROUP" "$SERVICE_USER" 2>/dev/null || true
        fi
        
        # Function to setup database file
        setup_database_file() {
            local db_path="$1"
            local description="$2"
            
            echo -e "${BLUE}Setting up: $description${NC}"
            echo "  Path: $db_path"
            
            # Create parent directory if needed
            local parent_dir=$(dirname "$db_path")
            if [ ! -d "$parent_dir" ]; then
                mkdir -p "$parent_dir"
                echo -e "  ${GREEN}✓${NC} Đã tạo thư mục: $parent_dir"
            else
                echo -e "  ${GREEN}✓${NC} Thư mục đã tồn tại: $parent_dir"
            fi
            
            # Create database file if it doesn't exist
            if [ ! -f "$db_path" ]; then
                touch "$db_path"
                echo -e "  ${GREEN}✓${NC} Đã tạo file: $db_path"
            else
                echo -e "  ${GREEN}✓${NC} File đã tồn tại: $db_path"
            fi
            
            # Set ownership
            chown "$SERVICE_USER:$SERVICE_GROUP" "$db_path"
            chown -R "$SERVICE_USER:$SERVICE_GROUP" "$parent_dir"
            echo -e "  ${GREEN}✓${NC} Đã set ownership: $SERVICE_USER:$SERVICE_GROUP"
            
            # Set permissions
            if [ "$PERMISSION_MODE" = "full" ]; then
                chmod 666 "$db_path"
                chmod 777 "$parent_dir"
                echo -e "  ${YELLOW}⚠${NC} Đã cấp quyền FULL (666) cho file"
            else
                chmod 644 "$db_path"
                chmod 755 "$parent_dir"
                echo -e "  ${GREEN}✓${NC} Đã cấp quyền STANDARD (644) cho file"
            fi
            
            # Verify write permission
            if [ -w "$db_path" ]; then
                echo -e "  ${GREEN}✓${NC} Xác nhận: Có quyền ghi vào file"
            else
                echo -e "  ${RED}✗${NC} Lỗi: Không có quyền ghi vào file"
                return 1
            fi
            
            echo ""
            return 0
        }
        
        # Setup database files at all possible locations
        echo -e "${BLUE}Tạo và cấp quyền cho face_database.txt...${NC}"
        echo ""
        
        # 1. Production path
        setup_database_file "$INSTALL_DIR/data/$DB_FILENAME" "Production path"
        
        # 2. User directory (if HOME is set)
        if [ -n "$HOME" ] && [ "$HOME" != "" ]; then
            USER_DB_PATH="$HOME/.local/share/edge_ai_api/$DB_FILENAME"
            setup_database_file "$USER_DB_PATH" "User directory"
        else
            echo -e "${YELLOW}⚠${NC} HOME không được set, bỏ qua user directory"
            echo ""
        fi
        
        # 3. Current directory (if running from project root)
        CURRENT_DB_PATH="$PROJECT_ROOT/$DB_FILENAME"
        if [ -d "$PROJECT_ROOT" ]; then
            setup_database_file "$CURRENT_DB_PATH" "Current directory (project root)"
        fi
        
        # Summary
        echo -e "${GREEN}===========================================${NC}"
        echo -e "${GREEN}✓ Hoàn tất!${NC}"
        echo -e "${GREEN}===========================================${NC}"
        echo ""
        echo "Các file face_database.txt đã được setup:"
        echo "  1. $INSTALL_DIR/data/$DB_FILENAME (production)"
        if [ -n "$HOME" ] && [ "$HOME" != "" ]; then
            echo "  2. $HOME/.local/share/edge_ai_api/$DB_FILENAME (user directory)"
        fi
        if [ -d "$PROJECT_ROOT" ]; then
            echo "  3. $CURRENT_DB_PATH (current directory)"
        fi
        echo ""
        echo "Quyền sở hữu: $SERVICE_USER:$SERVICE_GROUP"
        echo ""
        if [ "$PERMISSION_MODE" = "full" ]; then
            echo -e "${YELLOW}Quyền hiện tại: FULL (666)${NC}"
            echo "  - File: -rw-rw-rw- (mọi người có thể đọc/ghi)"
            echo "  - Directory: drwxrwxrwx (mọi người có thể truy cập)"
        else
            echo -e "${GREEN}Quyền hiện tại: STANDARD (644)${NC}"
            echo "  - File: -rw-r--r-- (chỉ owner có thể ghi)"
            echo "  - Directory: drwxr-xr-x (chỉ owner/group có thể ghi)"
        fi
        echo ""
        echo "Lưu ý:"
        echo "  - Service sẽ tự động chọn path phù hợp khi khởi động"
        echo "  - Priority: FACE_DATABASE_PATH env var > Production > User > Current"
        echo "  - Để set custom path: export FACE_DATABASE_PATH=/custom/path/face_database.txt"
        ;;
    
    help|--help|-h)
        echo "Usage: $0 <command> [options]"
        echo ""
        echo "Commands:"
        echo "  test                    Run unit tests"
        echo "  generate-solution       Generate default solution template"
        echo "  restore-solutions       Restore default solutions"
        echo "  setup-face-db           Setup face database permissions (requires sudo)"
        echo "  help                    Show this help"
        echo ""
        echo "Examples:"
        echo "  $0 test"
        echo "  $0 test build"
        echo "  $0 generate-solution"
        echo "  sudo $0 setup-face-db"
        echo "  sudo $0 setup-face-db --full-permissions"
        ;;

    *)
        echo -e "${RED}Unknown command: $COMMAND${NC}"
        echo "Run '$0 help' for usage"
        exit 1
        ;;
esac
