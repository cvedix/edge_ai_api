#pragma once

#include "models/solution_config.h"
#include <json/json.h>
#include <string>
#include <vector>
#include <optional>
#include <filesystem>

/**
 * @brief Solution Storage
 * 
 * Handles persistent storage of custom solutions to/from JSON files.
 * Default solutions are not stored, only custom user-created solutions.
 */
class SolutionStorage {
public:
    /**
     * @brief Constructor
     * @param storage_dir Directory to store solution JSON files (default: ./solutions)
     */
    explicit SolutionStorage(const std::string& storage_dir = "./solutions");
    
    /**
     * @brief Save solution to JSON file
     * @param config Solution configuration
     * @return true if successful
     */
    bool saveSolution(const SolutionConfig& config);
    
    /**
     * @brief Load solution from JSON file
     * @param solutionId Solution ID
     * @return Solution config if found, empty optional otherwise
     */
    std::optional<SolutionConfig> loadSolution(const std::string& solutionId);
    
    /**
     * @brief Load all custom solutions from storage directory
     * @return Vector of solution configs that were loaded
     */
    std::vector<SolutionConfig> loadAllSolutions();
    
    /**
     * @brief Delete solution JSON file
     * @param solutionId Solution ID
     * @return true if successful
     */
    bool deleteSolution(const std::string& solutionId);
    
    /**
     * @brief Check if solution file exists
     * @param solutionId Solution ID
     * @return true if file exists
     */
    bool solutionExists(const std::string& solutionId) const;
    
    /**
     * @brief Get storage directory path
     */
    std::string getStorageDir() const { return storage_dir_; }
    
private:
    std::string storage_dir_;
    
    /**
     * @brief Ensure storage directory exists
     */
    void ensureStorageDir() const;
    
    /**
     * @brief Get solutions file path
     */
    std::string getSolutionsFilePath() const;
    
    /**
     * @brief Load solutions file
     */
    Json::Value loadSolutionsFile() const;
    
    /**
     * @brief Save solutions file
     */
    bool saveSolutionsFile(const Json::Value& solutions) const;
    
    /**
     * @brief Convert SolutionConfig to JSON
     */
    Json::Value solutionConfigToJson(const SolutionConfig& config) const;
    
    /**
     * @brief Convert JSON to SolutionConfig
     */
    std::optional<SolutionConfig> jsonToSolutionConfig(const Json::Value& json) const;
};

