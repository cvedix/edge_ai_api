# Feature: POST Configure Record Output for Instance

## 📋 Tổng quan / Overview

Tính năng này mở rộng endpoint POST `/v1/core/instance/{instanceId}/output/stream` để hỗ trợ **Record Output Mode** - cho phép lưu video vào file MP4 trên local disk, bên cạnh chế độ Stream Output đã có sẵn (RTMP/RTSP/HLS).

This feature extends the POST `/v1/core/instance/{instanceId}/output/stream` endpoint to support **Record Output Mode** - allowing video to be saved as MP4 files to local disk, in addition to the existing Stream Output mode (RTMP/RTSP/HLS).

## 🎯 Mục tiêu / Objectives

- Mở rộng endpoint hiện có để hỗ trợ cả Stream Output và Record Output
- Cho phép lưu video vào file MP4 trên local disk thông qua parameter `path`
- Validate và tự động tạo directory nếu chưa tồn tại
- Kiểm tra write permissions trước khi cấu hình
- Tự động restart instance khi cấu hình thay đổi (nếu instance đang chạy)
- Đảm bảo backward compatibility với Stream Output mode hiện có

- Extend existing endpoint to support both Stream Output and Record Output
- Allow saving video to MP4 files on local disk via `path` parameter
- Validate and automatically create directory if it doesn't exist
- Check write permissions before configuration
- Auto-restart instance when configuration changes (if instance is running)
- Ensure backward compatibility with existing Stream Output mode

## ✨ Tính năng chính / Key Features

### 1. Hai Chế Độ Hoạt Động / Two Operation Modes

#### Record Output Mode (Mới)
- **Mục đích**: Lưu video vào file MP4 trên local disk
- **Parameter**: `path` - đường dẫn thư mục để lưu file
- **Use case**: Lưu lại video để xem sau, phân tích, hoặc backup

#### Stream Output Mode (Đã có)
- **Mục đích**: Stream video trực tiếp qua RTMP/RTSP/HLS
- **Parameter**: `uri` - URI stream (rtmp://, rtsp://, hls://)
- **Use case**: Stream đến MediaMTX, YouTube Live, hoặc dịch vụ streaming khác

### 2. API Endpoint

**Endpoint:** `POST /v1/core/instance/{instanceId}/output/stream`

**Request Format - Record Output Mode:**
```json
{
  "enabled": true,
  "path": "/mnt/sb1/data"
}
```

**Request Format - Stream Output Mode:**
```json
{
  "enabled": true,
  "uri": "rtmp://localhost:1935/live/stream"
}
```

**Request Format - Disable Output:**
```json
{
  "enabled": false
}
```

### 3. Validation Logic

**Record Output Mode Validation:**
- ✅ `path` field phải tồn tại và là string
- ✅ `path` không được empty
- ✅ Directory sẽ được tự động tạo nếu chưa tồn tại
- ✅ Path phải là directory (không phải file)
- ✅ Path phải có write permissions (test bằng cách tạo test file)

**Stream Output Mode Validation:**
- ✅ `uri` field phải tồn tại và là string
- ✅ `uri` không được empty
- ✅ URI phải bắt đầu với `rtmp://`, `rtsp://`, hoặc `hls://`

**Mutual Exclusivity:**
- ✅ Khi `enabled=true`, phải có một trong hai: `path` (record) hoặc `uri` (stream)
- ✅ Không được có cả `path` và `uri` cùng lúc

### 4. Response Codes

- `204 No Content`: Cấu hình thành công
- `400 Bad Request`: Request không hợp lệ
  - Missing `path` hoặc `uri` khi `enabled=true`
  - Path không hợp lệ hoặc không có write permission
  - URI format không đúng
- `404 Not Found`: Instance không tồn tại
- `500 Internal Server Error`: Lỗi server khi cập nhật cấu hình

## 📁 Files Changed

### Core Implementation
- `src/api/instance_handler.cpp` (+~150 lines)
  - Mở rộng method `configureStreamOutput()` để hỗ trợ record output
  - Thêm logic phân biệt giữa `path` (record) và `uri` (stream)
  - Implement path validation:
    - Check path exists, create if not
    - Verify path is directory
    - Test write permissions
  - Lưu `RECORD_PATH` vào `AdditionalParams` khi dùng record mode
  - Clear cả `RTMP_URL` và `RECORD_PATH` khi disable

- `src/api/instance_handler.cpp` (GET endpoint updates)
  - Cập nhật `getStreamOutput()` để trả về cả `path` field
  - Check `RECORD_PATH` trong `additionalParams`
  - Determine `enabled` status từ cả `RTMP_URL` và `RECORD_PATH`

- `src/core/pipeline_builder.cpp` (+25 lines)
  - Cập nhật pipeline builder để handle `RECORD_PATH` parameter
  - Configure `file_des_node` để lưu video vào path được chỉ định

### API Documentation
- `openapi.yaml` (+~50 lines)
  - Cập nhật endpoint description để mô tả cả 2 modes
  - Thêm `path` field vào `ConfigureStreamOutputRequest` schema
  - Thêm `path` field vào `StreamOutputResponse` schema
  - Thêm example cho record output mode

### Documentation
- `docs/STREAM_RECORD_OUTPUT_GUIDE.md` (+329 lines)
  - Hướng dẫn chi tiết về cả 2 modes
  - Examples và use cases
  - Troubleshooting guide

- `docs/COMPLETE_RECORD_OUTPUT_TROUBLESHOOTING.md` (+199 lines)
  - Troubleshooting guide đầy đủ cho record output
  - Common issues và solutions

### Scripts
- `scripts/restart_instance_for_record.sh` (+136 lines)
  - Script helper để restart instance sau khi config record output
  - Validate configuration và check status

- `scripts/check_record_debug.sh` (+122 lines)
  - Debug script để check record output configuration
  - Verify path permissions và file creation

## 🔧 Technical Details

### Implementation Flow

1. **Request Validation**
   - Kiểm tra instance ID từ path parameter
   - Validate request body JSON format
   - Kiểm tra `enabled` field (required boolean)
   - Nếu `enabled=true`:
     - Check có `path` (record mode) hoặc `uri` (stream mode)
     - Validate format tương ứng

2. **Path Validation (Record Mode)**
   - Check path không empty
   - Tạo directory nếu chưa tồn tại (`fs::create_directories`)
   - Verify path là directory (`fs::is_directory`)
   - Test write permission bằng cách tạo test file
   - Clean up test file sau khi test

3. **URI Validation (Stream Mode)**
   - Check URI không empty
   - Validate URI format (rtmp://, rtsp://, hls://)

4. **Configuration Update**
   - Build config JSON:
     - Record mode: `AdditionalParams["RECORD_PATH"] = path`
     - Stream mode: `AdditionalParams["RTMP_URL"] = uri`
   - Gọi `InstanceRegistry::updateInstanceFromConfig()` để cập nhật
   - Method này tự động merge config và restart instance nếu đang chạy

5. **Disable Output**
   - Clear cả `RTMP_URL` và `RECORD_PATH` (set empty string)
   - Đảm bảo cả 2 modes đều được disable

6. **Response**
   - Trả về `204 No Content` khi thành công
   - Trả về error response với message chi tiết khi có lỗi

### Data Storage Strategy

**Record Output Mode:**
- Lưu `RECORD_PATH` vào `AdditionalParams["RECORD_PATH"]`
- **Không** overwrite `RTMP_URL` để preserve stream config nếu có
- Pipeline sử dụng `file_des_node` để lưu video vào path

**Stream Output Mode:**
- Lưu `RTMP_URL` vào `AdditionalParams["RTMP_URL"]`
- Pipeline sử dụng `rtmp_des_node` để stream

**Disable:**
- Clear cả hai: `RTMP_URL = ""` và `RECORD_PATH = ""`

### Integration Points

- **InstanceRegistry**: Sử dụng `updateInstanceFromConfig()` để cập nhật config
- **Pipeline Builder**: 
  - Handle `RECORD_PATH` để configure `file_des_node`
  - Handle `RTMP_URL` để configure `rtmp_des_node`
- **Instance Storage**: Config được lưu vào storage để persist
- **GET Endpoint**: Trả về cả `path` và `uri` trong response

## 🚀 Tổng quan quá trình thực hiện / Development Process Overview

### Các bước đã thực hiện / Steps Completed

#### 1. Phân tích yêu cầu và thiết kế / Requirements Analysis & Design
- ✅ Phân tích yêu cầu: Cần hỗ trợ lưu video vào file MP4
- ✅ Thiết kế API: Mở rộng endpoint hiện có thay vì tạo endpoint mới
- ✅ Quyết định sử dụng `path` parameter để phân biệt với `uri` (stream)
- ✅ Thiết kế validation logic cho path (directory, permissions)
- ✅ Đảm bảo backward compatibility với Stream Output mode

#### 2. Implementation Core Logic / Triển khai Logic Chính
- ✅ Mở rộng `configureStreamOutput()` method:
  - Thêm logic detect `path` vs `uri` parameter
  - Implement path validation với filesystem operations
  - Handle cả 2 modes trong cùng một method
- ✅ Path validation implementation:
  - Auto-create directory nếu không tồn tại
  - Verify directory type và write permissions
  - Error handling cho filesystem operations
- ✅ Configuration storage:
  - Lưu `RECORD_PATH` vào `AdditionalParams`
  - Preserve `RTMP_URL` khi dùng record mode (không overwrite)
  - Clear cả 2 khi disable

#### 3. Pipeline Builder Integration / Tích hợp Pipeline Builder
- ✅ Cập nhật `pipeline_builder.cpp` để handle `RECORD_PATH`
- ✅ Configure `file_des_node` để lưu video vào path
- ✅ Đảm bảo pipeline hoạt động đúng với cả 2 modes

#### 4. GET Endpoint Updates / Cập nhật GET Endpoint
- ✅ Cập nhật `getStreamOutput()` để trả về `path` field
- ✅ Check `RECORD_PATH` trong `additionalParams`
- ✅ Determine `enabled` từ cả `RTMP_URL` và `RECORD_PATH`
- ✅ Response format bao gồm cả `uri` và `path`

#### 5. API Documentation / Tài liệu API
- ✅ Cập nhật `openapi.yaml`:
  - Mô tả cả 2 modes trong endpoint description
  - Thêm `path` field vào request schema
  - Thêm `path` field vào response schema
  - Thêm examples cho record output mode

#### 6. Documentation / Tài liệu
- ✅ Tạo `STREAM_RECORD_OUTPUT_GUIDE.md`:
  - Hướng dẫn chi tiết về cả 2 modes
  - Examples và use cases
  - Troubleshooting tips
- ✅ Tạo `COMPLETE_RECORD_OUTPUT_TROUBLESHOOTING.md`:
  - Troubleshooting guide đầy đủ
  - Common issues và solutions

#### 7. Helper Scripts / Scripts Hỗ trợ
- ✅ Tạo `restart_instance_for_record.sh`:
  - Helper script để restart instance sau khi config
  - Validate configuration
- ✅ Tạo `check_record_debug.sh`:
  - Debug script để check configuration
  - Verify path permissions

#### 8. Testing và Validation / Kiểm thử và Xác thực
- ✅ Test path validation với các scenarios:
  - Path không tồn tại (auto-create)
  - Path là file (error)
  - Path không có write permission (error)
  - Path hợp lệ (success)
- ✅ Test mutual exclusivity:
  - Có cả `path` và `uri` (error)
  - Không có cả 2 khi `enabled=true` (error)
- ✅ Test backward compatibility:
  - Stream mode vẫn hoạt động như cũ
  - GET endpoint trả về đúng format

### Các quyết định kỹ thuật quan trọng / Key Technical Decisions

1. **Mở rộng endpoint hiện có thay vì tạo mới**
   - **Rationale**: Giữ API consistent, tránh duplicate code
   - **Trade-off**: Method name có thể gây confusion (configureStreamOutput nhưng support cả record)

2. **Sử dụng `path` parameter thay vì tạo endpoint riêng**
   - **Rationale**: Simple và intuitive, dễ phân biệt với `uri`
   - **Design**: Mutual exclusivity - chỉ một trong hai được dùng

3. **Auto-create directory**
   - **Rationale**: User-friendly, giảm manual steps
   - **Security**: Vẫn check write permissions sau khi create

4. **Preserve RTMP_URL khi dùng record mode**
   - **Rationale**: Cho phép switch giữa 2 modes mà không mất config cũ
   - **Design**: Chỉ set field tương ứng với mode đang dùng

5. **Test write permission bằng test file**
   - **Rationale**: Reliable way to verify write access
   - **Implementation**: Create, test, cleanup test file

6. **Clear cả RTMP_URL và RECORD_PATH khi disable**
   - **Rationale**: Đảm bảo clean state, không có leftover config
   - **Design**: Set cả 2 về empty string

### Challenges và Solutions / Thách thức và Giải pháp

1. **Challenge**: Làm sao phân biệt record mode và stream mode?
   - **Solution**: Check `path` vs `uri` parameter - mutual exclusivity

2. **Challenge**: Validate path permissions một cách reliable?
   - **Solution**: Tạo test file để verify write access, cleanup sau đó

3. **Challenge**: Đảm bảo backward compatibility với Stream Output?
   - **Solution**: Giữ nguyên logic stream mode, chỉ thêm record mode logic

4. **Challenge**: Pipeline builder cần handle cả 2 modes?
   - **Solution**: Check cả `RECORD_PATH` và `RTMP_URL`, configure node tương ứng

5. **Challenge**: GET endpoint cần trả về cả 2 fields?
   - **Solution**: Return cả `uri` và `path`, empty string nếu không dùng

### Kết quả đạt được / Achievements

- ✅ Feature hoàn chỉnh với cả 2 modes
- ✅ Backward compatible với Stream Output mode
- ✅ Comprehensive validation và error handling
- ✅ Auto-create directory với permission check
- ✅ API documentation đầy đủ
- ✅ Helper scripts và troubleshooting guides
- ✅ GET endpoint updated để support cả 2 modes

## 🧪 Testing

### Test Scenarios

**Record Output Mode:**
- ✅ Enable record output với path hợp lệ
- ✅ Auto-create directory nếu chưa tồn tại
- ✅ Error khi path là file (không phải directory)
- ✅ Error khi path không có write permission
- ✅ Disable record output

**Stream Output Mode:**
- ✅ Stream mode vẫn hoạt động như cũ (backward compatibility)
- ✅ Enable stream với URI hợp lệ
- ✅ Disable stream output

**Validation:**
- ✅ Error khi có cả `path` và `uri` cùng lúc
- ✅ Error khi không có cả `path` và `uri` khi `enabled=true`
- ✅ Error khi `path` empty
- ✅ Error khi `uri` empty hoặc invalid format

**Integration:**
- ✅ GET endpoint trả về đúng `path` và `uri`
- ✅ Switch giữa record và stream mode
- ✅ Instance restart khi config thay đổi

## 📝 Usage Examples

### Enable Record Output

```bash
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/output/stream \
  -H "Content-Type: application/json" \
  -d '{
    "enabled": true,
    "path": "/mnt/sb1/data"
  }'
```

### Enable Stream Output (Backward Compatible)

```bash
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/output/stream \
  -H "Content-Type: application/json" \
  -d '{
    "enabled": true,
    "uri": "rtmp://localhost:1935/live/stream"
  }'
```

### Disable Output

```bash
curl -X POST http://localhost:8080/v1/core/instance/{instanceId}/output/stream \
  -H "Content-Type: application/json" \
  -d '{
    "enabled": false
  }'
```

### Get Output Configuration

```bash
curl -X GET http://localhost:8080/v1/core/instance/{instanceId}/output/stream
```

**Response (Record Mode Enabled):**
```json
{
  "enabled": true,
  "uri": "",
  "path": "/mnt/sb1/data"
}
```

**Response (Stream Mode Enabled):**
```json
{
  "enabled": true,
  "uri": "rtmp://localhost:1935/live/stream",
  "path": ""
}
```

## 📊 Statistics

- **Total Changes**: 5+ files changed, ~500+ insertions
- **New Features**: Record Output Mode
- **Backward Compatibility**: ✅ Maintained
- **API Documentation**: Complete OpenAPI specification
- **Helper Scripts**: 2 scripts created
- **Documentation**: 2 comprehensive guides

## ✅ Checklist for Reviewers

- [ ] Code follows project coding standards
- [ ] API endpoint properly documented in OpenAPI spec
- [ ] Request validation is comprehensive
- [ ] Path validation handles all edge cases
- [ ] Error handling covers all scenarios
- [ ] Backward compatibility maintained
- [ ] GET endpoint returns correct format
- [ ] Pipeline builder handles both modes correctly
- [ ] Helper scripts work as expected
- [ ] Documentation is clear and complete

## 🔗 Related Features

- **GET /v1/core/instance/{instanceId}/output/stream**: Get output configuration endpoint
- **POST /v1/core/instance/{instanceId}/output/stream**: Configure output endpoint (extended)
- **Stream Output Mode**: Original streaming functionality

## 👤 Author

Nhatnt99

## 📅 Timeline

- Feature implementation: 2025-12-10
- Commits:
  - `95e91b3` - update function method POST config record
  - `f240cea` - update full permission for config record
  - `e5a82a6` - update handler instance POST config record

## 🔄 Relationship với Stream Output

Tính năng này mở rộng endpoint POST `/v1/core/instance/{instanceId}/output/stream` để hỗ trợ cả Stream Output và Record Output:

- **Stream Output** (đã có): Sử dụng `uri` parameter, stream video qua RTMP/RTSP/HLS
- **Record Output** (mới): Sử dụng `path` parameter, lưu video vào file MP4

Cả 2 modes:
- Sử dụng cùng endpoint
- Cùng request/response format (khác parameter)
- Cùng validation và error handling pattern
- Cùng auto-restart behavior

