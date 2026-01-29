#!/bin/bash

# Script quản lý VM VirtualBox
# Usage: ./vm_manage.sh [command] [VM_NAME]

VM_NAME="${2:-Ubuntu-Edge-AI-Test}"

# Màu sắc
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Kiểm tra VBoxManage
if ! command -v VBoxManage &> /dev/null; then
    echo -e "${RED}❌ Lỗi: VBoxManage không tìm thấy${NC}"
    exit 1
fi

show_help() {
    echo "Usage: $0 [command] [VM_NAME]"
    echo ""
    echo "Commands:"
    echo "  start       Khởi động VM (GUI)"
    echo "  start-headless  Khởi động VM (headless)"
    echo "  stop        Dừng VM (graceful shutdown)"
    echo "  poweroff    Tắt VM ngay lập tức"
    echo "  pause       Tạm dừng VM"
    echo "  resume      Tiếp tục VM"
    echo "  reset       Reset VM"
    echo "  info        Hiển thị thông tin VM"
    echo "  list        Liệt kê tất cả VM"
    echo "  running     Liệt kê VM đang chạy"
    echo "  delete      Xóa VM"
    echo "  ssh         Hiển thị thông tin SSH (nếu có)"
    echo ""
    echo "Default VM_NAME: Ubuntu-Edge-AI-Test"
    echo ""
    echo "Examples:"
    echo "  $0 start Ubuntu-Test"
    echo "  $0 stop"
    echo "  $0 info"
}

case "$1" in
    start)
        echo -e "${BLUE}🚀 Đang khởi động VM: $VM_NAME${NC}"
        VBoxManage startvm "$VM_NAME" --type gui
        ;;
    start-headless)
        echo -e "${BLUE}🚀 Đang khởi động VM (headless): $VM_NAME${NC}"
        VBoxManage startvm "$VM_NAME" --type headless
        ;;
    stop)
        echo -e "${YELLOW}⏹️  Đang dừng VM: $VM_NAME${NC}"
        VBoxManage controlvm "$VM_NAME" acpipowerbutton
        ;;
    poweroff)
        echo -e "${RED}🔌 Đang tắt VM: $VM_NAME${NC}"
        VBoxManage controlvm "$VM_NAME" poweroff
        ;;
    pause)
        echo -e "${YELLOW}⏸️  Đang tạm dừng VM: $VM_NAME${NC}"
        VBoxManage controlvm "$VM_NAME" pause
        ;;
    resume)
        echo -e "${GREEN}▶️  Đang tiếp tục VM: $VM_NAME${NC}"
        VBoxManage controlvm "$VM_NAME" resume
        ;;
    reset)
        echo -e "${RED}🔄 Đang reset VM: $VM_NAME${NC}"
        VBoxManage controlvm "$VM_NAME" reset
        ;;
    info)
        echo -e "${BLUE}📋 Thông tin VM: $VM_NAME${NC}"
        VBoxManage showvminfo "$VM_NAME"
        ;;
    list)
        echo -e "${BLUE}📋 Danh sách VM:${NC}"
        VBoxManage list vms
        ;;
    running)
        echo -e "${BLUE}▶️  VM đang chạy:${NC}"
        VBoxManage list runningvms
        ;;
    delete)
        echo -e "${RED}🗑️  Cảnh báo: Bạn sắp xóa VM: $VM_NAME${NC}"
        read -p "Bạn có chắc chắn? (yes/no): " confirm
        if [ "$confirm" = "yes" ]; then
            if VBoxManage list runningvms | grep -q "\"$VM_NAME\""; then
                echo "Đang dừng VM..."
                VBoxManage controlvm "$VM_NAME" poweroff
                sleep 2
            fi
            VBoxManage unregistervm "$VM_NAME" --delete
            echo -e "${GREEN}✅ VM đã được xóa${NC}"
        else
            echo "Đã hủy"
        fi
        ;;
    ssh)
        echo -e "${BLUE}🔍 Thông tin SSH cho VM: $VM_NAME${NC}"
        echo ""
        echo "Để kết nối SSH, bạn cần:"
        echo "1. Cài SSH server trong VM: sudo apt install openssh-server"
        echo "2. Tìm IP của VM:"
        echo ""
        echo "   Trong VM, chạy: ip addr show | grep 'inet '"
        echo ""
        echo "3. Hoặc dùng port forwarding:"
        echo "   VBoxManage modifyvm '$VM_NAME' --natpf1 'ssh,tcp,,2222,,22'"
        echo "   ssh -p 2222 user@localhost"
        ;;
    *)
        show_help
        exit 1
        ;;
esac

