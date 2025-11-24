#!/bin/bash
# Script to install dependencies for Edge AI API project
#
# NOTE: This is a helper script for development setup only.
# Modern C++ projects should use CMake FetchContent or package managers
# like vcpkg/Conan instead of shell scripts.
#
# This script is provided for convenience but is NOT required for building.
# CMake will check dependencies automatically and provide instructions.

set -e

echo "=========================================="
echo "Installing dependencies for Edge AI API"
echo "=========================================="
echo ""
echo "NOTE: This script is optional. CMake will check dependencies automatically."
echo "      Modern approach: Use CMake FetchContent (already configured)"
echo ""

# Detect OS
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$ID
else
    echo "Cannot detect OS. Assuming Ubuntu/Debian"
    OS="ubuntu"
fi

echo "Detected OS: $OS"
echo ""

# Install dependencies based on OS
if [ "$OS" = "ubuntu" ] || [ "$OS" = "debian" ]; then
    echo "Installing dependencies for Ubuntu/Debian..."
    sudo apt-get update
    sudo apt-get install -y \
        build-essential \
        cmake \
        git \
        libssl-dev \
        zlib1g-dev \
        libjsoncpp-dev \
        uuid-dev \
        pkg-config
    
    echo ""
    echo "✅ Dependencies installed successfully!"
    
elif [ "$OS" = "centos" ] || [ "$OS" = "rhel" ] || [ "$OS" = "fedora" ]; then
    echo "Installing dependencies for CentOS/RHEL/Fedora..."
    
    if command -v dnf &> /dev/null; then
        sudo dnf install -y \
            gcc-c++ \
            cmake \
            git \
            openssl-devel \
            zlib-devel \
            jsoncpp-devel \
            libuuid-devel \
            pkgconfig
    else
        sudo yum install -y \
            gcc-c++ \
            cmake \
            git \
            openssl-devel \
            zlib-devel \
            jsoncpp-devel \
            libuuid-devel \
            pkgconfig
    fi
    
    echo ""
    echo "✅ Dependencies installed successfully!"
    
else
    echo "⚠️  Unknown OS. Please install dependencies manually:"
    echo "   - build-essential / gcc-c++"
    echo "   - cmake (3.14+)"
    echo "   - git"
    echo "   - libssl-dev / openssl-devel"
    echo "   - zlib1g-dev / zlib-devel"
    echo "   - libjsoncpp-dev / jsoncpp-devel"
    echo "   - uuid-dev / libuuid-devel"
    echo "   - pkg-config / pkgconfig"
    exit 1
fi

echo ""
echo "=========================================="
echo "Dependencies installation complete!"
echo "=========================================="
echo ""
echo "You can now build the project:"
echo "  mkdir build && cd build"
echo "  cmake .."
echo "  make -j\$(nproc)"
echo ""

