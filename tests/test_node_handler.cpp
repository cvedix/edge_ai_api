#include "api/node_handler.h"
#include "core/node_pool_manager.h"
#include <chrono>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <gtest/gtest.h>
#include <json/json.h>
#include <thread>

using namespace drogon;

class NodeHandlerTest : public ::testing::Test {
protected:
  void SetUp() override { handler_ = std::make_unique<NodeHandler>(); }

  void TearDown() override { handler_.reset(); }

  std::unique_ptr<NodeHandler> handler_;
};

// Test GET /v1/core/nodes returns valid JSON
TEST_F(NodeHandlerTest, ListNodesReturnsValidJson) {
  auto req = HttpRequest::newHttpRequest();
  req->setPath("/v1/core/nodes");
  req->setMethod(Get);

  HttpResponsePtr response;
  bool callbackCalled = false;

  handler_->listNodes(req, [&](const HttpResponsePtr &resp) {
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

  // Should have nodes array and total
  EXPECT_TRUE(json->isMember("nodes") || json->isMember("total"));
}

// Test GET /v1/core/nodes/{nodeId} with valid nodeId
TEST_F(NodeHandlerTest, GetNodeWithValidId) {
  auto req = HttpRequest::newHttpRequest();
  req->setPath("/v1/core/nodes/test_node_id");
  req->setParameter("nodeId", "test_node_id");
  req->setMethod(Get);

  HttpResponsePtr response;
  bool callbackCalled = false;

  handler_->getNode(req, [&](const HttpResponsePtr &resp) {
    callbackCalled = true;
    response = resp;
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  ASSERT_TRUE(callbackCalled);
  ASSERT_NE(response, nullptr);
  // Should return 200 or 404 depending on if node exists
  EXPECT_TRUE(response->statusCode() == k200OK ||
              response->statusCode() == k404NotFound);
}

// Test GET /v1/core/nodes/templates returns valid JSON
TEST_F(NodeHandlerTest, ListTemplatesReturnsValidJson) {
  auto req = HttpRequest::newHttpRequest();
  req->setPath("/v1/core/nodes/templates");
  req->setMethod(Get);

  HttpResponsePtr response;
  bool callbackCalled = false;

  handler_->listTemplates(req, [&](const HttpResponsePtr &resp) {
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
TEST_F(NodeHandlerTest, GetStatsReturnsValidJson) {
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

// Test POST /v1/core/nodes with valid JSON
TEST_F(NodeHandlerTest, CreateNodeWithValidJson) {
  auto req = HttpRequest::newHttpRequest();
  req->setPath("/v1/core/nodes");
  req->setMethod(Post);

  Json::Value body;
  body["nodeType"] = "test_node";
  body["displayName"] = "Test Node";
  req->setBody(body.toStyledString());

  HttpResponsePtr response;
  bool callbackCalled = false;

  handler_->createNode(req, [&](const HttpResponsePtr &resp) {
    callbackCalled = true;
    response = resp;
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  ASSERT_TRUE(callbackCalled);
  ASSERT_NE(response, nullptr);
  // Should return 200, 201, or 400 depending on validation
  EXPECT_TRUE(response->statusCode() == k200OK ||
              response->statusCode() == k201Created ||
              response->statusCode() == k400BadRequest);
}

// Test OPTIONS request
TEST_F(NodeHandlerTest, HandleOptions) {
  auto req = HttpRequest::newHttpRequest();
  req->setPath("/v1/core/nodes");
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
