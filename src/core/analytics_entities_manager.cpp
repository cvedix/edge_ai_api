#include "core/analytics_entities_manager.h"
#include "core/securt_line_manager.h"
#include "instances/instance_manager.h"
#include <iostream>

Json::Value AnalyticsEntitiesManager::getAnalyticsEntities(
    const std::string &instanceId) const {
  Json::Value result;
  
  // Areas
  result["crossingAreas"] = getCrossingAreas(instanceId);
  result["intrusionAreas"] = getIntrusionAreas(instanceId);
  result["loiteringAreas"] = getLoiteringAreas(instanceId);
  result["crowdingAreas"] = getCrowdingAreas(instanceId);
  result["occupancyAreas"] = getOccupancyAreas(instanceId);
  result["crowdEstimationAreas"] = getCrowdEstimationAreas(instanceId);
  result["dwellingAreas"] = getDwellingAreas(instanceId);
  result["armedPersonAreas"] = getArmedPersonAreas(instanceId);
  result["objectLeftAreas"] = getObjectLeftAreas(instanceId);
  result["objectRemovedAreas"] = getObjectRemovedAreas(instanceId);
  result["fallenPersonAreas"] = getFallenPersonAreas(instanceId);
  
  // Lines
  result["crossingLines"] = getCrossingLines(instanceId);
  result["countingLines"] = getCountingLines(instanceId);
  result["tailgatingLines"] = getTailgatingLines(instanceId);
  
  return result;
}

Json::Value AnalyticsEntitiesManager::getCrossingAreas(
    const std::string &instanceId) const {
  // TODO: Integrate with Areas manager when available (TASK-008)
  Json::Value areas(Json::arrayValue);
  return areas;
}

Json::Value AnalyticsEntitiesManager::getIntrusionAreas(
    const std::string &instanceId) const {
  // TODO: Integrate with Areas manager when available (TASK-008)
  Json::Value areas(Json::arrayValue);
  return areas;
}

Json::Value AnalyticsEntitiesManager::getLoiteringAreas(
    const std::string &instanceId) const {
  // TODO: Integrate with Areas manager when available (TASK-008)
  Json::Value areas(Json::arrayValue);
  return areas;
}

Json::Value AnalyticsEntitiesManager::getCrowdingAreas(
    const std::string &instanceId) const {
  // TODO: Integrate with Areas manager when available (TASK-008)
  Json::Value areas(Json::arrayValue);
  return areas;
}

Json::Value AnalyticsEntitiesManager::getOccupancyAreas(
    const std::string &instanceId) const {
  // TODO: Integrate with Areas manager when available (TASK-008)
  Json::Value areas(Json::arrayValue);
  return areas;
}

Json::Value AnalyticsEntitiesManager::getCrowdEstimationAreas(
    const std::string &instanceId) const {
  // TODO: Integrate with Areas manager when available (TASK-008)
  Json::Value areas(Json::arrayValue);
  return areas;
}

Json::Value AnalyticsEntitiesManager::getDwellingAreas(
    const std::string &instanceId) const {
  // TODO: Integrate with Areas manager when available (TASK-008)
  Json::Value areas(Json::arrayValue);
  return areas;
}

Json::Value AnalyticsEntitiesManager::getArmedPersonAreas(
    const std::string &instanceId) const {
  // TODO: Integrate with Areas manager when available (TASK-008)
  Json::Value areas(Json::arrayValue);
  return areas;
}

Json::Value AnalyticsEntitiesManager::getObjectLeftAreas(
    const std::string &instanceId) const {
  // TODO: Integrate with Areas manager when available (TASK-008)
  Json::Value areas(Json::arrayValue);
  return areas;
}

Json::Value AnalyticsEntitiesManager::getObjectRemovedAreas(
    const std::string &instanceId) const {
  // TODO: Integrate with Areas manager when available (TASK-008)
  Json::Value areas(Json::arrayValue);
  return areas;
}

Json::Value AnalyticsEntitiesManager::getFallenPersonAreas(
    const std::string &instanceId) const {
  // TODO: Integrate with Areas manager when available (TASK-008)
  Json::Value areas(Json::arrayValue);
  return areas;
}

void AnalyticsEntitiesManager::setLineManager(SecuRTLineManager *manager) {
  line_manager_ = manager;
}

Json::Value AnalyticsEntitiesManager::getCrossingLines(
    const std::string &instanceId) const {
  if (!line_manager_) {
    Json::Value lines(Json::arrayValue);
    return lines;
  }
  return line_manager_->getCrossingLines(instanceId);
}

Json::Value AnalyticsEntitiesManager::getCountingLines(
    const std::string &instanceId) const {
  if (!line_manager_) {
    Json::Value lines(Json::arrayValue);
    return lines;
  }
  return line_manager_->getCountingLines(instanceId);
}

Json::Value AnalyticsEntitiesManager::getTailgatingLines(
    const std::string &instanceId) const {
  if (!line_manager_) {
    Json::Value lines(Json::arrayValue);
    return lines;
  }
  return line_manager_->getTailgatingLines(instanceId);
}

