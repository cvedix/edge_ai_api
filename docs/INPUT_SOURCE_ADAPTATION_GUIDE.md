# Hướng Dẫn Adapt Input Source - Flexible Input System

## Tổng Quan

Hệ thống hỗ trợ **auto-detect input type** từ `FILE_PATH` hoặc các parameters rõ ràng, cho phép user tự do adapt với nhiều loại input khác nhau mà không cần thay đổi solution config.

## Các Loại Input Được Hỗ Trợ

### 1. Local File
```json
{
  "additionalParams": {
    "FILE_PATH": "/path/to/video.mp4"
  }
}
```
- **Node type**: `file_src`
- **Hỗ trợ**: mp4, avi, mov, và các format video khác

### 2. RTSP Stream
```json
{
  "additionalParams": {
    "FILE_PATH": "rtsp://localhost:8554/mystream"
    // HOẶC
    "RTSP_SRC_URL": "rtsp://localhost:8554/mystream"
  }
}
```
- **Node type**: `rtsp_src` (auto-converted từ `file_src`)
- **Hỗ trợ**: RTSP streams
- **Additional parameters**: `RTSP_TRANSPORT`, `GST_DECODER_NAME`, `SKIP_INTERVAL`, `CODEC_TYPE`

### 3. RTMP Stream
```json
{
  "additionalParams": {
    "FILE_PATH": "rtmp://example.com/live/stream"
    // HOẶC
    "RTMP_SRC_URL": "rtmp://example.com/live/stream"
  }
}
```
- **Node type**: `rtmp_src` (auto-converted từ `file_src`)
- **Hỗ trợ**: RTMP streams

### 4. HLS Stream
```json
{
  "additionalParams": {
    "FILE_PATH": "http://example.com/playlist.m3u8"
    // HOẶC
    "HLS_URL": "http://example.com/playlist.m3u8"
  }
}
```
- **Node type**: `ff_src` (auto-converted từ `file_src`)
- **Hỗ trợ**: HLS streams (.m3u8)

### 5. HTTP Stream
```json
{
  "additionalParams": {
    "FILE_PATH": "http://example.com/video.mp4"
    // HOẶC
    "HTTP_URL": "http://example.com/video.mp4"
  }
}
```
- **Node type**: `ff_src` (auto-converted từ `file_src`)
- **Hỗ trợ**: HTTP/HTTPS video streams

## Priority Order (Thứ Tự Ưu Tiên)

Khi có nhiều parameters cùng lúc, hệ thống sẽ sử dụng theo thứ tự sau:

### Priority 1: Explicit Parameters (Ưu tiên cao nhất)
1. `RTSP_SRC_URL` → Chuyển sang `rtsp_src`
2. `RTMP_SRC_URL` → Chuyển sang `rtmp_src`
3. `HLS_URL` → Chuyển sang `ff_src`
4. `HTTP_URL` → Chuyển sang `ff_src`

### Priority 2: Auto-detect từ FILE_PATH
- Nếu không có explicit parameters → Auto-detect từ `FILE_PATH`
- `rtsp://...` → `rtsp_src`
- `rtmp://...` → `rtmp_src`
- `http://...`, `https://...`, `.m3u8` → `ff_src`
- Local file path → `file_src` (default)

## Conflict Handling

### Khi có Conflict (Cả hai cùng tồn tại)

**Ví dụ**: User cung cấp cả `RTSP_SRC_URL` và `FILE_PATH` với RTSP URL:

```json
{
  "additionalParams": {
    "RTSP_SRC_URL": "rtsp://server1.com/stream",
    "FILE_PATH": "rtsp://server2.com/stream"
  }
}
```

**Kết quả**:
- ✅ `RTSP_SRC_URL` được sử dụng (priority cao hơn)
- ⚠️ `FILE_PATH` sẽ bị ignore
- ⚠️ Warning sẽ được log: `"WARNING: Both RTSP_SRC_URL and FILE_PATH (with RTSP URL) are provided. Using RTSP_SRC_URL (priority). FILE_PATH will be ignored."`

## Best Practices

### 1. Sử dụng Explicit Parameters (Khuyến nghị)
```json
{
  "additionalParams": {
    "RTSP_SRC_URL": "rtsp://example.com/stream",
    "RTSP_TRANSPORT": "tcp",
    "RESIZE_RATIO": "0.5"
  }
}
```
- ✅ Rõ ràng, dễ hiểu
- ✅ Không có ambiguity
- ✅ Dễ maintain

### 2. Sử dụng FILE_PATH với Auto-detect
```json
{
  "additionalParams": {
    "FILE_PATH": "rtsp://example.com/stream"
  }
}
```
- ✅ Đơn giản, chỉ cần một parameter
- ✅ Tự động detect type
- ⚠️ Có thể gây confusion nếu không biết priority

### 3. Tránh Conflict
```json
// ❌ KHÔNG NÊN: Cả hai cùng tồn tại
{
  "additionalParams": {
    "RTSP_SRC_URL": "rtsp://server1.com/stream",
    "FILE_PATH": "rtsp://server2.com/stream"  // Sẽ bị ignore
  }
}

// ✅ NÊN: Chỉ dùng một trong hai
{
  "additionalParams": {
    "RTSP_SRC_URL": "rtsp://server1.com/stream"
  }
}
```

## Examples

### Example 1: RTSP từ FILE_PATH
```json
{
  "name": "rtsp_from_file_path",
  "solution": "ba_crossline_with_mqtt",
  "additionalParams": {
    "FILE_PATH": "rtsp://localhost:8554/mystream",
    "RTSP_TRANSPORT": "tcp",
    "RESIZE_RATIO": "0.05"
  }
}
```
**Kết quả**: Tự động chuyển sang `rtsp_src`

### Example 2: RTSP từ RTSP_SRC_URL
```json
{
  "name": "rtsp_from_explicit",
  "solution": "ba_crossline_with_mqtt",
  "additionalParams": {
    "RTSP_SRC_URL": "rtsp://localhost:8554/mystream",
    "RTSP_TRANSPORT": "tcp",
    "RESIZE_RATIO": "0.05"
  }
}
```
**Kết quả**: Sử dụng `rtsp_src` trực tiếp

### Example 3: HLS Stream
```json
{
  "name": "hls_stream",
  "solution": "ba_crossline_with_mqtt",
  "additionalParams": {
    "FILE_PATH": "http://example.com/playlist.m3u8"
  }
}
```
**Kết quả**: Tự động chuyển sang `ff_src` cho HLS

### Example 4: Local File
```json
{
  "name": "local_file",
  "solution": "ba_crossline_with_mqtt",
  "additionalParams": {
    "FILE_PATH": "/path/to/video.mp4"
  }
}
```
**Kết quả**: Sử dụng `file_src` (default)

## Troubleshooting

### Vấn đề: FILE_PATH không được detect đúng

**Nguyên nhân có thể**:
1. URL có whitespace → Được trim tự động
2. URL không đúng format → Kiểm tra protocol prefix
3. Conflict với explicit parameters → Explicit parameters có priority cao hơn

**Giải pháp**:
- Sử dụng explicit parameters (`RTSP_SRC_URL`, `RTMP_SRC_URL`, etc.)
- Kiểm tra logs để xem input type được detect là gì
- Đảm bảo URL đúng format (có `://`)

### Vấn đề: Muốn force file_src ngay cả khi FILE_PATH là URL

**Giải pháp**: Không hỗ trợ trực tiếp, nhưng có thể:
- Sử dụng solution config với `rtsp_src` node type thay vì `file_src`
- Hoặc rename file để không match URL pattern

## Summary

✅ **Hệ thống hỗ trợ đầy đủ**:
- Auto-detect từ FILE_PATH
- Explicit parameters với priority cao
- Warning khi có conflict
- Flexible và dễ adapt

⚠️ **Lưu ý**:
- Explicit parameters có priority cao hơn FILE_PATH
- Nếu có conflict, explicit parameters sẽ được sử dụng
- FILE_PATH với URL sẽ được auto-detect và convert

🎯 **Khuyến nghị**:
- Sử dụng explicit parameters cho production
- Sử dụng FILE_PATH với auto-detect cho development/testing
- Tránh cung cấp cả hai cùng lúc để tránh confusion

