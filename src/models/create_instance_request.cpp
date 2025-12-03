#include "models/create_instance_request.h"
#include <regex>
#include <algorithm>

bool CreateInstanceRequest::validate() const {
    validation_error_.clear();
    
    // Validate name (required, pattern: ^[A-Za-z0-9 -_]+$)
    if (name.empty()) {
        validation_error_ = "name is required";
        return false;
    }
    
    std::regex name_pattern("^[A-Za-z0-9 -_]+$");
    if (!std::regex_match(name, name_pattern)) {
        validation_error_ = "name must match pattern: ^[A-Za-z0-9 -_]+$";
        return false;
    }
    
    // Validate group (optional, but if provided must match pattern)
    if (!group.empty()) {
        std::regex group_pattern("^[A-Za-z0-9 -_]+$");
        if (!std::regex_match(group, group_pattern)) {
            validation_error_ = "group must match pattern: ^[A-Za-z0-9 -_]+$";
            return false;
        }
    }
    
    // Validate detectionSensitivity
    if (detectionSensitivity != "Low" && 
        detectionSensitivity != "Medium" && 
        detectionSensitivity != "High") {
        validation_error_ = "detectionSensitivity must be Low, Medium, or High";
        return false;
    }
    
    // Validate movementSensitivity
    if (movementSensitivity != "Low" && 
        movementSensitivity != "Medium" && 
        movementSensitivity != "High") {
        validation_error_ = "movementSensitivity must be Low, Medium, or High";
        return false;
    }
    
    // Validate sensorModality
    if (sensorModality != "RGB" && sensorModality != "Thermal") {
        validation_error_ = "sensorModality must be RGB or Thermal";
        return false;
    }
    
    // Validate detectorMode
    if (detectorMode != "SmartDetection") {
        validation_error_ = "detectorMode must be SmartDetection";
        return false;
    }
    
    // Validate inputOrientation
    if (inputOrientation < 0 || inputOrientation > 3) {
        validation_error_ = "inputOrientation must be between 0 and 3";
        return false;
    }
    
    // Validate frameRateLimit
    if (frameRateLimit < 0) {
        validation_error_ = "frameRateLimit must be >= 0";
        return false;
    }
    
    return true;
}

std::string CreateInstanceRequest::getValidationError() const {
    return validation_error_;
}

