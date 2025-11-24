#include <gtest/gtest.h>
#include "api/version_handler.h"
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <json/json.h>
#include <thread>
#include <chrono>

using namespace drogon;

class VersionHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        handler_ = std::make_unique<VersionHandler>();
    }

    void TearDown() override {
        handler_.reset();
    }

    std::unique_ptr<VersionHandler> handler_;
};

// Test version endpoint returns valid JSON
TEST_F(VersionHandlerTest, VersionEndpointReturnsValidJson) {
    bool callbackCalled = false;
    HttpResponsePtr response;
    
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/core/version");
    req->setMethod(Get);
    
    handler_->getVersion(req, [&](const HttpResponsePtr &resp) {
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
    
    // Check required fields
    EXPECT_TRUE(json->isMember("version"));
    EXPECT_TRUE(json->isMember("build_time"));
    EXPECT_TRUE(json->isMember("git_commit"));
    EXPECT_TRUE(json->isMember("api_version"));
    EXPECT_TRUE(json->isMember("service"));
}

// Test version endpoint field types
TEST_F(VersionHandlerTest, VersionFieldTypes) {
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/core/version");
    req->setMethod(Get);
    
    HttpResponsePtr response;
    bool callbackCalled = false;
    
    handler_->getVersion(req, [&](const HttpResponsePtr &resp) {
        callbackCalled = true;
        response = resp;
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    ASSERT_TRUE(callbackCalled);
    auto json = response->getJsonObject();
    ASSERT_NE(json, nullptr);
    
    // All fields should be strings
    EXPECT_TRUE((*json)["version"].isString());
    EXPECT_TRUE((*json)["build_time"].isString());
    EXPECT_TRUE((*json)["git_commit"].isString());
    EXPECT_TRUE((*json)["api_version"].isString());
    EXPECT_TRUE((*json)["service"].isString());
}

// Test version endpoint service name
TEST_F(VersionHandlerTest, VersionServiceName) {
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/core/version");
    req->setMethod(Get);
    
    HttpResponsePtr response;
    bool callbackCalled = false;
    
    handler_->getVersion(req, [&](const HttpResponsePtr &resp) {
        callbackCalled = true;
        response = resp;
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    ASSERT_TRUE(callbackCalled);
    auto json = response->getJsonObject();
    ASSERT_NE(json, nullptr);
    
    EXPECT_EQ((*json)["service"].asString(), "edge_ai_api");
    EXPECT_EQ((*json)["api_version"].asString(), "v1");
}

// Test version endpoint api version format
TEST_F(VersionHandlerTest, VersionApiVersionFormat) {
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/core/version");
    req->setMethod(Get);
    
    HttpResponsePtr response;
    bool callbackCalled = false;
    
    handler_->getVersion(req, [&](const HttpResponsePtr &resp) {
        callbackCalled = true;
        response = resp;
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    ASSERT_TRUE(callbackCalled);
    auto json = response->getJsonObject();
    ASSERT_NE(json, nullptr);
    
    std::string apiVersion = (*json)["api_version"].asString();
    EXPECT_FALSE(apiVersion.empty());
    EXPECT_EQ(apiVersion[0], 'v');
}

