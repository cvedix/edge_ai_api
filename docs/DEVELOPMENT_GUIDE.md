# Hướng Dẫn Phát Triển - Edge AI API

Tài liệu này hướng dẫn cách phát triển các API features mới theo codebase đã triển khai, bao gồm code patterns, unit tests, Swagger documentation, và best practices.

## 📋 Mục Lục

1. [Cấu Trúc Code Base](#cấu-trúc-code-base)
2. [Tạo API Handler Mới](#tạo-api-handler-mới)
3. [Viết Unit Tests](#viết-unit-tests)
4. [Cập Nhật Swagger/OpenAPI](#cập-nhật-swaggeropenapi)
5. [Best Practices](#best-practices)
6. [Ví Dụ Hoàn Chỉnh](#ví-dụ-hoàn-chỉnh)

## 🏗️ Cấu Trúc Code Base

### Tổng Quan

Project sử dụng **Drogon Framework** (C++ HTTP framework) với cấu trúc:

```
edge_ai_api/
├── src/                    # Source code implementation
│   ├── main.cpp           # Entry point, khởi tạo server và handlers
│   ├── api/               # API handlers (business logic)
│   └── core/              # Core components (watchdog, health monitor, etc.)
├── include/               # Header files
│   ├── api/               # Handler headers
│   └── core/              # Core component headers
├── tests/                 # Unit tests
├── docs/                  # Documentation
├── openapi.yaml           # OpenAPI specification
└── CMakeLists.txt         # Build configuration
```

### Pattern: HttpController

Tất cả API handlers kế thừa từ `drogon::HttpController`:

```cpp
class MyHandler : public drogon::HttpController<MyHandler> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(MyHandler::myMethod, "/v1/path/to/endpoint", Get);
    METHOD_LIST_END
    
    void myMethod(const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback);
};
```

## 🆕 Tạo API Handler Mới

### Bước 1: Tạo Header File

Tạo file `include/api/my_handler.h`:

```cpp
#pragma once

#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <json/json.h>

using namespace drogon;

/**
 * @brief My feature handler
 * 
 * Endpoints:
 * - GET /v1/my/feature - Get feature data
 * - POST /v1/my/feature - Create feature
 */
class MyHandler : public drogon::HttpController<MyHandler> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(MyHandler::getFeature, "/v1/my/feature", Get);
        ADD_METHOD_TO(MyHandler::createFeature, "/v1/my/feature", Post);
    METHOD_LIST_END

    /**
     * @brief Handle GET /v1/my/feature
     */
    void getFeature(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback);

    /**
     * @brief Handle POST /v1/my/feature
     */
    void createFeature(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback);

private:
    // Helper methods nếu cần
    Json::Value buildResponse(const std::string& data);
};
```

### Bước 2: Implement Handler

Tạo file `src/api/my_handler.cpp`:

```cpp
#include "api/my_handler.h"
#include <drogon/HttpResponse.h>
#include <json/json.h>

void MyHandler::getFeature(const HttpRequestPtr &req,
                          std::function<void(const HttpResponsePtr &)> &&callback)
{
    try {
        // 1. Parse request parameters
        auto id = req->getParameter("id");
        
        // 2. Validate input
        if (id.empty()) {
            Json::Value errorResponse;
            errorResponse["error"] = "Missing parameter: id";
            auto resp = HttpResponse::newHttpJsonResponse(errorResponse);
            resp->setStatusCode(k400BadRequest);
            callback(resp);
            return;
        }
        
        // 3. Business logic
        Json::Value response;
        response["id"] = id;
        response["data"] = "feature data";
        response["timestamp"] = "2024-01-01T00:00:00.000Z";
        
        // 4. Create response
        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k200OK);
        
        // 5. Add CORS headers (nếu cần)
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
        
        // 6. Call callback
        callback(resp);
        
    } catch (const std::exception& e) {
        // Error handling
        Json::Value errorResponse;
        errorResponse["error"] = "Internal server error";
        errorResponse["message"] = e.what();
        
        auto resp = HttpResponse::newHttpJsonResponse(errorResponse);
        resp->setStatusCode(k500InternalServerError);
        callback(resp);
    }
}

void MyHandler::createFeature(const HttpRequestPtr &req,
                              std::function<void(const HttpResponsePtr &)> &&callback)
{
    try {
        // 1. Parse JSON body
        auto json = req->getJsonObject();
        if (!json) {
            Json::Value errorResponse;
            errorResponse["error"] = "Invalid JSON body";
            auto resp = HttpResponse::newHttpJsonResponse(errorResponse);
            resp->setStatusCode(k400BadRequest);
            callback(resp);
            return;
        }
        
        // 2. Validate required fields
        if (!json->isMember("name") || (*json)["name"].asString().empty()) {
            Json::Value errorResponse;
            errorResponse["error"] = "Missing required field: name";
            auto resp = HttpResponse::newHttpJsonResponse(errorResponse);
            resp->setStatusCode(k400BadRequest);
            callback(resp);
            return;
        }
        
        // 3. Business logic
        std::string name = (*json)["name"].asString();
        
        // 4. Build response
        Json::Value response;
        response["id"] = "generated-id";
        response["name"] = name;
        response["created_at"] = "2024-01-01T00:00:00.000Z";
        
        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k201Created);
        
        callback(resp);
        
    } catch (const std::exception& e) {
        Json::Value errorResponse;
        errorResponse["error"] = "Internal server error";
        errorResponse["message"] = e.what();
        
        auto resp = HttpResponse::newHttpJsonResponse(errorResponse);
        resp->setStatusCode(k500InternalServerError);
        callback(resp);
    }
}
```

### Bước 3: Đăng Ký Handler trong main.cpp

Thêm vào `src/main.cpp`:

```cpp
// 1. Include header
#include "api/my_handler.h"

// 2. Trong hàm main(), tạo instance để đăng ký
static MyHandler myHandler;
```

**Lưu ý:** Drogon tự động đăng ký handlers khi tạo instance, không cần gọi thêm hàm nào.

### Bước 4: Thêm vào CMakeLists.txt

Thêm source files vào `CMakeLists.txt`:

```cmake
set(SOURCES
    # ... existing files ...
    src/api/my_handler.cpp
)
```

### Bước 5: Build và Test

```bash
cd build
cmake ..
make -j$(nproc)
./edge_ai_api
```

Test API:
```bash
curl http://localhost:8080/v1/my/feature?id=123
```

## 🧪 Viết Unit Tests

### Cấu Trúc Test File

Tạo file `tests/test_my_handler.cpp`:

```cpp
#include <gtest/gtest.h>
#include "api/my_handler.h"
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <json/json.h>
#include <thread>
#include <chrono>

using namespace drogon;

class MyHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        handler_ = std::make_unique<MyHandler>();
    }

    void TearDown() override {
        handler_.reset();
    }

    std::unique_ptr<MyHandler> handler_;
};

// Test GET endpoint returns valid JSON
TEST_F(MyHandlerTest, GetFeatureReturnsValidJson) {
    bool callbackCalled = false;
    HttpResponsePtr response;
    
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/my/feature");
    req->setMethod(Get);
    req->setParameter("id", "123");
    
    handler_->getFeature(req, [&](const HttpResponsePtr &resp) {
        callbackCalled = true;
        response = resp;
    });
    
    // Wait for async callback
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    ASSERT_TRUE(callbackCalled);
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->statusCode(), k200OK);
    EXPECT_EQ(response->contentType(), CT_APPLICATION_JSON);
    
    // Parse and validate JSON
    auto json = response->getJsonObject();
    ASSERT_NE(json, nullptr);
    EXPECT_TRUE(json->isMember("id"));
    EXPECT_TRUE(json->isMember("data"));
    EXPECT_EQ((*json)["id"].asString(), "123");
}

// Test GET endpoint with missing parameter
TEST_F(MyHandlerTest, GetFeatureMissingParameter) {
    bool callbackCalled = false;
    HttpResponsePtr response;
    
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/my/feature");
    req->setMethod(Get);
    // Không set parameter "id"
    
    handler_->getFeature(req, [&](const HttpResponsePtr &resp) {
        callbackCalled = true;
        response = resp;
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    ASSERT_TRUE(callbackCalled);
    EXPECT_EQ(response->statusCode(), k400BadRequest);
    
    auto json = response->getJsonObject();
    ASSERT_NE(json, nullptr);
    EXPECT_TRUE(json->isMember("error"));
}

// Test POST endpoint creates feature
TEST_F(MyHandlerTest, CreateFeatureSuccess) {
    bool callbackCalled = false;
    HttpResponsePtr response;
    
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/my/feature");
    req->setMethod(Post);
    
    // Set JSON body
    Json::Value body;
    body["name"] = "Test Feature";
    req->setBody(body.toStyledString());
    
    handler_->createFeature(req, [&](const HttpResponsePtr &resp) {
        callbackCalled = true;
        response = resp;
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    ASSERT_TRUE(callbackCalled);
    EXPECT_EQ(response->statusCode(), k201Created);
    
    auto json = response->getJsonObject();
    ASSERT_NE(json, nullptr);
    EXPECT_TRUE(json->isMember("id"));
    EXPECT_TRUE(json->isMember("name"));
    EXPECT_EQ((*json)["name"].asString(), "Test Feature");
}

// Test POST endpoint with invalid JSON
TEST_F(MyHandlerTest, CreateFeatureInvalidJson) {
    bool callbackCalled = false;
    HttpResponsePtr response;
    
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/my/feature");
    req->setMethod(Post);
    req->setBody("invalid json");
    
    handler_->createFeature(req, [&](const HttpResponsePtr &resp) {
        callbackCalled = true;
        response = resp;
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    ASSERT_TRUE(callbackCalled);
    EXPECT_EQ(response->statusCode(), k400BadRequest);
}
```

### Đăng Ký Test File

Thêm vào `tests/CMakeLists.txt`:

```cmake
set(TEST_SOURCES
    # ... existing files ...
    test_my_handler.cpp
)
```

Và thêm source file vào compilation:

```cmake
target_sources(edge_ai_api_tests PRIVATE
    # ... existing files ...
    ${CMAKE_SOURCE_DIR}/src/api/my_handler.cpp
)
```

### Chạy Tests

```bash
cd build
cmake .. -DBUILD_TESTS=ON
make -j$(nproc)
./bin/edge_ai_api_tests

# Hoặc chạy test cụ thể
./bin/edge_ai_api_tests --gtest_filter=MyHandlerTest.*
```

## 📝 Cập Nhật Swagger/OpenAPI

### Cấu Trúc OpenAPI Spec

File `openapi.yaml` định nghĩa API specification. Thêm endpoint mới:

```yaml
paths:
  /v1/my/feature:
    get:
      summary: Get feature data
      description: Returns feature data by ID
      operationId: getFeature
      tags:
        - My Feature
      parameters:
        - name: id
          in: query
          required: true
          schema:
            type: string
          description: Feature ID
      responses:
        '200':
          description: Success
          content:
            application/json:
              schema:
                $ref: '#/components/schemas/FeatureResponse'
        '400':
          description: Bad request
          content:
            application/json:
              schema:
                $ref: '#/components/schemas/ErrorResponse'
        '500':
          description: Internal server error
          content:
            application/json:
              schema:
                $ref: '#/components/schemas/ErrorResponse'
    
    post:
      summary: Create new feature
      description: Creates a new feature
      operationId: createFeature
      tags:
        - My Feature
      requestBody:
        required: true
        content:
          application/json:
            schema:
              $ref: '#/components/schemas/CreateFeatureRequest'
      responses:
        '201':
          description: Feature created
          content:
            application/json:
              schema:
                $ref: '#/components/schemas/FeatureResponse'
        '400':
          description: Bad request
          content:
            application/json:
              schema:
                $ref: '#/components/schemas/ErrorResponse'

components:
  schemas:
    FeatureResponse:
      type: object
      properties:
        id:
          type: string
        data:
          type: string
        timestamp:
          type: string
          format: date-time
      required:
        - id
        - data
    
    CreateFeatureRequest:
      type: object
      properties:
        name:
          type: string
      required:
        - name
    
    ErrorResponse:
      type: object
      properties:
        error:
          type: string
        message:
          type: string
```

### Swagger Handler

Swagger UI tự động được tạo từ `openapi.yaml`. Không cần code thêm, chỉ cần:
1. Cập nhật `openapi.yaml`
2. Restart server
3. Truy cập `http://localhost:8080/swagger` để xem

## ✅ Best Practices

### 1. Error Handling

Luôn sử dụng try-catch và trả về error response phù hợp:

```cpp
try {
    // Business logic
} catch (const std::exception& e) {
    Json::Value errorResponse;
    errorResponse["error"] = "Internal server error";
    errorResponse["message"] = e.what();
    
    auto resp = HttpResponse::newHttpJsonResponse(errorResponse);
    resp->setStatusCode(k500InternalServerError);
    callback(resp);
}
```

### 2. Input Validation

Luôn validate input trước khi xử lý:

```cpp
// Validate required parameters
if (id.empty()) {
    // Return 400 Bad Request
}

// Validate JSON body
auto json = req->getJsonObject();
if (!json || !json->isMember("required_field")) {
    // Return 400 Bad Request
}
```

### 3. Response Format

Sử dụng format JSON nhất quán:

```cpp
Json::Value response;
response["field1"] = value1;
response["field2"] = value2;
response["timestamp"] = getCurrentTimestamp();

auto resp = HttpResponse::newHttpJsonResponse(response);
resp->setStatusCode(k200OK);
```

### 4. HTTP Status Codes

Sử dụng đúng status codes:
- `200 OK`: Success
- `201 Created`: Resource created
- `400 Bad Request`: Invalid input
- `404 Not Found`: Resource not found
- `500 Internal Server Error`: Server error
- `503 Service Unavailable`: Service unavailable

### 5. CORS Headers

Thêm CORS headers nếu cần:

```cpp
resp->addHeader("Access-Control-Allow-Origin", "*");
resp->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
```

### 6. Code Organization

- **Header files** (`include/api/`): Định nghĩa class và methods
- **Source files** (`src/api/`): Implementation
- **Tests** (`tests/`): Unit tests cho mỗi handler
- **Documentation**: Cập nhật `openapi.yaml` và docs

### 7. Naming Conventions

- **Handlers**: `XxxHandler` (PascalCase)
- **Files**: `xxx_handler.h`, `xxx_handler.cpp` (snake_case)
- **Endpoints**: `/v1/xxx/yyy` (lowercase, kebab-case)
- **Methods**: `getXxx`, `createXxx` (camelCase)

### 8. Testing

- Viết tests cho mọi handler
- Test cả success và failure cases
- Test edge cases (empty input, invalid format, etc.)
- Đảm bảo tests độc lập (không phụ thuộc vào nhau)

## 📖 Ví Dụ Hoàn Chỉnh

Xem các handlers hiện có để tham khảo:

1. **HealthHandler** (`include/api/health_handler.h`, `src/api/health_handler.cpp`)
   - Simple GET endpoint
   - JSON response
   - Error handling

2. **VersionHandler** (`include/api/version_handler.h`, `src/api/version_handler.cpp`)
   - GET endpoint với compile-time constants
   - CORS headers

3. **WatchdogHandler** (`include/api/watchdog_handler.h`, `src/api/watchdog_handler.cpp`)
   - Handler với dependencies (watchdog, health monitor)
   - Static methods để set dependencies

### Test Examples

Xem các test files:
- `tests/test_health_handler.cpp` - Basic GET endpoint tests
- `tests/test_version_handler.cpp` - Response validation tests
- `tests/test_swagger_handler.cpp` - Path validation và security tests

## 🔄 Workflow Phát Triển Feature Mới

1. **Tạo Handler**
   - Tạo `include/api/xxx_handler.h`
   - Tạo `src/api/xxx_handler.cpp`
   - Implement business logic

2. **Đăng Ký Handler**
   - Include header trong `src/main.cpp`
   - Tạo instance trong `main()`
   - Thêm source vào `CMakeLists.txt`

3. **Viết Tests**
   - Tạo `tests/test_xxx_handler.cpp`
   - Viết tests cho success và failure cases
   - Thêm vào `tests/CMakeLists.txt`

4. **Cập Nhật OpenAPI**
   - Thêm endpoint vào `openapi.yaml`
   - Định nghĩa schemas
   - Test trên Swagger UI

5. **Build và Test**
   ```bash
   cd build
   cmake .. -DBUILD_TESTS=ON
   make -j$(nproc)
   ./bin/edge_ai_api_tests  # Run tests
   ./edge_ai_api             # Start server
   ```

6. **Verify**
   - Test API với curl/Postman
   - Verify Swagger UI
   - Check logs

## 📚 Tài Liệu Liên Quan

- [Setup Môi Trường Phát Triển](DEVELOPMENT_SETUP.md)
- [Hướng Dẫn Khởi Động](GETTING_STARTED.md)
- [Drogon Framework Documentation](https://drogon.docsforge.com/)
- [Google Test Documentation](https://google.github.io/googletest/)
- [OpenAPI Specification](https://swagger.io/specification/)

## 🆘 Troubleshooting

### Handler không được đăng ký

- Đảm bảo đã include header trong `main.cpp`
- Đảm bảo đã tạo instance trong `main()`
- Kiểm tra `METHOD_LIST_BEGIN/END` đúng format
- Rebuild project: `rm -rf build && mkdir build && cd build && cmake .. && make`

### Tests không compile

- Đảm bảo đã thêm test file vào `tests/CMakeLists.txt`
- Đảm bảo đã thêm source file vào `target_sources`
- Rebuild với `-DBUILD_TESTS=ON`

### Swagger không hiển thị endpoint

- Kiểm tra `openapi.yaml` syntax đúng
- Restart server sau khi sửa `openapi.yaml`
- Kiểm tra endpoint path trong `openapi.yaml` khớp với code

