#!/bin/bash
# ============================================
# Edge AI API - Development Setup Script
# ============================================
#
# Script tổng hợp cho development setup:
# 1. Install system dependencies
# 2. Fix symlinks (CVEDIX SDK, Cereal, cpp-base64, OpenCV)
# 3. Build project (optional)
# 4. Setup face database (optional, requires sudo)
#
# Usage:
#   ./scripts/dev_setup.sh [options]
#
# Options:
#   --all, --full-setup  Setup everything including face database (requires sudo) - mặc định
#   --skip-deps      Skip installing dependencies
#   --skip-symlinks  Skip fixing symlinks
#   --skip-build     Skip building project
#   --build-only     Only build, skip other steps
#   --setup-face-db  Setup face database permissions (requires sudo)
#   --help, -h       Show help
#
# ============================================

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Flags
SKIP_DEPS=false
SKIP_SYMLINKS=false
SKIP_BUILD=false
BUILD_ONLY=false
# Mặc định: setup face database nếu có quyền root
SETUP_FACE_DB=false
if [ "$EUID" -eq 0 ]; then
    SETUP_FACE_DB=true
fi
FULL_SETUP=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --skip-deps)
            SKIP_DEPS=true
            shift
            ;;
        --skip-symlinks)
            SKIP_SYMLINKS=true
            shift
            ;;
        --skip-build)
            SKIP_BUILD=true
            shift
            ;;
        --build-only)
            BUILD_ONLY=true
            SKIP_DEPS=true
            SKIP_SYMLINKS=true
            shift
            ;;
        --setup-face-db)
            SETUP_FACE_DB=true
            shift
            ;;
        --all|--full-setup)
            FULL_SETUP=true
            SETUP_FACE_DB=true
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --skip-deps      Skip installing dependencies"
            echo "  --skip-symlinks  Skip fixing symlinks"
            echo "  --skip-build     Skip building project"
            echo "  --build-only     Only build, skip other steps"
            echo "  --setup-face-db  Setup face database permissions (requires sudo)"
            echo "  --all, --full-setup  Setup everything including face database (requires sudo)"
            echo "  --help, -h       Show this help"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            echo "Run '$0 --help' for usage"
            exit 1
            ;;
    esac
done

echo "=========================================="
echo "Edge AI API - Development Setup"
echo "=========================================="
echo ""

# Show setup mode
if [ "$FULL_SETUP" = true ]; then
    echo -e "${BLUE}Full setup mode: Setup everything including face database${NC}"
    echo ""
elif [ "$SETUP_FACE_DB" = true ] && [ "$EUID" -eq 0 ]; then
    echo -e "${BLUE}Auto-setup mode: Setup everything (face database enabled - running as root)${NC}"
    echo ""
fi

# ============================================
# Step 1: Install Dependencies
# ============================================
if [ "$SKIP_DEPS" = false ] && [ "$BUILD_ONLY" = false ]; then
    echo -e "${BLUE}[1/3]${NC} Installing system dependencies..."

    # Detect OS
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        OS=$ID
    else
        OS="ubuntu"
    fi

    echo "Detected OS: $OS"

    if [ "$OS" = "ubuntu" ] || [ "$OS" = "debian" ]; then
        sudo apt-get update
        sudo apt-get install -y \
            build-essential \
            cmake \
            git \
            libssl-dev \
            zlib1g-dev \
            libjsoncpp-dev \
            uuid-dev \
            pkg-config \
            libopencv-dev \
            libgstreamer1.0-dev \
            libgstreamer-plugins-base1.0-dev \
            libmosquitto-dev
        echo -e "${GREEN}✓${NC} Dependencies installed"
    elif [ "$OS" = "centos" ] || [ "$OS" = "rhel" ] || [ "$OS" = "fedora" ]; then
        if command -v dnf &> /dev/null; then
            sudo dnf install -y gcc-c++ cmake git openssl-devel zlib-devel \
                jsoncpp-devel libuuid-devel pkgconfig opencv-devel \
                gstreamer1-devel gstreamer1-plugins-base-devel mosquitto-devel
        else
            sudo yum install -y gcc-c++ cmake git openssl-devel zlib-devel \
                jsoncpp-devel libuuid-devel pkgconfig opencv-devel \
                gstreamer1-devel gstreamer1-plugins-base-devel mosquitto-devel
        fi
        echo -e "${GREEN}✓${NC} Dependencies installed"
    else
        echo -e "${YELLOW}⚠${NC}  Unknown OS. Please install dependencies manually"
    fi
    echo ""
fi

# ============================================
# Step 2: Fix Symlinks
# ============================================
if [ "$SKIP_SYMLINKS" = false ] && [ "$BUILD_ONLY" = false ]; then
    echo -e "${BLUE}[2/3]${NC} Fixing symlinks..."

    if [ "$EUID" -ne 0 ]; then
        echo -e "${YELLOW}⚠${NC}  Need sudo to fix symlinks. Skipping..."
    else
        # Fix CVEDIX SDK libraries
        CVEDIX_LIB_DIR="/opt/cvedix/lib"
        if [ -d "$CVEDIX_LIB_DIR" ]; then
            for lib in libtinyexpr.so libcvedix_instance_sdk.so; do
                if [ -f "$CVEDIX_LIB_DIR/$lib" ]; then
                    ln -sf "$CVEDIX_LIB_DIR/$lib" "/usr/lib/$lib" 2>/dev/null || true
                fi
            done
        fi

        # Fix Cereal symlink
        if [ -d "$PROJECT_ROOT/build/_deps/cereal-src/include/cereal" ]; then
            mkdir -p /usr/include/cvedix/third_party
            ln -sf "$PROJECT_ROOT/build/_deps/cereal-src/include/cereal" \
                "/usr/include/cvedix/third_party/cereal" 2>/dev/null || true
        fi

        # Fix cpp-base64 symlink
        if [ -f "$PROJECT_ROOT/build/_deps/cpp-base64-src/base64.h" ]; then
            mkdir -p /usr/include/cvedix/third_party/cpp_base64
            ln -sf "$PROJECT_ROOT/build/_deps/cpp-base64-src/base64.h" \
                "/usr/include/cvedix/third_party/cpp_base64/base64.h" 2>/dev/null || true
        fi

        echo -e "${GREEN}✓${NC} Symlinks fixed"
    fi
    echo ""
fi

# ============================================
# Step 3: Build Project
# ============================================
if [ "$SKIP_BUILD" = false ]; then
    echo -e "${BLUE}[3/3]${NC} Building project..."
    cd "$PROJECT_ROOT"

    if [ ! -d "build" ]; then
        mkdir -p build
    fi

    cd build

    if [ ! -f "CMakeCache.txt" ]; then
        echo "Running CMake..."
        cmake .. -DCMAKE_BUILD_TYPE=Release \
                 -DAUTO_DOWNLOAD_DEPENDENCIES=ON \
                 -DDROGON_USE_FETCHCONTENT=ON
    fi

    echo "Building (using all CPU cores)..."
    make -j$(nproc)

    cd ..
    echo -e "${GREEN}✓${NC} Build completed"
    echo ""
fi

# ============================================
# Step 4: Setup Face Database (Optional)
# ============================================
if [ "$SETUP_FACE_DB" = true ]; then
    echo -e "${BLUE}[4/4]${NC} Setup face database permissions..."

    if [ "$EUID" -ne 0 ]; then
        echo -e "${YELLOW}⚠${NC}  Cần quyền root để setup face database. Bỏ qua."
        echo "  Chạy thủ công: sudo ./scripts/utils.sh setup-face-db"
    else
        if [ -f "$PROJECT_ROOT/scripts/utils.sh" ]; then
            "$PROJECT_ROOT/scripts/utils.sh" setup-face-db --standard-permissions
            echo -e "${GREEN}✓${NC} Face database setup hoàn tất!"
        else
            echo -e "${YELLOW}⚠${NC}  utils.sh không tồn tại. Bỏ qua setup face database."
        fi
    fi
    echo ""
fi

echo "=========================================="
echo -e "${GREEN}Setup completed!${NC}"
echo "=========================================="
echo ""
echo "To run the server:"
echo "  ./scripts/load_env.sh"
echo ""
echo "Or manually:"
if [ -f "build/bin/edge_ai_api" ]; then
    echo "  ./build/bin/edge_ai_api"
elif [ -f "build/edge_ai_api" ]; then
    echo "  ./build/edge_ai_api"
fi
echo ""
