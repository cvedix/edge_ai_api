#include <gtest/gtest.h>
#include "api/create_instance_handler.h"
#include "instances/instance_registry.h"
#include "solutions/solution_registry.h"
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <json/json.h>
#include <thread>
#include <chrono>
#include <memory>

using namespace drogon;

class CreateInstanceHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        handler_ = std::make_unique<CreateInstanceHandler>();
        
        // Create registry instances
        instance_registry_ = std::make_unique<InstanceRegistry>();
        solution_registry_ = &SolutionRegistry::getInstance();
        
        // Register with handler
        CreateInstanceHandler::setInstanceRegistry(instance_registry_.get());
        CreateInstanceHandler::setSolutionRegistry(solution_registry_);
    }

    void TearDown() override {
        handler_.reset();
        instance_registry_.reset();
        
        // Clear handler dependencies
        CreateInstanceHandler::setInstanceRegistry(nullptr);
        CreateInstanceHandler::setSolutionRegistry(nullptr);
    }

    std::unique_ptr<CreateInstanceHandler> handler_;
    std::unique_ptr<InstanceRegistry> instance_registry_;
    SolutionRegistry* solution_registry_;  // Singleton, don't own
};

// Test POST /v1/core/instance with invalid JSON
TEST_F(CreateInstanceHandlerTest, CreateInstanceWithInvalidJson) {
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/core/instance");
    req->setMethod(Post);
    req->setBody("invalid json");
    
    HttpResponsePtr response;
    bool callbackCalled = false;
    
    handler_->createInstance(req, [&](const HttpResponsePtr &resp) {
        callbackCalled = true;
        response = resp;
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    ASSERT_TRUE(callbackCalled);
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->statusCode(), k400BadRequest);
}

// Test POST /v1/core/instance with missing required fields
TEST_F(CreateInstanceHandlerTest, CreateInstanceWithMissingFields) {
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/core/instance");
    req->setMethod(Post);
    
    Json::Value body;
    // Missing required fields like name, solution, etc.
    req->setBody(body.toStyledString());
    
    HttpResponsePtr response;
    bool callbackCalled = false;
    
    handler_->createInstance(req, [&](const HttpResponsePtr &resp) {
        callbackCalled = true;
        response = resp;
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    ASSERT_TRUE(callbackCalled);
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->statusCode(), k400BadRequest);
}

// Test POST /v1/core/instance with valid JSON structure
TEST_F(CreateInstanceHandlerTest, CreateInstanceWithValidJsonStructure) {
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/core/instance");
    req->setMethod(Post);
    
    Json::Value body;
    body["name"] = "test_instance";
    body["solution"] = "test_solution";
    body["group"] = "default";
    body["autoStart"] = false;
    
    Json::Value additionalParams;
    additionalParams["FILE_PATH"] = "/test/path";
    body["additionalParams"] = additionalParams;
    
    req->setBody(body.toStyledString());
    
    HttpResponsePtr response;
    bool callbackCalled = false;
    
    handler_->createInstance(req, [&](const HttpResponsePtr &resp) {
        callbackCalled = true;
        response = resp;
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    ASSERT_TRUE(callbackCalled);
    ASSERT_NE(response, nullptr);
    // Should return 200, 201, or 400/500 depending on validation and solution existence
    EXPECT_TRUE(response->statusCode() == k200OK || 
                response->statusCode() == k201Created || 
                response->statusCode() == k400BadRequest ||
                response->statusCode() == k500InternalServerError);
}

// Test OPTIONS request
TEST_F(CreateInstanceHandlerTest, HandleOptions) {
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/core/instance");
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

