#pragma once

#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <json/json.h>
#include <string>
#include <vector>

using namespace drogon;

/**
 * @brief Face Recognition Handler
 * 
 * Handles face recognition endpoints for recognizing and registering faces.
 * 
 * Endpoints:
 * - POST /v1/recognition/recognize - Recognize faces from image
 * - POST /v1/recognition/faces - Register face subject
 * - GET /v1/recognition/faces - List face subjects
 */
class RecognitionHandler : public drogon::HttpController<RecognitionHandler> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(RecognitionHandler::recognizeFaces, "/v1/recognition/recognize", Post);
        ADD_METHOD_TO(RecognitionHandler::registerFaceSubject, "/v1/recognition/faces", Post);
        ADD_METHOD_TO(RecognitionHandler::listFaceSubjects, "/v1/recognition/faces", Get);
        ADD_METHOD_TO(RecognitionHandler::handleOptions, "/v1/recognition/recognize", Options);
        ADD_METHOD_TO(RecognitionHandler::handleOptionsFaces, "/v1/recognition/faces", Options);
    METHOD_LIST_END
    
    /**
     * @brief Handle POST /v1/recognition/recognize
     * Recognizes faces from an uploaded image
     */
    void recognizeFaces(const HttpRequestPtr &req,
                       std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle POST /v1/recognition/faces
     * Registers a face subject by storing the image
     */
    void registerFaceSubject(const HttpRequestPtr &req,
                            std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle GET /v1/recognition/faces
     * Lists all saved face subjects with pagination support
     */
    void listFaceSubjects(const HttpRequestPtr &req,
                        std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle OPTIONS request for CORS preflight (recognize endpoint)
     */
    void handleOptions(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle OPTIONS request for CORS preflight (faces endpoint)
     */
    void handleOptionsFaces(const HttpRequestPtr &req,
                           std::function<void(const HttpResponsePtr &)> &&callback);

private:
    /**
     * @brief Validate API key from request header
     */
    bool validateApiKey(const HttpRequestPtr &req, std::string& error) const;
    
    /**
     * @brief Extract base64 image data from multipart form data
     */
    bool extractImageData(const HttpRequestPtr &req, std::vector<unsigned char>& imageData, std::string& error) const;
    
    /**
     * @brief Decode base64 string to binary data
     */
    bool decodeBase64(const std::string& base64Str, std::vector<unsigned char>& output) const;
    
    /**
     * @brief Check if string is base64 encoded
     */
    bool isBase64(const std::string& str) const;
    
    /**
     * @brief Parse query parameters
     */
    void parseQueryParameters(const HttpRequestPtr &req, 
                             int& limit,
                             int& predictionCount,
                             double& detProbThreshold,
                             std::string& facePlugins,
                             std::string& status,
                             bool& detectFaces) const;
    
    /**
     * @brief Process face recognition on image
     */
    Json::Value processFaceRecognition(const std::vector<unsigned char>& imageData,
                                      int limit,
                                      int predictionCount,
                                      double detProbThreshold,
                                      const std::string& facePlugins,
                                      bool detectFaces) const;
    
    /**
     * @brief Extract base64 image data from JSON body
     */
    bool extractImageFromJson(const HttpRequestPtr &req, std::vector<unsigned char>& imageData, std::string& error) const;
    
    /**
     * @brief Generate unique image ID (UUID-like)
     */
    std::string generateImageId() const;
    
    /**
     * @brief Register face subject by storing image
     */
    bool registerSubject(const std::string& subjectName,
                       const std::vector<unsigned char>& imageData,
                       double detProbThreshold,
                       std::string& imageId,
                       std::string& error) const;
    
    /**
     * @brief Get list of face subjects with pagination
     */
    Json::Value getFaceSubjects(int page, int size, const std::string& subjectFilter) const;
    
    /**
     * @brief Create error response
     */
    HttpResponsePtr createErrorResponse(int statusCode, const std::string& error, const std::string& message = "") const;
};

