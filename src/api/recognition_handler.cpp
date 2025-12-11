#include "api/recognition_handler.h"
#include "core/logging_flags.h"
#include "core/logger.h"
#include <drogon/HttpResponse.h>
#include <json/json.h>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <opencv2/opencv.hpp>

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

