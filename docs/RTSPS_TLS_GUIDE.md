# RTSP over TLS (rtsps://) Configuration Guide

## Tổng Quan

Hệ thống hỗ trợ RTSP over TLS (`rtsps://`) để kết nối an toàn với RTSP servers sử dụng SSL/TLS encryption.

## Cấu Hình

### 1. URL Format

URL `rtsps://` có format tương tự `rtsp://` nhưng sử dụng TLS encryption:

```
rtsps://host:port/path?query_params
```

**Ví dụ:**
```
rtsps://vcloud.vcv.vn:20972/livestream/ANS.SCCTest001?type=index.rtsp&sessionId=xxx&sessionKey=yyy
```

### 2. TLS Certificate Validation

Mặc định, hệ thống sẽ **disable strict TLS validation** để tương thích với self-signed certificates (thường dùng trong development/testing).

#### Disable Validation (Mặc định - Development)

Không cần cấu hình gì thêm. Hệ thống tự động disable strict validation cho `rtsps://`.

#### Enable Strict Validation (Production)

Để enable strict TLS validation trong production:

```bash
export GST_RTSP_TLS_VALIDATION=strict
export GST_RTSP_CA_CERT_FILE=/path/to/ca-certificate.pem
```

### 3. Transport Protocol

**Quan trọng:** `rtsps://` **yêu cầu TCP transport**. Hệ thống sẽ tự động force TCP cho `rtsps://` URLs.

Bạn có thể set explicitly:
```bash
export GST_RTSP_PROTOCOLS=tcp
```

Hoặc trong request:
```json
{
  "additionalParams": {
    "RTSP_TRANSPORT": "tcp"
  }
}
```

### 4. URL Encoding

Nếu URL có query parameters dài hoặc ký tự đặc biệt, đảm bảo chúng được **URL-encoded** đúng cách:

**Ví dụ:**
- `&` → `%26`
- `=` → `%3D`
- Space → `%20` hoặc `+`

**Ví dụ URL với query parameters:**
```
rtsps://host:port/path?param1=value1&param2=value2
```

Nếu có ký tự đặc biệt:
```
rtsps://host:port/path?param1=value%201&param2=value%202
```

## Troubleshooting

### Lỗi: Connection Failed

1. **Kiểm tra URL encoding:**
   - Đảm bảo query parameters được URL-encoded
   - Kiểm tra log: `[PipelineBuilder] URL preview (first 100 chars): ...`

2. **Kiểm tra TLS certificate:**
   - Nếu dùng self-signed cert, hệ thống sẽ tự động disable validation
   - Nếu vẫn lỗi, thử set `GST_RTSP_TLS_VALIDATION=none`

3. **Kiểm tra transport protocol:**
   - `rtsps://` yêu cầu TCP
   - Log sẽ hiển thị: `[PipelineBuilder] WARNING: rtsps:// requires TCP transport. Forcing RTSP_TRANSPORT=tcp`

4. **Enable GStreamer debug:**
   ```bash
   export GST_DEBUG=rtspsrc:4,avdec_h264:4
   ```

### Lỗi: Certificate Validation Failed

Nếu gặp lỗi certificate validation:

1. **Disable validation (development only):**
   ```bash
   export GST_RTSP_TLS_VALIDATION=none
   ```

2. **Hoặc cung cấp CA certificate:**
   ```bash
   export GST_RTSP_CA_CERT_FILE=/path/to/ca-cert.pem
   export GST_RTSP_TLS_VALIDATION=strict
   ```

### Lỗi: URL Too Long

Nếu URL quá dài (>200 characters), hệ thống sẽ log warning. Đảm bảo:
- Query parameters được URL-encoded
- URL không có ký tự đặc biệt không được encode

## Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `GST_RTSP_TLS_VALIDATION` | `none` | TLS validation mode: `none`, `strict` |
| `GST_RTSP_CA_CERT_FILE` | - | Path to CA certificate file (for strict validation) |
| `GST_RTSP_PROTOCOLS` | `tcp` (auto for rtsps://) | Transport protocol: `tcp`, `udp` |
| `GST_RTSP_BUFFER_MODE` | `1` | Buffer mode: `0`=auto, `1`=synced, `2`=slave |
| `GST_RTSP_BUFFER_SIZE` | `10485760` | Buffer size in bytes (10MB) |
| `GST_RTSP_TIMEOUT` | `10000000000` | Timeout in nanoseconds (10s) |
| `GST_RTSP_LATENCY` | `2000000000` | Latency buffer in nanoseconds (2s) |

## Ví Dụ Sử Dụng

### Example 1: Basic rtsps:// URL

```json
{
  "solutionId": "your_solution_id",
  "additionalParams": {
    "RTSP_URL": "rtsps://vcloud.vcv.vn:20972/livestream/ANS.SCCTest001?type=index.rtsp&sessionId=xxx&sessionKey=yyy"
  }
}
```

### Example 2: With Explicit TCP Transport

```json
{
  "solutionId": "your_solution_id",
  "additionalParams": {
    "RTSP_URL": "rtsps://vcloud.vcv.vn:20972/livestream/ANS.SCCTest001?type=index.rtsp&sessionId=xxx&sessionKey=yyy",
    "RTSP_TRANSPORT": "tcp"
  }
}
```

### Example 3: With Strict TLS Validation

```bash
export GST_RTSP_TLS_VALIDATION=strict
export GST_RTSP_CA_CERT_FILE=/path/to/ca-cert.pem
```

Sau đó gửi request như bình thường.

## Log Messages

Khi sử dụng `rtsps://`, bạn sẽ thấy các log messages sau:

```
[PipelineBuilder] Detected rtsps:// (RTSP over TLS) URL
[PipelineBuilder] Set GST_RTSP_TLS_VALIDATION=none (disable strict TLS validation for rtsps://)
[PipelineBuilder] WARNING: rtsps:// requires TCP transport. Forcing RTSP_TRANSPORT=tcp
[PipelineBuilder] NOTE: URL is very long (XXX chars). This may cause issues if not properly URL-encoded.
[PipelineBuilder] NOTE: URL contains query parameters. Make sure they are properly URL-encoded.
```

## Lưu Ý

1. **Security:** Disable TLS validation (`GST_RTSP_TLS_VALIDATION=none`) chỉ nên dùng trong development/testing. Production nên dùng strict validation với CA certificate.

2. **Performance:** `rtsps://` có thể chậm hơn `rtsp://` do overhead của TLS encryption.

3. **Compatibility:** Một số RTSP servers có thể không hỗ trợ `rtsps://`. Kiểm tra documentation của server.

4. **URL Length:** URLs quá dài (>200 chars) có thể gây vấn đề. Đảm bảo query parameters được URL-encoded đúng cách.


