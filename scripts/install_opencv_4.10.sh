#!/bin/bash
# Script to build and install OpenCV 4.10 from source
# Required for CVEDIX SDK compatibility

set -e

echo "=========================================="
echo "Installing OpenCV 4.10 from source"
echo "=========================================="
echo ""

# Check if running as root
if [ "$EUID" -eq 0 ]; then 
   echo "Please do not run this script as root"
   exit 1
fi

# Install dependencies
echo "Step 1: Installing build dependencies..."
sudo apt-get update

# Detect Ubuntu version and adjust package names
UBUNTU_VERSION=$(lsb_release -rs 2>/dev/null || echo "22.04")
echo "Detected Ubuntu version: $UBUNTU_VERSION"

# Fix any broken dependencies first
echo "Fixing broken dependencies..."
echo "Note: If you see version mismatch errors with cvedix packages, you may need to:"
echo "  1. Remove cvedix-ai-runtime-dev if installed: sudo apt remove cvedix-ai-runtime-dev"
echo "  2. Or update it to match cvedix-ai-runtime version"
echo ""
sudo apt --fix-broken install -y || {
    echo ""
    echo "Warning: Some dependencies may have issues. Continuing anyway..."
    echo "You may need to resolve package conflicts manually."
    echo ""
}

# Base packages (common for all versions)
# Split into essential and optional to handle dependencies better
echo "Installing essential build tools..."
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config

echo "Installing image/video processing libraries..."
sudo apt-get install -y \
    libgtk-3-dev \
    libavcodec-dev \
    libavformat-dev \
    libswscale-dev \
    libv4l-dev \
    libxvidcore-dev \
    libx264-dev \
    libjpeg-dev \
    libpng-dev \
    libtiff-dev \
    libopenexr-dev

echo "Installing math and scientific libraries..."
sudo apt-get install -y \
    gfortran \
    libatlas-base-dev \
    libeigen3-dev \
    libblas-dev \
    liblapack-dev

echo "Installing Python support..."
sudo apt-get install -y \
    python3-dev \
    python3-numpy

echo "Installing GStreamer support..."
sudo apt-get install -y \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev

# Optional packages - install separately to handle failures gracefully
echo "Installing optional dependencies (may skip if unavailable)..."
sudo apt-get install -y libhdf5-dev || echo "Warning: libhdf5-dev not available, skipping..."
sudo apt-get install -y libprotobuf-dev protobuf-compiler || echo "Warning: protobuf packages not available, skipping..."
sudo apt-get install -y libgoogle-glog-dev || echo "Warning: libgoogle-glog-dev not available, skipping..."
sudo apt-get install -y libgflags-dev || echo "Warning: libgflags-dev not available, skipping..."

# Version-specific packages
# Ubuntu 24.04+ uses libtbb-dev and libdc1394-dev (not libtbb2 or libdc1394-22-dev)
# Try Ubuntu 24.04 packages first, fallback to older versions if needed
echo "Installing version-specific packages..."
if ! sudo apt-get install -y libtbb-dev libdc1394-dev 2>/dev/null; then
    echo "Trying alternative package names for older Ubuntu versions..."
    sudo apt-get install -y libtbb2 libtbb-dev libdc1394-22-dev || {
        echo "Warning: Some packages may not be available, continuing anyway..."
    }
fi

echo ""
echo "Step 2: Cloning OpenCV repositories..."
cd ~
if [ -d "opencv" ]; then
    echo "OpenCV directory exists, updating..."
    cd opencv
    git fetch
    git checkout 4.10.0 || git checkout master
    git pull
else
    echo "Cloning OpenCV..."
    git clone https://github.com/opencv/opencv.git
    cd opencv
    git checkout 4.10.0
fi

cd ~
if [ -d "opencv_contrib" ]; then
    echo "OpenCV contrib directory exists, updating..."
    cd opencv_contrib
    git fetch
    git checkout 4.10.0 || git checkout master
    git pull
else
    echo "Cloning OpenCV contrib..."
    git clone https://github.com/opencv/opencv_contrib.git
    cd opencv_contrib
    git checkout 4.10.0
fi

echo ""
echo "Step 3: Building OpenCV 4.10..."
cd ~/opencv
mkdir -p build
cd build

# Configure CMake
cmake -D CMAKE_BUILD_TYPE=RELEASE \
      -D CMAKE_INSTALL_PREFIX=/usr/local \
      -D OPENCV_EXTRA_MODULES_PATH=~/opencv_contrib/modules \
      -D OPENCV_GENERATE_PKGCONFIG=ON \
      -D BUILD_EXAMPLES=OFF \
      -D BUILD_DOCS=OFF \
      -D BUILD_TESTS=OFF \
      -D BUILD_PERF_TESTS=OFF \
      -D BUILD_opencv_python2=OFF \
      -D BUILD_opencv_python3=ON \
      -D WITH_CUDA=OFF \
      -D WITH_OPENGL=ON \
      -D WITH_TBB=ON \
      -D WITH_V4L=ON \
      -D WITH_FFMPEG=ON \
      -D WITH_GSTREAMER=ON \
      -D OPENCV_ENABLE_NONFREE=OFF \
      -D ENABLE_CXX11=ON \
      -D CMAKE_CXX_STANDARD=11 \
      ..

echo ""
echo "Step 4: Compiling OpenCV (this may take 30-60 minutes)..."
make -j$(nproc)

echo ""
echo "Step 5: Installing OpenCV..."
sudo make install

echo ""
echo "Step 6: Updating library cache..."
sudo ldconfig

echo ""
echo "=========================================="
echo "OpenCV 4.10 installation completed!"
echo "=========================================="
echo ""
echo "Verifying installation..."
pkg-config --modversion opencv4 || echo "pkg-config not found, but installation should be complete"
echo ""
echo "OpenCV 4.10 libraries should now be available at:"
echo "  /usr/local/lib/libopencv_*.so.410"
echo ""
echo "You can now rebuild your project:"
echo "  cd ~/project/edge_ai_api/build"
echo "  cmake .."
echo "  make -j\$(nproc)"

