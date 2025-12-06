#include "groups/group_storage.h"
#include <json/json.h>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

GroupStorage::GroupStorage(const std::string& storage_dir)
    : storage_dir_(storage_dir) {
    ensureStorageDir();
}

void GroupStorage::ensureStorageDir() const {
    try {
        if (!fs::exists(storage_dir_)) {
            std::cerr << "[GroupStorage] Creating storage directory: " << storage_dir_ << std::endl;
            fs::create_directories(storage_dir_);
            std::cerr << "[GroupStorage] Storage directory created successfully" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[GroupStorage] Error creating storage directory: " << e.what() << std::endl;
        throw;
    }
}

std::string GroupStorage::getGroupFilePath(const std::string& groupId) const {
    return storage_dir_ + "/" + groupId + ".json";
}

bool GroupStorage::saveGroup(const GroupInfo& group) {
    try {
        ensureStorageDir();
        
        std::string error;
        Json::Value json = groupInfoToJson(group, &error);
        if (json.isNull()) {
            std::cerr << "[GroupStorage] Failed to convert group to JSON: " << error << std::endl;
            return false;
        }
        
        std::string filepath = getGroupFilePath(group.groupId);
        std::ofstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "[GroupStorage] Failed to open file for writing: " << filepath << std::endl;
            return false;
        }
        
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "    ";
        std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
        writer->write(json, &file);
        file.close();
        
        std::cerr << "[GroupStorage] Saved group: " << group.groupId << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[GroupStorage] Exception saving group: " << e.what() << std::endl;
        return false;
    }
}

std::optional<GroupInfo> GroupStorage::loadGroup(const std::string& groupId) {
    try {
        std::string filepath = getGroupFilePath(groupId);
        if (!fs::exists(filepath)) {
            return std::nullopt;
        }
        
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return std::nullopt;
        }
        
        Json::CharReaderBuilder builder;
        Json::Value json;
        std::string errors;
        if (!Json::parseFromStream(builder, file, &json, &errors)) {
            std::cerr << "[GroupStorage] Failed to parse group file: " << errors << std::endl;
            return std::nullopt;
        }
        
        std::string error;
        auto group = jsonToGroupInfo(json, &error);
        if (!group.has_value()) {
            std::cerr << "[GroupStorage] Failed to convert JSON to group: " << error << std::endl;
            return std::nullopt;
        }
        
        return group;
    } catch (const std::exception& e) {
        std::cerr << "[GroupStorage] Exception loading group: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::vector<GroupInfo> GroupStorage::loadAllGroups() {
    std::vector<GroupInfo> groups;
    
    try {
        if (!fs::exists(storage_dir_)) {
            return groups;
        }
        
        for (const auto& entry : fs::directory_iterator(storage_dir_)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                std::string filename = entry.path().filename().string();
                std::string groupId = filename.substr(0, filename.length() - 5); // Remove .json
                
                auto group = loadGroup(groupId);
                if (group.has_value()) {
                    groups.push_back(group.value());
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[GroupStorage] Exception loading all groups: " << e.what() << std::endl;
    }
    
    return groups;
}

bool GroupStorage::deleteGroup(const std::string& groupId) {
    try {
        std::string filepath = getGroupFilePath(groupId);
        if (fs::exists(filepath)) {
            fs::remove(filepath);
            std::cerr << "[GroupStorage] Deleted group file: " << groupId << std::endl;
            return true;
        }
        return false;
    } catch (const std::exception& e) {
        std::cerr << "[GroupStorage] Exception deleting group: " << e.what() << std::endl;
        return false;
    }
}

bool GroupStorage::groupFileExists(const std::string& groupId) const {
    return fs::exists(getGroupFilePath(groupId));
}

Json::Value GroupStorage::groupInfoToJson(const GroupInfo& group, std::string* error) const {
    Json::Value json(Json::objectValue);
    
    json["groupId"] = group.groupId;
    json["groupName"] = group.groupName;
    json["description"] = group.description;
    json["isDefault"] = group.isDefault;
    json["readOnly"] = group.readOnly;
    json["instanceCount"] = group.instanceCount;
    json["createdAt"] = group.createdAt;
    json["updatedAt"] = group.updatedAt;
    
    return json;
}

std::optional<GroupInfo> GroupStorage::jsonToGroupInfo(const Json::Value& json, std::string* error) const {
    try {
        GroupInfo group;
        
        if (!json.isMember("groupId") || !json["groupId"].isString()) {
            if (error) *error = "Missing or invalid groupId";
            return std::nullopt;
        }
        group.groupId = json["groupId"].asString();
        
        if (!json.isMember("groupName") || !json["groupName"].isString()) {
            if (error) *error = "Missing or invalid groupName";
            return std::nullopt;
        }
        group.groupName = json["groupName"].asString();
        
        if (json.isMember("description") && json["description"].isString()) {
            group.description = json["description"].asString();
        }
        
        if (json.isMember("isDefault") && json["isDefault"].isBool()) {
            group.isDefault = json["isDefault"].asBool();
        }
        
        if (json.isMember("readOnly") && json["readOnly"].isBool()) {
            group.readOnly = json["readOnly"].asBool();
        }
        
        if (json.isMember("instanceCount") && json["instanceCount"].isInt()) {
            group.instanceCount = json["instanceCount"].asInt();
        }
        
        if (json.isMember("createdAt") && json["createdAt"].isString()) {
            group.createdAt = json["createdAt"].asString();
        }
        
        if (json.isMember("updatedAt") && json["updatedAt"].isString()) {
            group.updatedAt = json["updatedAt"].asString();
        }
        
        // Validate
        std::string validationError;
        if (!group.validate(validationError)) {
            if (error) *error = validationError;
            return std::nullopt;
        }
        
        return group;
    } catch (const std::exception& e) {
        if (error) *error = e.what();
        return std::nullopt;
    }
}

