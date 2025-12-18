#include "instances/inprocess_instance_manager.h"

InProcessInstanceManager::InProcessInstanceManager(InstanceRegistry& registry)
    : registry_(registry) {}

std::string InProcessInstanceManager::createInstance(const CreateInstanceRequest& req) {
    return registry_.createInstance(req);
}

bool InProcessInstanceManager::deleteInstance(const std::string& instanceId) {
    return registry_.deleteInstance(instanceId);
}

bool InProcessInstanceManager::startInstance(const std::string& instanceId, bool skipAutoStop) {
    return registry_.startInstance(instanceId, skipAutoStop);
}

bool InProcessInstanceManager::stopInstance(const std::string& instanceId) {
    return registry_.stopInstance(instanceId);
}

bool InProcessInstanceManager::updateInstance(const std::string& instanceId, const Json::Value& configJson) {
    return registry_.updateInstanceFromConfig(instanceId, configJson);
}

std::optional<InstanceInfo> InProcessInstanceManager::getInstance(const std::string& instanceId) const {
    return registry_.getInstance(instanceId);
}

std::vector<std::string> InProcessInstanceManager::listInstances() const {
    return registry_.listInstances();
}

std::vector<InstanceInfo> InProcessInstanceManager::getAllInstances() const {
    auto map = registry_.getAllInstances();
    std::vector<InstanceInfo> result;
    result.reserve(map.size());
    for (const auto& [_, info] : map) {
        result.push_back(info);
    }
    return result;
}

bool InProcessInstanceManager::hasInstance(const std::string& instanceId) const {
    return registry_.hasInstance(instanceId);
}

int InProcessInstanceManager::getInstanceCount() const {
    return registry_.getInstanceCount();
}

std::optional<InstanceStatistics> InProcessInstanceManager::getInstanceStatistics(const std::string& instanceId) {
    return registry_.getInstanceStatistics(instanceId);
}

std::string InProcessInstanceManager::getLastFrame(const std::string& instanceId) const {
    return registry_.getLastFrame(instanceId);
}

Json::Value InProcessInstanceManager::getInstanceConfig(const std::string& instanceId) const {
    return registry_.getInstanceConfig(instanceId);
}

bool InProcessInstanceManager::updateInstanceFromConfig(const std::string& instanceId, const Json::Value& configJson) {
    return registry_.updateInstanceFromConfig(instanceId, configJson);
}

bool InProcessInstanceManager::hasRTMPOutput(const std::string& instanceId) const {
    return registry_.hasRTMPOutput(instanceId);
}

void InProcessInstanceManager::loadPersistentInstances() {
    registry_.loadPersistentInstances();
}

int InProcessInstanceManager::checkAndHandleRetryLimits() {
    return registry_.checkAndHandleRetryLimits();
}

