#!/bin/bash

# Script tạo VM Ubuntu trong VirtualBox để test Debian package
# Usage: ./create_vm.sh [VM_NAME] [RAM_MB] [HDD_SIZE_MB] [CPUS] [ISO_PATH] [SHARED_FOLDER]

# Cấu hình mặc định
VM_NAME="${1:-Ubuntu-Edge-AI-Test}"
VM_RAM="${2:-4096}"
VM_HDD_SIZE="${3:-50000}"
VM_CPUS="${4:-2}"
ISO_PATH="${5:-}"
SHARED_FOLDER="${6:-$HOME/Data/project/edge_ai_api}"

# Màu sắc cho output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Kiểm tra VBoxManage
if ! command -v VBoxManage &> /dev/null; then
    echo -e "${RED}❌ Lỗi: VBoxManage không tìm thấy.${NC}"
    echo "   Đảm bảo VirtualBox đã được cài đặt."
    echo "   Cài đặt: sudo apt install virtualbox"
    exit 1
fi

# Kiểm tra ISO
if [ -z "$ISO_PATH" ] || [ ! -f "$ISO_PATH" ]; then
    echo -e "${RED}❌ Lỗi: Cần chỉ định đường dẫn đến file ISO Ubuntu${NC}"
    echo ""
    echo "Usage: $0 [VM_NAME] [RAM_MB] [HDD_SIZE_MB] [CPUS] [ISO_PATH] [SHARED_FOLDER]"
    echo ""
    echo "Parameters:"
    echo "  VM_NAME        Tên VM (mặc định: Ubuntu-Edge-AI-Test)"
    echo "  RAM_MB         RAM tính bằng MB (mặc định: 4096 = 4GB)"
    echo "  HDD_SIZE_MB    Dung lượng ổ cứng MB (mặc định: 50000 = 50GB)"
    echo "  CPUS           Số CPU cores (mặc định: 2)"
    echo "  ISO_PATH       Đường dẫn đến file ISO Ubuntu (bắt buộc)"
    echo "  SHARED_FOLDER  Thư mục shared folder (mặc định: \$HOME/Data/project/edge_ai_api)"
    echo ""
    echo "Example:"
    echo "  $0 Ubuntu-Test 4096 50000 2 ~/Downloads/ubuntu-22.04.3-desktop-amd64.iso"
    echo "  $0 Ubuntu-Test 4096 50000 2 ~/Downloads/ubuntu.iso ~/project/edge_ai_api"
    exit 1
fi

# Kiểm tra Shared Folder
if [ ! -d "$SHARED_FOLDER" ]; then
    echo -e "${YELLOW}⚠️  Cảnh báo: Shared folder không tồn tại: $SHARED_FOLDER${NC}"
    echo "   Bạn có muốn tiếp tục? (y/n)"
    read -r response
    if [[ ! "$response" =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

echo -e "${BLUE}🔧 Đang tạo VM: ${VM_NAME}${NC}"
echo "   RAM: ${VM_RAM}MB"
echo "   HDD: ${VM_HDD_SIZE}MB"
echo "   CPUs: $VM_CPUS"
echo "   ISO: $ISO_PATH"
echo "   Shared Folder: $SHARED_FOLDER"
echo ""

# Kiểm tra VM đã tồn tại
if VBoxManage list vms | grep -q "\"$VM_NAME\""; then
    echo -e "${YELLOW}⚠️  VM '$VM_NAME' đã tồn tại.${NC}"
    echo "   Bạn muốn:"
    echo "   1) Xóa VM cũ và tạo mới"
    echo "   2) Thoát"
    read -r -p "   Chọn (1/2): " choice
    case $choice in
        1)
            echo -e "${BLUE}🗑️  Đang xóa VM cũ...${NC}"
            # Dừng VM nếu đang chạy
            if VBoxManage list runningvms | grep -q "\"$VM_NAME\""; then
                VBoxManage controlvm "$VM_NAME" poweroff 2>/dev/null
                sleep 2
            fi
            VBoxManage unregistervm "$VM_NAME" --delete 2>/dev/null
            echo -e "${GREEN}✅ VM cũ đã được xóa${NC}"
            ;;
        2)
            exit 0
            ;;
        *)
            echo -e "${RED}❌ Lựa chọn không hợp lệ${NC}"
            exit 1
            ;;
    esac
fi

# Tạo VM
echo -e "${BLUE}📦 Đang tạo VM...${NC}"
VBoxManage createvm --name "$VM_NAME" --ostype "Ubuntu_64" --register

# Cấu hình cơ bản
echo -e "${BLUE}⚙️  Đang cấu hình VM...${NC}"
VBoxManage modifyvm "$VM_NAME" --memory "$VM_RAM" --cpus "$VM_CPUS"
VBoxManage modifyvm "$VM_NAME" --nic1 nat
VBoxManage modifyvm "$VM_NAME" --ioapic on --pae on
VBoxManage modifyvm "$VM_NAME" --vram 128 --graphicscontroller vboxsvga

# Tạo và gắn HDD
echo -e "${BLUE}💾 Đang tạo virtual hard disk...${NC}"
VBoxManage createhd --filename "$HOME/VirtualBox VMs/$VM_NAME/$VM_NAME.vdi" \
    --size "$VM_HDD_SIZE" --format VDI --variant Standard

VBoxManage storagectl "$VM_NAME" --name "SATA Controller" --add sata --controller IntelAHCI
VBoxManage storageattach "$VM_NAME" --storagectl "SATA Controller" \
    --port 0 --device 0 --type hdd \
    --medium "$HOME/VirtualBox VMs/$VM_NAME/$VM_NAME.vdi"

# Gắn ISO
echo -e "${BLUE}💿 Đang gắn ISO...${NC}"
VBoxManage storagectl "$VM_NAME" --name "IDE Controller" --add ide --controller PIIX4
VBoxManage storageattach "$VM_NAME" --storagectl "IDE Controller" \
    --port 0 --device 0 --type dvddrive \
    --medium "$ISO_PATH"

# Shared Folder
if [ -d "$SHARED_FOLDER" ]; then
    echo -e "${BLUE}📁 Đang cấu hình shared folder...${NC}"
    VBoxManage sharedfolder add "$VM_NAME" \
        --name "edge_ai_api" \
        --hostpath "$SHARED_FOLDER" \
        --automount 2>/dev/null || true
    echo -e "${GREEN}✅ Shared folder đã được cấu hình: $SHARED_FOLDER${NC}"
fi

# Port Forwarding cho API
echo -e "${BLUE}🌐 Đang cấu hình port forwarding...${NC}"
VBoxManage modifyvm "$VM_NAME" --natpf1 "edge-ai-api,tcp,,8080,,8080" 2>/dev/null || true
echo -e "${GREEN}✅ Port forwarding: Host 8080 -> Guest 8080${NC}"

echo ""
echo -e "${GREEN}✅ VM '$VM_NAME' đã được tạo thành công!${NC}"
echo ""
echo -e "${BLUE}📝 Các lệnh hữu ích:${NC}"
echo "   Khởi động VM (GUI):     ${GREEN}VBoxManage startvm '$VM_NAME' --type gui${NC}"
echo "   Khởi động VM (headless): ${GREEN}VBoxManage startvm '$VM_NAME' --type headless${NC}"
echo "   Dừng VM:                ${GREEN}VBoxManage controlvm '$VM_NAME' acpipowerbutton${NC}"
echo "   Tắt VM ngay:            ${GREEN}VBoxManage controlvm '$VM_NAME' poweroff${NC}"
echo "   Xem thông tin:          ${GREEN}VBoxManage showvminfo '$VM_NAME'${NC}"
echo "   Xem danh sách VM:       ${GREEN}VBoxManage list vms${NC}"
echo ""
echo -e "${YELLOW}📌 Bước tiếp theo:${NC}"
echo "   1. Khởi động VM: VBoxManage startvm '$VM_NAME' --type gui"
echo "   2. Cài đặt Ubuntu từ ISO"
echo "   3. Cài Guest Additions (Devices > Insert Guest Additions CD)"
echo "   4. Copy file .deb từ shared folder: /media/sf_edge_ai_api/"
echo "   5. Cài đặt package: sudo dpkg -i edge-ai-api-with-sdk-*.deb"
echo ""

