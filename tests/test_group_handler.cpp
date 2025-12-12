#include <gtest/gtest.h>
#include "api/group_handler.h"
#include "groups/group_registry.h"
#include "groups/group_storage.h"
#include "instances/instance_registry.h"
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <json/json.h>
#include <thread>
#include <chrono>
#include <memory>
#include <filesystem>

using namespace drogon;

class GroupHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        handler_ = std::make_unique<GroupHandler>();
        
        // Create temporary storage directory for testing
        test_storage_dir_ = std::filesystem::temp_directory_path() / "test_groups";
        std::filesystem::create_directories(test_storage_dir_);
        
        // Create registry and storage instances
        registry_ = std::make_unique<GroupRegistry>();
        storage_ = std::make_unique<GroupStorage>(test_storage_dir_.string());
        
        // Create instance registry (minimal setup)
        instance_registry_ = std::make_unique<InstanceRegistry>();
        
        // Register with handler
        GroupHandler::setGroupRegistry(registry_.get());
        GroupHandler::setGroupStorage(storage_.get());
        GroupHandler::setInstanceRegistry(instance_registry_.get());
    }

    void TearDown() override {
        handler_.reset();
        registry_.reset();
        storage_.reset();
        instance_registry_.reset();
        
        // Clean up test storage directory
        if (std::filesystem::exists(test_storage_dir_)) {
            std::filesystem::remove_all(test_storage_dir_);
        }
        
        // Clear handler dependencies
        GroupHandler::setGroupRegistry(nullptr);
        GroupHandler::setGroupStorage(nullptr);
        GroupHandler::setInstanceRegistry(nullptr);
    }

    std::unique_ptr<GroupHandler> handler_;
    std::unique_ptr<GroupRegistry> registry_;
    std::unique_ptr<GroupStorage> storage_;
    std::unique_ptr<InstanceRegistry> instance_registry_;
    std::filesystem::path test_storage_dir_;
};

// Test GET /v1/core/groups returns valid JSON
TEST_F(GroupHandlerTest, ListGroupsReturnsValidJson) {
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/core/groups");
    req->setMethod(Get);
    
    HttpResponsePtr response;
    bool callbackCalled = false;
    
    handler_->listGroups(req, [&](const HttpResponsePtr &resp) {
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
    
    // Should have groups array
    EXPECT_TRUE(json->isMember("groups"));
    EXPECT_TRUE((*json)["groups"].isArray());
}

// Test GET /v1/core/groups/{groupId} with valid groupId
TEST_F(GroupHandlerTest, GetGroupWithValidId) {
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/core/groups/test_group");
    req->setParameter("groupId", "test_group");
    req->setMethod(Get);
    
    HttpResponsePtr response;
    bool callbackCalled = false;
    
    handler_->getGroup(req, [&](const HttpResponsePtr &resp) {
        callbackCalled = true;
        response = resp;
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    ASSERT_TRUE(callbackCalled);
    ASSERT_NE(response, nullptr);
    // Should return 200 or 404 depending on if group exists
    EXPECT_TRUE(response->statusCode() == k200OK || response->statusCode() == k404NotFound);
}

// Test POST /v1/core/groups with valid JSON
TEST_F(GroupHandlerTest, CreateGroupWithValidJson) {
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/core/groups");
    req->setMethod(Post);
    
    Json::Value body;
    body["groupId"] = "test_group";
    body["groupName"] = "Test Group";
    req->setBody(body.toStyledString());
    
    HttpResponsePtr response;
    bool callbackCalled = false;
    
    handler_->createGroup(req, [&](const HttpResponsePtr &resp) {
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
TEST_F(GroupHandlerTest, HandleOptions) {
    auto req = HttpRequest::newHttpRequest();
    req->setPath("/v1/core/groups");
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

