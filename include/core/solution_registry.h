#pragma once

#include "models/solution_config.h"
#include <string>
#include <unordered_map>
#include <optional>
#include <vector>
#include <mutex>

/**
 * @brief Solution Registry
 * 
 * Manages available solutions and their configurations.
 * Solutions define how to build pipelines for specific use cases.
 */
class SolutionRegistry {
public:
    static SolutionRegistry& getInstance() {
        static SolutionRegistry instance;
        return instance;
    }
    
    /**
     * @brief Register a solution configuration
     * @param config Solution configuration to register
     */
    void registerSolution(const SolutionConfig& config);
    
    /**
     * @brief Get solution configuration by ID
     * @param solutionId Solution ID
     * @return Solution config if found, empty optional otherwise
     */
    std::optional<SolutionConfig> getSolution(const std::string& solutionId) const;
    
    /**
     * @brief List all registered solution IDs
     * @return Vector of solution IDs
     */
    std::vector<std::string> listSolutions() const;
    
    /**
     * @brief Check if solution exists
     * @param solutionId Solution ID
     * @return true if solution exists
     */
    bool hasSolution(const std::string& solutionId) const;
    
    /**
     * @brief Initialize default solutions (face_detection, etc.)
     */
    void initializeDefaultSolutions();
    
private:
    SolutionRegistry() = default;
    ~SolutionRegistry() = default;
    SolutionRegistry(const SolutionRegistry&) = delete;
    SolutionRegistry& operator=(const SolutionRegistry&) = delete;
    
    mutable std::mutex mutex_;
    std::unordered_map<std::string, SolutionConfig> solutions_;
    
    /**
     * @brief Register face detection solution
     */
    void registerFaceDetectionSolution();
    
    /**
     * @brief Register object detection solution (YOLO)
     */
    void registerObjectDetectionSolution();
};

