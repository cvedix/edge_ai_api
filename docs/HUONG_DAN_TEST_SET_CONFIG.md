# Hướng Dẫn Test Chức Năng Set Config

## Tổng Quan

API `POST /v1/core/instance/{instanceId}/config` cho phép bạn cập nhật từng field cụ thể trong config của instance mà không cần gửi toàn bộ config.

## Endpoint

```
POST /v1/core/instance/{instanceId}/config
```

## Request Body Format

```json
{
  "path": "Đường/dẫn/đến/field",
  "jsonValue": "Giá trị JSON dạng string (phải escape)"
}
```

### Lưu ý quan trọng:

1. **`path`**: Đường dẫn đến field cần update, sử dụng `/` để phân cách các level nested
2. **`jsonValue`**: Phải là một JSON string hợp lệ, được escape đúng cách:
   - String: `"\"my string\""` (có dấu ngoặc kép bên ngoài và escape bên trong)
   - Number: `"0.5"` hoặc `"20"` (có thể không cần dấu ngoặc kép)
   - Boolean: `"true"` hoặc `"false"` (có thể không cần dấu ngoặc kép)
   - Object: `"{\"key\":\"value\"}"` (JSON object được escape)

## Các Ví Dụ Test

### 1. Set DisplayName (String)

```bash
curl -X POST http://localhost:8848/v1/core/instance/1a484dc9-81f2-4f4d-a361-00056a6c24b8/config \
  -H "Content-Type: application/json" \
  -d '{
    "path": "DisplayName",
    "jsonValue": "\"face_detection_demo_1_updated\""
  }'
```

**Response**: HTTP 204 (No Content) nếu thành công

### 2. Set Detector Confidence Threshold (Number)

```bash
curl -X POST http://localhost:8848/v1/core/instance/1a484dc9-81f2-4f4d-a361-00056a6c24b8/config \
  -H "Content-Type: application/json" \
  -d '{
    "path": "Detector/conf_threshold",
    "jsonValue": "0.5"
  }'
```

### 3. Set Face Confidence Threshold (Number)

```bash
curl -X POST http://localhost:8848/v1/core/instance/1a484dc9-81f2-4f4d-a361-00056a6c24b8/config \
  -H "Content-Type: application/json" \
  -d '{
    "path": "Detector/face_confidence_threshold",
    "jsonValue": "0.2"
  }'
```

### 4. Set Sensitivity Preset (String)

```bash
curl -X POST http://localhost:8848/v1/core/instance/1a484dc9-81f2-4f4d-a361-00056a6c24b8/config \
  -H "Content-Type: application/json" \
  -d '{
    "path": "Detector/current_sensitivity_preset",
    "jsonValue": "\"High\""
  }'
```

### 5. Set AutoStart (Boolean)

```bash
curl -X POST http://localhost:8848/v1/core/instance/1a484dc9-81f2-4f4d-a361-00056a6c24b8/config \
  -H "Content-Type: application/json" \
  -d '{
    "path": "AutoStart",
    "jsonValue": "false"
  }'
```

### 6. Set Nested Handler Enabled (Boolean trong nested object)

```bash
curl -X POST http://localhost:8848/v1/core/instance/1a484dc9-81f2-4f4d-a361-00056a6c24b8/config \
  -H "Content-Type: application/json" \
  -d '{
    "path": "Output/handlers/rtsp:--0.0.0.0:8554-stream1/enabled",
    "jsonValue": "true"
  }'
```

### 7. Set Nested Object (JSON Object)

```bash
curl -X POST http://localhost:8848/v1/core/instance/1a484dc9-81f2-4f4d-a361-00056a6c24b8/config \
  -H "Content-Type: application/json" \
  -d '{
    "path": "Detector/preset_values/MosaicInference",
    "jsonValue": "{\"Detector/model_file\":\"pva_det_mosaic_320\",\"Detector/conf_threshold\":0.4}"
  }'
```

### 8. Set Frame Rate Limit (Number)

```bash
curl -X POST http://localhost:8848/v1/core/instance/1a484dc9-81f2-4f4d-a361-00056a6c24b8/config \
  -H "Content-Type: application/json" \
  -d '{
    "path": "SolutionManager/frame_rate_limit",
    "jsonValue": "20"
  }'
```

### 9. Set Performance Mode (String)

```bash
curl -X POST http://localhost:8848/v1/core/instance/1a484dc9-81f2-4f4d-a361-00056a6c24b8/config \
  -H "Content-Type: application/json" \
  -d '{
    "path": "PerformanceMode/current_preset",
    "jsonValue": "\"Performance\""
  }'
```

## Sử Dụng Script Test Tự Động

Chạy script test tự động:

```bash
cd /home/pnsang/project/edge_ai_api
./examples/instances/test_set_config_api.sh
```

Script này sẽ:
1. Kiểm tra instance có tồn tại không
2. Test các trường hợp set config khác nhau
3. Verify giá trị đã được set đúng chưa
4. Hiển thị config cuối cùng

## Kiểm Tra Config Sau Khi Set

Sau khi set config, bạn có thể kiểm tra bằng cách:

```bash
# Lấy toàn bộ config của instance
curl http://localhost:8848/v1/core/instance/1a484dc9-81f2-4f4d-a361-00056a6c24b8 | jq '.'

# Hoặc chỉ xem một số field cụ thể
curl http://localhost:8848/v1/core/instance/1a484dc9-81f2-4f4d-a361-00056a6c24b8 | jq '{
  displayName: .displayName,
  detector: .detector,
  solutionManager: .solutionManager
}'
```

## Các Trường Hợp Lỗi

### 1. Instance không tồn tại
**Response**: HTTP 404
```json
{
  "error": "Instance not found",
  "message": "Instance not found: {instanceId}"
}
```

### 2. Path hoặc jsonValue thiếu
**Response**: HTTP 400
```json
{
  "error": "Bad request",
  "message": "Field 'path' is required and must be a string"
}
```

### 3. jsonValue không phải JSON hợp lệ
**Response**: HTTP 400
```json
{
  "error": "Bad request",
  "message": "Field 'jsonValue' must contain valid JSON: {error details}"
}
```

### 4. Instance đang ở chế độ read-only
**Response**: HTTP 500
```json
{
  "error": "Internal server error",
  "message": "Failed to update instance configuration"
}
```

## Lưu Ý Quan Trọng

1. **Instance sẽ tự động restart** nếu đang chạy khi config được update
2. **Path phải đúng format**: Sử dụng `/` để phân cách các level, ví dụ: `Detector/conf_threshold`
3. **jsonValue phải là JSON string hợp lệ**: 
   - String: `"\"value\""` (có dấu ngoặc kép và escape)
   - Number: `"0.5"` hoặc `0.5` (có thể không cần dấu ngoặc kép)
   - Boolean: `"true"` hoặc `true` (có thể không cần dấu ngoặc kép)
   - Object: `"{\"key\":\"value\"}"` (JSON object được escape)

## Ví Dụ Với jq (Để Tạo JSON Request Dễ Dàng)

```bash
# Tạo request body với jq để tránh lỗi escape
jq -n \
  --arg path "Detector/conf_threshold" \
  --arg jsonValue "0.5" \
  '{path: $path, jsonValue: $jsonValue}' | \
curl -X POST http://localhost:8848/v1/core/instance/1a484dc9-81f2-4f4d-a361-00056a6c24b8/config \
  -H "Content-Type: application/json" \
  -d @-
```

## Test Cases Đề Xuất

1. ✅ Set string value (DisplayName)
2. ✅ Set number value (conf_threshold)
3. ✅ Set boolean value (AutoStart)
4. ✅ Set nested string (Detector/current_sensitivity_preset)
5. ✅ Set nested boolean (Output/handlers/.../enabled)
6. ✅ Set nested object (preset_values)
7. ✅ Set multiple values liên tiếp
8. ✅ Verify giá trị sau khi set
9. ✅ Test error cases (invalid path, invalid JSON)

