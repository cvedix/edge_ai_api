# Phân Tích config.json và instance.json

Tài liệu này phân tích xem 2 file `config.json` và `instance.json` ở root có cần thiết không và có thể xóa được không.

## 📊 Phân Tích Chi Tiết

### 1. config.json

**Vị trí:** `/home/cvedix/project/edge_ai_api/config.json`

**Kích thước:** 2.7KB

**Được sử dụng:**
- ✅ **CẦN THIẾT** - Được sử dụng bởi `SystemConfig` class trong `src/config/system_config.cpp`
- ✅ Được load trong `main.cpp` (line 1294-1297)
- ✅ Code tự động tìm và tạo file này với fallback mechanism

**Cách hoạt động:**
1. Code tự động tìm `config.json` theo thứ tự ưu tiên:
   - `CONFIG_FILE` environment variable
   - `./config.json` (current directory)
   - `/opt/edge_ai_api/config/config.json` (production)
   - `/etc/edge_ai_api/config.json` (system)
   - `~/.config/edge_ai_api/config.json` (user config)
   - `./config.json` (last resort)

2. Nếu file không tồn tại, code tự động tạo với default values

**Kết luận:**
- ✅ **GIỮ LẠI** - File này là example/template để user tham khảo
- File ở root giúp user hiểu cấu trúc config
- Code sẽ tự động tạo file nếu không tồn tại, nhưng có example file sẽ tốt hơn
- **Có thể di chuyển vào `examples/` nhưng không cần thiết**

### 2. instance.json

**Vị trí:** `/home/cvedix/project/edge_ai_api/instance.json`

**Kích thước:** 41KB

**Được sử dụng:**
- ❌ **KHÔNG được sử dụng** - Không tìm thấy reference trong code
- ❌ InstanceStorage sử dụng `instances.json` (không phải `instance.json`) trong storage directory
- ❌ Không được load hoặc đọc từ root directory

**Nội dung:**
- File chứa một instance configuration example với TensorRT model
- Có vẻ là example file cũ hoặc test file

**Các example files khác:**
- `examples/instances/example_*.json` - Nhiều example files đã có sẵn
- `examples/instances/create/*.json` - Create examples
- `examples/instances/update/*.json` - Update examples

**Kết luận:**
- ❌ **CÓ THỂ XÓA** - File này không được sử dụng trong code
- File lớn (41KB) và không cần thiết
- Đã có nhiều example files trong `examples/instances/`
- **Nên xóa hoặc di chuyển vào `examples/instances/` nếu muốn giữ làm reference**

## 🎯 Đề Xuất

### Option 1: Xóa instance.json (Khuyến Nghị)

```bash
# Xóa file không cần thiết
rm instance.json
git rm instance.json
git commit -m "remove: unused instance.json file"
```

**Lý do:**
- Không được sử dụng trong code
- Đã có nhiều example files trong `examples/instances/`
- Giảm kích thước repository

### Option 2: Di chuyển vào examples (Nếu muốn giữ)

```bash
# Di chuyển vào examples nếu muốn giữ làm reference
mv instance.json examples/instances/instance_example_tensorrt.json
git mv instance.json examples/instances/instance_example_tensorrt.json
git commit -m "move: instance.json to examples as reference"
```

### Option 3: Giữ nguyên config.json

```bash
# Giữ lại config.json ở root
# File này là example/template hữu ích cho user
```

**Lý do:**
- Được sử dụng như example/template
- Giúp user hiểu cấu trúc config
- Code sẽ tự động tạo nếu không có, nhưng có example tốt hơn

## 📝 Tóm Tắt

| File | Cần Thiết? | Được Sử Dụng? | Hành Động Đề Xuất |
|------|------------|----------------|-------------------|
| `config.json` | ✅ Có | ✅ Có (example/template) | **GIỮ LẠI** ở root |
| `instance.json` | ❌ Không | ❌ Không | **XÓA** hoặc di chuyển vào examples |

## ✅ Kết Luận

- **config.json**: Giữ lại ở root (là example/template hữu ích)
- **instance.json**: Có thể xóa an toàn (không được sử dụng, đã có nhiều examples khác)

