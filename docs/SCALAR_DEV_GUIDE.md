# Hướng dẫn Development Scalar API Documentation

## Tổng quan

Scalar API Reference được sử dụng để hiển thị tài liệu API với hỗ trợ đa ngôn ngữ (English/Tiếng Việt).

## Cấu trúc Files

```
api-specs/
├── openapi/
│   ├── en/
│   │   └── openapi.yaml        # OpenAPI spec tiếng Anh
│   └── vi/
│       └── openapi.yaml        # OpenAPI spec tiếng Việt
└── scalar/
    └── index.html              # Scalar HTML template với logic đa ngôn ngữ
```

## Endpoints

### OpenAPI Specification
- `GET /v1/openapi.yaml` - OpenAPI spec mặc định
- `GET /v1/openapi/en/openapi.yaml` - OpenAPI spec tiếng Anh
- `GET /v1/openapi/vi/openapi.yaml` - OpenAPI spec tiếng Việt

### Documentation
- `GET /v1/document` - Scalar documentation với language selector
- `GET /v1/document?lang=en` - Scalar documentation tiếng Anh
- `GET /v1/document?lang=vi` - Scalar documentation tiếng Việt

## Cách Scalar hoạt động

### 1. Auto-initialization từ data-spec-url

Scalar tự động khởi tạo khi tìm thấy element có `data-spec-url` attribute:

```html
<div id="api-reference" data-spec-url="http://localhost:8080/v1/openapi/en/openapi.yaml"></div>
```

### 2. Manual initialization (fallback)

Nếu auto-initialization không hoạt động, code sẽ fallback sang manual initialization:

```javascript
ScalarApiReference.default({
    url: 'http://localhost:8080/v1/openapi/en/openapi.yaml',
    target: '#api-reference'
});
```

## Phiên bản Scalar

**Version hiện tại:** `1.24.0` (pinned, không dùng @latest)

**Lý do pin version:**
- Đảm bảo tính ổn định
- Tránh breaking changes từ version mới
- Dễ debug và reproduce issues

**CDN URLs:**
- Primary: `https://cdn.jsdelivr.net/npm/@scalar/api-reference@1.24.0/dist/browser/standalone.js`
- Fallback 1: `https://unpkg.com/@scalar/api-reference@1.24.0/dist/browser/standalone.js`
- Fallback 2: `https://fastly.jsdelivr.net/npm/@scalar/api-reference@1.24.0/dist/browser/standalone.js`

## Cập nhật Scalar Version

Khi cần update Scalar version:

1. **Kiểm tra version mới:**
```bash
npm view @scalar/api-reference versions --json | tail -5
```

2. **Test version mới:**
   - Update version trong `api-specs/scalar/index.html`
   - Update version trong `src/api/swagger_handler.cpp` (hardcoded HTML fallback)
   - Test cả English và Vietnamese
   - Test trên các browser khác nhau

3. **Commit changes:**
```bash
git add api-specs/scalar/index.html src/api/swagger_handler.cpp
git commit -m "chore: update Scalar to version X.Y.Z"
```

## Logic đa ngôn ngữ

### 1. Language Detection

```javascript
// Priority:
// 1. URL parameter (?lang=en hoặc ?lang=vi)
// 2. localStorage ('api-docs-language')
// 3. Default: 'en'
```

### 2. Spec URL Building

```javascript
// Format: /{version}/openapi/{lang}/openapi.yaml
// Example: /v1/openapi/en/openapi.yaml
const specUrl = baseUrl + '/' + version + '/openapi/' + language + '/openapi.yaml';
```

### 3. Language Switching

Khi user chọn ngôn ngữ từ dropdown:
1. Lưu vào localStorage
2. Update URL parameter
3. Reinitialize Scalar với spec URL mới

## Code Structure

### Backend (C++)

**File:** `include/api/swagger_handler.h`
- `getScalarDocument()` - Handler cho `/v1/document`
- `getOpenAPISpecWithLang()` - Handler cho `/v1/openapi/{lang}/openapi.yaml`
- `readOpenAPIFile(version, requestHost, language)` - Đọc OpenAPI file theo ngôn ngữ

**File:** `src/api/swagger_handler.cpp`
- `generateScalarDocumentHTML()` - Generate HTML với placeholders
- `readOpenAPIFile()` - Đọc từ `api-specs/openapi/{lang}/openapi.yaml`
- Cache mechanism với language-aware keys

### Frontend (JavaScript)

**File:** `api-specs/scalar/index.html`

**Key Functions:**
- `getLanguage()` - Lấy ngôn ngữ hiện tại
- `initializeScalar(language)` - Khởi tạo Scalar với ngôn ngữ
- `handleLanguageChange()` - Xử lý khi user đổi ngôn ngữ

**Configuration:**
```javascript
{
    baseUrl: "http://localhost:8080",
    version: "v1",
    fallbackUrl: "http://localhost:8080/v1/openapi.yaml"
}
```

## Development Workflow

### 1. Thêm/Sửa OpenAPI Spec

**Tiếng Anh:**
```bash
# Edit file
vim api-specs/openapi/en/openapi.yaml

# Validate YAML
yamllint api-specs/openapi/en/openapi.yaml
```

**Tiếng Việt:**
```bash
# Edit file
vim api-specs/openapi/vi/openapi.yaml

# Đảm bảo cấu trúc giống file tiếng Anh
# Chỉ dịch các phần description, title, etc.
```

### 2. Test Changes

```bash
# Rebuild server
cd build && make -j$(nproc)

# Restart server
pkill -f edge_ai_api && ./bin/edge_ai_api

# Test endpoints
curl http://localhost:8080/v1/openapi/en/openapi.yaml | head -20
curl http://localhost:8080/v1/openapi/vi/openapi.yaml | head -20

# Test documentation
# Mở browser: http://localhost:8080/v1/document
# Test cả English và Vietnamese
```

### 3. Debug Issues

**Vấn đề: 404 khi truy cập `/v1/openapi/en/openapi.yaml`**

Kiểm tra:
1. Route đã được register chưa trong `swagger_handler.h`
2. File `api-specs/openapi/en/openapi.yaml` có tồn tại không
3. Server đã rebuild chưa

**Vấn đề: Scalar không hiển thị**

Kiểm tra browser console:
1. Scalar script có load được không
2. `data-spec-url` attribute có được set không
3. OpenAPI spec URL có accessible không

**Debug steps:**
```javascript
// Trong browser console:
console.log(document.getElementById('api-reference').getAttribute('data-spec-url'));
console.log(typeof ScalarApiReference);
fetch('http://localhost:8080/v1/openapi/en/openapi.yaml').then(r => console.log(r.status));
```

## Best Practices

### 1. Đồng bộ OpenAPI Specs

- Luôn cập nhật cả 2 file `en/openapi.yaml` và `vi/openapi.yaml`
- Đảm bảo cấu trúc giống nhau (paths, schemas, etc.)
- Chỉ dịch các phần text, không thay đổi structure

### 2. Version Management

- Pin Scalar version, không dùng @latest
- Document breaking changes khi update version
- Test kỹ trước khi update

### 3. Error Handling

- Luôn có fallback URL nếu language-specific URL không tồn tại
- Log errors để debug dễ hơn
- Hiển thị error message rõ ràng cho user

### 4. Performance

- Cache OpenAPI file content (đã implement)
- Cache key bao gồm version và language
- Invalidate cache khi file thay đổi

## Testing Checklist

Trước khi commit:

- [ ] Test `/v1/document` hiển thị đúng
- [ ] Test language switching hoạt động
- [ ] Test `/v1/openapi/en/openapi.yaml` trả về đúng
- [ ] Test `/v1/openapi/vi/openapi.yaml` trả về đúng
- [ ] Test fallback khi language-specific URL không tồn tại
- [ ] Test trên Chrome, Firefox, Safari
- [ ] Test responsive trên mobile
- [ ] Kiểm tra browser console không có errors

## Troubleshooting

### Lỗi: "Couldn't find a [data-spec-url] element"

**Nguyên nhân:** `data-spec-url` attribute chưa được set

**Giải pháp:** 
- Kiểm tra `initializeScalar()` có được gọi không
- Kiểm tra `apiRef.setAttribute('data-spec-url', specUrl)` có chạy không

### Lỗi: "ScalarApiReference is not defined"

**Nguyên nhân:** Scalar script chưa load xong

**Giải pháp:**
- Đợi script load xong trước khi khởi tạo
- Kiểm tra CDN có accessible không
- Check network tab trong browser DevTools

### Lỗi: 404 khi truy cập language-specific URL

**Nguyên nhân:** Route chưa được register hoặc file không tồn tại

**Giải pháp:**
- Kiểm tra route trong `swagger_handler.h`
- Kiểm tra file `api-specs/openapi/{lang}/openapi.yaml` có tồn tại không
- Rebuild và restart server

## References

- [Scalar Documentation](https://github.com/scalar/scalar)
- [OpenAPI Specification](https://swagger.io/specification/)
- [Scalar API Reference NPM](https://www.npmjs.com/package/@scalar/api-reference)
