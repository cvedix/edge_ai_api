#include <gtest/gtest.h>
#include "api/node_pool_handler.h"
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <json/json.h>
#include <thread>
#include <chrono>

using namespace drogon;

class NodePoolHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        handler_ = std::make_unique<NodePoolHandler>();
    }

    void TearDown() override {
        handler_.reset();
    }

    std::unique_ptr<NodePoolHandler> handler_;
};

// Test GET /v1/core/nodes/templates returns valid JSON
TEST_F(NodePoolHandlerTest, GetTemplatesReturnsValidJson) {
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/core/nodes/templates");
    req->setMethod(Get);
    
    HttpResponsePtr response;
    bool callbackCalled = false;
    
    handler_->getTemplates(req, [&](const HttpResponsePtr &resp) {
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
}

// Test GET /v1/core/nodes/templates/{category} returns valid JSON
TEST_F(NodePoolHandlerTest, GetTemplatesByCategoryReturnsValidJson) {
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/core/nodes/templates/inference");
    req->setParameter("category", "inference");
    req->setMethod(Get);
    
    HttpResponsePtr response;
    bool callbackCalled = false;
    
    handler_->getTemplatesByCategory(req, [&](const HttpResponsePtr &resp) {
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
}

// Test GET /v1/core/nodes/preconfigured returns valid JSON
TEST_F(NodePoolHandlerTest, GetPreConfiguredNodesReturnsValidJson) {
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/core/nodes/preconfigured");
    req->setMethod(Get);
    
    HttpResponsePtr response;
    bool callbackCalled = false;
    
    handler_->getPreConfiguredNodes(req, [&](const HttpResponsePtr &resp) {
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
}

// Test GET /v1/core/nodes/preconfigured/available returns valid JSON
TEST_F(NodePoolHandlerTest, GetAvailableNodesReturnsValidJson) {
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/core/nodes/preconfigured/available");
    req->setMethod(Get);
    
    HttpResponsePtr response;
    bool callbackCalled = false;
    
    handler_->getAvailableNodes(req, [&](const HttpResponsePtr &resp) {
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
}

// Test GET /v1/core/nodes/stats returns valid JSON
TEST_F(NodePoolHandlerTest, GetStatsReturnsValidJson) {
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/core/nodes/stats");
    req->setMethod(Get);
    
    HttpResponsePtr response;
    bool callbackCalled = false;
    
    handler_->getStats(req, [&](const HttpResponsePtr &resp) {
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
}

// Test OPTIONS request
TEST_F(NodePoolHandlerTest, HandleOptions) {
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/core/nodes/templates");
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

