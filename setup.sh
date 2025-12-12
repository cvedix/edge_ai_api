#!/bin/bash
# ============================================
# Edge AI API - Complete Setup Script
# ============================================
# 
# Script này tự động setup và start server từ đầu đến cuối:
# 1. Kiểm tra prerequisites
# 2. Cài đặt system dependencies
# 3. Build project
# 4. Setup systemd service (optional)
# 5. Start server
#
# Usage:
#   ./setup.sh                    # Setup và chạy development mode
#   ./setup.sh --production       # Setup và deploy production (cần sudo)
#   ./setup.sh --skip-deps        # Bỏ qua cài đặt dependencies
#   ./setup.sh --skip-build       # Bỏ qua build (dùng build có sẵn)
#   ./setup.sh --no-service       # Không setup systemd service
#   ./setup.sh --help             # Hiển thị help
#
# ============================================

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"

# Configuration
SERVICE_USER="edgeai"
SERVICE_GROUP="edgeai"
INSTALL_DIR="/opt/edge_ai_api"
BIN_DIR="/usr/local/bin"
SERVICE_NAME="edge-ai-api"

# Flags
SKIP_DEPS=false
SKIP_BUILD=false
NO_SERVICE=false
PRODUCTION_MODE=false
START_SERVER=true

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --skip-deps)
            SKIP_DEPS=true
            shift
            ;;
        --skip-build)
            SKIP_BUILD=true
            shift
            ;;
        --no-service)
            NO_SERVICE=true
            shift
            ;;
        --production|--prod)
            PRODUCTION_MODE=true
            shift
            ;;
        --no-start)
            START_SERVER=false
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --skip-deps       Skip cài đặt system dependencies"
            echo "  --skip-build      Skip build (dùng build có sẵn)"
            echo "  --no-service      Không setup systemd service (chỉ chạy development)"
            echo "  --production      Setup production với systemd service (cần sudo)"
            echo "  --no-start        Không tự động start server sau khi setup"
            echo "  --help, -h        Hiển thị help này"
            echo ""
            echo "Examples:"
            echo "  ./setup.sh                      # Development setup"
            echo "  ./setup.sh --production         # Production setup (cần sudo)"
            echo "  ./setup.sh --skip-deps          # Bỏ qua cài dependencies"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            echo "Run './setup.sh --help' for usage information"
            exit 1
            ;;
    esac
done

echo "=========================================="
echo "Edge AI API - Complete Setup"
echo "=========================================="
echo ""
echo "Mode: $([ "$PRODUCTION_MODE" = true ] && echo "Production" || echo "Development")"
echo "Options:"
echo "  Skip dependencies: $SKIP_DEPS"
echo "  Skip build:        $SKIP_BUILD"
echo "  Setup service:     $([ "$NO_SERVICE" = true ] && echo "No" || echo "Yes")"
echo "  Start server:      $START_SERVER"
echo ""

# ============================================
# Step 1: Check Prerequisites
# ============================================
echo -e "${BLUE}[1/5]${NC} Kiểm tra prerequisites..."

# Check if running as root (only needed for production)
if [ "$PRODUCTION_MODE" = true ] && [ "$EUID" -ne 0 ]; then
    echo -e "${RED}Error: Production mode cần chạy với quyền sudo${NC}"
    echo "Usage: sudo ./setup.sh --production"
    exit 1
fi

# Check OS
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$ID
    OS_VERSION=$VERSION_ID
else
    echo -e "${YELLOW}⚠${NC}  Cannot detect OS. Assuming Ubuntu/Debian"
    OS="ubuntu"
fi

echo -e "${GREEN}✓${NC} Detected OS: $OS $OS_VERSION"

# Check required commands
MISSING_CMDS=()
for cmd in git cmake make; do
    if ! command -v $cmd &> /dev/null; then
        MISSING_CMDS+=($cmd)
    fi
done

if [ ${#MISSING_CMDS[@]} -gt 0 ] && [ "$SKIP_DEPS" = false ]; then
    echo -e "${YELLOW}⚠${NC}  Missing commands: ${MISSING_CMDS[*]}"
    echo "  Will be installed in next step"
elif [ ${#MISSING_CMDS[@]} -gt 0 ]; then
    echo -e "${RED}Error: Missing required commands: ${MISSING_CMDS[*]}${NC}"
    echo "  Install manually or run without --skip-deps"
    exit 1
else
    echo -e "${GREEN}✓${NC} All required commands available"
fi

# Check CMake version
if command -v cmake &> /dev/null; then
    CMAKE_VERSION=$(cmake --version | head -n1 | cut -d' ' -f3)
    CMAKE_MAJOR=$(echo $CMAKE_VERSION | cut -d'.' -f1)
    CMAKE_MINOR=$(echo $CMAKE_VERSION | cut -d'.' -f2)
    
    if [ "$CMAKE_MAJOR" -lt 3 ] || ([ "$CMAKE_MAJOR" -eq 3 ] && [ "$CMAKE_MINOR" -lt 14 ]); then
        echo -e "${YELLOW}⚠${NC}  CMake version $CMAKE_VERSION is too old (need 3.14+)"
        if [ "$SKIP_DEPS" = false ]; then
            echo "  Will try to install newer version"
        else
            echo -e "${RED}Error: CMake version too old. Please upgrade or run without --skip-deps${NC}"
            exit 1
        fi
    else
        echo -e "${GREEN}✓${NC} CMake version: $CMAKE_VERSION"
    fi
fi

echo ""

# ============================================
# Step 2: Install System Dependencies
# ============================================
if [ "$SKIP_DEPS" = false ]; then
    echo -e "${BLUE}[2/5]${NC} Cài đặt system dependencies..."
    
    if [ "$OS" = "ubuntu" ] || [ "$OS" = "debian" ]; then
        echo "Installing dependencies for Ubuntu/Debian..."
        
        # Update package list (continue even if some repos fail)
        set +e
        apt-get update > /dev/null 2>&1
        UPDATE_EXIT_CODE=$?
        set -e
        
        if [ $UPDATE_EXIT_CODE -eq 0 ]; then
            echo -e "${GREEN}✓${NC} Package lists updated"
        else
            echo -e "${YELLOW}⚠${NC}  Some repositories had errors (continuing anyway)"
        fi
        
        # Install dependencies
        set +e
        apt-get install -y \
            build-essential \
            cmake \
            git \
            libssl-dev \
            zlib1g-dev \
            libjsoncpp-dev \
            uuid-dev \
            pkg-config \
            > /dev/null 2>&1
        INSTALL_EXIT_CODE=$?
        set -e
        
        if [ $INSTALL_EXIT_CODE -eq 0 ]; then
            echo -e "${GREEN}✓${NC} Dependencies installed successfully"
        else
            echo -e "${YELLOW}⚠${NC}  Some packages failed to install"
            echo "  This might be OK if packages are already installed"
        fi
        
    elif [ "$OS" = "centos" ] || [ "$OS" = "rhel" ] || [ "$OS" = "fedora" ]; then
        echo "Installing dependencies for CentOS/RHEL/Fedora..."
        
        if command -v dnf &> /dev/null; then
            dnf install -y \
                gcc-c++ \
                cmake \
                git \
                openssl-devel \
                zlib-devel \
                jsoncpp-devel \
                libuuid-devel \
                pkgconfig \
                > /dev/null 2>&1
        else
            yum install -y \
                gcc-c++ \
                cmake \
                git \
                openssl-devel \
                zlib-devel \
                jsoncpp-devel \
                libuuid-devel \
                pkgconfig \
                > /dev/null 2>&1
        fi
        
        echo -e "${GREEN}✓${NC} Dependencies installed successfully"
    else
        echo -e "${YELLOW}⚠${NC}  Unknown OS. Please install dependencies manually"
    fi
    echo ""
else
    echo -e "${YELLOW}[2/5]${NC} Skip cài đặt dependencies"
    echo ""
fi

# ============================================
# Step 3: Build Project
# ============================================
if [ "$SKIP_BUILD" = false ]; then
    echo -e "${BLUE}[3/5]${NC} Build project..."
    cd "$PROJECT_ROOT" || exit 1
    
    # Check if CMakeLists.txt exists
    if [ ! -f "CMakeLists.txt" ]; then
        echo -e "${RED}Error: Không tìm thấy CMakeLists.txt trong $PROJECT_ROOT${NC}"
        exit 1
    fi
    
    # Create build directory if not exists
    if [ ! -d "build" ]; then
        echo "Tạo thư mục build..."
        mkdir -p build
    fi
    
    cd build
    
    # Configure with CMake
    if [ ! -f "CMakeCache.txt" ]; then
        echo "Chạy CMake configuration..."
        if ! cmake ..; then
            echo -e "${RED}Error: CMake configuration failed${NC}"
            exit 1
        fi
    else
        echo "CMake đã được cấu hình, chỉ build..."
    fi
    
    # Build project
    echo "Build project (sử dụng tất cả CPU cores)..."
    CPU_CORES=$(nproc)
    echo "Sử dụng $CPU_CORES CPU cores..."
    if ! make -j$CPU_CORES; then
        echo -e "${RED}Error: Build failed${NC}"
        exit 1
    fi
    
    cd ..
    echo -e "${GREEN}✓${NC} Build hoàn tất!"
    echo ""
else
    echo -e "${YELLOW}[3/5]${NC} Skip build (dùng build có sẵn)"
    echo ""
fi

# ============================================
# Step 4: Setup Systemd Service (Production)
# ============================================
if [ "$NO_SERVICE" = false ] && [ "$PRODUCTION_MODE" = true ]; then
    echo -e "${BLUE}[4/5]${NC} Setup systemd service..."
    
    # Check if deploy/build.sh exists (use it for production setup)
    if [ -f "$PROJECT_ROOT/deploy/build.sh" ]; then
        echo "Sử dụng deploy/build.sh để setup production..."
        if [ "$START_SERVER" = false ]; then
            "$PROJECT_ROOT/deploy/build.sh" --no-start
        else
            "$PROJECT_ROOT/deploy/build.sh"
        fi
        echo -e "${GREEN}✓${NC} Production setup hoàn tất!"
    else
        echo -e "${YELLOW}⚠${NC}  deploy/build.sh không tồn tại. Bỏ qua setup service."
    fi
    echo ""
elif [ "$NO_SERVICE" = false ] && [ "$PRODUCTION_MODE" = false ]; then
    echo -e "${YELLOW}[4/5]${NC} Development mode - không setup systemd service"
    echo "  Để setup production, chạy: sudo ./setup.sh --production"
    echo ""
else
    echo -e "${YELLOW}[4/5]${NC} Skip setup service (--no-service)"
    echo ""
fi

# ============================================
# Step 5: Start Server
# ============================================
if [ "$START_SERVER" = true ]; then
    echo -e "${BLUE}[5/5]${NC} Khởi động server..."
    cd "$PROJECT_ROOT" || exit 1
    
    # Find executable
    EXECUTABLE=""
    EXECUTABLE_PATHS=(
        "$PROJECT_ROOT/build/bin/edge_ai_api"
        "$PROJECT_ROOT/build/edge_ai_api"
    )
    
    for path in "${EXECUTABLE_PATHS[@]}"; do
        if [ -f "$path" ] && [ -x "$path" ]; then
            EXECUTABLE="$path"
            break
        fi
    done
    
    if [ -z "$EXECUTABLE" ]; then
        echo -e "${RED}Error: Không tìm thấy executable${NC}"
        echo "Đã kiểm tra các vị trí sau:"
        for path in "${EXECUTABLE_PATHS[@]}"; do
            echo "  - $path"
        done
        echo ""
        echo "Vui lòng build project trước hoặc bỏ --skip-build"
        exit 1
    fi
    
    # Check if service is running (production mode)
    if [ "$PRODUCTION_MODE" = true ] && [ "$NO_SERVICE" = false ]; then
        if systemctl is-active --quiet "${SERVICE_NAME}.service" 2>/dev/null; then
            echo -e "${GREEN}✓${NC} Service đang chạy: ${SERVICE_NAME}.service"
            echo ""
            echo "Kiểm tra trạng thái:"
            systemctl status "${SERVICE_NAME}.service" --no-pager -l | head -n 10
            
            # Try to get API endpoint info
            echo ""
            echo "Kiểm tra API endpoint..."
            sleep 2
            if command -v curl &> /dev/null; then
                API_PORT=$(grep -E "^API_PORT=" "$INSTALL_DIR/config/.env" 2>/dev/null | cut -d'=' -f2 || echo "8080")
                API_HOST="localhost"
                if curl -s -f "http://${API_HOST}:${API_PORT}/v1/core/health" > /dev/null 2>&1; then
                    echo -e "${GREEN}✓${NC} API endpoint đang phản hồi: http://${API_HOST}:${API_PORT}/v1/core/health"
                else
                    echo -e "${YELLOW}⚠${NC}  API endpoint chưa sẵn sàng (có thể cần thêm thời gian)"
                fi
            fi
        else
            echo -e "${YELLOW}⚠${NC}  Service không chạy. Kiểm tra log:"
            echo "  sudo journalctl -u ${SERVICE_NAME}.service -n 50"
        fi
    else
        # Development mode - run directly
        echo "Executable: $EXECUTABLE"
        echo ""
        
        # Check for .env file
        if [ -f "$PROJECT_ROOT/.env" ]; then
            echo "Tìm thấy file .env, sử dụng script load_env.sh..."
            echo ""
            "$PROJECT_ROOT/scripts/load_env.sh"
        else
            # Create .env from .env.example if exists
            if [ -f "$PROJECT_ROOT/.env.example" ]; then
                echo "Tạo .env từ .env.example..."
                cp "$PROJECT_ROOT/.env.example" "$PROJECT_ROOT/.env"
                echo -e "${GREEN}✓${NC} Đã tạo .env file"
                echo "  Vui lòng chỉnh sửa .env nếu cần, sau đó chạy lại:"
                echo "  ./scripts/load_env.sh"
                echo ""
                echo "Hoặc chạy trực tiếp:"
                echo "  cd $(dirname "$EXECUTABLE")"
                echo "  ./$(basename "$EXECUTABLE")"
            else
                echo "Chạy server trực tiếp..."
                echo ""
                cd "$(dirname "$EXECUTABLE")" || exit 1
                ./$(basename "$EXECUTABLE")
            fi
        fi
    fi
else
    echo -e "${YELLOW}[5/5]${NC} Skip khởi động server (--no-start)"
    echo ""
    echo "Để khởi động server:"
    if [ "$PRODUCTION_MODE" = true ] && [ "$NO_SERVICE" = false ]; then
        echo "  sudo systemctl start ${SERVICE_NAME}.service"
    else
        echo "  ./scripts/load_env.sh"
        echo "  hoặc"
        echo "  cd build && ./edge_ai_api"
    fi
fi

echo ""
echo "=========================================="
echo -e "${GREEN}Setup hoàn tất!${NC}"
echo "=========================================="
echo ""
echo "Các lệnh hữu ích:"
if [ "$PRODUCTION_MODE" = true ] && [ "$NO_SERVICE" = false ]; then
    echo "  Xem trạng thái:    sudo systemctl status ${SERVICE_NAME}"
    echo "  Xem log:           sudo journalctl -u ${SERVICE_NAME} -f"
    echo "  Khởi động lại:     sudo systemctl restart ${SERVICE_NAME}"
    echo "  Dừng service:      sudo systemctl stop ${SERVICE_NAME}"
else
    echo "  Chạy server:       ./scripts/load_env.sh"
    echo "  Build lại:         cd build && make -j\$(nproc)"
    echo "  Clean build:       rm -rf build && mkdir build && cd build && cmake .. && make -j\$(nproc)"
fi
echo ""
echo "Tài liệu:"
echo "  docs/DEVELOPMENT_SETUP.md  - Hướng dẫn setup môi trường phát triển"
echo "  docs/GETTING_STARTED.md    - Hướng dẫn khởi động và sử dụng"
echo "  deploy/README.md           - Hướng dẫn triển khai production"
echo ""

