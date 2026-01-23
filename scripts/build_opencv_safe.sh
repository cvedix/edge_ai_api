#!/usr/bin/env bash
set -euo pipefail
IFS=$'\n\t'

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
      # Validate zip
      if unzip -t "${out}" >/dev/null 2>&1; then
        log "Downloaded and validated ${out}"
        return 0
      else
        log "Downloaded ${out} but validation failed. Removing and retrying."
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

# Run CMake
cmake -D CMAKE_BUILD_TYPE=RELEASE \
  -D CMAKE_INSTALL_PREFIX=/usr/local \
  -D OPENCV_EXTRA_MODULES_PATH="${WORKDIR}/${CONTRIB_DIR}/modules" \
  -D WITH_TBB=ON \
  -D ENABLE_FAST_MATH=1 \
  -D CUDA_FAST_MATH=1 \
  -D WITH_CUBLAS=1 \
  -D WITH_CUDA=ON \
  -D BUILD_opencv_cudacodec=ON \
  -D WITH_CUDNN=ON \
  -D OPENCV_DNN_CUDA=ON \
  -D WITH_QT=OFF \
  -D WITH_OPENGL=ON \
  -D WITH_GTK=ON \
  -D BUILD_opencv_apps=ON \
  -D BUILD_opencv_python2=OFF \
  -D BUILD_opencv_python3=ON \
  -D PYTHON3_EXECUTABLE="${PYTHON3_EXECUTABLE}" \
  -D PYTHON3_PACKAGES_PATH="${PYTHON3_PACKAGES_PATH}" \
  -D OPENCV_GENERATE_PKGCONFIG=ON \
  -D OPENCV_PC_FILE_NAME=opencv.pc \
  -D OPENCV_ENABLE_NONFREE=ON \
  -D INSTALL_PYTHON_EXAMPLES=OFF \
  -D INSTALL_C_EXAMPLES=OFF \
  -D BUILD_EXAMPLES=OFF \
  -D WITH_FFMPEG=ON \
  -D WITH_GSTREAMER=ON \
  -D WITH_NVCUVID=ON \
  -D WITH_NVCUVENC=ON \
  ..

log "--- Bắt đầu biên dịch (make) ---"
make -j"$(nproc)"

log "--- Cài đặt (cần quyền root) ---"
sudo make install
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

log "Hoàn thành."
