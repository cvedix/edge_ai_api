# Hướng dẫn Setup Scalar API Documentation

## Tổng quan

Endpoint `/v1/document` cung cấp tài liệu API sử dụng Scalar API Reference, một công cụ hiện đại để hiển thị OpenAPI specification.

## Yêu cầu

1. File `openapi.yaml` phải tồn tại tại `api-specs/openapi.yaml`
2. Server phải được rebuild sau khi thay đổi code
3. Endpoint `/v1/openapi.yaml` phải hoạt động và trả về OpenAPI spec

## Các bước Setup

### 1. Kiểm tra file OpenAPI spec

```bash
# Kiểm tra file có tồn tại không
ls -la api-specs/openapi.yaml

# Kiểm tra nội dung file (xem vài dòng đầu)
head -20 api-specs/openapi.yaml
```

### 2. Rebuild Server

Sau khi thay đổi code, bạn cần rebuild server:

```bash
cd /home/cvedix/Data/project/edge_ai_api

# Nếu dùng CMake build
mkdir -p build
cd build
cmake ..
make -j$(nproc)

# Hoặc nếu đã có build directory
cd build
make -j$(nproc)
```

### 3. Restart Server

Sau khi rebuild, restart server:

```bash
# Nếu chạy trực tiếp
pkill -f edge_ai_api
./bin/edge_ai_api

# Hoặc nếu dùng systemd
sudo systemctl restart edge-ai-api
```

### 4. Kiểm tra Endpoints

```bash
# Kiểm tra endpoint OpenAPI spec
curl -I http://localhost:8080/v1/openapi.yaml
# Phải trả về HTTP 200 OK

# Kiểm tra endpoint Scalar documentation
curl -I http://localhost:8080/v1/document
# Phải trả về HTTP 200 OK với Content-Type: text/html
```

### 5. Truy cập trong Browser

Mở browser và truy cập:
```
http://localhost:8080/v1/document
```

## Troubleshooting

### Vấn đề: Trang không hiển thị gì

**Nguyên nhân có thể:**
1. Server chưa được rebuild sau khi thay đổi code
2. Endpoint `/v1/openapi.yaml` không hoạt động
3. Scalar script không load được từ CDN
4. Có lỗi JavaScript trong browser console

**Giải pháp:**

1. **Kiểm tra server đã rebuild chưa:**
```bash
# Xem thời gian build của binary
ls -lh bin/edge_ai_api

# Rebuild lại nếu cần
cd build && make -j$(nproc)
```

2. **Kiểm tra endpoint OpenAPI spec:**
```bash
curl http://localhost:8080/v1/openapi.yaml | head -20
# Phải trả về nội dung YAML, không phải error
```

3. **Kiểm tra browser console:**
   - Mở browser DevTools (F12)
   - Vào tab Console
   - Xem có lỗi gì không
   - Vào tab Network để xem các request có thành công không

4. **Kiểm tra file openapi.yaml:**
```bash
# Đảm bảo file tồn tại
ls -la api-specs/openapi.yaml

# Kiểm tra quyền đọc file
cat api-specs/openapi.yaml | head -5
```

### Vấn đề: Lỗi 500 khi truy cập `/v1/openapi.yaml`

**Nguyên nhân:** Server không tìm thấy file `openapi.yaml`

**Giải pháp:**
- Đảm bảo file `api-specs/openapi.yaml` tồn tại
- Kiểm tra server đang chạy từ thư mục nào:
```bash
# Xem working directory của server process
ps aux | grep edge_ai_api | grep -v grep
```

- Nếu server chạy từ thư mục khác, có thể cần set environment variable:
```bash
export EDGE_AI_API_INSTALL_DIR=/path/to/project/root
```

### Vấn đề: Scalar không load được từ CDN

**Nguyên nhân:** Không có internet hoặc CDN bị chặn

**Giải pháp:**
- Kiểm tra kết nối internet
- Kiểm tra firewall/proxy settings
- Thử truy cập CDN URL trực tiếp trong browser:
  - https://cdn.jsdelivr.net/npm/@scalar/api-reference@latest/dist/browser/standalone.js

## Cấu trúc Code

### File liên quan:
- `include/api/swagger_handler.h` - Header file với method declarations
- `src/api/swagger_handler.cpp` - Implementation của handler
  - `getScalarDocument()` - Handler cho endpoint `/v1/document`
  - `generateScalarDocumentHTML()` - Generate HTML với Scalar configuration
  - `readOpenAPIFile()` - Đọc file `openapi.yaml` từ `api-specs/`

### Endpoints:
- `GET /v1/document` - Scalar API documentation cho API v1
- `GET /v1/openapi.yaml` - OpenAPI specification cho API v1

## Testing

Sau khi setup, test các endpoint:

```bash
# Test OpenAPI spec endpoint
curl http://localhost:8080/v1/openapi.yaml > /tmp/test.yaml
head -20 /tmp/test.yaml

# Test Scalar documentation endpoint
curl http://localhost:8080/v1/document > /tmp/test.html
head -30 /tmp/test.html

# Kiểm tra HTML có chứa Scalar script không
grep -i "scalar" /tmp/test.html
```

## Notes

- Scalar sử dụng CDN để load script, cần internet connection
- HTML được generate động với URL spec phù hợp
- File `openapi.yaml` được cache trong memory để tăng performance
- Cache sẽ tự động invalidate khi file thay đổi

