#include "solutions/solution_storage.h"
#include <json/json.h>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <sstream>

SolutionStorage::SolutionStorage(const std::string& storage_dir)
    : storage_dir_(storage_dir) {
    ensureStorageDir();
}

void SolutionStorage::ensureStorageDir() const {
    try {
        if (!std::filesystem::exists(storage_dir_)) {
            std::cerr << "[SolutionStorage] Creating storage directory: " << storage_dir_ << std::endl;
            std::filesystem::create_directories(storage_dir_);
            std::cerr << "[SolutionStorage] Storage directory created successfully" << std::endl;
        } else {
            std::cerr << "[SolutionStorage] Storage directory already exists: " << storage_dir_ << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[SolutionStorage] Error creating storage directory: " << e.what() << std::endl;
        throw;
    }
}

std::string SolutionStorage::getSolutionsFilePath() const {
    return storage_dir_ + "/solutions.json";
}

Json::Value SolutionStorage::loadSolutionsFile() const {
    Json::Value root(Json::objectValue);
    
    try {
        std::string filepath = getSolutionsFilePath();
        if (!std::filesystem::exists(filepath)) {
            return root; // Return empty object if file doesn't exist
        }
        
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return root;
        }
        
        Json::CharReaderBuilder builder;
        std::string errors;
        if (!Json::parseFromStream(builder, file, &root, &errors)) {
            std::cerr << "[SolutionStorage] Failed to parse solutions file: " << errors << std::endl;
            return Json::Value(Json::objectValue); // Return empty on parse error
        }
    } catch (const std::exception& e) {
        std::cerr << "[SolutionStorage] Exception loading solutions file: " << e.what() << std::endl;
        return Json::Value(Json::objectValue);
    }
    
    return root;
}

bool SolutionStorage::saveSolutionsFile(const Json::Value& solutions) const {
    try {
        ensureStorageDir();
        
        std::string filepath = getSolutionsFilePath();
        std::cerr << "[SolutionStorage] Saving solutions to: " << filepath << std::endl;
        
        std::ofstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "[SolutionStorage] Error: Failed to open file for writing: " << filepath << std::endl;
            return false;
        }
        
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "    "; // 4 spaces for indentation
        std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
        writer->write(solutions, &file);
        file.close();
        
        std::cerr << "[SolutionStorage] Successfully saved solutions file" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[SolutionStorage] Exception saving solutions file: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "[SolutionStorage] Unknown exception saving solutions file" << std::endl;
        return false;
    }
}

Json::Value SolutionStorage::solutionConfigToJson(const SolutionConfig& config) const {
    Json::Value json(Json::objectValue);
    
    json["solutionId"] = config.solutionId;
    json["solutionName"] = config.solutionName;
    json["solutionType"] = config.solutionType;
    json["isDefault"] = config.isDefault;
    
    // Convert pipeline
    Json::Value pipeline(Json::arrayValue);
    for (const auto& node : config.pipeline) {
        Json::Value nodeJson(Json::objectValue);
        nodeJson["nodeType"] = node.nodeType;
        nodeJson["nodeName"] = node.nodeName;
        
        Json::Value params(Json::objectValue);
        for (const auto& param : node.parameters) {
            params[param.first] = param.second;
        }
        nodeJson["parameters"] = params;
        
        pipeline.append(nodeJson);
    }
    json["pipeline"] = pipeline;
    
    // Convert defaults
    Json::Value defaults(Json::objectValue);
    for (const auto& def : config.defaults) {
        defaults[def.first] = def.second;
    }
    json["defaults"] = defaults;
    
    return json;
}

std::optional<SolutionConfig> SolutionStorage::jsonToSolutionConfig(const Json::Value& json) const {
    try {
        SolutionConfig config;
        
        if (!json.isMember("solutionId") || !json["solutionId"].isString()) {
            return std::nullopt;
        }
        config.solutionId = json["solutionId"].asString();
        
        if (!json.isMember("solutionName") || !json["solutionName"].isString()) {
            return std::nullopt;
        }
        config.solutionName = json["solutionName"].asString();
        
        if (!json.isMember("solutionType") || !json["solutionType"].isString()) {
            return std::nullopt;
        }
        config.solutionType = json["solutionType"].asString();
        
        // SECURITY: Always set isDefault to false when loading from storage
        // Users cannot create default solutions - they are hardcoded in the application
        // Even if someone manually edits the storage file, we ignore the isDefault flag
        config.isDefault = false;
        
        // Parse pipeline
        if (json.isMember("pipeline") && json["pipeline"].isArray()) {
            for (const auto& nodeJson : json["pipeline"]) {
                SolutionConfig::NodeConfig node;
                
                if (!nodeJson.isMember("nodeType") || !nodeJson["nodeType"].isString()) {
                    continue;
                }
                node.nodeType = nodeJson["nodeType"].asString();
                
                if (!nodeJson.isMember("nodeName") || !nodeJson["nodeName"].isString()) {
                    continue;
                }
                node.nodeName = nodeJson["nodeName"].asString();
                
                if (nodeJson.isMember("parameters") && nodeJson["parameters"].isObject()) {
                    for (const auto& key : nodeJson["parameters"].getMemberNames()) {
                        if (nodeJson["parameters"][key].isString()) {
                            node.parameters[key] = nodeJson["parameters"][key].asString();
                        }
                    }
                }
                
                config.pipeline.push_back(node);
            }
        }
        
        // Parse defaults
        if (json.isMember("defaults") && json["defaults"].isObject()) {
            for (const auto& key : json["defaults"].getMemberNames()) {
                if (json["defaults"][key].isString()) {
                    config.defaults[key] = json["defaults"][key].asString();
                }
            }
        }
        
        return config;
    } catch (const std::exception& e) {
        std::cerr << "[SolutionStorage] Exception converting JSON to SolutionConfig: " << e.what() << std::endl;
        return std::nullopt;
    }
}

bool SolutionStorage::saveSolution(const SolutionConfig& config) {
    std::cerr << "[SolutionStorage] saveSolution called for solution: " << config.solutionId << std::endl;
    std::cerr << "[SolutionStorage] Storage directory: " << storage_dir_ << std::endl;
    
    // SECURITY: Never save default solutions to storage
    // Default solutions are hardcoded in the application and should never be persisted
    if (config.isDefault) {
        std::cerr << "[SolutionStorage] Security: Attempted to save default solution '" 
                  << config.solutionId << "' to storage. This is not allowed. Ignoring save operation." << std::endl;
        return false;
    }
    
    try {
        Json::Value solutions = loadSolutionsFile();
        std::cerr << "[SolutionStorage] Loaded existing solutions file, found " << solutions.size() << " solutions" << std::endl;
        
        // Convert solution to JSON
        Json::Value solutionJson = solutionConfigToJson(config);
        // Ensure isDefault is always false in storage (double protection)
        solutionJson["isDefault"] = false;
        std::cerr << "[SolutionStorage] Successfully converted SolutionConfig to JSON" << std::endl;
        
        // Store solution
        solutions[config.solutionId] = solutionJson;
        std::cerr << "[SolutionStorage] Added solution to JSON object" << std::endl;
        
        // Save updated solutions file
        if (!saveSolutionsFile(solutions)) {
            std::cerr << "[SolutionStorage] Failed to save solutions file" << std::endl;
            return false;
        }
        
        std::cerr << "[SolutionStorage] ✓ Successfully saved solution: " << config.solutionId << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[SolutionStorage] Exception in saveSolution: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "[SolutionStorage] Unknown exception in saveSolution" << std::endl;
        return false;
    }
}

std::optional<SolutionConfig> SolutionStorage::loadSolution(const std::string& solutionId) {
    try {
        Json::Value solutions = loadSolutionsFile();
        
        if (!solutions.isMember(solutionId)) {
            return std::nullopt;
        }
        
        return jsonToSolutionConfig(solutions[solutionId]);
    } catch (const std::exception& e) {
        std::cerr << "[SolutionStorage] Exception in loadSolution: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::vector<SolutionConfig> SolutionStorage::loadAllSolutions() {
    std::vector<SolutionConfig> result;
    
    try {
        Json::Value solutions = loadSolutionsFile();
        
        for (const auto& key : solutions.getMemberNames()) {
            auto config = jsonToSolutionConfig(solutions[key]);
            if (config.has_value()) {
                // SECURITY: Filter out any solutions marked as default
                // Even if someone manually edits the storage file and sets isDefault=true,
                // we will never load them. Default solutions must come from code only.
                if (config.value().isDefault) {
                    std::cerr << "[SolutionStorage] Security: Found solution '" << config.value().solutionId 
                              << "' marked as default in storage. Skipping (default solutions must be hardcoded)." << std::endl;
                    continue;
                }
                // Ensure isDefault is false (double protection)
                config.value().isDefault = false;
                result.push_back(config.value());
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[SolutionStorage] Exception in loadAllSolutions: " << e.what() << std::endl;
    }
    
    return result;
}

bool SolutionStorage::deleteSolution(const std::string& solutionId) {
    try {
        Json::Value solutions = loadSolutionsFile();
        
        if (!solutions.isMember(solutionId)) {
            return false; // Solution doesn't exist
        }
        
        solutions.removeMember(solutionId);
        
        // Save updated solutions file
        if (!saveSolutionsFile(solutions)) {
            std::cerr << "[SolutionStorage] Failed to save solutions file after deletion" << std::endl;
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[SolutionStorage] Exception in deleteSolution: " << e.what() << std::endl;
        return false;
    }
}

bool SolutionStorage::solutionExists(const std::string& solutionId) const {
    try {
        Json::Value solutions = loadSolutionsFile();
        return solutions.isMember(solutionId);
    } catch (const std::exception& e) {
        return false;
    }
}

