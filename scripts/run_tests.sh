#!/bin/bash

# Script to run unit tests
# Usage: ./scripts/run_tests.sh [build_dir]

set -e

BUILD_DIR="${1:-build}"
TEST_EXEC="${BUILD_DIR}/bin/edge_ai_api_tests"

echo "========================================"
echo "Running Edge AI API Unit Tests"
echo "========================================"
echo ""

# Check if test executable exists
if [ ! -f "${TEST_EXEC}" ]; then
    echo "Error: Test executable not found at ${TEST_EXEC}"
    echo "Please build tests first:"
    echo "  cd ${BUILD_DIR}"
    echo "  cmake .. -DBUILD_TESTS=ON"
    echo "  make -j\$(nproc)"
    exit 1
fi

# Run tests
echo "Running tests..."
echo ""

"${TEST_EXEC}"

EXIT_CODE=$?

echo ""
if [ $EXIT_CODE -eq 0 ]; then
    echo "========================================"
    echo "✓ All tests PASSED!"
    echo "========================================"
else
    echo "========================================"
    echo "✗ Some tests FAILED!"
    echo "========================================"
fi

exit $EXIT_CODE

