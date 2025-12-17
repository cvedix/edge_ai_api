# Unit Tests - Edge AI API

## Tổng quan

Bộ unit tests cho Edge AI API sử dụng Google Test framework. Tests được thiết kế để chạy tự động sau mỗi lần build hoặc merge code để đảm bảo không có regression.

**Tổng số tests:** 15 tests từ 5 test suites
- HealthHandler: 3 tests
- VersionHandler: 4 tests
- SwaggerHandler: 6 tests
- WatchdogHandler: 1 test
- EndpointsHandler: 1 test

## Cấu trúc Tests

- `test_main.cpp` - Entry point cho tests
- `test_health_handler.cpp` - Tests cho HealthHandler
- `test_version_handler.cpp` - Tests cho VersionHandler
- `test_swagger_handler.cpp` - Tests cho SwaggerHandler (bao gồm validation, security)
- `test_watchdog_handler.cpp` - Tests cho WatchdogHandler
- `test_endpoints_handler.cpp` - Tests cho EndpointsHandler

## Build và Chạy Tests

### Build tests

```bash
cd build
cmake .. -DBUILD_TESTS=ON
make -j$(nproc)
```

**Lưu ý:** Test executable sẽ được tạo tại `build/bin/edge_ai_api_tests` (không phải `build/tests/`)

### Chạy tests

**Cách 1: Sử dụng script (khuyến nghị)**
```bash
# Từ project root
./scripts/run_tests.sh

# Hoặc chỉ định build directory
./scripts/run_tests.sh build
```

**Cách 2: Chạy trực tiếp từ build/bin**
```bash
cd build
./bin/edge_ai_api_tests
```

**Cách 3: Từ project root**
```bash
./build/bin/edge_ai_api_tests
```

**Cách 4: Sử dụng CTest**
```bash
cd build
ctest
```

## Test Coverage

### HealthHandler Tests
- ✅ Returns valid JSON
- ✅ Status values validation
- ✅ Timestamp format validation
- ✅ Required fields presence

### VersionHandler Tests
- ✅ Returns valid JSON
- ✅ Field types validation
- ✅ Service name validation
- ✅ API version format validation

### SwaggerHandler Tests
- ✅ Version format validation (v1, v2, etc.)
- ✅ Path sanitization (prevent path traversal)
- ✅ Swagger UI endpoint
- ✅ Version-specific Swagger UI
- ✅ Invalid version format handling
- ✅ Extract version from path via requests

### WatchdogHandler Tests
- ✅ Returns valid JSON
- ✅ Required fields presence

### EndpointsHandler Tests
- ✅ Returns valid JSON
- ✅ Endpoints array structure
- ✅ Total endpoints count

## Best Practices

1. **Mỗi API handler có file test riêng** - Dễ maintain và tìm kiếm
2. **Tests độc lập** - Mỗi test không phụ thuộc vào test khác
3. **Cleanup trong TearDown** - Đảm bảo không có side effects
4. **Test cả success và failure cases** - Đảm bảo error handling đúng

## Thêm Tests Mới

Khi thêm API handler mới:

1. Tạo file `test_<handler_name>.cpp` trong thư mục `tests/`
2. Thêm file vào `tests/CMakeLists.txt` trong `TEST_SOURCES`
3. Viết tests cho:
   - Valid requests
   - Invalid requests
   - Edge cases
   - Error handling

## CI/CD Integration

Tests có thể được tích hợp vào CI/CD pipeline:

```yaml
# Example GitHub Actions
- name: Build and Test
  run: |
    mkdir build && cd build
    cmake .. -DBUILD_TESTS=ON
    make -j$(nproc)
    ./bin/edge_ai_api_tests
```

Hoặc sử dụng script:
```yaml
- name: Build and Test
  run: |
    mkdir build && cd build
    cmake .. -DBUILD_TESTS=ON
    make -j$(nproc)
    cd ..
    ./scripts/run_tests.sh build
```

## Troubleshooting

### Tests không compile
- Kiểm tra Google Test đã được download (sẽ tự động qua FetchContent)
- Kiểm tra Drogon đã được build
- Kiểm tra jsoncpp dependencies
- Đảm bảo đã chạy `cmake .. -DBUILD_TESTS=ON` trước khi make

### Tests fail
- Kiểm tra log output để xem test nào fail
- Đảm bảo các dependencies (watchdog, health monitor) được setup đúng trong SetUp()
- Xem chi tiết lỗi trong test output

### Không tìm thấy test executable
- Test executable nằm tại `build/bin/edge_ai_api_tests` (không phải `build/tests/`)
- Sử dụng script `./scripts/run_tests.sh` để tự động tìm đúng vị trí
- Hoặc chạy trực tiếp: `./build/bin/edge_ai_api_tests`
