# Cập nhật Format Instance Detail

## Tổng quan

Đã cập nhật format của instance detail để **khớp hoàn toàn** với format trong `task/instance_detail.txt` để đảm bảo tương thích với hệ thống đang sử dụng.

## Format Yêu cầu

Format instance detail phải có các fields sau (theo thứ tự trong `task/instance_detail.txt`):

```json
{
    "AutoStart": false,
    "Detector": {
        "animal_confidence_threshold": 0.3,
        "conf_threshold": 0.2,
        "current_preset": "FullRegionInference",
        "current_sensitivity_preset": "High",
        "face_confidence_threshold": 0.1,
        "license_plate_confidence_threshold": 0.1,
        "model_file": "pva_det_full_frame_512",
        "person_confidence_threshold": 0.3,
        "preset_values": {
            "MosaicInference": {
                "Detector/model_file": "pva_det_mosaic_320"
            }
        },
        "vehicle_confidence_threshold": 0.3
    },
    "DetectorRegions": {},
    "DetectorThermal": {
        "model_file": "pva_det_mosaic_320"
    },
    "DisplayName": "CAMERA FACE",
    "Input": {
        "media_format": {
            "color_format": 0,
            "default_format": true,
            "height": 0,
            "is_software": false,
            "name": "Same as Source"
        },
        "media_type": "IP Camera",
        "uri": "gstreamer:///urisourcebin uri=rtsp://..."
    },
    "InstanceId": "83bbcddd-7ea1-1756-690a-e43eab51424f",
    "OriginatorInfo": {
        "address": "192.168.1.116"
    },
    "Output": {
        "JSONExport": {
            "enabled": false
        },
        "NXWitness": {
            "enabled": false
        },
        "handlers": {
            "rtsp:--0.0.0.0:8554-stream1": {
                "config": {
                    "debug": "4",
                    "fps": 10,
                    "pipeline": "( appsrc name=cvedia-rt ! ... )"
                },
                "enabled": true,
                "sink": "output-image",
                "uri": "rtsp://0.0.0.0:8554/stream1"
            }
        },
        "render_preset": "Default"
    },
    "PerformanceMode": {
        "current_preset": "Balanced"
    },
    "Solution": "securt",
    "SolutionManager": {
        "enable_debug": false,
        "frame_rate_limit": 15,
        "input_pixel_limit": 2000000,
        "recommended_frame_rate": 5,
        "run_statistics": true,
        "send_diagnostics": false,
        "send_metadata": true
    },
    "Tripwire": {
        "Tripwires": {}
    },
    "Zone": {
        "Zones": {}
    }
}
```

## Các Thay Đổi Đã Thực Hiện

### 1. Cập nhật `InstanceHandler::instanceInfoToJson()`

**File**: `src/api/instance_handler.cpp`

**Thay đổi**:
- ✅ Trả về format với **PascalCase** field names (InstanceId, DisplayName, etc.)
- ✅ Luôn include tất cả các fields bắt buộc (không bỏ qua fields rỗng)
- ✅ Cấu trúc đúng format:
  - `Input` với `media_format`, `media_type`, `uri`
  - `Output` với `JSONExport`, `NXWitness`, `handlers`, `render_preset`
  - `Detector` với đầy đủ thresholds và `preset_values`
  - `DetectorRegions`, `DetectorThermal`, `Tripwire`, `Zone` (có thể rỗng)
  - `PerformanceMode`, `SolutionManager`

**Trước**:
```cpp
json["instanceId"] = info.instanceId;  // camelCase
json["displayName"] = info.displayName;
// Thiếu nhiều fields
```

**Sau**:
```cpp
json["InstanceId"] = info.instanceId;  // PascalCase
json["DisplayName"] = info.displayName;
json["AutoStart"] = info.autoStart;
json["Solution"] = info.solutionId;
// Đầy đủ tất cả fields theo format yêu cầu
```

### 2. Cập nhật `InstanceStorage::instanceInfoToConfigJson()`

**File**: `src/instances/instance_storage.cpp`

**Thay đổi**:
- ✅ Luôn include tất cả các fields khi lưu vào storage
- ✅ Cấu trúc `Input` với `media_format` đầy đủ
- ✅ Cấu trúc `Output` với `handlers` đúng format
- ✅ `Detector` với `preset_values`
- ✅ Luôn include `DetectorRegions`, `Tripwire`, `Zone` (ngay cả khi rỗng)

**Trước**:
```cpp
// Chỉ lưu nếu có giá trị
if (info.inputPixelLimit > 0) {
    input["media_format"]["input_pixel_limit"] = info.inputPixelLimit;
}
```

**Sau**:
```cpp
// Luôn include với default values
Json::Value mediaFormat(Json::objectValue);
mediaFormat["color_format"] = 0;
mediaFormat["default_format"] = true;
mediaFormat["height"] = 0;
mediaFormat["is_software"] = false;
mediaFormat["name"] = "Same as Source";
input["media_format"] = mediaFormat;
```

### 3. Format Fields Mapping

| Field trong Code | Field trong JSON Output | Ghi chú |
|-----------------|------------------------|---------|
| `info.instanceId` | `InstanceId` | PascalCase |
| `info.displayName` | `DisplayName` | PascalCase |
| `info.autoStart` | `AutoStart` | PascalCase |
| `info.solutionId` | `Solution` | PascalCase |
| `info.rtspUrl` | `Input.uri` | GStreamer format |
| `info.filePath` | `Input.uri` | Direct path |
| `info.metadataMode` | `Output.JSONExport.enabled` | |
| `info.frameRateLimit` | `SolutionManager.frame_rate_limit` | |
| `info.detectorMode` | `Detector.current_preset` | |
| `info.detectionSensitivity` | `Detector.current_sensitivity_preset` | |
| `info.performanceMode` | `PerformanceMode.current_preset` | |

## Các Fields Luôn Được Include

Các fields sau **luôn được include** trong output, ngay cả khi rỗng:

- ✅ `DetectorRegions` - Luôn là `{}`
- ✅ `Tripwire` - Luôn có `{"Tripwires": {}}`
- ✅ `Zone` - Luôn có `{"Zones": {}}`
- ✅ `DetectorThermal` - Luôn có `{"model_file": "..."}`
- ✅ `Input.media_format` - Luôn có đầy đủ sub-fields
- ✅ `Output.handlers` - Luôn có (có thể rỗng nếu không có RTSP/RTMP)
- ✅ `PerformanceMode` - Luôn có `{"current_preset": "..."}`

## Default Values

Các default values được sử dụng khi field không có giá trị:

| Field | Default Value |
|-------|---------------|
| `Detector.model_file` | `"pva_det_full_frame_512"` |
| `Detector.current_preset` | `"FullRegionInference"` |
| `Detector.current_sensitivity_preset` | `"High"` |
| `Detector.animal_confidence_threshold` | `0.3` |
| `Detector.person_confidence_threshold` | `0.3` |
| `Detector.vehicle_confidence_threshold` | `0.3` |
| `Detector.face_confidence_threshold` | `0.1` |
| `Detector.license_plate_confidence_threshold` | `0.1` |
| `Detector.conf_threshold` | `0.2` |
| `DetectorThermal.model_file` | `"pva_det_mosaic_320"` |
| `PerformanceMode.current_preset` | `"Balanced"` |
| `SolutionManager.frame_rate_limit` | `15` |
| `SolutionManager.input_pixel_limit` | `2000000` |
| `SolutionManager.recommended_frame_rate` | `5` |
| `OriginatorInfo.address` | `"127.0.0.1"` |

## API Endpoints

### GET /v1/core/instances/{instanceId}

**Response format**: Khớp hoàn toàn với `task/instance_detail.txt`

**Ví dụ**:
```bash
curl http://localhost:8080/v1/core/instances/83bbcddd-7ea1-1756-690a-e43eab51424f
```

**Response**:
```json
{
    "InstanceId": "83bbcddd-7ea1-1756-690a-e43eab51424f",
    "DisplayName": "CAMERA FACE",
    "AutoStart": false,
    "Solution": "face_detection",
    "Input": {
        "media_format": {
            "color_format": 0,
            "default_format": true,
            "height": 0,
            "is_software": false,
            "name": "Same as Source"
        },
        "media_type": "IP Camera",
        "uri": "gstreamer:///urisourcebin uri=rtsp://..."
    },
    ...
}
```

## Storage Format

Khi lưu vào `instances.json`, format cũng khớp với yêu cầu:

**File**: `~/.local/share/edge_ai_api/instances/instances.json`

```json
{
    "83bbcddd-7ea1-1756-690a-e43eab51424f": {
        "InstanceId": "83bbcddd-7ea1-1756-690a-e43eab51424f",
        "DisplayName": "CAMERA FACE",
        "AutoStart": false,
        "Solution": "face_detection",
        ...
    }
}
```

## Tương Thích Ngược

- ✅ Instances cũ vẫn có thể load được (parse tự động)
- ✅ Các fields mới sẽ có default values nếu không có trong file cũ
- ✅ Format mới được áp dụng khi:
  - GET instance detail (API response)
  - Save instance to storage
  - Load instance from storage (với default values)

## Kiểm Tra

### Test GET instance detail:

```bash
# Tạo instance
curl -X POST http://localhost:8080/v1/core/instance \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Test Camera",
    "solution": "face_detection",
    "additionalParams": {
      "RTSP_URL": "rtsp://localhost:8554/live/vanphong"
    }
  }'

# Get instance detail
curl http://localhost:8080/v1/core/instances/{instanceId} | jq
```

### Verify format:

```bash
# So sánh với format yêu cầu
curl http://localhost:8080/v1/core/instances/{instanceId} | \
  jq 'keys' | \
  grep -E "InstanceId|DisplayName|AutoStart|Solution|Input|Output|Detector|Zone"
```

## Lưu Ý

1. **PascalCase**: Tất cả field names sử dụng PascalCase (InstanceId, DisplayName, etc.)
2. **Luôn include**: Tất cả fields bắt buộc luôn được include, không bỏ qua
3. **Default values**: Các fields có default values nếu không được set
4. **Empty objects**: DetectorRegions, Tripwire, Zone luôn có structure ngay cả khi rỗng
5. **GStreamer URI**: Input URI sử dụng GStreamer format cho RTSP

## Files Đã Cập Nhật

1. ✅ `src/api/instance_handler.cpp` - `instanceInfoToJson()`
2. ✅ `src/instances/instance_storage.cpp` - `instanceInfoToConfigJson()`
3. ✅ `src/instances/instance_storage.cpp` - `configJsonToInstanceInfo()` (đã có sẵn logic parse)

## Kết Luận

✅ **Format instance detail đã được cập nhật để khớp hoàn toàn với `task/instance_detail.txt`**

- GET `/v1/core/instances/{instanceId}` trả về format đúng
- Save instance to storage sử dụng format đúng
- Load instance from storage parse đúng format
- Tương thích với hệ thống đang sử dụng format này

