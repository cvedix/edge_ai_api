#include <gtest/gtest.h>
#include "api/instance_handler.h"
#include "instances/instance_registry.h"
#include "solutions/solution_registry.h"
#include "core/pipeline_builder.h"
#include "instances/instance_storage.h"
#include "instances/instance_info.h"
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <json/json.h>
#include <thread>
#include <chrono>
#include <memory>
#include <filesystem>
#include <unistd.h>

using namespace drogon;

class InstanceStatusSummaryTest : public ::testing::Test {
protected:
    void SetUp() override {
        handler_ = std::make_unique<InstanceHandler>();
        
        // Create temporary storage directory for testing
        test_storage_dir_ = std::filesystem::temp_directory_path() / ("test_instances_" + std::to_string(getpid()));
        std::filesystem::create_directories(test_storage_dir_);
        
        // Create dependencies for InstanceRegistry
        solution_registry_ = &SolutionRegistry::getInstance();
        pipeline_builder_ = std::make_unique<PipelineBuilder>();
        instance_storage_ = std::make_unique<InstanceStorage>(test_storage_dir_.string());
        
        // Create InstanceRegistry
        instance_registry_ = std::make_unique<InstanceRegistry>(
            *solution_registry_,
            *pipeline_builder_,
            *instance_storage_
        );
        
        // Register with handler
        InstanceHandler::setInstanceRegistry(instance_registry_.get());
    }

    void TearDown() override {
        handler_.reset();
        instance_registry_.reset();
        instance_storage_.reset();
        pipeline_builder_.reset();
        
        // Clean up test storage directory
        if (std::filesystem::exists(test_storage_dir_)) {
            std::filesystem::remove_all(test_storage_dir_);
        }
        
        // Clear registry for next test
        InstanceHandler::setInstanceRegistry(nullptr);
    }

    std::unique_ptr<InstanceHandler> handler_;
    SolutionRegistry* solution_registry_;  // Singleton, don't own
    std::unique_ptr<PipelineBuilder> pipeline_builder_;
    std::unique_ptr<InstanceStorage> instance_storage_;
    std::unique_ptr<InstanceRegistry> instance_registry_;
    std::filesystem::path test_storage_dir_;
};

// Test status summary endpoint returns valid JSON
TEST_F(InstanceStatusSummaryTest, StatusSummaryReturnsValidJson) {
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/core/instance/status/summary");
    req->setMethod(Get);
    
    HttpResponsePtr response;
    bool callbackCalled = false;
    
    handler_->getStatusSummary(req, [&](const HttpResponsePtr &resp) {
        callbackCalled = true;
        response = resp;
    });
    
    // Wait a bit for async callback
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    ASSERT_TRUE(callbackCalled);
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->statusCode(), k200OK);
    
    // Check content type
    EXPECT_EQ(response->contentType(), CT_APPLICATION_JSON);
    
    // Parse JSON response
    auto json = response->getJsonObject();
    ASSERT_NE(json, nullptr);
    
    // Check required fields
    EXPECT_TRUE(json->isMember("total"));
    EXPECT_TRUE(json->isMember("configured"));
    EXPECT_TRUE(json->isMember("running"));
    EXPECT_TRUE(json->isMember("stopped"));
    EXPECT_TRUE(json->isMember("timestamp"));
    
    // Check types
    EXPECT_TRUE((*json)["total"].isInt());
    EXPECT_TRUE((*json)["configured"].isInt());
    EXPECT_TRUE((*json)["running"].isInt());
    EXPECT_TRUE((*json)["stopped"].isInt());
    EXPECT_TRUE((*json)["timestamp"].isString());
    
    // Check values are non-negative
    EXPECT_GE((*json)["total"].asInt(), 0);
    EXPECT_GE((*json)["configured"].asInt(), 0);
    EXPECT_GE((*json)["running"].asInt(), 0);
    EXPECT_GE((*json)["stopped"].asInt(), 0);
    
    // Check configured equals total
    EXPECT_EQ((*json)["configured"].asInt(), (*json)["total"].asInt());
    
    // Check running + stopped equals total
    EXPECT_EQ((*json)["running"].asInt() + (*json)["stopped"].asInt(), (*json)["total"].asInt());
}

// Test status summary endpoint with no instances
TEST_F(InstanceStatusSummaryTest, StatusSummaryWithNoInstances) {
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/core/instance/status/summary");
    req->setMethod(Get);
    
    HttpResponsePtr response;
    bool callbackCalled = false;
    
    handler_->getStatusSummary(req, [&](const HttpResponsePtr &resp) {
        callbackCalled = true;
        response = resp;
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    ASSERT_TRUE(callbackCalled);
    auto json = response->getJsonObject();
    ASSERT_NE(json, nullptr);
    
    EXPECT_EQ((*json)["total"].asInt(), 0);
    EXPECT_EQ((*json)["configured"].asInt(), 0);
    EXPECT_EQ((*json)["running"].asInt(), 0);
    EXPECT_EQ((*json)["stopped"].asInt(), 0);
}

// Test status summary endpoint when registry not initialized
TEST_F(InstanceStatusSummaryTest, StatusSummaryRegistryNotInitialized) {
    InstanceHandler::setInstanceRegistry(nullptr);
    
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/core/instance/status/summary");
    req->setMethod(Get);
    
    HttpResponsePtr response;
    bool callbackCalled = false;
    
    handler_->getStatusSummary(req, [&](const HttpResponsePtr &resp) {
        callbackCalled = true;
        response = resp;
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    ASSERT_TRUE(callbackCalled);
    EXPECT_EQ(response->statusCode(), k500InternalServerError);
    
    auto json = response->getJsonObject();
    ASSERT_NE(json, nullptr);
    EXPECT_TRUE(json->isMember("error"));
    
    // Restore registry for other tests
    InstanceHandler::setInstanceRegistry(instance_registry_.get());
}

// Test OPTIONS endpoint for CORS
TEST_F(InstanceStatusSummaryTest, StatusSummaryOptionsEndpoint) {
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/core/instance/status/summary");
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
    
    // Check CORS headers
    EXPECT_EQ(response->getHeader("Access-Control-Allow-Origin"), "*");
    EXPECT_EQ(response->getHeader("Access-Control-Allow-Methods"), "GET, POST, PUT, DELETE, OPTIONS");
    EXPECT_EQ(response->getHeader("Access-Control-Allow-Headers"), "Content-Type, Authorization");
}

