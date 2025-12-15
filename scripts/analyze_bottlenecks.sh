#!/bin/bash

# Script để phân tích bottleneck trong codebase
# Sử dụng các công cụ như grep, cloc, và phân tích code

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUTPUT_DIR="${PROJECT_ROOT}/bottleneck_analysis"
mkdir -p "${OUTPUT_DIR}"

echo "=========================================="
echo "BOTTLENECK ANALYSIS REPORT"
echo "=========================================="
echo ""

# 1. Đếm số lượng mutex/locks
echo "1. LOCK CONTENTION ANALYSIS"
echo "----------------------------"
LOCK_COUNT=$(grep -r "std::lock_guard\|std::unique_lock\|std::shared_lock\|mutex\|lock" \
    "${PROJECT_ROOT}/src" "${PROJECT_ROOT}/include" 2>/dev/null | wc -l)
echo "Total lock operations found: ${LOCK_COUNT}"
echo ""

# 2. Tìm các deep copy operations
echo "2. MEMORY COPY ANALYSIS"
echo "-----------------------"
COPY_COUNT=$(grep -r "copyTo\|\.clone()\|memcpy\|memmove" \
    "${PROJECT_ROOT}/src" "${PROJECT_ROOT}/include" 2>/dev/null | wc -l)
echo "Total copy operations found: ${COPY_COUNT}"
echo ""

# 3. Tìm sleep/wait operations
echo "3. SLEEP/WAIT OPERATIONS"
echo "------------------------"
SLEEP_COUNT=$(grep -ri "sleep\|wait\|usleep" \
    "${PROJECT_ROOT}/src" "${PROJECT_ROOT}/include" 2>/dev/null | wc -l)
echo "Total sleep/wait operations found: ${SLEEP_COUNT}"
echo ""

# 4. Tìm thread operations
echo "4. THREAD ANALYSIS"
echo "-----------------"
THREAD_COUNT=$(grep -r "std::thread\|pthread\|thread" \
    "${PROJECT_ROOT}/src" "${PROJECT_ROOT}/include" 2>/dev/null | wc -l)
echo "Total thread operations found: ${THREAD_COUNT}"
echo ""

# 5. Tìm I/O operations (RTSP/RTMP)
echo "5. I/O OPERATIONS ANALYSIS"
echo "--------------------------"
IO_COUNT=$(grep -ri "rtsp\|rtmp\|gstreamer\|mosquitto\|read\|write" \
    "${PROJECT_ROOT}/src" "${PROJECT_ROOT}/include" 2>/dev/null | wc -l)
echo "Total I/O operations found: ${IO_COUNT}"
echo ""

# 6. Tìm frame processing operations
echo "6. FRAME PROCESSING ANALYSIS"
echo "----------------------------"
FRAME_COUNT=$(grep -ri "cv::Mat\|frame\|processFrame" \
    "${PROJECT_ROOT}/src" "${PROJECT_ROOT}/include" 2>/dev/null | wc -l)
echo "Total frame operations found: ${FRAME_COUNT}"
echo ""

# 7. Tạo báo cáo chi tiết
echo "7. DETAILED ANALYSIS"
echo "--------------------"
echo "Creating detailed report..."

# Locks chi tiết
echo "Lock locations:" > "${OUTPUT_DIR}/locks.txt"
grep -rn "std::lock_guard\|std::unique_lock\|std::shared_lock" \
    "${PROJECT_ROOT}/src" "${PROJECT_ROOT}/include" 2>/dev/null \
    | head -50 >> "${OUTPUT_DIR}/locks.txt"

# Copies chi tiết
echo "Copy operations:" > "${OUTPUT_DIR}/copies.txt"
grep -rn "copyTo\|\.clone()" \
    "${PROJECT_ROOT}/src" "${PROJECT_ROOT}/include" 2>/dev/null \
    | head -50 >> "${OUTPUT_DIR}/copies.txt"

# Sleep operations chi tiết
echo "Sleep operations:" > "${OUTPUT_DIR}/sleeps.txt"
grep -rn "sleep\|wait" \
    "${PROJECT_ROOT}/src" "${PROJECT_ROOT}/include" 2>/dev/null \
    | head -50 >> "${OUTPUT_DIR}/sleeps.txt"

echo ""
echo "=========================================="
echo "Analysis complete!"
echo "Reports saved to: ${OUTPUT_DIR}/"
echo "=========================================="

