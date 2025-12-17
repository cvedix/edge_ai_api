# Hướng Dẫn Phát Triển - Edge AI API

Tài liệu này bao gồm setup môi trường, hướng dẫn phát triển API, và pre-commit hooks.

## 📋 Mục Lục

1. [Setup Môi Trường](#setup-môi-trường)
2. [Build Project](#build-project)
3. [Tạo API Handler Mới](#tạo-api-handler-mới)
4. [Viết Unit Tests](#viết-unit-tests)
5. [Cập Nhật Swagger/OpenAPI](#cập-nhật-swaggeropenapi)
6. [Pre-commit Hooks](#pre-commit-hooks)
7. [Best Practices](#best-practices)
8. [Troubleshooting](#troubleshooting)

---

## 🚀 Setup Môi Trường

### Setup Tự Động (Khuyến Nghị)

```bash
# Clone project
git clone https://github.com/cvedix/edge_ai_api.git
cd edge_ai_api

# Development setup
./setup.sh

# Production setup
sudo ./setup.sh --production
```

### Yêu Cầu Hệ Thống

- **OS**: Ubuntu 20.04+ / Debian 10+
- **CMake**: 3.14+
- **Dependencies**: build-essential, libssl-dev, zlib1g-dev, libjsoncpp-dev, uuid-dev

### Cài Dependencies Thủ Công

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y \
    build-essential cmake git pkg-config \
    libssl-dev zlib1g-dev libjsoncpp-dev uuid-dev
```

---

## 🏗️ Build Project

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Build với Tests

```bash
cmake .. -DBUILD_TESTS=ON
make -j$(nproc)
./bin/edge_ai_api_tests
```

### Chạy Server

```bash
# Development
./scripts/load_env.sh

# Hoặc trực tiếp
./build/bin/edge_ai_api
```

---

## 🆕 Tạo API Handler Mới

### Bước 1: Tạo Header File

Tạo `include/api/my_handler.h`:

```cpp
#pragma once

#include <drogon/HttpController.h>
#include <json/json.h>

using namespace drogon;

class MyHandler : public drogon::HttpController<MyHandler> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(MyHandler::getFeature, "/v1/my/feature", Get);
        ADD_METHOD_TO(MyHandler::createFeature, "/v1/my/feature", Post);
    METHOD_LIST_END

    void getFeature(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback);
    void createFeature(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback);
};
```

### Bước 2: Implement Handler

Tạo `src/api/my_handler.cpp`:

```cpp
#include "api/my_handler.h"

void MyHandler::getFeature(const HttpRequestPtr &req,
                          std::function<void(const HttpResponsePtr &)> &&callback)
{
    try {
        auto id = req->getParameter("id");
        if (id.empty()) {
            Json::Value error;
            error["error"] = "Missing parameter: id";
            auto resp = HttpResponse::newHttpJsonResponse(error);
            resp->setStatusCode(k400BadRequest);
            callback(resp);
            return;
        }

        Json::Value response;
        response["id"] = id;
        response["data"] = "feature data";

        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k200OK);
        resp->addHeader("Access-Control-Allow-Origin", "*");
        callback(resp);

    } catch (const std::exception& e) {
        Json::Value error;
        error["error"] = e.what();
        auto resp = HttpResponse::newHttpJsonResponse(error);
        resp->setStatusCode(k500InternalServerError);
        callback(resp);
    }
}
```

### Bước 3: Đăng Ký trong main.cpp

```cpp
#include "api/my_handler.h"

// Trong main()
static MyHandler myHandler;
```

### Bước 4: Thêm vào CMakeLists.txt

```cmake
set(SOURCES
    # ... existing files ...
    src/api/my_handler.cpp
)
```

---

## 🧪 Viết Unit Tests

Tạo `tests/test_my_handler.cpp`:

```cpp
#include <gtest/gtest.h>
#include "api/my_handler.h"
#include <drogon/HttpRequest.h>
#include <thread>
#include <chrono>

using namespace drogon;

class MyHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        handler_ = std::make_unique<MyHandler>();
    }
    std::unique_ptr<MyHandler> handler_;
};

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

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_TRUE(callbackCalled);
    EXPECT_EQ(response->statusCode(), k200OK);

    auto json = response->getJsonObject();
    ASSERT_NE(json, nullptr);
    EXPECT_EQ((*json)["id"].asString(), "123");
}
```

### Chạy Tests

```bash
cd build
./bin/edge_ai_api_tests --gtest_filter=MyHandlerTest.*
```

---

## 📝 Cập Nhật Swagger/OpenAPI

Thêm vào `openapi.yaml`:

```yaml
paths:
  /v1/my/feature:
    get:
      summary: Get feature data
      operationId: getFeature
      tags:
        - My Feature
      parameters:
        - name: id
          in: query
          required: true
          schema:
            type: string
      responses:
        '200':
          description: Success
          content:
            application/json:
              schema:
                $ref: '#/components/schemas/FeatureResponse'
```

Truy cập `http://localhost:8080/swagger` để xem.

---

## 🔧 Pre-commit Hooks

### Cài đặt

```bash
# Cài pre-commit
pip install pre-commit
# hoặc: pipx install pre-commit
# hoặc: sudo apt install pre-commit

# Cài hooks
pre-commit install
pre-commit install --hook-type pre-push
```

### Hooks được cấu hình

| Hook | Khi nào | Mục đích |
|------|---------|----------|
| `trailing-whitespace` | commit | Xóa whitespace cuối dòng |
| `end-of-file-fixer` | commit | File kết thúc bằng newline |
| `check-yaml` | commit | Validate YAML |
| `check-json` | commit | Validate JSON |
| `check-added-large-files` | commit | Cảnh báo file > 1MB |
| `check-merge-conflict` | commit | Phát hiện conflict markers |
| `mixed-line-ending` | commit | Đảm bảo dùng LF |
| `clang-format` | commit | Format C/C++ |
| `shellcheck` | commit | Lint shell scripts |
| `run-tests` | push | Build và chạy tests |

### Quy trình làm việc

```bash
# Commit - tự động format và validate
git add .
git commit -m "feat: add feature"

# Push - tự động build và test
git push
```

### Lệnh hữu ích

```bash
# Chạy tất cả hooks
pre-commit run --all-files

# Chạy hook cụ thể
pre-commit run clang-format --all-files

# Chạy tests
pre-commit run run-tests --hook-stage pre-push

# Skip hooks (khẩn cấp)
git commit --no-verify
git push --no-verify

# Cập nhật hooks
pre-commit autoupdate
```

---

## ✅ Best Practices

### Error Handling

```cpp
try {
    // Business logic
} catch (const std::exception& e) {
    Json::Value error;
    error["error"] = e.what();
    auto resp = HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(k500InternalServerError);
    callback(resp);
}
```

### Input Validation

```cpp
if (id.empty()) {
    // Return 400 Bad Request
}

auto json = req->getJsonObject();
if (!json || !json->isMember("required_field")) {
    // Return 400 Bad Request
}
```

### HTTP Status Codes

- `200 OK`: Success
- `201 Created`: Resource created
- `400 Bad Request`: Invalid input
- `404 Not Found`: Resource not found
- `500 Internal Server Error`: Server error

### Naming Conventions

- **Handlers**: `XxxHandler` (PascalCase)
- **Files**: `xxx_handler.h/cpp` (snake_case)
- **Endpoints**: `/v1/xxx/yyy` (lowercase)
- **Methods**: `getXxx`, `createXxx` (camelCase)

---

## ⚠️ Troubleshooting

### Build Errors

```bash
# Xóa cache và build lại
rm -rf build
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Lỗi "Could NOT find OpenSSL"

```bash
sudo apt-get install libssl-dev
```

### Lỗi CVEDIX SDK symlinks

```bash
sudo ln -sf /opt/cvedix/lib/libtinyexpr.so /usr/lib/libtinyexpr.so
sudo ln -sf /opt/cvedix/lib/libcvedix_instance_sdk.so /usr/lib/libcvedix_instance_sdk.so
```

### Pre-commit hooks không chạy

```bash
pre-commit uninstall
pre-commit install
pre-commit install --hook-type pre-push
```

### Tests fail

```bash
cd build
ctest --output-on-failure
```

---

## 📚 Tài Liệu Liên Quan

- [Hướng Dẫn Khởi Động](GETTING_STARTED.md)
- [Architecture](ARCHITECTURE.md)
- [Environment Variables](ENVIRONMENT_VARIABLES.md)
- [Drogon Framework](https://drogon.docsforge.com/)
- [Google Test](https://google.github.io/googletest/)

