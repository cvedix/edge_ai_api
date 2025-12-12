#include <gtest/gtest.h>
#include "api/config_handler.h"
#include "config/system_config.h"
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <json/json.h>
#include <thread>
#include <chrono>

using namespace drogon;

class ConfigHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        handler_ = std::make_unique<ConfigHandler>();
    }

    void TearDown() override {
        handler_.reset();
    }

    std::unique_ptr<ConfigHandler> handler_;
};

// Test GET /v1/core/config returns valid JSON
TEST_F(ConfigHandlerTest, GetConfigReturnsValidJson) {
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/core/config");
    req->setMethod(Get);
    
    HttpResponsePtr response;
    bool callbackCalled = false;
    
    handler_->getConfig(req, [&](const HttpResponsePtr &resp) {
        callbackCalled = true;
        response = resp;
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    ASSERT_TRUE(callbackCalled);
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->statusCode(), k200OK);
    EXPECT_EQ(response->contentType(), CT_APPLICATION_JSON);
    
    auto json = response->getJsonObject();
    ASSERT_NE(json, nullptr);
    
    // Config should be an object
    EXPECT_TRUE(json->isObject());
}

// Test GET /v1/core/config/{path} with valid path
TEST_F(ConfigHandlerTest, GetConfigSectionWithValidPath) {
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/core/config/max_running_instances");
    req->setMethod(Get);
    
    HttpResponsePtr response;
    bool callbackCalled = false;
    
    handler_->getConfigSection(req, [&](const HttpResponsePtr &resp) {
        callbackCalled = true;
        response = resp;
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    ASSERT_TRUE(callbackCalled);
    ASSERT_NE(response, nullptr);
    
    // Should return 200 or 404 depending on if path exists
    EXPECT_TRUE(response->statusCode() == k200OK || response->statusCode() == k404NotFound);
}

// Test GET /v1/core/config/{path} with invalid path
TEST_F(ConfigHandlerTest, GetConfigSectionWithInvalidPath) {
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/core/config/invalid_path_that_does_not_exist");
    req->setMethod(Get);
    
    HttpResponsePtr response;
    bool callbackCalled = false;
    
    handler_->getConfigSection(req, [&](const HttpResponsePtr &resp) {
        callbackCalled = true;
        response = resp;
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    ASSERT_TRUE(callbackCalled);
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->statusCode(), k404NotFound);
}

// Test POST /v1/core/config with valid JSON
TEST_F(ConfigHandlerTest, CreateOrUpdateConfigWithValidJson) {
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/core/config");
    req->setMethod(Post);
    
    Json::Value body;
    body["test_key"] = "test_value";
    req->setBody(body.toStyledString());
    
    HttpResponsePtr response;
    bool callbackCalled = false;
    
    handler_->createOrUpdateConfig(req, [&](const HttpResponsePtr &resp) {
        callbackCalled = true;
        response = resp;
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    ASSERT_TRUE(callbackCalled);
    ASSERT_NE(response, nullptr);
    // Should return 200 or 400 depending on validation
    EXPECT_TRUE(response->statusCode() == k200OK || response->statusCode() == k400BadRequest);
}

// Test POST /v1/core/config with invalid JSON
TEST_F(ConfigHandlerTest, CreateOrUpdateConfigWithInvalidJson) {
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/core/config");
    req->setMethod(Post);
    req->setBody("invalid json");
    
    HttpResponsePtr response;
    bool callbackCalled = false;
    
    handler_->createOrUpdateConfig(req, [&](const HttpResponsePtr &resp) {
        callbackCalled = true;
        response = resp;
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    ASSERT_TRUE(callbackCalled);
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->statusCode(), k400BadRequest);
}

// Test PUT /v1/core/config with valid JSON
TEST_F(ConfigHandlerTest, ReplaceConfigWithValidJson) {
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/core/config");
    req->setMethod(Put);
    
    Json::Value body;
    body["test_key"] = "test_value";
    req->setBody(body.toStyledString());
    
    HttpResponsePtr response;
    bool callbackCalled = false;
    
    handler_->replaceConfig(req, [&](const HttpResponsePtr &resp) {
        callbackCalled = true;
        response = resp;
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    ASSERT_TRUE(callbackCalled);
    ASSERT_NE(response, nullptr);
    // Should return 200 or 400 depending on validation
    EXPECT_TRUE(response->statusCode() == k200OK || response->statusCode() == k400BadRequest);
}

// Test PATCH /v1/core/config/{path} with valid JSON
TEST_F(ConfigHandlerTest, UpdateConfigSectionWithValidJson) {
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/core/config/test_path");
    req->setMethod(Patch);
    
    Json::Value body;
    body["test_key"] = "test_value";
    req->setBody(body.toStyledString());
    
    HttpResponsePtr response;
    bool callbackCalled = false;
    
    handler_->updateConfigSection(req, [&](const HttpResponsePtr &resp) {
        callbackCalled = true;
        response = resp;
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    ASSERT_TRUE(callbackCalled);
    ASSERT_NE(response, nullptr);
    // Should return 200, 400, or 404 depending on path existence and validation
    EXPECT_TRUE(response->statusCode() == k200OK || 
                response->statusCode() == k400BadRequest || 
                response->statusCode() == k404NotFound);
}

// Test DELETE /v1/core/config/{path}
TEST_F(ConfigHandlerTest, DeleteConfigSection) {
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/core/config/test_path");
    req->setMethod(Delete);
    
    HttpResponsePtr response;
    bool callbackCalled = false;
    
    handler_->deleteConfigSection(req, [&](const HttpResponsePtr &resp) {
        callbackCalled = true;
        response = resp;
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    ASSERT_TRUE(callbackCalled);
    ASSERT_NE(response, nullptr);
    // Should return 200 or 404 depending on if path exists
    EXPECT_TRUE(response->statusCode() == k200OK || response->statusCode() == k404NotFound);
}

// Test POST /v1/core/config/reset
TEST_F(ConfigHandlerTest, ResetConfig) {
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/core/config/reset");
    req->setMethod(Post);
    
    HttpResponsePtr response;
    bool callbackCalled = false;
    
    handler_->resetConfig(req, [&](const HttpResponsePtr &resp) {
        callbackCalled = true;
        response = resp;
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    ASSERT_TRUE(callbackCalled);
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->statusCode(), k200OK);
}

// Test OPTIONS request
TEST_F(ConfigHandlerTest, HandleOptions) {
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/core/config");
    req->setMethod(Options);
    
    HttpResponsePtr response;
    bool callbackCalled = false;
    
    handler_->handleOptions(req, [&](const HttpResponsePtr &resp) {
        callbackCalled = true;
        response = resp;
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    ASSERT_TRUE(callbackCalled);
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->statusCode(), k200OK);
}

