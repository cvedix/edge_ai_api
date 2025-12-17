<!-- 3b38908d-ebc2-41be-b40a-2a568ce9bc87 97c43c81-fc38-47ef-8978-59b145c542ed -->
# Lines Management API Implementation Plan

## Overview

Implement 4 API endpoints for managing crossing lines in ba_crossline instances:

- GET `/v1/core/instance/{instanceId}/lines` - Get all lines
- POST `/v1/core/instance/{instanceId}/lines` - Create a new line
- DELETE `/v1/core/instance/{instanceId}/lines` - Delete all lines
- DELETE `/v1/core/instance/{instanceId}/lines/{lineId}` - Delete a specific line

## Design Decisions (Based on Project Analysis)

1. **URL Path**: `/v1/core/instance/{instanceId}/lines` (consistent with existing pattern)
2. **Storage**: Lines stored in instance config JSON under `AdditionalParams["CrossingLines"]` as JSON array
3. **Line ID**: Auto-generated UUID using `UUIDGenerator::generateUUID()`
4. **Pipeline Update**: Save config only, require manual restart (safer, avoids runtime errors)
5. **Metadata**: Store `classes`, `direction`, `color` as metadata in config (for future use)
6. **Multiple Lines**: Support multiple lines per instance (array-based storage)

## Implementation Steps

### 1. Add Lines Storage Structure

- Store lines in `InstanceInfo.additionalParams["CrossingLines"]` as JSON string
- Format: `[{"id": "uuid", "name": "string", "coordinates": [...], "classes": [...], "direction": "...", "color": [...]}, ...]`
- Each line has: id, name (optional), coordinates (array of {x, y}), classes, direction, color

### 2. Create Lines Handler

- **File**: `include/api/lines_handler.h` (new)
- **File**: `src/api/lines_handler.cpp` (new)
- Extend `InstanceHandler` or create separate handler
- Use Drogon `HttpController` pattern like other handlers

### 3. Implement GET /v1/core/instance/{instanceId}/lines

- Extract instanceId from path
- Load instance config from `InstanceRegistry`
- Parse `CrossingLines` from `additionalParams`
- Return JSON array of lines with coordinates
- Handle 404 if instance not found

### 4. Implement POST /v1/core/instance/{instanceId}/lines

- Validate request body (coordinates array with at least 2 points)
- Generate UUID for line ID
- Create line object with: id, name, coordinates, classes, direction, color
- Load existing lines from config
- Append new line to array
- Save to instance config via `InstanceRegistry::updateInstanceFromConfig()`
- Return created line with ID
- Handle validation errors (400)

### 5. Implement DELETE /v1/core/instance/{instanceId}/lines

- Load instance config
- Clear `CrossingLines` from `additionalParams`
- Save updated config
- Return success response

### 6. Implement DELETE /v1/core/instance/{instanceId}/lines/{lineId}

- Extract instanceId and lineId from path
- Load existing lines from config
- Find and remove line with matching ID
- Save updated config
- Return 404 if line not found

### 7. Register Routes in main.cpp

- Include `lines_handler.h`
- Create static instance to auto-register routes
- Follow pattern from other handlers

### 8. Update Pipeline Builder Integration

- Modify `createBACrosslineNode()` in `pipeline_builder.cpp`
- Read lines from `AdditionalParams["CrossingLines"]` if available
- Convert stored lines format to `std::map<int, cvedix_line>` format
- Support multiple lines with different channels
- Fallback to default line if no lines configured

### 9. Add Validation

- Validate coordinates: at least 2 points, numeric x/y values
- Validate direction: "Up", "Down", "Both" (case-insensitive)
- Validate classes: array of strings from allowed values
- Validate color: array of 4 numbers (RGBA)

### 10. Update OpenAPI Documentation

- Add endpoints to `openapi.yaml`
- Document request/response schemas
- Include examples

## Files to Create/Modify

**New Files:**

- `include/api/lines_handler.h`
- `src/api/lines_handler.cpp`

**Modified Files:**

- `src/main.cpp` - Register lines handler
- `src/core/pipeline_builder.cpp` - Read lines from config
- `openapi.yaml` - Add API documentation

## Data Flow

```
API Request → LinesHandler → InstanceRegistry → InstanceStorage
                                                      ↓
                                            Update Instance Config
                                                      ↓
                                            Save to JSON File
                                                      ↓
                                    (Manual Restart Required)
                                                      ↓
                                    Pipeline Rebuild → Read Lines from Config
                                                      ↓
                                    createBACrosslineNode() → Apply Lines
```

## Error Handling

- 400: Invalid request body (missing coordinates, invalid format)
- 404: Instance not found, Line not found
- 500: Internal server error (config save failed, etc.)

### To-dos

- [ ] Create include/api/lines_handler.h with HttpController class and method declarations
- [ ] Implement src/api/lines_handler.cpp with all 4 endpoints (GET, POST, DELETE all, DELETE one)
- [ ] Register lines handler routes in src/main.cpp following existing handler pattern
- [ ] Modify createBACrosslineNode() in src/core/pipeline_builder.cpp to read lines from AdditionalParams[CrossingLines]
- [ ] Add validation for coordinates, direction, classes, and color in POST endpoint
- [ ] Add lines API endpoints documentation to openapi.yaml