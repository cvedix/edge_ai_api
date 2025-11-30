#include "models/model_upload_handler.h"
#include <drogon/HttpResponse.h>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <ctime>
#include <chrono>

std::string ModelUploadHandler::models_dir_ = "./models";

void ModelUploadHandler::setModelsDirectory(const std::string& dir) {
    models_dir_ = dir;
}

std::string ModelUploadHandler::getModelsDirectory() const {
    return models_dir_;
}

bool ModelUploadHandler::isValidModelFile(const std::string& filename) const {
    // Allow .onnx, .weights, .cfg, .pt, .pth, .pb, .tflite files
    std::string lower = filename;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    // Helper lambda to check suffix (C++17 compatible)
    auto hasSuffix = [](const std::string& str, const std::string& suffix) -> bool {
        if (suffix.length() > str.length()) return false;
        return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
    };
    
    return hasSuffix(lower, ".onnx") ||
           hasSuffix(lower, ".weights") ||
           hasSuffix(lower, ".cfg") ||
           hasSuffix(lower, ".pt") ||
           hasSuffix(lower, ".pth") ||
           hasSuffix(lower, ".pb") ||
           hasSuffix(lower, ".tflite");
}

std::string ModelUploadHandler::sanitizeFilename(const std::string& filename) const {
    std::string sanitized;
    for (char c : filename) {
        // Allow alphanumeric, dot, dash, underscore
        if (std::isalnum(c) || c == '.' || c == '-' || c == '_') {
            sanitized += c;
        }
    }
    // Remove any path traversal attempts
    if (sanitized.find("..") != std::string::npos) {
        sanitized.clear();
    }
    return sanitized;
}

void ModelUploadHandler::uploadModel(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
    
    try {
        // Check content type from header
        std::string contentType = req->getHeader("Content-Type");
        if (contentType.find("multipart/form-data") == std::string::npos &&
            contentType.find("application/octet-stream") == std::string::npos &&
            contentType.find("binary") == std::string::npos) {
            callback(createErrorResponse(400, "Invalid content type", 
                "Request must be multipart/form-data, application/octet-stream, or contain binary data"));
            return;
        }
        
        // Get filename from Content-Disposition header or query parameter
        std::string originalFilename;
        std::string contentDisposition = req->getHeader("Content-Disposition");
        
        // Try to extract filename from Content-Disposition header
        if (!contentDisposition.empty()) {
            size_t filenamePos = contentDisposition.find("filename=");
            if (filenamePos != std::string::npos) {
                filenamePos += 9; // length of "filename="
                if (contentDisposition[filenamePos] == '"') {
                    filenamePos++; // skip opening quote
                    size_t endPos = contentDisposition.find('"', filenamePos);
                    if (endPos != std::string::npos) {
                        originalFilename = contentDisposition.substr(filenamePos, endPos - filenamePos);
                    }
                } else {
                    size_t endPos = contentDisposition.find_first_of("; \r\n", filenamePos);
                    if (endPos != std::string::npos) {
                        originalFilename = contentDisposition.substr(filenamePos, endPos - filenamePos);
                    } else {
                        originalFilename = contentDisposition.substr(filenamePos);
                    }
                }
            }
        }
        
        // If no filename from header, try query parameter
        if (originalFilename.empty()) {
            originalFilename = req->getParameter("filename");
        }
        
        // If still no filename, use default
        if (originalFilename.empty()) {
            originalFilename = "uploaded_model.onnx";
        }
        
        // Validate file extension
        if (!isValidModelFile(originalFilename)) {
            callback(createErrorResponse(400, "Invalid file type", 
                "Only model files (.onnx, .weights, .cfg, .pt, .pth, .pb, .tflite) are allowed"));
            return;
        }
        
        // Sanitize filename
        std::string sanitizedFilename = sanitizeFilename(originalFilename);
        if (sanitizedFilename.empty()) {
            callback(createErrorResponse(400, "Invalid filename", 
                "Filename contains invalid characters"));
            return;
        }
        
        // Ensure models directory exists
        std::string modelsDir = getModelsDirectory();
        std::filesystem::path modelsPath(modelsDir);
        if (!std::filesystem::exists(modelsPath)) {
            std::filesystem::create_directories(modelsPath);
        }
        
        // Create full file path
        std::filesystem::path filePath = modelsPath / sanitizedFilename;
        
        // Check if file already exists
        if (std::filesystem::exists(filePath)) {
            callback(createErrorResponse(409, "File exists", 
                "A model file with this name already exists. Please delete it first or use a different name."));
            return;
        }
        
        // Get request body
        auto body = req->getBody();
        if (body.empty()) {
            callback(createErrorResponse(400, "No file data", 
                "Request body is empty. Please include file data."));
            return;
        }
        
        // Save file
        std::ofstream outFile(filePath, std::ios::binary);
        if (!outFile.is_open()) {
            callback(createErrorResponse(500, "File save failed", 
                "Could not save uploaded file"));
            return;
        }
        
        // Write file content from body
        outFile.write(reinterpret_cast<const char*>(body.data()), body.size());
        outFile.close();
        
        // Get file size
        auto fileSize = std::filesystem::file_size(filePath);
        
        // Build response
        Json::Value response;
        response["success"] = true;
        response["message"] = "Model file uploaded successfully";
        response["filename"] = sanitizedFilename;
        response["originalFilename"] = originalFilename;
        response["path"] = std::filesystem::canonical(filePath).string();
        response["size"] = static_cast<Json::Int64>(fileSize);
        response["url"] = "/v1/core/models/" + sanitizedFilename;
        
        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k201Created);
        // Add CORS headers for Swagger UI
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->addHeader("Access-Control-Allow-Methods", "POST, GET, DELETE, OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Content-Disposition, Authorization");
        resp->addHeader("Access-Control-Expose-Headers", "Content-Type, Content-Disposition");
        resp->addHeader("Access-Control-Max-Age", "3600");
        
        std::cerr << "[ModelUploadHandler] Model file uploaded: " << sanitizedFilename 
                  << " (" << fileSize << " bytes)" << std::endl;
        
        callback(resp);
        
    } catch (const std::exception& e) {
        std::cerr << "[ModelUploadHandler] Exception: " << e.what() << std::endl;
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        std::cerr << "[ModelUploadHandler] Unknown exception" << std::endl;
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void ModelUploadHandler::listModels(
    const HttpRequestPtr & /*req*/,
    std::function<void(const HttpResponsePtr &)> &&callback) {
    
    try {
        std::string modelsDir = getModelsDirectory();
        std::filesystem::path modelsPath(modelsDir);
        
        Json::Value response;
        Json::Value models(Json::arrayValue);
        
        if (!std::filesystem::exists(modelsPath)) {
            // Return empty list if directory doesn't exist
            response["success"] = true;
            response["models"] = models;
            response["count"] = 0;
            
            auto resp = HttpResponse::newHttpJsonResponse(response);
            resp->setStatusCode(k200OK);
            resp->addHeader("Access-Control-Allow-Origin", "*");
            resp->addHeader("Access-Control-Allow-Methods", "POST, GET, DELETE, OPTIONS");
            resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
            callback(resp);
            return;
        }
        
        // List all model files
        int count = 0;
        for (const auto& entry : std::filesystem::directory_iterator(modelsPath)) {
            if (entry.is_regular_file() && isValidModelFile(entry.path().filename().string())) {
                Json::Value model;
                model["filename"] = entry.path().filename().string();
                model["path"] = std::filesystem::canonical(entry.path()).string();
                model["size"] = static_cast<Json::Int64>(std::filesystem::file_size(entry.path()));
                
                auto modTime = std::filesystem::last_write_time(entry.path());
                auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    modTime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
                auto timeT = std::chrono::system_clock::to_time_t(sctp);
                
                std::stringstream ss;
                ss << std::put_time(std::localtime(&timeT), "%Y-%m-%d %H:%M:%S");
                model["modified"] = ss.str();
                
                models.append(model);
                count++;
            }
        }
        
        response["success"] = true;
        response["models"] = models;
        response["count"] = count;
        response["directory"] = std::filesystem::canonical(modelsPath).string();
        
        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k200OK);
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->addHeader("Access-Control-Allow-Methods", "POST, GET, DELETE, OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
        callback(resp);
        
    } catch (const std::exception& e) {
        std::cerr << "[ModelUploadHandler] Exception: " << e.what() << std::endl;
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        std::cerr << "[ModelUploadHandler] Unknown exception" << std::endl;
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void ModelUploadHandler::deleteModel(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
    
    try {
        // Get model name from path parameter
        std::string modelName = req->getParameter("modelName");
        if (modelName.empty()) {
            callback(createErrorResponse(400, "Invalid request", "Model name is required"));
            return;
        }
        
        // Sanitize filename
        std::string sanitizedFilename = sanitizeFilename(modelName);
        if (sanitizedFilename.empty() || sanitizedFilename != modelName) {
            callback(createErrorResponse(400, "Invalid filename", "Invalid model name"));
            return;
        }
        
        // Build file path
        std::string modelsDir = getModelsDirectory();
        std::filesystem::path filePath = std::filesystem::path(modelsDir) / sanitizedFilename;
        
        // Check if file exists
        if (!std::filesystem::exists(filePath)) {
            callback(createErrorResponse(404, "Not found", "Model file not found"));
            return;
        }
        
        // Delete file
        if (!std::filesystem::remove(filePath)) {
            callback(createErrorResponse(500, "Delete failed", "Could not delete model file"));
            return;
        }
        
        Json::Value response;
        response["success"] = true;
        response["message"] = "Model file deleted successfully";
        response["filename"] = sanitizedFilename;
        
        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k200OK);
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->addHeader("Access-Control-Allow-Methods", "POST, GET, DELETE, OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
        
        std::cerr << "[ModelUploadHandler] Model file deleted: " << sanitizedFilename << std::endl;
        
        callback(resp);
        
    } catch (const std::exception& e) {
        std::cerr << "[ModelUploadHandler] Exception: " << e.what() << std::endl;
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        std::cerr << "[ModelUploadHandler] Unknown exception" << std::endl;
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void ModelUploadHandler::handleOptions(
    const HttpRequestPtr & /*req*/,
    std::function<void(const HttpResponsePtr &)> &&callback) {
    
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "POST, GET, DELETE, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
    resp->addHeader("Access-Control-Max-Age", "3600");
    callback(resp);
}

HttpResponsePtr ModelUploadHandler::createErrorResponse(
    int statusCode,
    const std::string& error,
    const std::string& message) const {
    
    Json::Value errorJson;
    errorJson["success"] = false;
    errorJson["error"] = error;
    if (!message.empty()) {
        errorJson["message"] = message;
    }
    
    auto resp = HttpResponse::newHttpJsonResponse(errorJson);
    resp->setStatusCode(static_cast<HttpStatusCode>(statusCode));
    
    return resp;
}

