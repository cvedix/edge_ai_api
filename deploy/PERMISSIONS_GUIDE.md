# Hướng Dẫn Cấp Quyền cho Thư Mục /opt/edge_ai_api

## 📋 Tổng Quan

Khi cài đặt Edge AI API với `sudo`, bạn có thể chọn 2 mức quyền cho thư mục `/opt/edge_ai_api`:

1. **Quyền Chuẩn (755)** - `drwxr-xr-x` - An toàn, khuyến nghị cho production
2. **Quyền Đầy Đủ (777)** - `drwxrwxrwx` - Tiện lợi nhưng kém an toàn

## 🔍 So Sánh với Các Ứng Dụng Khác

### Quyền Chuẩn (755) - `drwxr-xr-x`
```bash
drwxr-xr-x  4 root root 4096 Oct 30 18:15 Tabby
drwxr-xr-x  3 root root 4096 Oct 30 17:37 google
drwxr-xr-x  4 root root 4096 Aug 25 23:30 nvidia
```

**Đặc điểm:**
- Chỉ owner (root) và group có quyền ghi
- Others chỉ có quyền đọc và thực thi
- **An toàn cho production**

### Quyền Đầy Đủ (777) - `drwxrwxrwx`
```bash
drwxrwxrwx 15 root root 4096 Dec  8 11:02 cvedix-rt
```

**Đặc điểm:**
- Mọi người đều có quyền đọc, ghi, thực thi
- **KHÔNG an toàn cho production**
- Chỉ nên dùng cho development hoặc môi trường nội bộ

## 🚀 Cách Sử Dụng

### 1. Cài Đặt với Quyền Chuẩn (755) - Mặc định

```bash
# Cách 1: Sử dụng script cài đặt thư mục
sudo ./deploy/install_directories.sh

# Cách 2: Sử dụng script build hoàn chỉnh
sudo ./deploy/build.sh

# Cách 3: Chỉ định rõ ràng
sudo ./deploy/install_directories.sh --standard-permissions
sudo ./deploy/build.sh --standard-permissions
```

### 2. Cài Đặt với Quyền Đầy Đủ (777)

```bash
# Cách 1: Sử dụng script cài đặt thư mục
sudo ./deploy/install_directories.sh --full-permissions

# Cách 2: Sử dụng script build hoàn chỉnh
sudo ./deploy/build.sh --full-permissions
```

### 3. Cấp Quyền 777 cho Thư Mục Đã Tồn Tại

Nếu bạn đã cài đặt với quyền 755 và muốn chuyển sang 777:

```bash
sudo ./deploy/set_full_permissions.sh
```

Script này sẽ:
- Cảnh báo về rủi ro bảo mật
- Yêu cầu xác nhận
- Cấp quyền 777 cho toàn bộ thư mục

### 4. Chuyển Từ 777 Về 755

```bash
sudo ./deploy/install_directories.sh --standard-permissions
```

## 🔍 Kiểm Tra Quyền Hiện Tại

```bash
# Xem quyền thư mục chính
ls -ld /opt/edge_ai_api

# Xem quyền tất cả thư mục con
ls -la /opt/edge_ai_api
```

**Kết quả mong đợi:**
- Quyền 755: `drwxr-xr-x`
- Quyền 777: `drwxrwxrwx`

## ⚠️ Cảnh Báo Bảo Mật

### Quyền 777 (Full Permissions)

**Rủi ro:**
- Mọi user trên hệ thống đều có thể đọc/ghi vào thư mục
- Nguy cơ bị xóa hoặc sửa đổi dữ liệu bởi user khác
- Không phù hợp cho production environment

**Khi nào nên dùng:**
- Development environment
- Môi trường nội bộ, đáng tin cậy
- Testing với nhiều user khác nhau
- Cần quyền truy cập linh hoạt

### Quyền 755 (Standard Permissions)

**Ưu điểm:**
- Chỉ owner và group có quyền ghi
- Bảo mật tốt hơn
- Phù hợp cho production

**Khi nào nên dùng:**
- Production environment
- Multi-user system
- Khi bảo mật là ưu tiên

## 📊 Bảng So Sánh

| Tiêu chí | Quyền 755 (Standard) | Quyền 777 (Full) |
|----------|---------------------|------------------|
| **Bảo mật** | ✅ Cao | ❌ Thấp |
| **Quyền ghi** | Owner/Group | Mọi người |
| **Production** | ✅ Khuyến nghị | ❌ Không nên |
| **Development** | ✅ OK | ✅ Tiện lợi |
| **Ví dụ** | Tabby, google, nvidia | cvedix-rt |

## 🎯 Khuyến Nghị

1. **Production**: Luôn dùng quyền 755 (standard)
2. **Development**: Có thể dùng 777 nếu cần, nhưng 755 vẫn an toàn hơn
3. **Kiểm tra**: Luôn kiểm tra quyền sau khi cài đặt bằng `ls -ld /opt/edge_ai_api`

## 📚 Tài Liệu Liên Quan

- [DEVELOPMENT_SETUP.md](../docs/DEVELOPMENT_SETUP.md) - Hướng dẫn chi tiết về tạo thư mục (xem phần "Tạo Thư Mục Tự Động với Fallback")
- [install_directories.sh](./install_directories.sh) - Script cài đặt thư mục
- [set_full_permissions.sh](./set_full_permissions.sh) - Script cấp quyền 777
- [build.sh](./build.sh) - Script build và deploy hoàn chỉnh

