#!/bin/bash
# ============================================
# Build script for MQTT JSON Receiver Sample
# ============================================
# Builds the sample in samples/build directory

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo -e "${BLUE}=========================================="
echo "MQTT JSON Receiver Sample - Build"
echo "==========================================${NC}"
echo ""

# Create build directory
if [ ! -d "build" ]; then
    echo "Tạo thư mục build..."
    mkdir -p build
fi

cd build

# Check if CMake is available
if ! command -v cmake &> /dev/null; then
    echo -e "${RED}Error: CMake không được cài đặt${NC}"
    echo "Vui lòng cài đặt: sudo apt-get install cmake"
    exit 1
fi

# Configure CMake
echo -e "${BLUE}[1/2]${NC} Cấu hình CMake..."
if [ ! -f "CMakeCache.txt" ]; then
    cmake .. -DAUTO_DOWNLOAD_DEPENDENCIES=ON
else
    echo "CMake đã được cấu hình, chỉ build..."
fi

# Build
echo -e "${BLUE}[2/2]${NC} Biên dịch..."
CPU_CORES=$(nproc)
echo "Sử dụng $CPU_CORES CPU cores..."
make -j$CPU_CORES

echo ""
echo -e "${GREEN}=========================================="
echo "Build hoàn tất!"
echo "==========================================${NC}"
echo ""
echo "Executable: $(pwd)/mqtt_json_receiver_sample"
echo ""
echo "Để chạy:"
echo "  ./mqtt_json_receiver_sample [broker_url] [port] [topic] [username] [password]"
echo ""
echo "Ví dụ:"
echo "  ./mqtt_json_receiver_sample anhoidong.datacenter.cvedix.com 1883 events"
echo "  ./mqtt_json_receiver_sample localhost 1883 events user pass"
echo ""

