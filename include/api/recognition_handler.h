#pragma once

#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <json/json.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

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
 * - DELETE /v1/recognition/faces/{image_id} - Delete face subject by ID
 * - POST /v1/recognition/faces/delete - Delete multiple face subjects
 * - PUT /v1/recognition/subjects/{subject} - Rename face subject
 */
class RecognitionHandler : public drogon::HttpController<RecognitionHandler> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(RecognitionHandler::recognizeFaces, "/v1/recognition/recognize", Post);
        ADD_METHOD_TO(RecognitionHandler::registerFaceSubject, "/v1/recognition/faces", Post);
        ADD_METHOD_TO(RecognitionHandler::listFaceSubjects, "/v1/recognition/faces", Get);
        ADD_METHOD_TO(RecognitionHandler::deleteFaceSubject, "/v1/recognition/faces/{image_id}", Delete);
        ADD_METHOD_TO(RecognitionHandler::deleteMultipleFaceSubjects, "/v1/recognition/faces/delete", Post);
        ADD_METHOD_TO(RecognitionHandler::renameSubject, "/v1/recognition/subjects/{subject}", Put);
        ADD_METHOD_TO(RecognitionHandler::handleOptions, "/v1/recognition/recognize", Options);
        ADD_METHOD_TO(RecognitionHandler::handleOptionsFaces, "/v1/recognition/faces", Options);
        ADD_METHOD_TO(RecognitionHandler::handleOptionsDeleteFaces, "/v1/recognition/faces/delete", Options);
        ADD_METHOD_TO(RecognitionHandler::handleOptionsSubjects, "/v1/recognition/subjects/{subject}", Options);
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
     * @brief Handle DELETE /v1/recognition/faces/{image_id}
     * Deletes a face subject by its image ID
     */
    void deleteFaceSubject(const HttpRequestPtr &req,
                          std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle POST /v1/recognition/faces/delete
     * Deletes multiple face subjects by their image IDs
     */
    void deleteMultipleFaceSubjects(const HttpRequestPtr &req,
                                   std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle PUT /v1/recognition/subjects/{subject}
     * Renames an existing subject. If the new subject name already exists, subjects are merged.
     */
    void renameSubject(const HttpRequestPtr &req,
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
    
    /**
     * @brief Handle OPTIONS request for CORS preflight (subjects endpoint)
     */
    void handleOptionsSubjects(const HttpRequestPtr &req,
                              std::function<void(const HttpResponsePtr &)> &&callback);
    
    /**
     * @brief Handle OPTIONS request for CORS preflight (delete faces endpoint)
     */
    void handleOptionsDeleteFaces(const HttpRequestPtr &req,
                                 std::function<void(const HttpResponsePtr &)> &&callback);

private:
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
     * @brief Validate image format and size
     * Supported formats: jpeg, jpg, ico, png, bmp, gif, tif, tiff, webp
     * Max size: 5MB
     */
    bool validateImageFormatAndSize(const std::vector<unsigned char>& imageData, std::string& error) const;
    
    /**
     * @brief Encode binary data to base64 string
     */
    std::string encodeBase64(const std::vector<unsigned char>& data) const;
    
    /**
     * @brief Extract image data from request (supports both JSON base64 and multipart/form-data)
     */
    bool extractImageFromRequest(const HttpRequestPtr &req, std::vector<unsigned char>& imageData, std::string& error) const;
    
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
     * @brief Extract subject name from URL path
     */
    std::string extractSubjectFromPath(const HttpRequestPtr &req) const;
    
    /**
     * @brief Rename/merge subject
     * @param oldSubjectName The current subject name
     * @param newSubjectName The new subject name
     * @param error Error message if operation fails
     * @return true if successful, false otherwise
     */
    bool renameSubjectName(const std::string& oldSubjectName,
                          const std::string& newSubjectName,
                          std::string& error) const;
    
    /**
     * @brief Create error response
     */
    HttpResponsePtr createErrorResponse(int statusCode, const std::string& error, const std::string& message = "") const;
    
    /**
     * @brief Check if subject exists
     */
    bool subjectExists(const std::string& subjectName) const;
    
    /**
     * @brief Get all image IDs for a subject
     */
    std::vector<std::string> getSubjectImageIds(const std::string& subjectName) const;
    
    /**
     * @brief Add image ID to subject
     */
    void addImageToSubject(const std::string& subjectName, const std::string& imageId) const;
    
    /**
     * @brief Remove subject from storage
     */
    void removeSubject(const std::string& subjectName) const;
    
    /**
     * @brief Merge faces from old subject to new subject
     */
    void mergeSubjects(const std::string& oldSubjectName, const std::string& newSubjectName) const;
    
    /**
     * @brief Rename subject (move all faces to new name)
     */
    void renameSubjectInStorage(const std::string& oldSubjectName, const std::string& newSubjectName) const;
    
    /**
     * @brief Find subject name for a given image ID
     * @param imageId Image ID to search for
     * @return Subject name if found, empty string otherwise
     */
    std::string findSubjectByImageId(const std::string& imageId) const;
    
    /**
     * @brief Remove image ID from subject
     * @param subjectName Subject name
     * @param imageId Image ID to remove
     * @return true if removed, false if not found
     */
    bool removeImageFromSubject(const std::string& subjectName, const std::string& imageId) const;
    
    /**
     * @brief Delete image ID from storage (find and remove)
     * @param imageId Image ID to delete
     * @param subjectName Output parameter for subject name if found
     * @return true if deleted, false if not found
     */
    bool deleteImageFromStorage(const std::string& imageId, std::string& subjectName) const;
    
    // Static storage for face subjects: subject name -> vector of image IDs
    static std::unordered_map<std::string, std::vector<std::string>> face_subjects_storage_;
    static std::mutex storage_mutex_;
};

