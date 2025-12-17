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
    
    help|--help|-h)
        echo "Usage: $0 <command> [options]"
        echo ""
        echo "Commands:"
        echo "  test                    Run unit tests"
        echo "  generate-solution       Generate default solution template"
        echo "  restore-solutions       Restore default solutions"
        echo "  help                    Show this help"
        echo ""
        echo "Examples:"
        echo "  $0 test"
        echo "  $0 test build"
        echo "  $0 generate-solution"
        ;;
    
    *)
        echo -e "${RED}Unknown command: $COMMAND${NC}"
        echo "Run '$0 help' for usage"
        exit 1
        ;;
esac

