#include "groups/group_storage.h"
#include "core/env_config.h"
#include <json/json.h>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <sstream>
#include <vector>
#include <set>
#include <algorithm>

namespace fs = std::filesystem;

GroupStorage::GroupStorage(const std::string& storage_dir)
    : storage_dir_(storage_dir) {
    ensureStorageDir();
}

void GroupStorage::ensureStorageDir() {
    // Extract subdir name from storage_dir_ for fallback
    std::filesystem::path path(storage_dir_);
    std::string subdir = path.filename().string();
    if (subdir.empty()) {
        subdir = "groups"; // Default fallback subdir
    }
    
    // Use resolveDirectory with 3-tier fallback strategy
    std::string resolved_dir = EnvConfig::resolveDirectory(storage_dir_, subdir);
    
    // Update storage_dir_ if fallback was used
    if (resolved_dir != storage_dir_) {
        std::cerr << "[GroupStorage] ⚠ Storage directory changed from " << storage_dir_ 
                  << " to " << resolved_dir << " (fallback)" << std::endl;
        storage_dir_ = resolved_dir;
    }
    
    // Don't throw - let the application continue even if directory creation failed
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
    // Extract subdir for checking all tiers
    std::filesystem::path path(storage_dir_);
    std::string subdir = path.filename().string();
    if (subdir.empty()) {
        subdir = "groups";
    }
    
    // Get all possible directories in priority order
    std::vector<std::string> allDirs = EnvConfig::getAllPossibleDirectories(subdir);
    
    // Try to load from all tiers (later tiers override earlier ones)
    // We iterate forward and keep the last found version
    std::optional<GroupInfo> result = std::nullopt;
    std::string foundInTier;
    
    for (const auto& dir : allDirs) {
        std::string filepath = dir + "/" + groupId + ".json";
        
        if (!fs::exists(filepath)) {
            continue; // Skip if file doesn't exist
        }
        
        try {
            std::ifstream file(filepath);
            if (!file.is_open()) {
                continue;
            }
            
            Json::CharReaderBuilder builder;
            Json::Value json;
            std::string errors;
            if (Json::parseFromStream(builder, file, &json, &errors)) {
                std::string error;
                auto group = jsonToGroupInfo(json, &error);
                if (group.has_value()) {
                    result = group; // Later tier overrides earlier one
                    foundInTier = dir;
                }
            }
        } catch (const std::exception& e) {
            // Continue to next tier
            continue;
        }
    }
    
    if (result.has_value()) {
        std::cerr << "[GroupStorage] Loaded group " << groupId << " from tier: " << foundInTier << std::endl;
    }
    
    return result;
}

std::vector<GroupInfo> GroupStorage::loadAllGroups() {
    std::vector<GroupInfo> groups;
    std::set<std::string> loadedGroupIds; // Track loaded group IDs to avoid duplicates
    
    // Extract subdir for checking all tiers
    std::filesystem::path path(storage_dir_);
    std::string subdir = path.filename().string();
    if (subdir.empty()) {
        subdir = "groups";
    }
    
    // Get all possible directories in priority order
    std::vector<std::string> allDirs = EnvConfig::getAllPossibleDirectories(subdir);
    
    // Load from all tiers (later tiers override earlier ones for same groupId)
    for (const auto& dir : allDirs) {
        if (!fs::exists(dir)) {
            continue; // Skip if directory doesn't exist
        }
        
        try {
            for (const auto& entry : fs::directory_iterator(dir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".json") {
                    std::string filename = entry.path().filename().string();
                    std::string groupId = filename.substr(0, filename.length() - 5); // Remove .json
                    
                    // Skip if already loaded (later tier takes precedence)
                    if (loadedGroupIds.find(groupId) != loadedGroupIds.end()) {
                        continue;
                    }
                    
                    // Try to load group from this tier
                    std::string filepath = dir + "/" + filename;
                    if (!fs::exists(filepath)) {
                        continue;
                    }
                    
                    std::ifstream file(filepath);
                    if (!file.is_open()) {
                        continue;
                    }
                    
                    Json::CharReaderBuilder builder;
                    Json::Value json;
                    std::string errors;
                    if (Json::parseFromStream(builder, file, &json, &errors)) {
                        std::string error;
                        auto group = jsonToGroupInfo(json, &error);
                        if (group.has_value()) {
                            // Remove old group if exists (from earlier tier)
                            groups.erase(
                                std::remove_if(groups.begin(), groups.end(),
                                    [&groupId](const GroupInfo& g) { return g.groupId == groupId; }),
                                groups.end()
                            );
                            groups.push_back(group.value());
                            loadedGroupIds.insert(groupId);
                            std::cerr << "[GroupStorage] Loaded group " << groupId << " from tier: " << dir << std::endl;
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            // Continue to next tier
            continue;
        }
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

