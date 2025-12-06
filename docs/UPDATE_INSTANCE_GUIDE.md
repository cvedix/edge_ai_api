# Hướng Dẫn Update Instance

## Tổng Quan

API update instance hỗ trợ **2 cách** để cập nhật instance:

1. **Cách 1: Update từng field riêng lẻ** (camelCase) - Cách truyền thống
2. **Cách 2: Update trực tiếp từ JSON config** (PascalCase) - Format khớp với `instance_detail.txt`

## Cách 1: Update Từng Field (CamelCase)

### Endpoint

```
PUT /v1/core/instances/{instanceId}
```

### Request Body (camelCase)

```json
{
  "name": "New Display Name",
  "autoStart": true,
  "frameRateLimit": 20,
  "metadataMode": true,
  "detectorMode": "SmartDetection",
  "detectionSensitivity": "High",
  "additionalParams": {
    "RTSP_URL": "rtsp://new-url:8554/stream",
    "MODEL_PATH": "/path/to/model"
  }
}
```

### Các Fields Hỗ Trợ

| Field | Type | Mô tả |
|-------|------|-------|
| `name` | string | Display name |
| `group` | string | Group name |
| `persistent` | boolean | Lưu vào storage |
| `autoStart` | boolean | Tự động start khi load |
| `autoRestart` | boolean | Tự động restart khi crash |
| `frameRateLimit` | integer | Giới hạn FPS |
| `metadataMode` | boolean | Gửi metadata |
| `statisticsMode` | boolean | Chạy statistics |
| `diagnosticsMode` | boolean | Gửi diagnostics |
| `debugMode` | boolean | Enable debug |
| `detectorMode` | string | Detector preset (SmartDetection, FullRegionInference, etc.) |
| `detectionSensitivity` | string | Sensitivity (Low, Medium, High) |
| `movementSensitivity` | string | Movement sensitivity |
| `sensorModality` | string | RGB or Thermal |
| `inputOrientation` | integer | Input orientation (0-3) |
| `inputPixelLimit` | integer | Input pixel limit |
| `additionalParams` | object | Các params bổ sung (RTSP_URL, MODEL_PATH, etc.) |

## Cách 2: Update Trực Tiếp Từ JSON Config (PascalCase)

### Endpoint

```
PUT /v1/core/instances/{instanceId}
```

### Request Body (PascalCase - Format khớp với instance_detail.txt)

Bạn có thể gửi **toàn bộ hoặc một phần** của JSON config theo format PascalCase:

```json
{
  "DisplayName": "face_detection_demo_1",
  "AutoStart": true,
  "Detector": {
    "current_preset": "SmartDetection",
    "current_sensitivity_preset": "Low",
    "model_file": "pva_det_full_frame_512",
    "animal_confidence_threshold": 0.3,
    "person_confidence_threshold": 0.3,
    "vehicle_confidence_threshold": 0.3,
    "face_confidence_threshold": 0.1,
    "license_plate_confidence_threshold": 0.1,
    "conf_threshold": 0.2
  },
  "Input": {
    "media_type": "IP Camera",
    "uri": "gstreamer:///urisourcebin uri=rtsp://new-url:8554/stream ! decodebin ! videoconvert ! video/x-raw, format=NV12 ! appsink drop=true name=cvdsink"
  },
  "Output": {
    "JSONExport": {
      "enabled": false
    },
    "handlers": {
      "rtsp:--0.0.0.0:8554-stream1": {
        "config": {
          "debug": "0",
          "fps": 10
        },
        "enabled": true,
        "uri": "rtsp://new-output:8554/stream"
      }
    }
  },
  "SolutionManager": {
    "frame_rate_limit": 15,
    "send_metadata": false,
    "run_statistics": false,
    "enable_debug": false,
    "input_pixel_limit": 2000000
  },
  "PerformanceMode": {
    "current_preset": "Balanced"
  },
  "DetectorThermal": {
    "model_file": "pva_det_mosaic_320"
  },
  "Zone": {
    "Zones": {}
  },
  "Tripwire": {
    "Tripwires": {}
  },
  "DetectorRegions": {}
}
```

### Các Nested Objects Hỗ Trợ

#### Detector

```json
{
  "Detector": {
    "current_preset": "SmartDetection",
    "current_sensitivity_preset": "Low",
    "model_file": "pva_det_full_frame_512",
    "animal_confidence_threshold": 0.3,
    "person_confidence_threshold": 0.3,
    "vehicle_confidence_threshold": 0.3,
    "face_confidence_threshold": 0.1,
    "license_plate_confidence_threshold": 0.1,
    "conf_threshold": 0.2
  }
}
```

#### Input

```json
{
  "Input": {
    "media_type": "IP Camera",
    "uri": "gstreamer:///urisourcebin uri=rtsp://... ! decodebin ! videoconvert ! video/x-raw, format=NV12 ! appsink drop=true name=cvdsink"
  }
}
```

**Lưu ý**: URI có thể là:
- GStreamer format cho RTSP: `gstreamer:///urisourcebin uri=rtsp://... ! ...`
- Direct file path cho File: `/path/to/video.mp4`

#### Output

```json
{
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
          "debug": "0",
          "fps": 10,
          "pipeline": "( appsrc name=cvedia-rt ! ... )"
        },
        "enabled": true,
        "sink": "output-image",
        "uri": "rtsp://0.0.0.0:8554/stream1"
      }
    },
    "render_preset": "Default"
  }
}
```

#### SolutionManager

```json
{
  "SolutionManager": {
    "frame_rate_limit": 15,
    "send_metadata": true,
    "run_statistics": false,
    "send_diagnostics": false,
    "enable_debug": false,
    "input_pixel_limit": 2000000,
    "recommended_frame_rate": 5
  }
}
```

#### PerformanceMode

```json
{
  "PerformanceMode": {
    "current_preset": "Balanced"
  }
}
```

#### DetectorThermal

```json
{
  "DetectorThermal": {
    "model_file": "pva_det_mosaic_320"
  }
}
```

#### Zone

```json
{
  "Zone": {
    "Zones": {
      "zone-id-1": {
        // Zone config
      }
    }
  }
}
```

#### Tripwire

```json
{
  "Tripwire": {
    "Tripwires": {
      "tripwire-id-1": {
        // Tripwire config
      }
    }
  }
}
```

#### DetectorRegions

```json
{
  "DetectorRegions": {
    "region-id-1": {
      // Region config
    }
  }
}
```

## Ví Dụ Update

### Ví Dụ 1: Update DisplayName và AutoStart

```bash
curl -X PUT http://localhost:8080/v1/core/instances/b9bfa916-34c5-422c-9d7d-3391b4cab853 \
  -H "Content-Type: application/json" \
  -d '{
    "DisplayName": "Updated Camera Name",
    "AutoStart": false
  }'
```

### Ví Dụ 2: Update Detector Settings

```bash
curl -X PUT http://localhost:8080/v1/core/instances/b9bfa916-34c5-422c-9d7d-3391b4cab853 \
  -H "Content-Type: application/json" \
  -d '{
    "Detector": {
      "current_preset": "SmartDetection",
      "current_sensitivity_preset": "High",
      "person_confidence_threshold": 0.5
    }
  }'
```

### Ví Dụ 3: Update Input URI

```bash
curl -X PUT http://localhost:8080/v1/core/instances/b9bfa916-34c5-422c-9d7d-3391b4cab853 \
  -H "Content-Type: application/json" \
  -d '{
    "Input": {
      "uri": "gstreamer:///urisourcebin uri=rtsp://new-camera:8554/stream ! decodebin ! videoconvert ! video/x-raw, format=NV12 ! appsink drop=true name=cvdsink"
    }
  }'
```

### Ví Dụ 4: Update Output Settings

```bash
curl -X PUT http://localhost:8080/v1/core/instances/b9bfa916-34c5-422c-9d7d-3391b4cab853 \
  -H "Content-Type: application/json" \
  -d '{
    "Output": {
      "JSONExport": {
        "enabled": true
      },
      "handlers": {
        "rtsp:--0.0.0.0:8554-stream1": {
          "config": {
            "fps": 15
          }
        }
      }
    }
  }'
```

### Ví Dụ 5: Update Multiple Fields

```bash
curl -X PUT http://localhost:8080/v1/core/instances/b9bfa916-34c5-422c-9d7d-3391b4cab853 \
  -H "Content-Type: application/json" \
  -d '{
    "DisplayName": "Updated Camera",
    "AutoStart": true,
    "Detector": {
      "current_preset": "SmartDetection",
      "current_sensitivity_preset": "High"
    },
    "SolutionManager": {
      "frame_rate_limit": 20,
      "send_metadata": true
    },
    "PerformanceMode": {
      "current_preset": "HighPerformance"
    }
  }'
```

## Cách Hoạt Động

### Auto-Detection Format

API tự động phát hiện format của request:

- **Nếu có fields PascalCase** (`InstanceId`, `DisplayName`, `Detector`, etc.) → Sử dụng **Cách 2** (Direct Config Update)
- **Nếu chỉ có fields camelCase** (`name`, `autoStart`, etc.) → Sử dụng **Cách 1** (Traditional Update)

### Merge Logic

Khi update:

1. **Load existing config** từ storage
2. **Merge** JSON update vào existing config
3. **Preserve** các fields phức tạp (Zone, Tripwire, TensorRT models, etc.) nếu không có trong update
4. **Convert** merged config về InstanceInfo
5. **Update** instance trong registry
6. **Save** lại vào storage
7. **Restart** instance nếu đang chạy để apply changes

### Preserved Fields

Các fields sau được **preserve** (giữ nguyên) nếu không có trong update:

- `Zone` - Zone configurations
- `Tripwire` - Tripwire configurations
- `DetectorRegions` - Detector regions
- TensorRT model IDs (UUID-like keys)
- `AnimalTracker`, `PersonTracker`, `VehicleTracker`, etc.

## Response

### Success Response (200 OK)

```json
{
  "InstanceId": "b9bfa916-34c5-422c-9d7d-3391b4cab853",
  "DisplayName": "Updated Camera",
  "AutoStart": true,
  "Solution": "face_detection_rtmp",
  "Input": {
    ...
  },
  "Detector": {
    ...
  },
  ...
  "message": "Instance updated successfully"
}
```

### Error Responses

#### 400 Bad Request

```json
{
  "error": "Invalid request",
  "message": "Request body must be valid JSON"
}
```

#### 404 Not Found

```json
{
  "error": "Not found",
  "message": "Instance not found"
}
```

#### 500 Internal Server Error

```json
{
  "error": "Internal server error",
  "message": "Failed to update instance"
}
```

## Lưu Ý

1. **Instance Restart**: Nếu instance đang chạy, nó sẽ được **tự động restart** sau khi update để apply changes.

2. **Partial Update**: Bạn chỉ cần gửi các fields muốn update, không cần gửi toàn bộ config.

3. **Format Flexibility**: Có thể mix cả 2 cách trong cùng một request (một số fields PascalCase, một số camelCase).

4. **Read-Only Instances**: Không thể update read-only instances.

5. **Validation**: Các fields sẽ được validate trước khi update.

6. **Storage**: Chỉ persistent instances mới được lưu vào storage.

## Best Practices

1. **Update từng field nhỏ**: Update từng field một để dễ debug và rollback.

2. **Verify sau khi update**: Luôn GET instance detail sau khi update để verify.

3. **Backup trước khi update**: Nếu cần, backup instance config trước khi update.

4. **Test với instance không chạy**: Test update với instance đã stop trước để tránh ảnh hưởng đến production.

5. **Sử dụng format PascalCase**: Nếu có thể, sử dụng format PascalCase để khớp với format của hệ thống đang sử dụng.

