#include "instances/instance_manager_factory.h"
#include <cstdlib>
#include <iostream>
#include <algorithm>

// Static member initialization
std::unique_ptr<InstanceRegistry> InstanceManagerFactory::s_registry_ = nullptr;

std::unique_ptr<IInstanceManager> InstanceManagerFactory::create(
    InstanceExecutionMode mode,
    SolutionRegistry& solutionRegistry,
    PipelineBuilder& pipelineBuilder,
    InstanceStorage& instanceStorage,
    const std::string& workerExecutable) {
    
    std::cout << "[InstanceManagerFactory] Creating manager in " 
              << getModeName(mode) << " mode" << std::endl;
    
    switch (mode) {
        case InstanceExecutionMode::SUBPROCESS:
            return createSubprocess(solutionRegistry, instanceStorage, workerExecutable);
        
        case InstanceExecutionMode::IN_PROCESS:
        default:
            return createInProcess(solutionRegistry, pipelineBuilder, instanceStorage);
    }
}

std::unique_ptr<IInstanceManager> InstanceManagerFactory::createInProcess(
    SolutionRegistry& solutionRegistry,
    PipelineBuilder& pipelineBuilder,
    InstanceStorage& instanceStorage) {
    
    // Create InstanceRegistry if not exists
    if (!s_registry_) {
        s_registry_ = std::make_unique<InstanceRegistry>(
            solutionRegistry, pipelineBuilder, instanceStorage);
    }
    
    return std::make_unique<InProcessInstanceManager>(*s_registry_);
}

std::unique_ptr<IInstanceManager> InstanceManagerFactory::createSubprocess(
    SolutionRegistry& solutionRegistry,
    InstanceStorage& instanceStorage,
    const std::string& workerExecutable) {
    
    return std::make_unique<SubprocessInstanceManager>(
        solutionRegistry, instanceStorage, workerExecutable);
}

InstanceExecutionMode InstanceManagerFactory::getExecutionModeFromEnv() {
    const char* mode_env = std::getenv("EDGE_AI_EXECUTION_MODE");
    
    if (mode_env == nullptr) {
        // Default to in-process for backward compatibility
        return InstanceExecutionMode::IN_PROCESS;
    }
    
    std::string mode_str(mode_env);
    // Convert to lowercase for comparison
    std::transform(mode_str.begin(), mode_str.end(), mode_str.begin(), ::tolower);
    
    if (mode_str == "subprocess" || mode_str == "isolated" || mode_str == "worker") {
        return InstanceExecutionMode::SUBPROCESS;
    }
    
    // Default to in-process
    return InstanceExecutionMode::IN_PROCESS;
}

std::string InstanceManagerFactory::getModeName(InstanceExecutionMode mode) {
    switch (mode) {
        case InstanceExecutionMode::SUBPROCESS:
            return "subprocess (isolated)";
        case InstanceExecutionMode::IN_PROCESS:
        default:
            return "in-process (legacy)";
    }
}

