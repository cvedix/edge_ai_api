#include "api/recognition_handler.h"
#include "core/logging_flags.h"
#include "core/logger.h"
#include <drogon/HttpResponse.h>
#include <json/json.h>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <random>
#include <iomanip>
#include <opencv2/opencv.hpp>

// Static storage members
std::unordered_map<std::string, std::vector<std::string>> RecognitionHandler::face_subjects_storage_;
std::mutex RecognitionHandler::storage_mutex_;

HttpResponsePtr RecognitionHandler::createErrorResponse(int statusCode, const std::string& error, const std::string& message) const {
    Json::Value errorResponse;
    errorResponse["error"] = error;
    if (!message.empty()) {
        errorResponse["message"] = message;
    }
    
    auto resp = HttpResponse::newHttpJsonResponse(errorResponse);
    resp->setStatusCode(static_cast<HttpStatusCode>(statusCode));
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type, x-api-key");
    
    return resp;
}

bool RecognitionHandler::validateApiKey(const HttpRequestPtr &req, std::string& error) const {
    std::string apiKey = req->getHeader("x-api-key");
    if (apiKey.empty()) {
        error = "Missing x-api-key header";
        return false;
    }
    
    // TODO: Implement actual API key validation logic
    // For now, accept any non-empty API key
    // In production, validate against a database or configuration
    return true;
}

bool RecognitionHandler::isBase64(const std::string& str) const {
    if (str.empty()) return false;
    
    // Base64 characters: A-Z, a-z, 0-9, +, /, and = for padding
    for (char c : str) {
        if (!std::isalnum(c) && c != '+' && c != '/' && c != '=' && c != '\n' && c != '\r' && c != ' ') {
            return false;
        }
    }
    
    // Check for base64 padding pattern
    size_t paddingCount = 0;
    for (size_t i = str.length() - 1; i > 0 && str[i] == '='; --i) {
        paddingCount++;
    }
    
    return paddingCount <= 2;
}

bool RecognitionHandler::decodeBase64(const std::string& base64Str, std::vector<unsigned char>& output) const {
    // Remove whitespace
    std::string cleanBase64;
    for (char c : base64Str) {
        if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
            cleanBase64 += c;
        }
    }
    
    if (cleanBase64.empty()) {
        return false;
    }
    
    // Base64 decoding
    const std::string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    int val = 0, valb = -8;
    for (unsigned char c : cleanBase64) {
        if (c == '=') break;
        
        size_t pos = base64_chars.find(c);
        if (pos == std::string::npos) {
            return false; // Invalid character
        }
        
        val = (val << 6) + pos;
        valb += 6;
        
        if (valb >= 0) {
            output.push_back((val >> valb) & 0xFF);
            valb -= 8;
        }
    }
    
    return true;
}

bool RecognitionHandler::extractImageData(const HttpRequestPtr &req, std::vector<unsigned char>& imageData, std::string& error) const {
    std::string contentType = req->getHeader("Content-Type");
    bool isMultipart = contentType.find("multipart/form-data") != std::string::npos;
    
    if (!isMultipart) {
        error = "Content-Type must be multipart/form-data";
        return false;
    }
    
    // Get boundary from Content-Type header
    std::string boundary;
    size_t boundaryPos = contentType.find("boundary=");
    if (boundaryPos != std::string::npos) {
        boundaryPos += 9; // length of "boundary="
        size_t endPos = contentType.find_first_of("; \r\n", boundaryPos);
        if (endPos != std::string::npos) {
            boundary = contentType.substr(boundaryPos, endPos - boundaryPos);
        } else {
            boundary = contentType.substr(boundaryPos);
        }
        // Remove quotes if present
        if (!boundary.empty() && boundary.front() == '"' && boundary.back() == '"') {
            boundary = boundary.substr(1, boundary.length() - 2);
        }
    }
    
    if (boundary.empty()) {
        error = "Could not find boundary in Content-Type header";
        return false;
    }
    
    // Get request body
    auto body = req->getBody();
    if (body.empty()) {
        error = "Request body is empty";
        return false;
    }
    
    // Parse multipart body to find file field
    std::string bodyStr(reinterpret_cast<const char*>(body.data()), body.size());
    std::string boundaryMarker = "--" + boundary;
    
    // Find the part with name="file"
    size_t partStart = bodyStr.find(boundaryMarker);
    if (partStart == std::string::npos) {
        error = "Could not find multipart boundary";
        return false;
    }
    
    // Search for file field
    size_t fileFieldPos = bodyStr.find("name=\"file\"", partStart);
    if (fileFieldPos == std::string::npos) {
        fileFieldPos = bodyStr.find("name='file'", partStart);
    }
    if (fileFieldPos == std::string::npos) {
        fileFieldPos = bodyStr.find("name=file", partStart);
    }
    
    if (fileFieldPos == std::string::npos) {
        error = "Could not find 'file' field in multipart form data";
        return false;
    }
    
    // Find content start (after headers and blank line)
    size_t contentStart = bodyStr.find("\r\n\r\n", fileFieldPos);
    if (contentStart == std::string::npos) {
        contentStart = bodyStr.find("\n\n", fileFieldPos);
    }
    if (contentStart == std::string::npos) {
        error = "Could not find content start in multipart data";
        return false;
    }
    
    contentStart += 2; // Skip \r\n or \n
    if (contentStart < bodyStr.length() && 
        (bodyStr[contentStart] == '\r' || bodyStr[contentStart] == '\n')) {
        contentStart++;
    }
    
    // Find content end (before next boundary)
    size_t nextBoundary = bodyStr.find(boundaryMarker, contentStart);
    size_t contentEnd = (nextBoundary != std::string::npos) ? nextBoundary : bodyStr.length();
    
    // Remove trailing \r\n before boundary
    while (contentEnd > contentStart && 
           (bodyStr[contentEnd-1] == '\r' || bodyStr[contentEnd-1] == '\n')) {
        contentEnd--;
    }
    
    if (contentEnd <= contentStart) {
        error = "File field has no content";
        return false;
    }
    
    std::string fileContent = bodyStr.substr(contentStart, contentEnd - contentStart);
    
    // Check if content is base64 encoded
    if (isBase64(fileContent)) {
        // Decode base64
        if (!decodeBase64(fileContent, imageData)) {
            error = "Failed to decode base64 image data";
            return false;
        }
    } else {
        // Treat as binary data
        imageData.assign(fileContent.begin(), fileContent.end());
    }
    
    if (imageData.empty()) {
        error = "Image data is empty after extraction";
        return false;
    }
    
    return true;
}

void RecognitionHandler::parseQueryParameters(const HttpRequestPtr &req,
                                              int& limit,
                                              int& predictionCount,
                                              double& detProbThreshold,
                                              std::string& facePlugins,
                                              std::string& status,
                                              bool& detectFaces) const {
    // Parse limit (default: 0 = no limit)
    std::string limitStr = req->getParameter("limit");
    if (!limitStr.empty()) {
        try {
            limit = std::stoi(limitStr);
        } catch (...) {
            limit = 0;
        }
    } else {
        limit = 0;
    }
    
    // Parse prediction_count
    std::string predictionCountStr = req->getParameter("prediction_count");
    if (!predictionCountStr.empty()) {
        try {
            predictionCount = std::stoi(predictionCountStr);
        } catch (...) {
            predictionCount = 1;
        }
    } else {
        predictionCount = 1;
    }
    
    // Parse det_prob_threshold
    std::string detProbThresholdStr = req->getParameter("det_prob_threshold");
    if (!detProbThresholdStr.empty()) {
        try {
            detProbThreshold = std::stod(detProbThresholdStr);
        } catch (...) {
            detProbThreshold = 0.5;
        }
    } else {
        detProbThreshold = 0.5;
    }
    
    // Parse face_plugins
    facePlugins = req->getParameter("face_plugins");
    
    // Parse status
    status = req->getParameter("status");
    
    // Parse detect_faces
    std::string detectFacesStr = req->getParameter("detect_faces");
    if (!detectFacesStr.empty()) {
        std::transform(detectFacesStr.begin(), detectFacesStr.end(), detectFacesStr.begin(), ::tolower);
        detectFaces = (detectFacesStr == "true" || detectFacesStr == "1" || detectFacesStr == "yes");
    } else {
        detectFaces = true; // Default to true
    }
}

Json::Value RecognitionHandler::processFaceRecognition(const std::vector<unsigned char>& imageData,
                                                      int limit,
                                                      int predictionCount,
                                                      double detProbThreshold,
                                                      const std::string& facePlugins,
                                                      bool detectFaces) const {
    Json::Value result(Json::arrayValue);
    
    try {
        // Decode image from memory
        cv::Mat image = cv::imdecode(imageData, cv::IMREAD_COLOR);
        
        if (image.empty()) {
            // Return empty result if image cannot be decoded
        return result;
    }
    
        // TODO: Integrate with actual face recognition service
        // For now, return a mock response matching the expected format
        // This should be replaced with actual face recognition logic
        
        // Mock face detection result
        Json::Value faceResult;
        
        // Mock bounding box
        Json::Value box;
        box["probability"] = 1.0;
        box["x_max"] = image.cols - 100;
        box["y_max"] = image.rows - 100;
        box["x_min"] = 100;
        box["y_min"] = 100;
        faceResult["box"] = box;
        
        // Mock landmarks (5 points)
        Json::Value landmarks(Json::arrayValue);
        landmarks.append(Json::Value(Json::arrayValue));
        landmarks[0].append(image.cols / 2 - 50);
        landmarks[0].append(image.rows / 2 - 50);
        landmarks.append(Json::Value(Json::arrayValue));
        landmarks[1].append(image.cols / 2 + 50);
        landmarks[1].append(image.rows / 2 - 30);
        landmarks.append(Json::Value(Json::arrayValue));
        landmarks[2].append(image.cols / 2);
        landmarks[2].append(image.rows / 2);
        landmarks.append(Json::Value(Json::arrayValue));
        landmarks[3].append(image.cols / 2 - 30);
        landmarks[3].append(image.rows / 2 + 30);
        landmarks.append(Json::Value(Json::arrayValue));
        landmarks[4].append(image.cols / 2 + 30);
        landmarks[4].append(image.rows / 2 + 50);
        faceResult["landmarks"] = landmarks;
        
        // Mock subjects (recognition results)
        Json::Value subjects(Json::arrayValue);
        Json::Value subject;
        subject["similarity"] = 0.97858;
        subject["subject"] = "subject1";
        subjects.append(subject);
        faceResult["subjects"] = subjects;
        
        // Mock execution time
        Json::Value executionTime;
        executionTime["age"] = 28.0;
        executionTime["gender"] = 26.0;
        executionTime["detector"] = 117.0;
        executionTime["calculator"] = 45.0;
        executionTime["mask"] = 36.0;
        faceResult["execution_time"] = executionTime;
        
        result.append(faceResult);
        
    } catch (const std::exception& e) {
        PLOG_ERROR << "[RecognitionHandler] Error processing face recognition: " << e.what();
    }
    
    return result;
}

void RecognitionHandler::recognizeFaces(const HttpRequestPtr &req,
                                       std::function<void(const HttpResponsePtr &)> &&callback) {
    auto start_time = std::chrono::steady_clock::now();
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] POST /v1/recognition/recognize - Recognize faces";
        PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
    }
    
    try {
        // Validate API key
        std::string apiKeyError;
        if (!validateApiKey(req, apiKeyError)) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] POST /v1/recognition/recognize - " << apiKeyError;
            }
            callback(createErrorResponse(401, "Unauthorized", apiKeyError));
            return;
        }
        
        // Extract image data
        std::vector<unsigned char> imageData;
        std::string imageError;
        if (!extractImageData(req, imageData, imageError)) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] POST /v1/recognition/recognize - " << imageError;
            }
            callback(createErrorResponse(400, "Invalid request", imageError));
            return;
        }
        
        // Parse query parameters
        int limit = 0;
        int predictionCount = 1;
        double detProbThreshold = 0.5;
        std::string facePlugins;
        std::string status;
        bool detectFaces = true;
        
        parseQueryParameters(req, limit, predictionCount, detProbThreshold, facePlugins, status, detectFaces);
        
        if (isApiLoggingEnabled()) {
            PLOG_DEBUG << "[API] Recognition parameters - limit: " << limit 
                      << ", prediction_count: " << predictionCount
                      << ", det_prob_threshold: " << detProbThreshold
                      << ", detect_faces: " << (detectFaces ? "true" : "false");
        }
        
        // Process face recognition
        Json::Value recognitionResult = processFaceRecognition(imageData, limit, predictionCount, 
                                                               detProbThreshold, facePlugins, detectFaces);
        
        // Build response
        Json::Value response;
        response["result"] = recognitionResult;
        
        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k200OK);
        
        // Add CORS headers
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type, x-api-key");
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_INFO << "[API] POST /v1/recognition/recognize - Success - " << duration.count() << "ms";
        }
        
        callback(resp);
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] POST /v1/recognition/recognize - Exception: " << e.what() << " - " << duration.count() << "ms";
        }
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] POST /v1/recognition/recognize - Unknown exception - " << duration.count() << "ms";
        }
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void RecognitionHandler::handleOptions(const HttpRequestPtr &req,
                                     std::function<void(const HttpResponsePtr &)> &&callback) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type, x-api-key");
    resp->addHeader("Access-Control-Max-Age", "3600");
    callback(resp);
}

std::string RecognitionHandler::generateImageId() const {
    // Generate UUID-like ID: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    std::uniform_int_distribution<> dis2(8, 11);
    
    std::stringstream ss;
    int i;
    ss << std::hex;
    for (i = 0; i < 8; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 4; i++) {
        ss << dis(gen);
    }
    ss << "-";
    ss << dis2(gen);
    for (i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 4; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 12; i++) {
        ss << dis(gen);
    }
    return ss.str();
}

bool RecognitionHandler::extractImageFromJson(const HttpRequestPtr &req, std::vector<unsigned char>& imageData, std::string& error) const {
    // Parse JSON body
    auto json = req->getJsonObject();
    if (!json) {
        error = "Request body must be valid JSON";
        return false;
    }
    
    // Check if file field exists
    if (!json->isMember("file") || !(*json)["file"].isString()) {
        error = "Missing required field: file (base64 encoded image)";
        return false;
    }
    
    std::string fileBase64 = (*json)["file"].asString();
    if (fileBase64.empty()) {
        error = "File field is empty";
        return false;
    }
    
    // Decode base64
    if (!decodeBase64(fileBase64, imageData)) {
        error = "Failed to decode base64 image data";
        return false;
    }
    
    if (imageData.empty()) {
        error = "Image data is empty after decoding";
        return false;
    }
    
    return true;
}

bool RecognitionHandler::registerSubject(const std::string& subjectName,
                                        const std::vector<unsigned char>& imageData,
                                        double detProbThreshold,
                                        std::string& imageId,
                                        std::string& error) const {
    try {
        // Validate image can be decoded
        cv::Mat image = cv::imdecode(imageData, cv::IMREAD_COLOR);
        if (image.empty()) {
            error = "Invalid image format or corrupted image data";
            return false;
        }
        
        // TODO: Implement actual face detection and subject registration logic
        // For now, generate image ID and return success
        // In production, this should:
        // 1. Detect face in image
        // 2. Extract face features
        // 3. Store features associated with subject name
        // 4. Save image to storage
        
        imageId = generateImageId();
        
        // Store subject and image mapping in memory storage
        addImageToSubject(subjectName, imageId);
        
        return true;
        
    } catch (const std::exception& e) {
        error = "Error processing image: " + std::string(e.what());
        return false;
    }
}

void RecognitionHandler::registerFaceSubject(const HttpRequestPtr &req,
                                            std::function<void(const HttpResponsePtr &)> &&callback) {
    auto start_time = std::chrono::steady_clock::now();
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] POST /v1/recognition/faces - Register face subject";
        PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
    }
    
    try {
        // Validate API key
        std::string apiKeyError;
        if (!validateApiKey(req, apiKeyError)) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] POST /v1/recognition/faces - " << apiKeyError;
            }
            callback(createErrorResponse(401, "Unauthorized", apiKeyError));
            return;
        }
        
        // Parse query parameters
        std::string subjectName = req->getParameter("subject");
        if (subjectName.empty()) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] POST /v1/recognition/faces - Missing required parameter: subject";
            }
            callback(createErrorResponse(400, "Invalid request", "Missing required query parameter: subject"));
            return;
        }
        
        std::string detProbThresholdStr = req->getParameter("det_prob_threshold");
        double detProbThreshold = 0.5; // Default
        if (!detProbThresholdStr.empty()) {
            try {
                detProbThreshold = std::stod(detProbThresholdStr);
            } catch (...) {
                detProbThreshold = 0.5;
            }
        }
        
        if (isApiLoggingEnabled()) {
            PLOG_DEBUG << "[API] Register subject: " << subjectName 
                      << ", det_prob_threshold: " << detProbThreshold;
        }
        
        // Extract image data from JSON body
        std::vector<unsigned char> imageData;
        std::string imageError;
        if (!extractImageFromJson(req, imageData, imageError)) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] POST /v1/recognition/faces - " << imageError;
            }
            callback(createErrorResponse(400, "Invalid request", imageError));
            return;
        }
        
        // Register subject
        std::string imageId;
        std::string registerError;
        if (!registerSubject(subjectName, imageData, detProbThreshold, imageId, registerError)) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] POST /v1/recognition/faces - " << registerError;
            }
            callback(createErrorResponse(400, "Registration failed", registerError));
            return;
        }
        
        // Build response
        Json::Value response;
        response["image_id"] = imageId;
        response["subject"] = subjectName;
        
        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k200OK);
        
        // Add CORS headers
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type, x-api-key");
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_INFO << "[API] POST /v1/recognition/faces - Success: Registered subject '" 
                      << subjectName << "' with image_id '" << imageId << "' - " 
                      << duration.count() << "ms";
        }
        
        callback(resp);
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] POST /v1/recognition/faces - Exception: " << e.what() << " - " << duration.count() << "ms";
        }
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] POST /v1/recognition/faces - Unknown exception - " << duration.count() << "ms";
        }
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void RecognitionHandler::handleOptionsFaces(const HttpRequestPtr &req,
                                     std::function<void(const HttpResponsePtr &)> &&callback) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type, x-api-key");
    resp->addHeader("Access-Control-Max-Age", "3600");
    callback(resp);
}

Json::Value RecognitionHandler::getFaceSubjects(int page, int size, const std::string& subjectFilter) const {
    Json::Value result;
    Json::Value faces(Json::arrayValue);
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    // Collect all faces from storage
    std::vector<std::pair<std::string, std::string>> allFaces; // (subject, imageId)
    
    if (subjectFilter.empty()) {
        // Get all faces from all subjects
        for (const auto& [subject, imageIds] : face_subjects_storage_) {
            for (const auto& imageId : imageIds) {
                allFaces.push_back({subject, imageId});
            }
        }
    } else {
        // Get faces only from filtered subject
        auto it = face_subjects_storage_.find(subjectFilter);
        if (it != face_subjects_storage_.end()) {
            for (const auto& imageId : it->second) {
                allFaces.push_back({subjectFilter, imageId});
            }
        }
    }
    
    // Calculate pagination
    int totalElements = static_cast<int>(allFaces.size());
    int totalPages = (totalElements > 0) ? ((totalElements - 1) / size + 1) : 0;
    
    // Apply pagination
    int startIdx = page * size;
    int endIdx = std::min(startIdx + size, totalElements);
    
    for (int i = startIdx; i < endIdx; ++i) {
        Json::Value face;
        face["image_id"] = allFaces[i].second;
        face["subject"] = allFaces[i].first;
        faces.append(face);
    }
    
    result["faces"] = faces;
    result["page_number"] = page;
    result["page_size"] = size;
    result["total_pages"] = totalPages;
    result["total_elements"] = totalElements;
    
    return result;
}

void RecognitionHandler::listFaceSubjects(const HttpRequestPtr &req,
                                        std::function<void(const HttpResponsePtr &)> &&callback) {
    auto start_time = std::chrono::steady_clock::now();
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] GET /v1/recognition/faces - List face subjects";
        PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
    }
    
    try {
        // Validate API key
        std::string apiKeyError;
        if (!validateApiKey(req, apiKeyError)) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] GET /v1/recognition/faces - " << apiKeyError;
            }
            callback(createErrorResponse(401, "Unauthorized", apiKeyError));
            return;
        }
        
        // Parse query parameters
        std::string pageStr = req->getParameter("page");
        std::string sizeStr = req->getParameter("size");
        std::string subjectFilter = req->getParameter("subject");
        
        int page = 0; // Default
        int size = 20; // Default
        
        if (!pageStr.empty()) {
            try {
                page = std::stoi(pageStr);
                if (page < 0) page = 0;
            } catch (...) {
                page = 0;
            }
        }
        
        if (!sizeStr.empty()) {
            try {
                size = std::stoi(sizeStr);
                if (size < 1) size = 20;
                if (size > 100) size = 100; // Limit max page size
            } catch (...) {
                size = 20;
            }
        }
        
        if (isApiLoggingEnabled()) {
            PLOG_DEBUG << "[API] List parameters - page: " << page 
                      << ", size: " << size
                      << ", subject filter: " << (subjectFilter.empty() ? "all" : subjectFilter);
        }
        
        // Get face subjects
        Json::Value response = getFaceSubjects(page, size, subjectFilter);
        
        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k200OK);
        
        // Add CORS headers
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type, x-api-key");
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            int totalElements = response.get("total_elements", 0).asInt();
            PLOG_INFO << "[API] GET /v1/recognition/faces - Success: Found " 
                      << totalElements << " face(s) - " << duration.count() << "ms";
        }
        
        callback(resp);
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] GET /v1/recognition/faces - Exception: " << e.what() << " - " << duration.count() << "ms";
        }
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] GET /v1/recognition/faces - Unknown exception - " << duration.count() << "ms";
        }
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

std::string RecognitionHandler::extractSubjectFromPath(const HttpRequestPtr &req) const {
    // Try getParameter first (standard way)
    std::string subject = req->getParameter("subject");
    
    // Fallback: extract from path if getParameter doesn't work
    if (subject.empty()) {
        std::string path = req->getPath();
        size_t subjectsPos = path.find("/subjects/");
        if (subjectsPos != std::string::npos) {
            size_t start = subjectsPos + 10; // length of "/subjects/"
            size_t end = path.find("?", start); // Stop at query string if present
            if (end == std::string::npos) {
                end = path.length();
            }
            subject = path.substr(start, end - start);
        }
    }
    
    // URL decode the subject name if it contains encoded characters
    if (!subject.empty()) {
        std::string decoded;
        decoded.reserve(subject.length());
        for (size_t i = 0; i < subject.length(); ++i) {
            if (subject[i] == '%' && i + 2 < subject.length()) {
                // Try to decode hex value
                char hex[3] = {subject[i+1], subject[i+2], '\0'};
                char* end;
                unsigned long value = std::strtoul(hex, &end, 16);
                if (*end == '\0' && value <= 255) {
                    decoded += static_cast<char>(value);
                    i += 2; // Skip the hex digits
                } else {
                    decoded += subject[i]; // Invalid encoding, keep as-is
                }
            } else {
                decoded += subject[i];
            }
        }
        subject = decoded;
    }
    
    return subject;
}

bool RecognitionHandler::renameSubjectName(const std::string& oldSubjectName,
                                          const std::string& newSubjectName,
                                          std::string& error) const {
    if (oldSubjectName.empty()) {
        error = "Old subject name cannot be empty";
        return false;
    }
    
    if (newSubjectName.empty()) {
        error = "New subject name cannot be empty";
        return false;
    }
    
    if (oldSubjectName == newSubjectName) {
        // No change needed, but this is still considered successful
        return true;
    }
    
    // Check if old subject exists
    if (!subjectExists(oldSubjectName)) {
        error = "Subject '" + oldSubjectName + "' does not exist";
        return false;
    }
    
    // Check if new subject already exists
    if (subjectExists(newSubjectName)) {
        // Merge: move all faces from old subject to new subject
        mergeSubjects(oldSubjectName, newSubjectName);
    } else {
        // Rename: move all faces from old subject to new subject name
        renameSubjectInStorage(oldSubjectName, newSubjectName);
    }
    
    return true;
}

void RecognitionHandler::renameSubject(const HttpRequestPtr &req,
                                      std::function<void(const HttpResponsePtr &)> &&callback) {
    auto start_time = std::chrono::steady_clock::now();
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] PUT /v1/recognition/subjects/{subject} - Rename face subject";
        PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
    }
    
    try {
        // Validate API key
        std::string apiKeyError;
        if (!validateApiKey(req, apiKeyError)) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] PUT /v1/recognition/subjects/{subject} - " << apiKeyError;
            }
            callback(createErrorResponse(401, "Unauthorized", apiKeyError));
            return;
        }
        
        // Extract old subject name from URL path
        std::string oldSubjectName = extractSubjectFromPath(req);
        if (oldSubjectName.empty()) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] PUT /v1/recognition/subjects/{subject} - Missing subject in path";
            }
            callback(createErrorResponse(400, "Invalid request", "Missing subject in URL path"));
            return;
        }
        
        // Parse JSON body
        auto json = req->getJsonObject();
        if (!json) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] PUT /v1/recognition/subjects/{subject} - Invalid JSON body";
            }
            callback(createErrorResponse(400, "Invalid request", "Request body must be valid JSON"));
            return;
        }
        
        // Validate required field: subject
        if (!json->isMember("subject") || !(*json)["subject"].isString()) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] PUT /v1/recognition/subjects/{subject} - Missing required field: subject";
            }
            callback(createErrorResponse(400, "Invalid request", "Missing required field: subject"));
            return;
        }
        
        std::string newSubjectName = (*json)["subject"].asString();
        if (newSubjectName.empty()) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] PUT /v1/recognition/subjects/{subject} - Subject field is empty";
            }
            callback(createErrorResponse(400, "Invalid request", "Subject field cannot be empty"));
            return;
        }
        
        if (isApiLoggingEnabled()) {
            PLOG_DEBUG << "[API] Rename subject: '" << oldSubjectName << "' -> '" << newSubjectName << "'";
        }
        
        // Rename/merge subject
        std::string renameError;
        bool success = renameSubjectName(oldSubjectName, newSubjectName, renameError);
        
        // Build response
        Json::Value response;
        response["updated"] = success ? "true" : "false";
        
        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(success ? k200OK : k400BadRequest);
        
        // Add CORS headers
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->addHeader("Access-Control-Allow-Methods", "PUT, OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type, x-api-key");
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            if (success) {
                PLOG_INFO << "[API] PUT /v1/recognition/subjects/{subject} - Success: Renamed subject '" 
                          << oldSubjectName << "' to '" << newSubjectName << "' - " 
                          << duration.count() << "ms";
            } else {
                PLOG_WARNING << "[API] PUT /v1/recognition/subjects/{subject} - Failed: " 
                             << renameError << " - " << duration.count() << "ms";
            }
        }
        
        callback(resp);
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] PUT /v1/recognition/subjects/{subject} - Exception: " << e.what() << " - " << duration.count() << "ms";
        }
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] PUT /v1/recognition/subjects/{subject} - Unknown exception - " << duration.count() << "ms";
        }
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void RecognitionHandler::handleOptionsSubjects(const HttpRequestPtr &req,
                                              std::function<void(const HttpResponsePtr &)> &&callback) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "PUT, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type, x-api-key");
    resp->addHeader("Access-Control-Max-Age", "3600");
    callback(resp);
}

bool RecognitionHandler::subjectExists(const std::string& subjectName) const {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    auto it = face_subjects_storage_.find(subjectName);
    return (it != face_subjects_storage_.end() && !it->second.empty());
}

std::vector<std::string> RecognitionHandler::getSubjectImageIds(const std::string& subjectName) const {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    auto it = face_subjects_storage_.find(subjectName);
    if (it != face_subjects_storage_.end()) {
        return it->second;
    }
    return std::vector<std::string>();
}

void RecognitionHandler::addImageToSubject(const std::string& subjectName, const std::string& imageId) const {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    face_subjects_storage_[subjectName].push_back(imageId);
}

void RecognitionHandler::removeSubject(const std::string& subjectName) const {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    face_subjects_storage_.erase(subjectName);
}

void RecognitionHandler::mergeSubjects(const std::string& oldSubjectName, const std::string& newSubjectName) const {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto oldIt = face_subjects_storage_.find(oldSubjectName);
    if (oldIt == face_subjects_storage_.end() || oldIt->second.empty()) {
        return; // Nothing to merge
    }
    
    auto newIt = face_subjects_storage_.find(newSubjectName);
    if (newIt == face_subjects_storage_.end()) {
        // New subject doesn't exist, create it
        face_subjects_storage_[newSubjectName] = oldIt->second;
    } else {
        // New subject exists, merge: append all image IDs from old to new
        newIt->second.insert(newIt->second.end(), oldIt->second.begin(), oldIt->second.end());
    }
    
    // Remove old subject
    face_subjects_storage_.erase(oldIt);
}

void RecognitionHandler::renameSubjectInStorage(const std::string& oldSubjectName, const std::string& newSubjectName) const {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto oldIt = face_subjects_storage_.find(oldSubjectName);
    if (oldIt == face_subjects_storage_.end()) {
        return; // Subject doesn't exist
    }
    
    // Move all image IDs to new subject name
    face_subjects_storage_[newSubjectName] = std::move(oldIt->second);
    
    // Remove old subject
    face_subjects_storage_.erase(oldIt);
}

std::string RecognitionHandler::findSubjectByImageId(const std::string& imageId) const {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    for (const auto& [subjectName, imageIds] : face_subjects_storage_) {
        for (const auto& id : imageIds) {
            if (id == imageId) {
                return subjectName;
            }
        }
    }
    
    return std::string();
}

bool RecognitionHandler::removeImageFromSubject(const std::string& subjectName, const std::string& imageId) const {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = face_subjects_storage_.find(subjectName);
    if (it == face_subjects_storage_.end()) {
        return false;
    }
    
    auto& imageIds = it->second;
    auto imageIt = std::find(imageIds.begin(), imageIds.end(), imageId);
    if (imageIt == imageIds.end()) {
        return false;
    }
    
    imageIds.erase(imageIt);
    
    // Remove subject if it has no more images
    if (imageIds.empty()) {
        face_subjects_storage_.erase(it);
    }
    
    return true;
}

bool RecognitionHandler::deleteImageFromStorage(const std::string& imageId, std::string& subjectName) const {
    subjectName = findSubjectByImageId(imageId);
    if (subjectName.empty()) {
        return false;
    }
    
    return removeImageFromSubject(subjectName, imageId);
}

void RecognitionHandler::deleteFaceSubject(const HttpRequestPtr &req,
                                          std::function<void(const HttpResponsePtr &)> &&callback) {
    auto start_time = std::chrono::steady_clock::now();
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] DELETE /v1/recognition/faces/{image_id} - Delete face subject";
        PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
    }
    
    try {
        // Validate API key
        std::string apiKeyError;
        if (!validateApiKey(req, apiKeyError)) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] DELETE /v1/recognition/faces/{image_id} - " << apiKeyError;
            }
            callback(createErrorResponse(401, "Unauthorized", apiKeyError));
            return;
        }
        
        // Extract image ID from URL path
        std::string path = req->getPath();
        size_t facesPos = path.find("/faces/");
        if (facesPos == std::string::npos) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] DELETE /v1/recognition/faces/{image_id} - Invalid path";
            }
            callback(createErrorResponse(400, "Invalid request", "Invalid URL path"));
            return;
        }
        
        size_t start = facesPos + 7; // length of "/faces/"
        size_t end = path.find("?", start);
        if (end == std::string::npos) {
            end = path.length();
        }
        std::string imageId = path.substr(start, end - start);
        
        if (imageId.empty()) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] DELETE /v1/recognition/faces/{image_id} - Missing image_id in path";
            }
            callback(createErrorResponse(400, "Invalid request", "Missing image_id in URL path"));
            return;
        }
        
        if (isApiLoggingEnabled()) {
            PLOG_DEBUG << "[API] Delete face subject with image_id: " << imageId;
        }
        
        // Find and delete image from storage
        std::string subjectName;
        if (!deleteImageFromStorage(imageId, subjectName)) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] DELETE /v1/recognition/faces/{image_id} - Face not found: " << imageId;
            }
            callback(createErrorResponse(404, "Not Found", "Face subject with image_id '" + imageId + "' not found"));
            return;
        }
        
        // Build response
        Json::Value response;
        response["image_id"] = imageId;
        response["subject"] = subjectName;
        
        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k200OK);
        
        // Add CORS headers
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->addHeader("Access-Control-Allow-Methods", "DELETE, OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type, x-api-key");
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        if (isApiLoggingEnabled()) {
            PLOG_INFO << "[API] DELETE /v1/recognition/faces/{image_id} - Success: Deleted face subject '" 
                      << imageId << "' (subject: '" << subjectName << "') - " 
                      << duration.count() << "ms";
        }
        
        callback(resp);
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] DELETE /v1/recognition/faces/{image_id} - Exception: " << e.what() << " - " << duration.count() << "ms";
        }
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] DELETE /v1/recognition/faces/{image_id} - Unknown exception - " << duration.count() << "ms";
        }
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void RecognitionHandler::deleteMultipleFaceSubjects(const HttpRequestPtr &req,
                                                   std::function<void(const HttpResponsePtr &)> &&callback) {
    auto start_time = std::chrono::steady_clock::now();
    
    if (isApiLoggingEnabled()) {
        PLOG_INFO << "[API] POST /v1/recognition/faces/delete - Delete multiple face subjects";
        PLOG_DEBUG << "[API] Request from: " << req->getPeerAddr().toIpPort();
    }
    
    try {
        // Validate API key
        std::string apiKeyError;
        if (!validateApiKey(req, apiKeyError)) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] POST /v1/recognition/faces/delete - " << apiKeyError;
            }
            callback(createErrorResponse(401, "Unauthorized", apiKeyError));
            return;
        }
        
        // Parse JSON body
        auto json = req->getJsonObject();
        if (!json) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] POST /v1/recognition/faces/delete - Invalid JSON body";
            }
            callback(createErrorResponse(400, "Invalid request", "Request body must be valid JSON"));
            return;
        }
        
        // Validate that body is an array
        if (!json->isArray()) {
            if (isApiLoggingEnabled()) {
                PLOG_WARNING << "[API] POST /v1/recognition/faces/delete - Request body must be an array of image IDs";
            }
            callback(createErrorResponse(400, "Invalid request", "Request body must be an array of image IDs"));
            return;
        }
        
        // Process each image ID
        Json::Value deletedFaces(Json::arrayValue);
        
        for (const auto& item : *json) {
            if (!item.isString()) {
                continue; // Skip invalid entries
            }
            
            std::string imageId = item.asString();
            if (imageId.empty()) {
                continue; // Skip empty IDs
            }
            
            std::string subjectName;
            if (deleteImageFromStorage(imageId, subjectName)) {
                Json::Value deletedFace;
                deletedFace["image_id"] = imageId;
                deletedFace["subject"] = subjectName;
                deletedFaces.append(deletedFace);
            }
            // If not found, ignore it (as per spec: "If some IDs do not exist, they will be ignored")
        }
        
        // Build response
        Json::Value response;
        response["deleted"] = deletedFaces;
        
        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k200OK);
        
        // Add CORS headers
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type, x-api-key");
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        int deletedCount = deletedFaces.size();
        if (isApiLoggingEnabled()) {
            PLOG_INFO << "[API] POST /v1/recognition/faces/delete - Success: Deleted " 
                      << deletedCount << " face subject(s) - " 
                      << duration.count() << "ms";
        }
        
        callback(resp);
        
    } catch (const std::exception& e) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] POST /v1/recognition/faces/delete - Exception: " << e.what() << " - " << duration.count() << "ms";
        }
        callback(createErrorResponse(500, "Internal server error", e.what()));
    } catch (...) {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        if (isApiLoggingEnabled()) {
            PLOG_ERROR << "[API] POST /v1/recognition/faces/delete - Unknown exception - " << duration.count() << "ms";
        }
        callback(createErrorResponse(500, "Internal server error", "Unknown error occurred"));
    }
}

void RecognitionHandler::handleOptionsDeleteFaces(const HttpRequestPtr &req,
                                                  std::function<void(const HttpResponsePtr &)> &&callback) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    resp->addHeader("Access-Control-Allow-Origin", "*");
    resp->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    resp->addHeader("Access-Control-Allow-Headers", "Content-Type, x-api-key");
    resp->addHeader("Access-Control-Max-Age", "3600");
    callback(resp);
}

