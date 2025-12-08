# Hướng Dẫn Tạo Thư Mục Tự Động trong Production

## 📋 Tổng Quan

Tài liệu này mô tả cách xử lý việc tạo thư mục tự động trong production environment, đặc biệt là các thư mục trong `/opt/` hoặc các thư mục hệ thống khác yêu cầu quyền root.

## 🎯 Vấn Đề

### Vấn đề cốt lõi:
- **Code C++ không thể tự động tạo thư mục trong `/opt/`** vì cần quyền root
- **Production cần thư mục cố định** (`/opt/edge_ai_api/instances`) để dễ quản lý
- **Development cần linh hoạt** để không cần sudo khi chạy
- **User không muốn phải chạy sudo** mỗi lần

### Yêu cầu:
- ✅ Code tự động tạo thư mục khi có thể
- ✅ Fallback tự động nếu không có quyền
- ✅ Hệ thống luôn chạy được (không crash)
- ✅ Production dùng `/opt/`, development tự động fallback

## 💡 Giải Pháp

### Chiến lược 3 tầng:

1. **Tầng 1: Thử tạo thư mục production** (`/opt/edge_ai_api/instances`)
   - Nếu thành công → dùng thư mục này
   - Nếu không có quyền → chuyển sang tầng 2

2. **Tầng 2: Fallback sang user directory** (`~/.local/share/edge_ai_api/instances`)
   - Tuân thủ XDG Base Directory Specification
   - Không cần quyền root
   - Tự động tạo được

3. **Tầng 3: Fallback cuối cùng** (`./instances`)
   - Current working directory
   - Luôn có quyền ghi
   - Đảm bảo hệ thống luôn chạy được

## 🔧 Implementation

### Pattern Code (C++)

```cpp
#include <filesystem>
#include <iostream>
#include <fstream>

std::string resolveDirectory(const std::string& preferred_path) {
    std::string final_path = preferred_path;
    
    // Try to create preferred directory
    if (!std::filesystem::exists(final_path)) {
        try {
            std::filesystem::create_directories(final_path);
            std::cerr << "✓ Created directory: " << final_path << std::endl;
            return final_path;
        } catch (const std::filesystem::filesystem_error& e) {
            if (e.code() == std::errc::permission_denied) {
                std::cerr << "⚠ Cannot create " << final_path << " (permission denied)" << std::endl;
                
                // Fallback 1: User directory
                const char* home = std::getenv("HOME");
                if (home) {
                    std::string fallback = std::string(home) + "/.local/share/edge_ai_api/instances";
                    try {
                        std::filesystem::create_directories(fallback);
                        std::cerr << "✓ Using fallback: " << fallback << std::endl;
                        return fallback;
                    } catch (...) {
                        // Fallback 2: Current directory
                        std::string last_resort = "./instances";
                        std::filesystem::create_directories(last_resort);
                        std::cerr << "✓ Using last resort: " << last_resort << std::endl;
                        return last_resort;
                    }
                }
            }
        }
    }
    
    return final_path;
}
```

### Pattern với Environment Variable Override

```cpp
std::string resolveDirectory(const std::string& env_var, const std::string& default_path) {
    // Priority 1: Environment variable
    const char* env_value = std::getenv(env_var.c_str());
    if (env_value && strlen(env_value) > 0) {
        std::string path = std::string(env_value);
        try {
            std::filesystem::create_directories(path);
            return path;
        } catch (...) {
            std::cerr << "⚠ Cannot create user-specified directory: " << path << std::endl;
        }
    }
    
    // Priority 2: Default path with fallback
    return resolveDirectory(default_path);
}
```

## 📝 Best Practices

### 1. Luôn có Fallback

```cpp
// ❌ BAD: Không có fallback, sẽ crash nếu không có quyền
std::filesystem::create_directories("/opt/myapp/data");

// ✅ GOOD: Có fallback, hệ thống luôn chạy được
std::string data_dir = resolveDirectory("/opt/myapp/data");
```

### 2. Kiểm tra Quyền Trước Khi Tạo

```cpp
// Kiểm tra parent directory có tồn tại và có quyền ghi không
std::filesystem::path parent = path.parent_path();
if (std::filesystem::exists(parent)) {
    // Test write permission
    std::filesystem::path test_file = parent / ".write_test";
    std::ofstream test(test_file);
    if (test.is_open()) {
        test.close();
        std::filesystem::remove(test_file);
        // Có quyền, có thể tạo subdirectory
    }
}
```

### 3. Log Rõ Ràng

```cpp
std::cerr << "[Main] ✓ Successfully created directory: " << path << std::endl;
std::cerr << "[Main] ⚠ Cannot create " << preferred << ", using fallback: " << fallback << std::endl;
std::cerr << "[Main] ℹ Note: To use production path, run: sudo mkdir -p " << preferred << std::endl;
```

### 4. Không Throw Exception

```cpp
// ❌ BAD: Throw exception, ứng dụng sẽ crash
if (!can_create) {
    throw std::runtime_error("Cannot create directory");
}

// ✅ GOOD: Log warning, fallback, ứng dụng vẫn chạy
if (!can_create) {
    std::cerr << "⚠ Warning: Using fallback directory" << std::endl;
    path = fallback_path;
}
```

## 🎨 Áp Dụng cho Các Trường Hợp Khác

### Trường hợp 1: Log Directory

```cpp
std::string resolveLogDirectory() {
    // Production: /var/log/myapp
    // Fallback: ~/.local/share/myapp/logs
    // Last resort: ./logs
    
    const char* env_log = std::getenv("LOG_DIR");
    if (env_log) return resolveDirectory(env_log);
    
    return resolveDirectory("/var/log/myapp");
}
```

### Trường hợp 2: Config Directory

```cpp
std::string resolveConfigDirectory() {
    // Production: /etc/myapp
    // Fallback: ~/.config/myapp
    // Last resort: ./config
    
    const char* env_config = std::getenv("CONFIG_DIR");
    if (env_config) return resolveDirectory(env_config);
    
    // Try /etc first
    std::string etc_path = "/etc/myapp";
    if (std::filesystem::exists(etc_path)) {
        return etc_path; // Read-only, but exists
    }
    
    // Fallback to user config
    const char* home = std::getenv("HOME");
    if (home) {
        return resolveDirectory(std::string(home) + "/.config/myapp");
    }
    
    return "./config";
}
```

### Trường hợp 3: Data Directory

```cpp
std::string resolveDataDirectory() {
    // Production: /var/lib/myapp
    // Fallback: ~/.local/share/myapp
    // Last resort: ./data
    
    const char* env_data = std::getenv("DATA_DIR");
    if (env_data) return resolveDirectory(env_data);
    
    return resolveDirectory("/var/lib/myapp");
}
```

### Trường hợp 4: Cache Directory

```cpp
std::string resolveCacheDirectory() {
    // Production: /var/cache/myapp
    // Fallback: ~/.cache/myapp
    // Last resort: ./cache
    
    const char* env_cache = std::getenv("CACHE_DIR");
    if (env_cache) return resolveDirectory(env_cache);
    
    const char* home = std::getenv("HOME");
    if (home) {
        return resolveDirectory(std::string(home) + "/.cache/myapp");
    }
    
    return "./cache";
}
```

## 🚀 Deployment Strategies

### Strategy 1: Script Deploy (Khuyến nghị)

```bash
#!/bin/bash
# deploy/setup_directories.sh

INSTALL_DIR="/opt/myapp"
SERVICE_USER="myapp"

# Create parent directory with sudo (one time)
sudo mkdir -p "$INSTALL_DIR"
sudo chown "$SERVICE_USER:$SERVICE_USER" "$INSTALL_DIR"
sudo chmod 755 "$INSTALL_DIR"

# Code can now create subdirectories automatically
```

**Ưu điểm:**
- Tạo parent directory một lần
- Code tự động tạo subdirectories
- Không cần sudo khi chạy ứng dụng

### Strategy 2: Debian Package postinst

```bash
#!/bin/bash
# debian/postinst

INSTALL_DIR="/opt/myapp"
SERVICE_USER="myapp"

# Create directories during package installation
mkdir -p "$INSTALL_DIR"/{data,logs,config}
chown -R "$SERVICE_USER:$SERVICE_USER" "$INSTALL_DIR"
chmod 755 "$INSTALL_DIR"
```

**Ưu điểm:**
- Tự động khi cài package
- Không cần script riêng
- Chuẩn Debian

### Strategy 3: Systemd Service với ReadWritePaths

```ini
[Service]
User=myapp
ReadWritePaths=/opt/myapp/data /opt/myapp/logs
```

**Ưu điểm:**
- Service có quyền ghi vào thư mục cụ thể
- Bảo mật tốt (chỉ cho phép thư mục cần thiết)

## 📊 So Sánh Các Giải Pháp

| Giải pháp | Ưu điểm | Nhược điểm | Khi nào dùng |
|-----------|---------|------------|--------------|
| **Auto-fallback** | Đơn giản, user không cần làm gì | Có thể dùng thư mục khác mong muốn | Development, testing |
| **Script deploy** | Kiểm soát tốt, production-ready | Cần chạy script trước | Production deployment |
| **Debian package** | Tự động, chuẩn | Cần build package | Distribution |
| **Systemd ReadWritePaths** | Bảo mật cao | Cần config service | Production với systemd |

## 🔍 Troubleshooting

### Vấn đề: "Permission denied"

**Nguyên nhân:**
- Thư mục cha không tồn tại
- Không có quyền ghi vào thư mục cha

**Giải pháp:**
```bash
# Tạo parent directory
sudo mkdir -p /opt/myapp
sudo chown $USER:$USER /opt/myapp
sudo chmod 755 /opt/myapp

# Code sẽ tự động tạo subdirectories
```

### Vấn đề: "Directory created but can't write"

**Nguyên nhân:**
- Thư mục được tạo bởi user khác
- Permissions không đúng

**Giải pháp:**
```bash
# Fix ownership
sudo chown -R myapp:myapp /opt/myapp
sudo chmod -R 755 /opt/myapp
```

### Vấn đề: "Fallback không hoạt động"

**Nguyên nhân:**
- HOME environment variable không set
- Không có quyền tạo thư mục user

**Giải pháp:**
```bash
# Set HOME if not set
export HOME=/home/username

# Hoặc dùng INSTANCES_DIR env var
export INSTANCES_DIR=/path/to/custom/directory
```

## 📚 Tài Liệu Tham Khảo

- [XDG Base Directory Specification](https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html)
- [Filesystem Hierarchy Standard (FHS)](https://refspecs.linuxfoundation.org/FHS_3.0/fhs/index.html)
- [C++17 Filesystem Library](https://en.cppreference.com/w/cpp/filesystem)

## ✅ Checklist Implementation

Khi implement cho thư mục mới:

- [ ] Có environment variable override (e.g., `INSTANCES_DIR`)
- [ ] Có fallback sang user directory (`~/.local/share/`)
- [ ] Có fallback cuối cùng (current directory)
- [ ] Log rõ ràng thư mục đang dùng
- [ ] Không throw exception khi không tạo được
- [ ] Kiểm tra quyền trước khi tạo
- [ ] Có script deploy để tạo parent directory
- [ ] Document trong code comments

## 🎯 Kết Luận

**Nguyên tắc vàng:**
> Code nên tự động xử lý mọi trường hợp, user không cần can thiệp. Hệ thống phải luôn chạy được, dù không có quyền tạo thư mục production.

**Workflow khuyến nghị:**
1. Production: Tạo parent directory một lần với script deploy
2. Code: Tự động tạo subdirectories khi có quyền
3. Fallback: Tự động fallback nếu không có quyền
4. Log: Thông báo rõ ràng thư mục đang dùng

Bằng cách này, hệ thống sẽ:
- ✅ Chạy được ngay cả khi chưa deploy
- ✅ Tự động dùng production path khi có quyền
- ✅ Không cần user can thiệp
- ✅ Dễ debug với log rõ ràng

