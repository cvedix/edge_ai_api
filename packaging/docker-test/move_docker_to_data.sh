#!/bin/bash
# Script để di chuyển Docker data sang /home/cvedix/Data

set -e

DOCKER_NEW_ROOT="/home/cvedix/Data/docker"
DOCKER_OLD_ROOT="/var/lib/docker"
CONTAINERD_NEW_ROOT="/home/cvedix/Data/containerd"
CONTAINERD_OLD_ROOT="/var/lib/containerd"

echo "=== Di chuyển Docker sang /home/cvedix/Data ==="
echo ""

# Kiểm tra quyền root
if [ "$EUID" -ne 0 ]; then 
    echo "❌ Script này cần chạy với sudo"
    echo "   Chạy: sudo $0"
    exit 1
fi

# Kiểm tra Docker đang chạy
if systemctl is-active --quiet docker; then
    echo "⚠️  Docker đang chạy. Cần dừng Docker trước."
    read -p "Bạn có muốn dừng Docker? (y/n): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "Đang dừng Docker..."
        systemctl stop docker
        systemctl stop containerd 2>/dev/null || true
    else
        echo "❌ Hủy bỏ. Vui lòng dừng Docker thủ công trước."
        exit 1
    fi
fi

# Tạo thư mục mới
echo "📁 Tạo thư mục mới..."
mkdir -p "$DOCKER_NEW_ROOT"
mkdir -p "$CONTAINERD_NEW_ROOT"

# Di chuyển dữ liệu Docker (nếu có)
if [ -d "$DOCKER_OLD_ROOT" ] && [ "$(ls -A $DOCKER_OLD_ROOT 2>/dev/null)" ]; then
    echo "📦 Di chuyển Docker data từ $DOCKER_OLD_ROOT..."
    echo "   (Có thể mất vài phút tùy vào dung lượng...)"
    rsync -aAXv "$DOCKER_OLD_ROOT/" "$DOCKER_NEW_ROOT/" || {
        echo "⚠️  Lỗi khi di chuyển. Bạn có thể bỏ qua và tạo mới."
        read -p "Tiếp tục với thư mục mới? (y/n): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            exit 1
        fi
    }
    # Backup thư mục cũ
    mv "$DOCKER_OLD_ROOT" "${DOCKER_OLD_ROOT}.backup.$(date +%Y%m%d_%H%M%S)"
fi

# Di chuyển dữ liệu containerd (nếu có)
if [ -d "$CONTAINERD_OLD_ROOT" ] && [ "$(ls -A $CONTAINERD_OLD_ROOT 2>/dev/null)" ]; then
    echo "📦 Di chuyển containerd data từ $CONTAINERD_OLD_ROOT..."
    rsync -aAXv "$CONTAINERD_OLD_ROOT/" "$CONTAINERD_NEW_ROOT/" || {
        echo "⚠️  Lỗi khi di chuyển containerd data"
    }
    # Backup thư mục cũ
    mv "$CONTAINERD_OLD_ROOT" "${CONTAINERD_OLD_ROOT}.backup.$(date +%Y%m%d_%H%M%S)"
fi

# Tạo symlink (fallback nếu cấu hình không hoạt động)
echo "🔗 Tạo symlink backup..."
ln -sf "$DOCKER_NEW_ROOT" "$DOCKER_OLD_ROOT"
ln -sf "$CONTAINERD_NEW_ROOT" "$CONTAINERD_OLD_ROOT"

# Tạo cấu hình Docker daemon
echo "⚙️  Tạo cấu hình Docker daemon..."
mkdir -p /etc/docker

cat > /etc/docker/daemon.json <<EOF
{
  "data-root": "$DOCKER_NEW_ROOT"
}
EOF

# Tạo cấu hình containerd (nếu cần)
if [ -f /etc/containerd/config.toml ]; then
    echo "⚙️  Cập nhật cấu hình containerd..."
    # Backup config cũ
    cp /etc/containerd/config.toml /etc/containerd/config.toml.backup.$(date +%Y%m%d_%H%M%S)
    # Thêm root path vào config (nếu chưa có)
    if ! grep -q "root = \"$CONTAINERD_NEW_ROOT\"" /etc/containerd/config.toml; then
        sed -i "s|root = \"/var/lib/containerd\"|root = \"$CONTAINERD_NEW_ROOT\"|g" /etc/containerd/config.toml || \
        echo "root = \"$CONTAINERD_NEW_ROOT\"" >> /etc/containerd/config.toml
    fi
fi

echo ""
echo "✅ Cấu hình hoàn tất!"
echo ""
echo "📋 Các bước tiếp theo:"
echo "   1. Khởi động lại Docker:"
echo "      sudo systemctl start docker"
echo ""
echo "   2. Kiểm tra cấu hình:"
echo "      docker info | grep 'Docker Root Dir'"
echo ""
echo "   3. Nếu mọi thứ hoạt động tốt, bạn có thể xóa backup:"
echo "      sudo rm -rf ${DOCKER_OLD_ROOT}.backup.*"
echo "      sudo rm -rf ${CONTAINERD_OLD_ROOT}.backup.*"
echo ""
echo "⚠️  Lưu ý: Nếu có vấn đề, bạn có thể khôi phục bằng cách:"
echo "   - Xóa symlink: sudo rm $DOCKER_OLD_ROOT $CONTAINERD_OLD_ROOT"
echo "   - Khôi phục từ backup: sudo mv ${DOCKER_OLD_ROOT}.backup.* $DOCKER_OLD_ROOT"
echo "   - Xóa /etc/docker/daemon.json và restart Docker"

