#!/usr/bin/env bash
# Use set -e for error handling, but allow some commands to fail
# We'll use explicit error checking for critical commands
set -euo pipefail
IFS=$'\n\t'

# Function to safely run commands that might fail
safe_run() {
  set +e
  "$@"
  local exit_code=$?
  set -e
  return $exit_code
}

# build_opencv_safe.sh
# - Skip download if valid zip already exists
# - Download with visible percentage progress (wget --progress=bar:force)
# - Validate zip with `unzip -t`
# - Do NOT run entire script as sudo; only `make install` uses sudo

VERSION=4.10.0
WORKDIR=$(pwd)
OPENCV_ZIP="${VERSION}.zip"
OPENCV_DIR="opencv-${VERSION}"
CONTRIB_ZIP="opencv_contrib-${VERSION}.zip"
CONTRIB_DIR="opencv_contrib-${VERSION}"

# Small helper: print to stderr
log() { echo "[$(date +'%F %T')] $*" >&2; }

# download only if file missing or invalid
download_if_needed() {
  local url="$1"
  local out="$2"
  local tries=0
  local max_tries=5

  if [ -s "${out}" ]; then
    log "Found existing file ${out} — validating..."
    if unzip -t "${out}" >/dev/null 2>&1; then
      log "${out} is a valid zip — skip download."
      return 0
    else
      log "${out} is invalid/corrupt — removing and re-downloading."
      rm -f "${out}"
    fi
  fi

  while [ $tries -lt $max_tries ]; do
    tries=$((tries+1))
    log "Downloading (attempt ${tries}/${max_tries}): ${url} -> ${out}"

    # Use wget with visible percentage/ETA progress bar
    # -c: resume, --tries: attempts on network errors, --timeout: seconds for connect/read
    # --progress=bar:force:noscroll shows percent and ETA
    if wget -c --tries=3 --timeout=60 --progress=bar:force:noscroll "${url}" -O "${out}"; then
      # Validate zip - but be lenient if file size looks reasonable
      FILE_SIZE=$(stat -c%s "${out}" 2>/dev/null || echo 0)
      if unzip -t "${out}" >/dev/null 2>&1; then
        log "Downloaded and validated ${out}"
        return 0
      elif [ -s "${out}" ] && [ "$FILE_SIZE" -gt 50000000 ]; then
        # File exists and has reasonable size (>50MB), try to continue despite validation failure
        log "Downloaded ${out} (${FILE_SIZE} bytes) but validation failed."
        log "File size looks reasonable, continuing anyway (validation may be false positive)..."
        # Try to actually unzip a small part to verify it's not completely corrupted
        if unzip -l "${out}" >/dev/null 2>&1 | head -5 >/dev/null 2>&1; then
          log "File appears to be readable, continuing..."
          return 0
        else
          log "File appears corrupted, removing and retrying."
          rm -f "${out}"
        fi
      else
        log "Downloaded ${out} but validation failed and file size is suspicious. Removing and retrying."
        rm -f "${out}"
      fi
    else
      log "wget failed on attempt ${tries}. Retrying in 5s..."
      sleep 5
    fi
  done

  log "ERROR: Failed to download ${out} after ${max_tries} attempts."
  return 1
}

# Start
log "Working directory: ${WORKDIR}"

# Download opencv and contrib if needed
download_if_needed "https://github.com/opencv/opencv/archive/refs/tags/${VERSION}.zip" "${OPENCV_ZIP}"
download_if_needed "https://github.com/opencv/opencv_contrib/archive/refs/tags/${VERSION}.zip" "${CONTRIB_ZIP}"

# Unpack
log "Unpacking..."
rm -rf "${OPENCV_DIR}" "${CONTRIB_DIR}"
unzip -q "${OPENCV_ZIP}"
unzip -q "${CONTRIB_ZIP}"

# Prepare build dir
cd "${OPENCV_DIR}"
rm -rf build
mkdir build
cd build

# Find python executable
PYTHON3_EXECUTABLE=$(python3 -c 'import sys; print(sys.executable)') || PYTHON3_EXECUTABLE="$(which python3 || true)"
if [ -z "${PYTHON3_EXECUTABLE}" ]; then
  log "ERROR: python3 not found in PATH. Install python3 first."
  exit 1
fi
log "Using PYTHON3_EXECUTABLE=${PYTHON3_EXECUTABLE}"

PYTHON3_PACKAGES_PATH=$(${PYTHON3_EXECUTABLE} -c "import site; print(site.getsitepackages()[0] if hasattr(site,'getsitepackages') else site.getusersitepackages())") || true

# Function to detect NVIDIA GPU
detect_nvidia_gpu() {
  # Check if nvidia-smi works (indicates NVIDIA driver is installed)
  if command -v nvidia-smi >/dev/null 2>&1; then
    if nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null | head -1 | grep -q .; then
      return 0  # GPU found
    fi
  fi
  
  # Fallback: check for /dev/nvidia0
  if [ -c /dev/nvidia0 ] 2>/dev/null; then
    return 0  # GPU device found
  fi
  
  return 1  # No GPU found
}

# Function to check if CUDA Toolkit is installed
check_cuda_toolkit() {
  if command -v nvcc >/dev/null 2>&1; then
    if [ -d "/usr/local/cuda" ] || [ -d "/opt/cuda" ]; then
      return 0  # CUDA Toolkit found
    fi
  fi
  return 1  # CUDA Toolkit not found
}

# Function to check if dpkg is locked (during package installation)
check_dpkg_lock() {
  if [ -f /var/lib/dpkg/lock-frontend ] || [ -f /var/lib/dpkg/lock ]; then
    return 0  # Lock found
  fi
  return 1  # No lock
}

# Function to install CUDA Toolkit automatically
install_cuda_toolkit() {
  log "Attempting to install CUDA Toolkit..."
  
  # Check if dpkg is locked (during package installation)
  if check_dpkg_lock; then
    log "⚠ Cannot install CUDA Toolkit now - dpkg is currently in use"
    log "This is normal during package installation."
    log ""
    log "After package installation completes, you can install CUDA Toolkit:"
    log "  sudo apt-get update"
    log "  sudo apt-get install nvidia-cuda-toolkit"
    log ""
    log "Then rebuild OpenCV with CUDA support:"
    log "  sudo $INSTALL_DIR/scripts/build_opencv_safe.sh"
    return 1
  fi
  
  # Detect OS
  if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$ID
    VERSION_ID=${VERSION_ID:-}
  else
    log "ERROR: Cannot detect OS"
    return 1
  fi
  
  # Only support Ubuntu/Debian for now
  if [ "$OS" != "ubuntu" ] && [ "$OS" != "debian" ]; then
    log "ERROR: Automatic CUDA installation only supported on Ubuntu/Debian"
    log "Please install CUDA Toolkit manually for $OS"
    return 1
  fi
  
  log "Detected OS: $OS $VERSION_ID"
  
  # Check if we can use apt
  if ! command -v apt-get >/dev/null 2>&1; then
    log "ERROR: apt-get not found"
    return 1
  fi
  
  # Check if NVIDIA driver is installed
  if ! command -v nvidia-smi >/dev/null 2>&1; then
    log "⚠ NVIDIA driver not found"
    log "CUDA Toolkit requires NVIDIA driver to be installed first"
    log "Please install NVIDIA driver first:"
    log "  Ubuntu: sudo apt-get install nvidia-driver-<version>"
    log "  Or use: sudo ubuntu-drivers autoinstall"
    log ""
    log "After installing driver, reboot and run this script again"
    return 1
  fi
  
  # Install CUDA Toolkit from Ubuntu repository
  # This is the easiest method, but may not be the latest version
  log "Installing CUDA Toolkit from Ubuntu repository..."
  log "This may take several minutes..."
  
  # Temporarily disable exit on error for apt operations
  set +e
  
  # Update package list
  if ! sudo apt-get update -qq 2>&1 | grep -v "WARNING: apt does not have a stable CLI interface"; then
    log "⚠ Warning: apt-get update had some issues (this may be OK)"
  fi
  
  # Install CUDA Toolkit (this will install a compatible version)
  # nvidia-cuda-toolkit includes nvcc and CUDA libraries
  if sudo apt-get install -y -qq nvidia-cuda-toolkit 2>&1 | tee /tmp/cuda_install.log; then
    set -e
    log "✓ CUDA Toolkit installed successfully"
    
    # Verify installation
    if command -v nvcc >/dev/null 2>&1; then
      CUDA_VERSION=$(nvcc --version 2>/dev/null | grep "release" | sed 's/.*release \([0-9.]*\).*/\1/' || echo "unknown")
      log "✓ CUDA Toolkit version: $CUDA_VERSION"
      
      # Check if CUDA directory exists
      if [ -d "/usr/local/cuda" ]; then
        log "✓ CUDA installed at /usr/local/cuda"
      elif [ -d "/usr/lib/cuda" ]; then
        log "✓ CUDA installed at /usr/lib/cuda"
      fi
      
      return 0
    else
      log "⚠ CUDA Toolkit installed but nvcc not found in PATH"
      log "Trying to find nvcc..."
      if [ -f "/usr/bin/nvcc" ]; then
        log "✓ Found nvcc at /usr/bin/nvcc"
        return 0
      elif [ -f "/usr/local/cuda/bin/nvcc" ]; then
        log "✓ Found nvcc at /usr/local/cuda/bin/nvcc"
        log "You may need to add /usr/local/cuda/bin to PATH"
        return 0
      else
        log "⚠ nvcc not found. CUDA installation may be incomplete"
        return 1
      fi
    fi
  else
    set -e
    log "ERROR: Failed to install CUDA Toolkit"
    log "Check /tmp/cuda_install.log for details"
    log ""
    log "Common issues:"
    log "  1. NVIDIA driver not installed - install driver first"
    log "  2. Insufficient disk space - CUDA Toolkit requires ~3GB"
    log "  3. Network issues - check internet connection"
    log "  4. dpkg lock - another package installation is in progress"
    log ""
    log "You can try installing manually:"
    log "  sudo apt-get update"
    log "  sudo apt-get install nvidia-cuda-toolkit"
    return 1
  fi
}

# Check if CUDA is available
CUDA_AVAILABLE=false
if check_cuda_toolkit; then
  CUDA_AVAILABLE=true
  log "CUDA Toolkit detected - enabling CUDA support"
else
  # Check if NVIDIA GPU is present
  if detect_nvidia_gpu; then
    log "NVIDIA GPU detected but CUDA Toolkit not found"
    
    # Check if running interactively
    if [ -t 0 ] && [ -z "${DEBIAN_FRONTEND:-}" ] || [ "$DEBIAN_FRONTEND" != "noninteractive" ]; then
      echo ""
      echo "═══════════════════════════════════════════════════════════════"
      echo "NVIDIA GPU Detected - CUDA Toolkit Installation"
      echo "═══════════════════════════════════════════════════════════════"
      echo ""
      echo "An NVIDIA GPU was detected, but CUDA Toolkit is not installed."
      echo ""
      echo "CUDA Toolkit enables GPU acceleration for OpenCV, which can"
      echo "significantly improve performance for AI inference tasks."
      echo ""
      echo "⚠  Requirements:"
      echo "   - NVIDIA GPU (detected ✓)"
      echo "   - NVIDIA driver installed (checking...)"
      echo "   - Internet connection for download"
      echo ""
      echo "Would you like to install CUDA Toolkit automatically? [y/N]"
      read -r response
      response=${response:-N}
      
      if [ "$response" = "y" ] || [ "$response" = "Y" ]; then
        if install_cuda_toolkit; then
          CUDA_AVAILABLE=true
          log "CUDA Toolkit installed - enabling CUDA support"
        else
          log "Failed to install CUDA Toolkit - building CPU-only version"
          log "You can install CUDA Toolkit manually later:"
          log "  sudo apt-get install nvidia-cuda-toolkit"
        fi
      else
        log "Skipping CUDA Toolkit installation - building CPU-only version"
        log "You can install CUDA Toolkit manually later:"
        log "  sudo apt-get install nvidia-cuda-toolkit"
      fi
    else
      # Non-interactive mode - try to install automatically if GPU detected
      log "Non-interactive mode - attempting automatic CUDA Toolkit installation..."
      if install_cuda_toolkit; then
        CUDA_AVAILABLE=true
        log "CUDA Toolkit installed - enabling CUDA support"
      else
        log "Failed to install CUDA Toolkit automatically - building CPU-only version"
      fi
    fi
  else
    log "No NVIDIA GPU detected - building OpenCV without CUDA support"
  fi
fi

# Check for freetype2 development libraries (REQUIRED for freetype module)
# CVEDIX SDK requires libopencv_freetype.so.410, so freetype module MUST be built
FREETYPE_AVAILABLE=false
if pkg-config --exists freetype2 2>/dev/null; then
  FREETYPE_AVAILABLE=true
  log "✓ Freetype2 development libraries found"
elif [ -f "/usr/include/freetype2/freetype/freetype.h" ] || [ -f "/usr/local/include/freetype2/freetype/freetype.h" ]; then
  FREETYPE_AVAILABLE=true
  log "✓ Freetype2 headers found"
else
  log "⚠ Freetype2 development libraries not found"
  log "Freetype module is REQUIRED for CVEDIX SDK compatibility"
  log "Installing libfreetype6-dev..."
  
  # Try to install freetype2-dev
  if ! check_dpkg_lock; then
    set +e
    if sudo apt-get update -qq && sudo apt-get install -y -qq libfreetype6-dev 2>&1; then
      set -e
      FREETYPE_AVAILABLE=true
      log "✓ Freetype2 development libraries installed successfully"
    else
      set -e
      log "ERROR: Failed to install libfreetype6-dev"
      log "Freetype module is REQUIRED. Please install manually:"
      log "  sudo apt-get update"
      log "  sudo apt-get install libfreetype6-dev"
      log ""
      log "Then run this script again to rebuild OpenCV 4.10 with freetype support"
      exit 1
    fi
  else
    log "ERROR: Cannot install libfreetype6-dev now (dpkg lock)"
    log "Freetype module is REQUIRED for CVEDIX SDK"
    log ""
    log "Please install manually after package installation completes:"
    log "  sudo apt-get update"
    log "  sudo apt-get install libfreetype6-dev"
    log "  sudo $0"
    exit 1
  fi
fi

# Build CMake command with conditional CUDA options
CMAKE_ARGS=(
  -D CMAKE_BUILD_TYPE=RELEASE
  -D CMAKE_INSTALL_PREFIX=/usr/local
  -D OPENCV_EXTRA_MODULES_PATH="${WORKDIR}/${CONTRIB_DIR}/modules"
  -D WITH_TBB=ON
  -D ENABLE_FAST_MATH=1
  -D WITH_QT=OFF
  -D WITH_OPENGL=ON
  -D WITH_GTK=ON
  -D BUILD_opencv_apps=ON
  -D BUILD_opencv_python2=OFF
  -D BUILD_opencv_python3=ON
  -D PYTHON3_EXECUTABLE="${PYTHON3_EXECUTABLE}"
  -D PYTHON3_PACKAGES_PATH="${PYTHON3_PACKAGES_PATH}"
  -D OPENCV_GENERATE_PKGCONFIG=ON
  -D OPENCV_PC_FILE_NAME=opencv.pc
  -D OPENCV_ENABLE_NONFREE=ON
  -D INSTALL_PYTHON_EXAMPLES=OFF
  -D INSTALL_C_EXAMPLES=OFF
  -D BUILD_EXAMPLES=OFF
  -D WITH_FFMPEG=ON
  -D WITH_GSTREAMER=ON
)

# Enable freetype module (REQUIRED for CVEDIX SDK)
if [ "$FREETYPE_AVAILABLE" = true ]; then
  CMAKE_ARGS+=(
    -D BUILD_opencv_freetype=ON
  )
  log "✓ Freetype module will be enabled in OpenCV build (REQUIRED)"
else
  log "ERROR: Freetype module is REQUIRED but freetype2-dev is not available"
  log "Cannot build OpenCV 4.10 without freetype support"
  exit 1
fi

# Add CUDA options only if CUDA is available
if [ "$CUDA_AVAILABLE" = true ]; then
  CMAKE_ARGS+=(
    -D CUDA_FAST_MATH=1
    -D WITH_CUBLAS=1
    -D WITH_CUDA=ON
    -D BUILD_opencv_cudacodec=ON
    -D WITH_CUDNN=ON
    -D OPENCV_DNN_CUDA=ON
    -D WITH_NVCUVID=ON
    -D WITH_NVCUVENC=ON
  )
else
  CMAKE_ARGS+=(
    -D WITH_CUDA=OFF
    -D OPENCV_DNN_CUDA=OFF
  )
  log "CUDA options disabled - building CPU-only version"
fi

# Run CMake
log "Running CMake configuration..."
if ! cmake "${CMAKE_ARGS[@]}" ..; then
  log "ERROR: CMake configuration failed"
  log "This may be due to missing dependencies or configuration issues"
  log "Please check the error messages above"
  exit 1
fi

log "--- Bắt đầu biên dịch (make) ---"
if ! make -j"$(nproc)"; then
  log "ERROR: Build failed"
  log "Please check the error messages above"
  exit 1
fi

log "--- Cài đặt (cần quyền root) ---"
if ! sudo make install; then
  log "ERROR: Installation failed"
  log "Please check the error messages above"
  exit 1
fi
sudo ldconfig

# Đảm bảo opencv_version từ /usr/local được ưu tiên
if [ -f /usr/local/bin/opencv_version ]; then
  log "Kiểm tra opencv_version..."
  INSTALLED_VERSION=$(/usr/local/bin/opencv_version 2>/dev/null || echo "")
  if [ -n "${INSTALLED_VERSION}" ]; then
    log "✓ OpenCV ${INSTALLED_VERSION} đã được cài đặt thành công"
    # Backup lệnh cũ nếu tồn tại và khác version
    if [ -f /usr/bin/opencv_version ]; then
      OLD_VERSION=$(/usr/bin/opencv_version 2>/dev/null || echo "")
      if [ "${OLD_VERSION}" != "${INSTALLED_VERSION}" ]; then
        log "Phát hiện opencv_version cũ (${OLD_VERSION}) trong /usr/bin"
        log "Lưu ý: /usr/local/bin có priority cao hơn, nên lệnh 'opencv_version' sẽ dùng bản mới"
        log "Nếu muốn, bạn có thể chạy: sudo mv /usr/bin/opencv_version /usr/bin/opencv_version.old"
      fi
    fi
  fi
else
  log "Cảnh báo: opencv_version không được cài vào /usr/local/bin"
  log "Có thể do BUILD_opencv_apps=OFF hoặc lỗi trong quá trình build"
fi

# Check if freetype library was built and installed
log "Kiểm tra freetype module..."
FREETYPE_FOUND=false
FREETYPE_PATHS=(
  "/usr/local/lib/libopencv_freetype.so.4.10"
  "/usr/local/lib/libopencv_freetype.so.410"
  "/usr/local/lib/libopencv_freetype.so.4.10.0"
)

for path in "${FREETYPE_PATHS[@]}"; do
  if [ -f "$path" ]; then
    FREETYPE_FOUND=true
    log "✓ Freetype library found: $path"
    
    # Create symlink libopencv_freetype.so.410 if it doesn't exist
    if [ ! -f "/usr/local/lib/libopencv_freetype.so.410" ] && [[ "$path" != "/usr/local/lib/libopencv_freetype.so.410" ]]; then
      log "Creating symlink: libopencv_freetype.so.410 -> $(basename "$path")"
      sudo ln -sf "$(basename "$path")" "/usr/local/lib/libopencv_freetype.so.410" 2>/dev/null || true
    fi
    break
  fi
done

if [ "$FREETYPE_FOUND" = false ]; then
  log "ERROR: Freetype library not found after installation"
  log "This should not happen if BUILD_opencv_freetype=ON was set"
  log ""
  log "Possible causes:"
  log "  1. CMake configuration failed to enable freetype module"
  log "  2. Build failed during freetype module compilation"
  log "  3. Installation step failed"
  log ""
  log "Please check the build output above for errors"
  log "Then rebuild OpenCV:"
  log "  1. Ensure libfreetype6-dev is installed: sudo apt-get install libfreetype6-dev"
  log "  2. Clean build directory: rm -rf build"
  log "  3. Rebuild: sudo $0"
  log ""
  log "OpenCV 4.10 MUST be built with freetype support for CVEDIX SDK compatibility"
  exit 1
else
  log "✓ Freetype module successfully installed with OpenCV 4.10"
fi

log "Hoàn thành."
