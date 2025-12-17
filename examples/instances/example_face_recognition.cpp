#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yunet_face_detector_node.h"
#include "cvedix/nodes/infers/cvedix_insight_face_recognition_node.h"
#include "cvedix/nodes/osd/cvedix_face_osd_node_v2.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/utils/logger/cvedix_logger.h"

#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>

/*
 * ============================================================================
 * InsightFace ONNX Face Registration & Recognition Sample
 * ============================================================================
 * 
 * Mô tả:
 *   Sample này demo cách đăng ký khuôn mặt vào database và nhận diện 
 *   khuôn mặt từ video/image stream sử dụng InsightFace với ONNX model.
 * 
 * Chức năng:
 *   1. Đăng ký khuôn mặt từ ảnh vào database
 *   2. Nhận diện khuôn mặt trong video/image stream
 *   3. Hiển thị tên người được nhận diện
 * 
 * Usage:
 *   ./insightface_register_recognize_face_sample [mode] [args...]
 * 
 *   Mode: register
 *     ./insightface_register_recognize_face_sample register <image_path> <person_name> [onnx_model_path]
 *     Ví dụ: ./insightface_register_recognize_face_sample register alice.jpg "Alice"
 * 
 *   Mode: recognize
 *     ./insightface_register_recognize_face_sample recognize [video_path|image_path] [onnx_model_path]
 *     Ví dụ: ./insightface_register_recognize_face_sample recognize face.mp4
 *     Ví dụ: ./insightface_register_recognize_face_sample recognize photo.jpg
 * 
 * Database:
 *   Database được lưu trong file: ./face_database.txt
 *   Format: name|embedding1,embedding2,embedding3,...
 * 
 * Yêu cầu:
 *   - ONNX model: face_recognition_sface_2021dec.onnx hoặc model InsightFace khác
 *   - Build với: cmake .. (không cần TensorRT)
 * 
 * ============================================================================
 */

// Helper function: cosine similarity
static float cosine_similarity(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size() || a.empty()) return 0.0f;
    
    float dot_product = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;
    
    for (size_t i = 0; i < a.size(); i++) {
        dot_product += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    
    float denominator = std::sqrt(norm_a) * std::sqrt(norm_b);
    if (denominator < 1e-6) return 0.0f;
    
    return dot_product / denominator;
}

// Helper function: Average embeddings
static std::vector<float> average_embeddings(const std::vector<std::vector<float>>& embeddings) {
    if (embeddings.empty() || embeddings[0].empty()) return std::vector<float>();
    
    size_t dim = embeddings[0].size();
    std::vector<float> avg_embedding(dim, 0.0f);
    
    for (const auto& emb : embeddings) {
        if (emb.size() != dim) continue;
        for (size_t i = 0; i < dim; i++) {
            avg_embedding[i] += emb[i];
        }
    }
    
    float count = static_cast<float>(embeddings.size());
    for (size_t i = 0; i < dim; i++) {
        avg_embedding[i] /= count;
    }
    
    // L2 normalize
    float norm = 0.0f;
    for (float val : avg_embedding) {
        norm += val * val;
    }
    norm = std::sqrt(norm);
    if (norm > 1e-6) {
        for (float& val : avg_embedding) {
            val /= norm;
        }
    }
    
    return avg_embedding;
}

// Helper function: Face alignment using landmarks (if available)
static cv::Mat align_face_using_landmarks(const cv::Mat& image, const cv::Mat& faces, int face_idx) {
    // YuNet format: (x, y, w, h, re_x, re_y, le_x, le_y, nt_x, nt_y, rcm_x, rcm_y, lcm_x, lcm_y, score)
    // Landmarks order: right_eye, left_eye, nose_tip, right_mouth_corner, left_mouth_corner
    
    float re_x = faces.at<float>(face_idx, 4);
    float re_y = faces.at<float>(face_idx, 5);
    float le_x = faces.at<float>(face_idx, 6);
    float le_y = faces.at<float>(face_idx, 7);
    float nt_x = faces.at<float>(face_idx, 8);
    float nt_y = faces.at<float>(face_idx, 9);
    float rcm_x = faces.at<float>(face_idx, 10);
    float rcm_y = faces.at<float>(face_idx, 11);
    float lcm_x = faces.at<float>(face_idx, 12);
    float lcm_y = faces.at<float>(face_idx, 13);
    
    // Standard face template for 112x112 (InsightFace)
    float dst[5][2] = {
        {38.2946f, 51.6963f},  // right eye
        {73.5318f, 51.5014f},  // left eye
        {56.0252f, 71.7366f},  // nose tip
        {41.5493f, 92.3655f},  // right mouth corner
        {70.7299f, 92.2041f}   // left mouth corner
    };
    
    float src[5][2] = {
        {re_x, re_y},
        {le_x, le_y},
        {nt_x, nt_y},
        {rcm_x, rcm_y},
        {lcm_x, lcm_y}
    };
    
    // Compute similarity transform matrix
    float src_mean[2] = {
        (src[0][0] + src[1][0] + src[2][0] + src[3][0] + src[4][0]) / 5.0f,
        (src[0][1] + src[1][1] + src[2][1] + src[3][1] + src[4][1]) / 5.0f
    };
    float dst_mean[2] = {56.0262f, 71.9008f};
    
    float src_demean[5][2], dst_demean[5][2];
    for (int i = 0; i < 5; i++) {
        src_demean[i][0] = src[i][0] - src_mean[0];
        src_demean[i][1] = src[i][1] - src_mean[1];
        dst_demean[i][0] = dst[i][0] - dst_mean[0];
        dst_demean[i][1] = dst[i][1] - dst_mean[1];
    }
    
    double A00 = 0.0, A01 = 0.0, A10 = 0.0, A11 = 0.0;
    for (int i = 0; i < 5; i++) {
        A00 += dst_demean[i][0] * src_demean[i][0];
        A01 += dst_demean[i][0] * src_demean[i][1];
        A10 += dst_demean[i][1] * src_demean[i][0];
        A11 += dst_demean[i][1] * src_demean[i][1];
    }
    A00 /= 5.0; A01 /= 5.0; A10 /= 5.0; A11 /= 5.0;
    
    double detA = A00 * A11 - A01 * A10;
    double d[2] = {1.0, (detA < 0) ? -1.0 : 1.0};
    
    cv::Mat A = (cv::Mat_<double>(2, 2) << A00, A01, A10, A11);
    cv::Mat s, u, vt;
    cv::SVD::compute(A, s, u, vt);
    
    double smax = std::max(s.at<double>(0), s.at<double>(1));
    double tol = smax * 2 * FLT_MIN;
    int rank = 0;
    if (s.at<double>(0) > tol) rank++;
    if (s.at<double>(1) > tol) rank++;
    
    if (rank == 0) {
        // Fallback to simple resize if alignment fails
        cv::Mat aligned;
        cv::resize(image, aligned, cv::Size(112, 112));
        return aligned;
    }
    
    cv::Mat T = u * cv::Mat::diag(cv::Mat(cv::Vec2d(d[0], d[1]))) * vt;
    
    double var1 = 0.0, var2 = 0.0;
    for (int i = 0; i < 5; i++) {
        var1 += src_demean[i][0] * src_demean[i][0];
        var2 += src_demean[i][1] * src_demean[i][1];
    }
    var1 /= 5.0;
    var2 /= 5.0;
    
    double scale = 1.0 / (var1 + var2) * (s.at<double>(0) * d[0] + s.at<double>(1) * d[1]);
    double TS[2] = {
        T.at<double>(0, 0) * src_mean[0] + T.at<double>(0, 1) * src_mean[1],
        T.at<double>(1, 0) * src_mean[0] + T.at<double>(1, 1) * src_mean[1]
    };
    
    cv::Mat transform_mat = (cv::Mat_<double>(2, 3) <<
        T.at<double>(0, 0) * scale, T.at<double>(0, 1) * scale, dst_mean[0] - scale * TS[0],
        T.at<double>(1, 0) * scale, T.at<double>(1, 1) * scale, dst_mean[1] - scale * TS[1]);
    
    cv::Mat aligned;
    cv::warpAffine(image, aligned, transform_mat, cv::Size(112, 112), cv::INTER_LINEAR);
    return aligned;
}

// Helper function: extract embedding from aligned face image
static std::vector<float> extract_embedding_from_image(
    const cv::Mat& aligned_face, 
    const std::string& onnx_model_path) {
    
    // Load ONNX model
    cv::dnn::Net net = cv::dnn::readNetFromONNX(onnx_model_path);
    if (net.empty()) {
        std::cerr << "[Error] Failed to load ONNX model: " << onnx_model_path << std::endl;
        return std::vector<float>();
    }
    
    #ifdef CVEDIX_WITH_CUDA
    net.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
    net.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
    #else
    net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    #endif
    
    // Preprocess: BGR->RGB, normalize (pixel - 127.5) / 128.0
    cv::Mat rgb;
    cv::cvtColor(aligned_face, rgb, cv::COLOR_BGR2RGB);
    
    // Ensure 112x112
    if (rgb.rows != 112 || rgb.cols != 112) {
        cv::resize(rgb, rgb, cv::Size(112, 112), 0, 0, cv::INTER_LINEAR);
    }
    
    // Create blob: (pixel - 127.5) / 128.0
    cv::Mat blob;
    cv::dnn::blobFromImage(rgb, blob, 1.0f / 128.0f,
                          cv::Size(), cv::Scalar(127.5f, 127.5f, 127.5f),
                          false, false, CV_32F);
    
    // Run inference
    net.setInput(blob);
    std::vector<cv::Mat> outputs;
    net.forward(outputs, net.getUnconnectedOutLayersNames());
    
    if (outputs.empty()) {
        return std::vector<float>();
    }
    
    // Extract embedding
    const cv::Mat& output = outputs[0];
    int emb_dim = (output.dims == 2) ? output.size[1] : output.size[0];
    
    std::vector<float> embedding(emb_dim);
    const float* output_ptr = output.ptr<float>();
    std::copy(output_ptr, output_ptr + emb_dim, embedding.begin());
    
    // L2 normalize
    float norm = 0.0f;
    for (float val : embedding) {
        norm += val * val;
    }
    norm = std::sqrt(norm);
    if (norm > 1e-6) {
        for (float& val : embedding) {
            val /= norm;
        }
    }
    
    return embedding;
}

static std::string get_project_root(const std::string& executable_path) {
    std::filesystem::path current = std::filesystem::absolute(executable_path).parent_path();
    
    for (int depth = 0; depth < 5; depth++) {
        if (current.filename() == "bin" && current.parent_path().filename() == "build") {
            return current.parent_path().parent_path().string();
        }
        if (std::filesystem::exists(current / "cvedix_data")) {
            return current.string();
        }
        if (current.has_parent_path() && current != current.parent_path()) {
            current = current.parent_path();
        } else {
            break;
        }
    }
    return std::filesystem::current_path().string();
}

static std::string resolve_path(const std::string& executable_path, const std::string& relative_path) {
    if (std::filesystem::path(relative_path).is_absolute()) {
        return relative_path;
    }
    
    std::string project_root = get_project_root(executable_path);
    std::filesystem::path full_path = std::filesystem::path(project_root) / relative_path;
    
    if (std::filesystem::exists(full_path)) return full_path.string();
    
    std::filesystem::path current_path = std::filesystem::current_path() / relative_path;
    if (std::filesystem::exists(current_path)) return current_path.string();
    
    return full_path.string();
}

// Face Database Class
class FaceDatabase {
private:
    std::map<std::string, std::vector<float>> database_;
    std::string db_file_path_;
    std::string project_root_;
    float threshold_ = 0.7f;  // Increased from 0.6 to 0.7 for better accuracy
    std::string onnx_model_path_;
    
    std::string resolve_model_path(const std::string& relative_path) {
        std::filesystem::path full_path = std::filesystem::path(project_root_) / relative_path;
        if (std::filesystem::exists(full_path)) return full_path.string();
        
        std::filesystem::path current_path = std::filesystem::current_path() / relative_path;
        if (std::filesystem::exists(current_path)) return current_path.string();
        if (std::filesystem::exists(relative_path)) return relative_path;
        
        return full_path.string();
    }

    void load_database() {
        std::ifstream file(db_file_path_);
        if (!file.is_open()) {
            std::ofstream create_file(db_file_path_);
            std::cout << "[DB] Creating new database" << std::endl;
            return;
        }

        std::string line;
        int count = 0;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            size_t pos = line.find('|');
            if (pos == std::string::npos) continue;

            std::string name = line.substr(0, pos);
            std::string embedding_str = line.substr(pos + 1);
            std::vector<float> embedding;
            std::stringstream ss(embedding_str);
            std::string value;
            
            while (std::getline(ss, value, ',')) {
                embedding.push_back(std::stof(value));
            }

            // Accept any embedding size (not just 512)
            if (!embedding.empty()) {
                database_[name] = embedding;
                count++;
            }
        }
        std::cout << "[DB] Loaded " << count << " faces" << std::endl;
    }

    void save_database() {
        std::ofstream file(db_file_path_);
        if (!file.is_open()) {
            std::cerr << "[DB] Error: Cannot save to " << db_file_path_ << std::endl;
            return;
        }

        for (const auto& [name, embedding] : database_) {
            file << name << "|";
            for (size_t i = 0; i < embedding.size(); i++) {
                file << std::fixed << std::setprecision(6) << embedding[i];
                if (i < embedding.size() - 1) file << ",";
            }
            file << "\n";
        }
        std::cout << "[DB] Saved " << database_.size() << " faces" << std::endl;
    }

public:
    FaceDatabase(const std::string& executable_path, const std::string& db_path = "./face_database.txt") 
        : project_root_(get_project_root(executable_path)),
          db_file_path_(resolve_path(executable_path, db_path)) {
        
        load_database();
        
        // Find ONNX model
        std::vector<std::string> model_paths = {
            resolve_path(executable_path, "build/bin/cvedix_data/models/face/face_recognition_sface_2021dec.onnx"),
            resolve_path(executable_path, "cvedix_data/models/face/face_recognition_sface_2021dec.onnx"),
            (std::filesystem::path(project_root_) / "build/bin/cvedix_data/models/face/face_recognition_sface_2021dec.onnx").string(),
            (std::filesystem::path(project_root_) / "cvedix_data/models/face/face_recognition_sface_2021dec.onnx").string()
        };
        
        for (const auto& path : model_paths) {
            if (std::filesystem::exists(path)) {
                onnx_model_path_ = path;
                break;
            }
        }
        
        if (onnx_model_path_.empty()) {
            std::cerr << "[DB] Warning: ONNX model not found, using default path" << std::endl;
            onnx_model_path_ = model_paths[0];
        }
    }
    
    void set_model_path(const std::string& model_path) {
        onnx_model_path_ = resolve_path("", model_path);
    }

    bool register_face_from_image(const std::string& image_path, const std::string& person_name) {
        std::cout << "\n[Register] Image: " << image_path << ", Name: " << person_name << std::endl;

        if (!std::filesystem::exists(image_path)) {
            std::cerr << "[Register] Error: File not found" << std::endl;
            return false;
        }

        cv::Mat image = cv::imread(image_path);
        if (image.empty()) {
            std::cerr << "[Register] Error: Cannot read image" << std::endl;
            return false;
        }

        std::vector<std::string> detector_paths = {
            resolve_model_path("build/bin/cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx"),
            resolve_model_path("cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx"),
            (std::filesystem::path(project_root_) / "build/bin/cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx").string(),
            (std::filesystem::path(project_root_) / "cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx").string()
        };
        
        std::string detector_model_path = detector_paths[0];
        for (const auto& path : detector_paths) {
            if (std::filesystem::exists(path)) {
                detector_model_path = path;
                break;
            }
        }

        cv::Ptr<cv::FaceDetectorYN> face_detector;
        try {
            face_detector = cv::FaceDetectorYN::create(
                detector_model_path, "", cv::Size(320, 320), 0.6f, 0.3f, 5000,
                cv::dnn::DNN_BACKEND_OPENCV, cv::dnn::DNN_TARGET_CPU);
        } catch (const std::exception& e) {
            std::cerr << "[Register] Error: Failed to create detector: " << e.what() << std::endl;
            return false;
        }
        
        if (face_detector.empty()) {
            std::cerr << "[Register] Error: Detector handle is empty" << std::endl;
            return false;
        }

        face_detector->setInputSize(image.size());
        cv::Mat faces;
        try {
            face_detector->detect(image, faces);
        } catch (const cv::Exception& e) {
            std::cerr << "[Register] Error: Detection failed: " << e.what() << std::endl;
            return false;
        }

        if (faces.rows == 0 || faces.empty()) {
            std::cerr << "[Register] Error: No face detected" << std::endl;
            return false;
        }

        float x = faces.at<float>(0, 0), y = faces.at<float>(0, 1);
        float w = faces.at<float>(0, 2), h = faces.at<float>(0, 3);
        float score = faces.at<float>(0, 14);

        x = std::max(0.0f, std::min(x, (float)(image.cols - 1)));
        y = std::max(0.0f, std::min(y, (float)(image.rows - 1)));
        w = std::max(1.0f, std::min(w, (float)(image.cols - x)));
        h = std::max(1.0f, std::min(h, (float)(image.rows - y)));

        std::cout << "[Register] Face: (" << (int)x << "," << (int)y << ") " 
                  << (int)w << "x" << (int)h << " (score: " << score << ")" << std::endl;

        // Use face alignment with landmarks if available
        cv::Mat aligned_face;
        if (faces.cols >= 15) {  // YuNet has landmarks
            aligned_face = align_face_using_landmarks(image, faces, 0);
            std::cout << "[Register] Using landmark-based alignment" << std::endl;
        } else {
            // Fallback to simple resize
            cv::Mat face_roi = image(cv::Rect((int)x, (int)y, (int)w, (int)h)).clone();
            cv::resize(face_roi, aligned_face, cv::Size(112, 112));
            std::cout << "[Register] Using simple resize (no landmarks)" << std::endl;
        }

        // Data augmentation: Create multiple variations and average embeddings
        std::vector<std::vector<float>> embeddings;
        
        // 1. Original
        std::vector<float> emb1 = extract_embedding_from_image(aligned_face, onnx_model_path_);
        if (!emb1.empty()) embeddings.push_back(emb1);
        
        // 2. Horizontal flip
        cv::Mat flipped;
        cv::flip(aligned_face, flipped, 1);
        std::vector<float> emb2 = extract_embedding_from_image(flipped, onnx_model_path_);
        if (!emb2.empty()) embeddings.push_back(emb2);
        
        // 3. Slight brightness increase
        cv::Mat bright;
        aligned_face.convertTo(bright, -1, 1.0, 15);
        std::vector<float> emb3 = extract_embedding_from_image(bright, onnx_model_path_);
        if (!emb3.empty()) embeddings.push_back(emb3);
        
        // 4. Slight brightness decrease
        cv::Mat dark;
        aligned_face.convertTo(dark, -1, 1.0, -15);
        std::vector<float> emb4 = extract_embedding_from_image(dark, onnx_model_path_);
        if (!emb4.empty()) embeddings.push_back(emb4);
        
        // 5. Slight contrast increase
        cv::Mat contrast;
        aligned_face.convertTo(contrast, -1, 1.1, 0);
        std::vector<float> emb5 = extract_embedding_from_image(contrast, onnx_model_path_);
        if (!emb5.empty()) embeddings.push_back(emb5);

        if (embeddings.empty()) {
            std::cerr << "[Register] Error: Failed to extract any embeddings" << std::endl;
            return false;
        }

        // Average all embeddings for more robust representation
        std::vector<float> final_embedding = average_embeddings(embeddings);
        std::cout << "[Register] Generated " << embeddings.size() << " embeddings, averaged to final embedding" << std::endl;

        database_[person_name] = final_embedding;
        save_database();
        std::cout << "[Register] ✓ Registered: " << person_name << " (using " << embeddings.size() << " augmented variations)" << std::endl;
        return true;
    }

    std::string identify(const std::vector<float>& query_embedding) {
        if (query_embedding.empty()) return "Unknown";

        // Debug: Check embedding sizes
        std::cout << "  [Debug] Query embedding size: " << query_embedding.size() << std::endl;
        if (!database_.empty()) {
            auto first_entry = database_.begin();
            std::cout << "  [Debug] Database embedding size: " << first_entry->second.size() << std::endl;
            
            if (query_embedding.size() != first_entry->second.size()) {
                std::cerr << "  [Error] Embedding size mismatch! Query: " << query_embedding.size() 
                          << ", Database: " << first_entry->second.size() << std::endl;
                std::cerr << "  [Error] This usually means using different models (ONNX vs TRT) or different model versions." << std::endl;
                std::cerr << "  [Error] Solution: Re-register all faces using the same model that's being used for recognition." << std::endl;
                return "Unknown";
            }
        }

        std::string best_match = "Unknown";
        std::string second_match = "Unknown";
        float best_sim = threshold_;
        float second_sim = threshold_;

        // Calculate similarity with all entries and find top 2 matches
        std::vector<std::pair<std::string, float>> similarities;
        for (const auto& [name, db_emb] : database_) {
            float sim = cosine_similarity(query_embedding, db_emb);
            similarities.push_back({name, sim});
            
            if (sim > best_sim) {
                second_sim = best_sim;
                second_match = best_match;
                best_sim = sim;
                best_match = name;
            } else if (sim > second_sim && sim <= best_sim) {
                second_sim = sim;
                second_match = name;
            }
        }

        // Debug: Print all similarities
        std::cout << "  [Debug] Similarity scores:" << std::endl;
        for (const auto& [name, sim] : similarities) {
            std::cout << "    " << name << ": " << std::fixed << std::setprecision(4) << sim << std::endl;
        }

        // Check if best match is significantly better than second match
        // If difference is too small (< 0.1), it might be ambiguous - reject it
        float confidence_gap = best_sim - second_sim;
        if (confidence_gap < 0.1 && best_match != "Unknown") {
            std::cout << "  [Warning] Low confidence gap (" << std::fixed << std::setprecision(4) 
                      << confidence_gap << ") between " << best_match << " (" << std::fixed << std::setprecision(4) << best_sim
                      << ") and " << second_match << " (" << std::fixed << std::setprecision(4) << second_sim << ")" << std::endl;
            std::cout << "  [Result] Rejecting match due to ambiguous similarity scores" << std::endl;
            return "Unknown";
        }

        // Additional check: best similarity must be significantly higher than threshold
        // Require at least 0.15 above threshold to ensure confident match
        float min_required_sim = threshold_ + 0.15f;
        if (best_sim < min_required_sim) {
            std::cout << "  [Warning] Best similarity (" << std::fixed << std::setprecision(4) << best_sim 
                      << ") is too close to threshold (" << threshold_ << ")" << std::endl;
            std::cout << "  [Result] Requiring similarity >= " << std::fixed << std::setprecision(4) << min_required_sim << std::endl;
            return "Unknown";
        }

        return (best_match != "Unknown") 
            ? best_match + " (" + std::to_string(best_sim).substr(0, 4) + ")"
            : "Unknown";
    }

    size_t size() const { return database_.size(); }

    void list_all() {
        std::cout << "\n[DB] Registered (" << database_.size() << "):" << std::endl;
        for (const auto& [name, _] : database_) {
            std::cout << "  - " << name << std::endl;
        }
    }

    void set_threshold(float threshold) { threshold_ = threshold; }
    float threshold() const { return threshold_; }
};

// Global database instance
std::shared_ptr<FaceDatabase> g_database;

int recognize_image(const std::string& executable_path, const std::string& image_path, const std::string& onnx_model_path);

void face_recognition_callback(std::string node_name, int queue_size, 
                               std::shared_ptr<cvedix_objects::cvedix_meta> meta) {
    auto frame_meta = std::dynamic_pointer_cast<cvedix_objects::cvedix_frame_meta>(meta);
    if (!frame_meta || frame_meta->face_targets.empty() || !g_database) return;

    for (auto& face : frame_meta->face_targets) {
        if (!face->embeddings.empty()) {
            std::string person_name = g_database->identify(face->embeddings);
            std::cout << "[Recognition] (" << face->x << "," << face->y 
                      << ") -> " << person_name << std::endl;
        }
    }
}

int recognize_image(const std::string& executable_path, const std::string& image_path, const std::string& onnx_model_path) {
    std::cout << "\n[Image Recognition] Processing: " << image_path << std::endl;
    
    cv::Mat image = cv::imread(image_path);
    if (image.empty()) {
        std::cerr << "[Error] Cannot read image" << std::endl;
        return 1;
    }
    
    std::string project_root = get_project_root(executable_path);
    std::vector<std::string> detector_paths = {
        resolve_path(executable_path, "build/bin/cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx"),
        resolve_path(executable_path, "cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx"),
        (std::filesystem::path(project_root) / "build/bin/cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx").string(),
        (std::filesystem::path(project_root) / "cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx").string()
    };
    
    std::string detector_model_path = detector_paths[0];
    bool found_detector = false;
    for (const auto& path : detector_paths) {
        if (std::filesystem::exists(path)) {
            detector_model_path = path;
            found_detector = true;
            break;
        }
    }
    
    if (!found_detector) {
        std::cerr << "[Error] Face detector model not found. Tried:" << std::endl;
        for (const auto& path : detector_paths) {
            std::cerr << "  - " << path << std::endl;
        }
        return 1;
    }
    
    std::cout << "[Image Recognition] Using detector: " << detector_model_path << std::endl;
    
    cv::Ptr<cv::FaceDetectorYN> face_detector;
    try {
        face_detector = cv::FaceDetectorYN::create(
            detector_model_path, "", cv::Size(320, 320), 0.6f, 0.3f, 5000,
            cv::dnn::DNN_BACKEND_OPENCV, cv::dnn::DNN_TARGET_CPU);
    } catch (const cv::Exception& e) {
        std::cerr << "[Error] Failed to create detector: " << e.what() << std::endl;
        return 1;
    }
    
    if (face_detector.empty()) {
        std::cerr << "[Error] Detector handle is empty" << std::endl;
        return 1;
    }
    
    face_detector->setInputSize(image.size());
    cv::Mat faces;
    try {
        face_detector->detect(image, faces);
    } catch (const cv::Exception& e) {
        std::cerr << "[Error] Detection failed: " << e.what() << std::endl;
        return 1;
    }
    
    if (faces.rows == 0) {
        std::cout << "\n[Result] No faces detected" << std::endl;
        return 0;
    }
    
    cv::Mat result_image = image.clone();
    std::cout << "\n[Results]" << std::endl;
    
        for (int i = 0; i < faces.rows; i++) {
            float x = faces.at<float>(i, 0), y = faces.at<float>(i, 1);
            float w = faces.at<float>(i, 2), h = faces.at<float>(i, 3);
            float score = faces.at<float>(i, 14);
            
            x = std::max(0.0f, std::min(x, (float)(image.cols - 1)));
            y = std::max(0.0f, std::min(y, (float)(image.rows - 1)));
            w = std::max(1.0f, std::min(w, (float)(image.cols - x)));
            h = std::max(1.0f, std::min(h, (float)(image.rows - y)));
            
            // Use face alignment with landmarks if available
            cv::Mat aligned_face;
            if (faces.cols >= 15) {  // YuNet has landmarks
                aligned_face = align_face_using_landmarks(image, faces, i);
            } else {
                // Fallback to simple resize
                cv::Mat face_roi = image(cv::Rect((int)x, (int)y, (int)w, (int)h)).clone();
                cv::resize(face_roi, aligned_face, cv::Size(112, 112));
            }
            
            // Extract embedding with augmentation (original + flip) for better accuracy
            std::vector<std::vector<float>> embeddings;
            
            // Original
            std::vector<float> emb1 = extract_embedding_from_image(aligned_face, onnx_model_path);
            if (!emb1.empty()) embeddings.push_back(emb1);
            
            // Horizontal flip
            cv::Mat flipped;
            cv::flip(aligned_face, flipped, 1);
            std::vector<float> emb2 = extract_embedding_from_image(flipped, onnx_model_path);
            if (!emb2.empty()) embeddings.push_back(emb2);
            
            if (embeddings.empty()) {
                std::cerr << "  [Error] Failed to extract embedding for face " << (i+1) << std::endl;
                continue;
            }
            
            // Average embeddings for more robust recognition
            std::vector<float> final_embedding = average_embeddings(embeddings);
            std::cout << "  [Debug] Extracted embedding size: " << final_embedding.size() 
                      << " (from " << embeddings.size() << " variations)" << std::endl;
            std::string person_name = g_database->identify(final_embedding);
        std::cout << "  Face " << (i+1) << ": (" << (int)x << "," << (int)y << ") " 
                  << (int)w << "x" << (int)h << " -> " << person_name << std::endl;
        
        cv::rectangle(result_image, cv::Rect((int)x, (int)y, (int)w, (int)h), cv::Scalar(0, 255, 0), 2);
        cv::putText(result_image, person_name, cv::Point((int)x, (int)y - 10),
                    cv::FONT_HERSHEY_SIMPLEX, 0.9, cv::Scalar(0, 255, 0), 2);
    }
    
    cv::imwrite("recognition_result.jpg", result_image);
    std::cout << "\n[Output] Result saved to: recognition_result.jpg" << std::endl;
    return 0;
}

int mode_register(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " register <image_path> <person_name> [onnx_model_path]" << std::endl;
        return 1;
    }

    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    std::cout << "\n=== Face Registration Mode ===" << std::endl;
    auto db = std::make_shared<FaceDatabase>(argv[0]);
    
    if (argc >= 5) {
        db->set_model_path(argv[4]);
    }
    
    std::string resolved_image_path = resolve_path(argv[0], argv[2]);
    bool success = db->register_face_from_image(resolved_image_path, argv[3]);
    
    if (success) {
        db->list_all();
        std::cout << "\n✓ Registration completed!" << std::endl;
        return 0;
    }
    std::cerr << "\n✗ Registration failed!" << std::endl;
    return 1;
}

bool is_image_file(const std::string& path) {
    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || 
            ext == ".bmp" || ext == ".tiff" || ext == ".webp");
}

int mode_recognize(int argc, char* argv[]) {
    std::string input_path = (argc >= 3) ? argv[2] : "./cvedix_data/test_video/face.mp4";
    std::string onnx_model_path = (argc >= 4) ? argv[3] : "";

    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    std::cout << "\n=== Face Recognition Mode ===" << std::endl;
    std::string resolved_input_path = resolve_path(argv[0], input_path);
    
    if (!std::filesystem::exists(resolved_input_path)) {
        std::cerr << "\n[Error] Input file not found: " << resolved_input_path << std::endl;
        return 1;
    }
    
    bool is_image = is_image_file(resolved_input_path);
    g_database = std::make_shared<FaceDatabase>(argv[0], "./face_database.txt");
    
    if (!onnx_model_path.empty()) {
        g_database->set_model_path(onnx_model_path);
    }
    
    g_database->list_all();

    if (g_database->size() == 0) {
        std::cerr << "\n[Error] Database is empty! Register faces first." << std::endl;
        std::cerr << "Usage: " << argv[0] << " register <image_path> <person_name>" << std::endl;
        return 1;
    }

    std::cout << "\nConfig: Input=" << resolved_input_path 
              << ", Type=" << (is_image ? "Image" : "Video")
              << ", DB=" << g_database->size() << " faces, Threshold=" << g_database->threshold() << "\n" << std::endl;
    
    if (is_image) {
        std::string model_path = onnx_model_path.empty() 
            ? "./cvedix_data/models/face/face_recognition_sface_2021dec.onnx"
            : onnx_model_path;
        return recognize_image(argv[0], resolved_input_path, resolve_path(argv[0], model_path));
    }

    std::string project_root = get_project_root(argv[0]);
    std::vector<std::string> detector_paths = {
        resolve_path(argv[0], "build/bin/cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx"),
        resolve_path(argv[0], "cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx"),
        (std::filesystem::path(project_root) / "build/bin/cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx").string(),
        (std::filesystem::path(project_root) / "cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx").string()
    };
    
    std::vector<std::string> model_paths = {
        resolve_path(argv[0], "build/bin/cvedix_data/models/face/face_recognition_sface_2021dec.onnx"),
        resolve_path(argv[0], "cvedix_data/models/face/face_recognition_sface_2021dec.onnx"),
        (std::filesystem::path(project_root) / "build/bin/cvedix_data/models/face/face_recognition_sface_2021dec.onnx").string(),
        (std::filesystem::path(project_root) / "cvedix_data/models/face/face_recognition_sface_2021dec.onnx").string()
    };
    
    if (!onnx_model_path.empty()) {
        model_paths.insert(model_paths.begin(), resolve_path(argv[0], onnx_model_path));
    }
    
    std::string detector_model = detector_paths[0];
    bool found_detector = false;
    for (const auto& path : detector_paths) {
        if (std::filesystem::exists(path)) {
            detector_model = path;
            found_detector = true;
            break;
        }
    }
    
    if (!found_detector) {
        std::cerr << "[Error] Face detector model not found. Tried:" << std::endl;
        for (const auto& path : detector_paths) {
            std::cerr << "  - " << path << std::endl;
        }
        return 1;
    }
    
    std::string onnx_model = model_paths[0];
    bool found_model = false;
    for (const auto& path : model_paths) {
        if (std::filesystem::exists(path)) {
            onnx_model = path;
            found_model = true;
            break;
        }
    }
    
    if (!found_model) {
        std::cerr << "[Error] Face recognition model not found. Tried:" << std::endl;
        for (const auto& path : model_paths) {
            std::cerr << "  - " << path << std::endl;
        }
        return 1;
    }
    
    std::cout << "[Video Recognition] Using detector: " << detector_model << std::endl;
    std::cout << "[Video Recognition] Using recognizer: " << onnx_model << std::endl;

    auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src", 0, resolved_input_path, 0.6);
    auto detector = std::make_shared<cvedix_nodes::cvedix_yunet_face_detector_node>("detector", detector_model, 0.9f, 0.3f, 5000);
    auto recognizer = std::make_shared<cvedix_nodes::cvedix_insight_face_recognition_node>("recognizer", onnx_model, 112, 112, true);
    auto osd = std::make_shared<cvedix_nodes::cvedix_face_osd_node_v2>("osd");
    auto screen = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen", 0);

    recognizer->set_meta_handled_hooker(face_recognition_callback);
    detector->attach_to({file_src});
    recognizer->attach_to({detector});
    osd->attach_to({recognizer});
    screen->attach_to({osd});

    std::cout << "Pipeline: file_src → detector → recognizer → osd → screen" << std::endl;
    std::cout << "Starting... Press ENTER to stop\n" << std::endl;

    file_src->start();
    std::string wait;
    std::getline(std::cin, wait);
    file_src->detach_recursively();
    std::cout << "Stopped." << std::endl;
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage:" << std::endl;
        std::cout << "  Register: " << argv[0] << " register <image_path> <person_name> [onnx_model_path]" << std::endl;
        std::cout << "  Recognize: " << argv[0] << " recognize [video_path|image_path] [onnx_model_path]" << std::endl;
        std::cout << "\nExamples:" << std::endl;
        std::cout << "  " << argv[0] << " register alice.jpg \"Alice\"" << std::endl;
        std::cout << "  " << argv[0] << " recognize face.mp4" << std::endl;
        std::cout << "  " << argv[0] << " recognize photo.jpg" << std::endl;
        return 1;
    }

    std::string mode = argv[1];
    if (mode == "register") return mode_register(argc, argv);
    if (mode == "recognize") return mode_recognize(argc, argv);
    
    std::cerr << "Unknown mode: " << mode << std::endl;
    std::cerr << "Valid modes: register, recognize" << std::endl;
    return 1;#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yunet_face_detector_node.h"
#include "cvedix/nodes/infers/cvedix_insight_face_recognition_node.h"
#include "cvedix/nodes/osd/cvedix_face_osd_node_v2.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/utils/logger/cvedix_logger.h"

#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>

/*
 * ============================================================================
 * InsightFace ONNX Face Registration & Recognition Sample
 * ============================================================================
 * 
 * Mô tả:
 *   Sample này demo cách đăng ký khuôn mặt vào database và nhận diện 
 *   khuôn mặt từ video/image stream sử dụng InsightFace với ONNX model.
 * 
 * Chức năng:
 *   1. Đăng ký khuôn mặt từ ảnh vào database
 *   2. Nhận diện khuôn mặt trong video/image stream
 *   3. Hiển thị tên người được nhận diện
 * 
 * Usage:
 *   ./insightface_register_recognize_face_sample [mode] [args...]
 * 
 *   Mode: register
 *     ./insightface_register_recognize_face_sample register <image_path> <person_name> [onnx_model_path]
 *     Ví dụ: ./insightface_register_recognize_face_sample register alice.jpg "Alice"
 * 
 *   Mode: recognize
 *     ./insightface_register_recognize_face_sample recognize [video_path|image_path] [onnx_model_path]
 *     Ví dụ: ./insightface_register_recognize_face_sample recognize face.mp4
 *     Ví dụ: ./insightface_register_recognize_face_sample recognize photo.jpg
 * 
 * Database:
 *   Database được lưu trong file: ./face_database.txt
 *   Format: name|embedding1,embedding2,embedding3,...
 * 
 * Yêu cầu:
 *   - ONNX model: face_recognition_sface_2021dec.onnx hoặc model InsightFace khác
 *   - Build với: cmake .. (không cần TensorRT)
 * 
 * ============================================================================
 */

// Helper function: cosine similarity
static float cosine_similarity(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size() || a.empty()) return 0.0f;
    
    float dot_product = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;
    
    for (size_t i = 0; i < a.size(); i++) {
        dot_product += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    
    float denominator = std::sqrt(norm_a) * std::sqrt(norm_b);
    if (denominator < 1e-6) return 0.0f;
    
    return dot_product / denominator;
}

// Helper function: Average embeddings
static std::vector<float> average_embeddings(const std::vector<std::vector<float>>& embeddings) {
    if (embeddings.empty() || embeddings[0].empty()) return std::vector<float>();
    
    size_t dim = embeddings[0].size();
    std::vector<float> avg_embedding(dim, 0.0f);
    
    for (const auto& emb : embeddings) {
        if (emb.size() != dim) continue;
        for (size_t i = 0; i < dim; i++) {
            avg_embedding[i] += emb[i];
        }
    }
    
    float count = static_cast<float>(embeddings.size());
    for (size_t i = 0; i < dim; i++) {
        avg_embedding[i] /= count;
    }
    
    // L2 normalize
    float norm = 0.0f;
    for (float val : avg_embedding) {
        norm += val * val;
    }
    norm = std::sqrt(norm);
    if (norm > 1e-6) {
        for (float& val : avg_embedding) {
            val /= norm;
        }
    }
    
    return avg_embedding;
}

// Helper function: Face alignment using landmarks (if available)
static cv::Mat align_face_using_landmarks(const cv::Mat& image, const cv::Mat& faces, int face_idx) {
    // YuNet format: (x, y, w, h, re_x, re_y, le_x, le_y, nt_x, nt_y, rcm_x, rcm_y, lcm_x, lcm_y, score)
    // Landmarks order: right_eye, left_eye, nose_tip, right_mouth_corner, left_mouth_corner
    
    float re_x = faces.at<float>(face_idx, 4);
    float re_y = faces.at<float>(face_idx, 5);
    float le_x = faces.at<float>(face_idx, 6);
    float le_y = faces.at<float>(face_idx, 7);
    float nt_x = faces.at<float>(face_idx, 8);
    float nt_y = faces.at<float>(face_idx, 9);
    float rcm_x = faces.at<float>(face_idx, 10);
    float rcm_y = faces.at<float>(face_idx, 11);
    float lcm_x = faces.at<float>(face_idx, 12);
    float lcm_y = faces.at<float>(face_idx, 13);
    
    // Standard face template for 112x112 (InsightFace)
    float dst[5][2] = {
        {38.2946f, 51.6963f},  // right eye
        {73.5318f, 51.5014f},  // left eye
        {56.0252f, 71.7366f},  // nose tip
        {41.5493f, 92.3655f},  // right mouth corner
        {70.7299f, 92.2041f}   // left mouth corner
    };
    
    float src[5][2] = {
        {re_x, re_y},
        {le_x, le_y},
        {nt_x, nt_y},
        {rcm_x, rcm_y},
        {lcm_x, lcm_y}
    };
    
    // Compute similarity transform matrix
    float src_mean[2] = {
        (src[0][0] + src[1][0] + src[2][0] + src[3][0] + src[4][0]) / 5.0f,
        (src[0][1] + src[1][1] + src[2][1] + src[3][1] + src[4][1]) / 5.0f
    };
    float dst_mean[2] = {56.0262f, 71.9008f};
    
    float src_demean[5][2], dst_demean[5][2];
    for (int i = 0; i < 5; i++) {
        src_demean[i][0] = src[i][0] - src_mean[0];
        src_demean[i][1] = src[i][1] - src_mean[1];
        dst_demean[i][0] = dst[i][0] - dst_mean[0];
        dst_demean[i][1] = dst[i][1] - dst_mean[1];
    }
    
    double A00 = 0.0, A01 = 0.0, A10 = 0.0, A11 = 0.0;
    for (int i = 0; i < 5; i++) {
        A00 += dst_demean[i][0] * src_demean[i][0];
        A01 += dst_demean[i][0] * src_demean[i][1];
        A10 += dst_demean[i][1] * src_demean[i][0];
        A11 += dst_demean[i][1] * src_demean[i][1];
    }
    A00 /= 5.0; A01 /= 5.0; A10 /= 5.0; A11 /= 5.0;
    
    double detA = A00 * A11 - A01 * A10;
    double d[2] = {1.0, (detA < 0) ? -1.0 : 1.0};
    
    cv::Mat A = (cv::Mat_<double>(2, 2) << A00, A01, A10, A11);
    cv::Mat s, u, vt;
    cv::SVD::compute(A, s, u, vt);
    
    double smax = std::max(s.at<double>(0), s.at<double>(1));
    double tol = smax * 2 * FLT_MIN;
    int rank = 0;
    if (s.at<double>(0) > tol) rank++;
    if (s.at<double>(1) > tol) rank++;
    
    if (rank == 0) {
        // Fallback to simple resize if alignment fails
        cv::Mat aligned;
        cv::resize(image, aligned, cv::Size(112, 112));
        return aligned;
    }
    
    cv::Mat T = u * cv::Mat::diag(cv::Mat(cv::Vec2d(d[0], d[1]))) * vt;
    
    double var1 = 0.0, var2 = 0.0;
    for (int i = 0; i < 5; i++) {
        var1 += src_demean[i][0] * src_demean[i][0];
        var2 += src_demean[i][1] * src_demean[i][1];
    }
    var1 /= 5.0;
    var2 /= 5.0;
    
    double scale = 1.0 / (var1 + var2) * (s.at<double>(0) * d[0] + s.at<double>(1) * d[1]);
    double TS[2] = {
        T.at<double>(0, 0) * src_mean[0] + T.at<double>(0, 1) * src_mean[1],
        T.at<double>(1, 0) * src_mean[0] + T.at<double>(1, 1) * src_mean[1]
    };
    
    cv::Mat transform_mat = (cv::Mat_<double>(2, 3) <<
        T.at<double>(0, 0) * scale, T.at<double>(0, 1) * scale, dst_mean[0] - scale * TS[0],
        T.at<double>(1, 0) * scale, T.at<double>(1, 1) * scale, dst_mean[1] - scale * TS[1]);
    
    cv::Mat aligned;
    cv::warpAffine(image, aligned, transform_mat, cv::Size(112, 112), cv::INTER_LINEAR);
    return aligned;
}

// Helper function: extract embedding from aligned face image
static std::vector<float> extract_embedding_from_image(
    const cv::Mat& aligned_face, 
    const std::string& onnx_model_path) {
    
    // Load ONNX model
    cv::dnn::Net net = cv::dnn::readNetFromONNX(onnx_model_path);
    if (net.empty()) {
        std::cerr << "[Error] Failed to load ONNX model: " << onnx_model_path << std::endl;
        return std::vector<float>();
    }
    
    #ifdef CVEDIX_WITH_CUDA
    net.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
    net.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
    #else
    net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    #endif
    
    // Preprocess: BGR->RGB, normalize (pixel - 127.5) / 128.0
    cv::Mat rgb;
    cv::cvtColor(aligned_face, rgb, cv::COLOR_BGR2RGB);
    
    // Ensure 112x112
    if (rgb.rows != 112 || rgb.cols != 112) {
        cv::resize(rgb, rgb, cv::Size(112, 112), 0, 0, cv::INTER_LINEAR);
    }
    
    // Create blob: (pixel - 127.5) / 128.0
    cv::Mat blob;
    cv::dnn::blobFromImage(rgb, blob, 1.0f / 128.0f,
                          cv::Size(), cv::Scalar(127.5f, 127.5f, 127.5f),
                          false, false, CV_32F);
    
    // Run inference
    net.setInput(blob);
    std::vector<cv::Mat> outputs;
    net.forward(outputs, net.getUnconnectedOutLayersNames());
    
    if (outputs.empty()) {
        return std::vector<float>();
    }
    
    // Extract embedding
    const cv::Mat& output = outputs[0];
    int emb_dim = (output.dims == 2) ? output.size[1] : output.size[0];
    
    std::vector<float> embedding(emb_dim);
    const float* output_ptr = output.ptr<float>();
    std::copy(output_ptr, output_ptr + emb_dim, embedding.begin());
    
    // L2 normalize
    float norm = 0.0f;
    for (float val : embedding) {
        norm += val * val;
    }
    norm = std::sqrt(norm);
    if (norm > 1e-6) {
        for (float& val : embedding) {
            val /= norm;
        }
    }
    
    return embedding;
}

static std::string get_project_root(const std::string& executable_path) {
    std::filesystem::path current = std::filesystem::absolute(executable_path).parent_path();
    
    for (int depth = 0; depth < 5; depth++) {
        if (current.filename() == "bin" && current.parent_path().filename() == "build") {
            return current.parent_path().parent_path().string();
        }
        if (std::filesystem::exists(current / "cvedix_data")) {
            return current.string();
        }
        if (current.has_parent_path() && current != current.parent_path()) {
            current = current.parent_path();
        } else {
            break;
        }
    }
    return std::filesystem::current_path().string();
}

static std::string resolve_path(const std::string& executable_path, const std::string& relative_path) {
    if (std::filesystem::path(relative_path).is_absolute()) {
        return relative_path;
    }
    
    std::string project_root = get_project_root(executable_path);
    std::filesystem::path full_path = std::filesystem::path(project_root) / relative_path;
    
    if (std::filesystem::exists(full_path)) return full_path.string();
    
    std::filesystem::path current_path = std::filesystem::current_path() / relative_path;
    if (std::filesystem::exists(current_path)) return current_path.string();
    
    return full_path.string();
}

// Face Database Class
class FaceDatabase {
private:
    std::map<std::string, std::vector<float>> database_;
    std::string db_file_path_;
    std::string project_root_;
    float threshold_ = 0.7f;  // Increased from 0.6 to 0.7 for better accuracy
    std::string onnx_model_path_;
    
    std::string resolve_model_path(const std::string& relative_path) {
        std::filesystem::path full_path = std::filesystem::path(project_root_) / relative_path;
        if (std::filesystem::exists(full_path)) return full_path.string();
        
        std::filesystem::path current_path = std::filesystem::current_path() / relative_path;
        if (std::filesystem::exists(current_path)) return current_path.string();
        if (std::filesystem::exists(relative_path)) return relative_path;
        
        return full_path.string();
    }

    void load_database() {
        std::ifstream file(db_file_path_);
        if (!file.is_open()) {
            std::ofstream create_file(db_file_path_);
            std::cout << "[DB] Creating new database" << std::endl;
            return;
        }

        std::string line;
        int count = 0;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            size_t pos = line.find('|');
            if (pos == std::string::npos) continue;

            std::string name = line.substr(0, pos);
            std::string embedding_str = line.substr(pos + 1);
            std::vector<float> embedding;
            std::stringstream ss(embedding_str);
            std::string value;
            
            while (std::getline(ss, value, ',')) {
                embedding.push_back(std::stof(value));
            }

            // Accept any embedding size (not just 512)
            if (!embedding.empty()) {
                database_[name] = embedding;
                count++;
            }
        }
        std::cout << "[DB] Loaded " << count << " faces" << std::endl;
    }

    void save_database() {
        std::ofstream file(db_file_path_);
        if (!file.is_open()) {
            std::cerr << "[DB] Error: Cannot save to " << db_file_path_ << std::endl;
            return;
        }

        for (const auto& [name, embedding] : database_) {
            file << name << "|";
            for (size_t i = 0; i < embedding.size(); i++) {
                file << std::fixed << std::setprecision(6) << embedding[i];
                if (i < embedding.size() - 1) file << ",";
            }
            file << "\n";
        }
        std::cout << "[DB] Saved " << database_.size() << " faces" << std::endl;
    }

public:
    FaceDatabase(const std::string& executable_path, const std::string& db_path = "./face_database.txt") 
        : project_root_(get_project_root(executable_path)),
          db_file_path_(resolve_path(executable_path, db_path)) {
        
        load_database();
        
        // Find ONNX model
        std::vector<std::string> model_paths = {
            resolve_path(executable_path, "build/bin/cvedix_data/models/face/face_recognition_sface_2021dec.onnx"),
            resolve_path(executable_path, "cvedix_data/models/face/face_recognition_sface_2021dec.onnx"),
            (std::filesystem::path(project_root_) / "build/bin/cvedix_data/models/face/face_recognition_sface_2021dec.onnx").string(),
            (std::filesystem::path(project_root_) / "cvedix_data/models/face/face_recognition_sface_2021dec.onnx").string()
        };
        
        for (const auto& path : model_paths) {
            if (std::filesystem::exists(path)) {
                onnx_model_path_ = path;
                break;
            }
        }
        
        if (onnx_model_path_.empty()) {
            std::cerr << "[DB] Warning: ONNX model not found, using default path" << std::endl;
            onnx_model_path_ = model_paths[0];
        }
    }
    
    void set_model_path(const std::string& model_path) {
        onnx_model_path_ = resolve_path("", model_path);
    }

    bool register_face_from_image(const std::string& image_path, const std::string& person_name) {
        std::cout << "\n[Register] Image: " << image_path << ", Name: " << person_name << std::endl;

        if (!std::filesystem::exists(image_path)) {
            std::cerr << "[Register] Error: File not found" << std::endl;
            return false;
        }

        cv::Mat image = cv::imread(image_path);
        if (image.empty()) {
            std::cerr << "[Register] Error: Cannot read image" << std::endl;
            return false;
        }

        std::vector<std::string> detector_paths = {
            resolve_model_path("build/bin/cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx"),
            resolve_model_path("cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx"),
            (std::filesystem::path(project_root_) / "build/bin/cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx").string(),
            (std::filesystem::path(project_root_) / "cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx").string()
        };
        
        std::string detector_model_path = detector_paths[0];
        for (const auto& path : detector_paths) {
            if (std::filesystem::exists(path)) {
                detector_model_path = path;
                break;
            }
        }

        cv::Ptr<cv::FaceDetectorYN> face_detector;
        try {
            face_detector = cv::FaceDetectorYN::create(
                detector_model_path, "", cv::Size(320, 320), 0.6f, 0.3f, 5000,
                cv::dnn::DNN_BACKEND_OPENCV, cv::dnn::DNN_TARGET_CPU);
        } catch (const std::exception& e) {
            std::cerr << "[Register] Error: Failed to create detector: " << e.what() << std::endl;
            return false;
        }
        
        if (face_detector.empty()) {
            std::cerr << "[Register] Error: Detector handle is empty" << std::endl;
            return false;
        }

        face_detector->setInputSize(image.size());
        cv::Mat faces;
        try {
            face_detector->detect(image, faces);
        } catch (const cv::Exception& e) {
            std::cerr << "[Register] Error: Detection failed: " << e.what() << std::endl;
            return false;
        }

        if (faces.rows == 0 || faces.empty()) {
            std::cerr << "[Register] Error: No face detected" << std::endl;
            return false;
        }

        float x = faces.at<float>(0, 0), y = faces.at<float>(0, 1);
        float w = faces.at<float>(0, 2), h = faces.at<float>(0, 3);
        float score = faces.at<float>(0, 14);

        x = std::max(0.0f, std::min(x, (float)(image.cols - 1)));
        y = std::max(0.0f, std::min(y, (float)(image.rows - 1)));
        w = std::max(1.0f, std::min(w, (float)(image.cols - x)));
        h = std::max(1.0f, std::min(h, (float)(image.rows - y)));

        std::cout << "[Register] Face: (" << (int)x << "," << (int)y << ") " 
                  << (int)w << "x" << (int)h << " (score: " << score << ")" << std::endl;

        // Use face alignment with landmarks if available
        cv::Mat aligned_face;
        if (faces.cols >= 15) {  // YuNet has landmarks
            aligned_face = align_face_using_landmarks(image, faces, 0);
            std::cout << "[Register] Using landmark-based alignment" << std::endl;
        } else {
            // Fallback to simple resize
            cv::Mat face_roi = image(cv::Rect((int)x, (int)y, (int)w, (int)h)).clone();
            cv::resize(face_roi, aligned_face, cv::Size(112, 112));
            std::cout << "[Register] Using simple resize (no landmarks)" << std::endl;
        }

        // Data augmentation: Create multiple variations and average embeddings
        std::vector<std::vector<float>> embeddings;
        
        // 1. Original
        std::vector<float> emb1 = extract_embedding_from_image(aligned_face, onnx_model_path_);
        if (!emb1.empty()) embeddings.push_back(emb1);
        
        // 2. Horizontal flip
        cv::Mat flipped;
        cv::flip(aligned_face, flipped, 1);
        std::vector<float> emb2 = extract_embedding_from_image(flipped, onnx_model_path_);
        if (!emb2.empty()) embeddings.push_back(emb2);
        
        // 3. Slight brightness increase
        cv::Mat bright;
        aligned_face.convertTo(bright, -1, 1.0, 15);
        std::vector<float> emb3 = extract_embedding_from_image(bright, onnx_model_path_);
        if (!emb3.empty()) embeddings.push_back(emb3);
        
        // 4. Slight brightness decrease
        cv::Mat dark;
        aligned_face.convertTo(dark, -1, 1.0, -15);
        std::vector<float> emb4 = extract_embedding_from_image(dark, onnx_model_path_);
        if (!emb4.empty()) embeddings.push_back(emb4);
        
        // 5. Slight contrast increase
        cv::Mat contrast;
        aligned_face.convertTo(contrast, -1, 1.1, 0);
        std::vector<float> emb5 = extract_embedding_from_image(contrast, onnx_model_path_);
        if (!emb5.empty()) embeddings.push_back(emb5);

        if (embeddings.empty()) {
            std::cerr << "[Register] Error: Failed to extract any embeddings" << std::endl;
            return false;
        }

        // Average all embeddings for more robust representation
        std::vector<float> final_embedding = average_embeddings(embeddings);
        std::cout << "[Register] Generated " << embeddings.size() << " embeddings, averaged to final embedding" << std::endl;

        database_[person_name] = final_embedding;
        save_database();
        std::cout << "[Register] ✓ Registered: " << person_name << " (using " << embeddings.size() << " augmented variations)" << std::endl;
        return true;
    }

    std::string identify(const std::vector<float>& query_embedding) {
        if (query_embedding.empty()) return "Unknown";

        // Debug: Check embedding sizes
        std::cout << "  [Debug] Query embedding size: " << query_embedding.size() << std::endl;
        if (!database_.empty()) {
            auto first_entry = database_.begin();
            std::cout << "  [Debug] Database embedding size: " << first_entry->second.size() << std::endl;
            
            if (query_embedding.size() != first_entry->second.size()) {
                std::cerr << "  [Error] Embedding size mismatch! Query: " << query_embedding.size() 
                          << ", Database: " << first_entry->second.size() << std::endl;
                std::cerr << "  [Error] This usually means using different models (ONNX vs TRT) or different model versions." << std::endl;
                std::cerr << "  [Error] Solution: Re-register all faces using the same model that's being used for recognition." << std::endl;
                return "Unknown";
            }
        }

        std::string best_match = "Unknown";
        std::string second_match = "Unknown";
        float best_sim = threshold_;
        float second_sim = threshold_;

        // Calculate similarity with all entries and find top 2 matches
        std::vector<std::pair<std::string, float>> similarities;
        for (const auto& [name, db_emb] : database_) {
            float sim = cosine_similarity(query_embedding, db_emb);
            similarities.push_back({name, sim});
            
            if (sim > best_sim) {
                second_sim = best_sim;
                second_match = best_match;
                best_sim = sim;
                best_match = name;
            } else if (sim > second_sim && sim <= best_sim) {
                second_sim = sim;
                second_match = name;
            }
        }

        // Debug: Print all similarities
        std::cout << "  [Debug] Similarity scores:" << std::endl;
        for (const auto& [name, sim] : similarities) {
            std::cout << "    " << name << ": " << std::fixed << std::setprecision(4) << sim << std::endl;
        }

        // Check if best match is significantly better than second match
        // If difference is too small (< 0.1), it might be ambiguous - reject it
        float confidence_gap = best_sim - second_sim;
        if (confidence_gap < 0.1 && best_match != "Unknown") {
            std::cout << "  [Warning] Low confidence gap (" << std::fixed << std::setprecision(4) 
                      << confidence_gap << ") between " << best_match << " (" << std::fixed << std::setprecision(4) << best_sim
                      << ") and " << second_match << " (" << std::fixed << std::setprecision(4) << second_sim << ")" << std::endl;
            std::cout << "  [Result] Rejecting match due to ambiguous similarity scores" << std::endl;
            return "Unknown";
        }

        // Additional check: best similarity must be significantly higher than threshold
        // Require at least 0.15 above threshold to ensure confident match
        float min_required_sim = threshold_ + 0.15f;
        if (best_sim < min_required_sim) {
            std::cout << "  [Warning] Best similarity (" << std::fixed << std::setprecision(4) << best_sim 
                      << ") is too close to threshold (" << threshold_ << ")" << std::endl;
            std::cout << "  [Result] Requiring similarity >= " << std::fixed << std::setprecision(4) << min_required_sim << std::endl;
            return "Unknown";
        }

        return (best_match != "Unknown") 
            ? best_match + " (" + std::to_string(best_sim).substr(0, 4) + ")"
            : "Unknown";
    }

    size_t size() const { return database_.size(); }

    void list_all() {
        std::cout << "\n[DB] Registered (" << database_.size() << "):" << std::endl;
        for (const auto& [name, _] : database_) {
            std::cout << "  - " << name << std::endl;
        }
    }

    void set_threshold(float threshold) { threshold_ = threshold; }
    float threshold() const { return threshold_; }
};

// Global database instance
std::shared_ptr<FaceDatabase> g_database;

int recognize_image(const std::string& executable_path, const std::string& image_path, const std::string& onnx_model_path);

void face_recognition_callback(std::string node_name, int queue_size, 
                               std::shared_ptr<cvedix_objects::cvedix_meta> meta) {
    auto frame_meta = std::dynamic_pointer_cast<cvedix_objects::cvedix_frame_meta>(meta);
    if (!frame_meta || frame_meta->face_targets.empty() || !g_database) return;

    for (auto& face : frame_meta->face_targets) {
        if (!face->embeddings.empty()) {
            std::string person_name = g_database->identify(face->embeddings);
            std::cout << "[Recognition] (" << face->x << "," << face->y 
                      << ") -> " << person_name << std::endl;
        }
    }
}

int recognize_image(const std::string& executable_path, const std::string& image_path, const std::string& onnx_model_path) {
    std::cout << "\n[Image Recognition] Processing: " << image_path << std::endl;
    
    cv::Mat image = cv::imread(image_path);
    if (image.empty()) {
        std::cerr << "[Error] Cannot read image" << std::endl;
        return 1;
    }
    
    std::string project_root = get_project_root(executable_path);
    std::vector<std::string> detector_paths = {
        resolve_path(executable_path, "build/bin/cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx"),
        resolve_path(executable_path, "cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx"),
        (std::filesystem::path(project_root) / "build/bin/cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx").string(),
        (std::filesystem::path(project_root) / "cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx").string()
    };
    
    std::string detector_model_path = detector_paths[0];
    bool found_detector = false;
    for (const auto& path : detector_paths) {
        if (std::filesystem::exists(path)) {
            detector_model_path = path;
            found_detector = true;
            break;
        }
    }
    
    if (!found_detector) {
        std::cerr << "[Error] Face detector model not found. Tried:" << std::endl;
        for (const auto& path : detector_paths) {
            std::cerr << "  - " << path << std::endl;
        }
        return 1;
    }
    
    std::cout << "[Image Recognition] Using detector: " << detector_model_path << std::endl;
    
    cv::Ptr<cv::FaceDetectorYN> face_detector;
    try {
        face_detector = cv::FaceDetectorYN::create(
            detector_model_path, "", cv::Size(320, 320), 0.6f, 0.3f, 5000,
            cv::dnn::DNN_BACKEND_OPENCV, cv::dnn::DNN_TARGET_CPU);
    } catch (const cv::Exception& e) {
        std::cerr << "[Error] Failed to create detector: " << e.what() << std::endl;
        return 1;
    }
    
    if (face_detector.empty()) {
        std::cerr << "[Error] Detector handle is empty" << std::endl;
        return 1;
    }
    
    face_detector->setInputSize(image.size());
    cv::Mat faces;
    try {
        face_detector->detect(image, faces);
    } catch (const cv::Exception& e) {
        std::cerr << "[Error] Detection failed: " << e.what() << std::endl;
        return 1;
    }
    
    if (faces.rows == 0) {
        std::cout << "\n[Result] No faces detected" << std::endl;
        return 0;
    }
    
    cv::Mat result_image = image.clone();
    std::cout << "\n[Results]" << std::endl;
    
        for (int i = 0; i < faces.rows; i++) {
            float x = faces.at<float>(i, 0), y = faces.at<float>(i, 1);
            float w = faces.at<float>(i, 2), h = faces.at<float>(i, 3);
            float score = faces.at<float>(i, 14);
            
            x = std::max(0.0f, std::min(x, (float)(image.cols - 1)));
            y = std::max(0.0f, std::min(y, (float)(image.rows - 1)));
            w = std::max(1.0f, std::min(w, (float)(image.cols - x)));
            h = std::max(1.0f, std::min(h, (float)(image.rows - y)));
            
            // Use face alignment with landmarks if available
            cv::Mat aligned_face;
            if (faces.cols >= 15) {  // YuNet has landmarks
                aligned_face = align_face_using_landmarks(image, faces, i);
            } else {
                // Fallback to simple resize
                cv::Mat face_roi = image(cv::Rect((int)x, (int)y, (int)w, (int)h)).clone();
                cv::resize(face_roi, aligned_face, cv::Size(112, 112));
            }
            
            // Extract embedding with augmentation (original + flip) for better accuracy
            std::vector<std::vector<float>> embeddings;
            
            // Original
            std::vector<float> emb1 = extract_embedding_from_image(aligned_face, onnx_model_path);
            if (!emb1.empty()) embeddings.push_back(emb1);
            
            // Horizontal flip
            cv::Mat flipped;
            cv::flip(aligned_face, flipped, 1);
            std::vector<float> emb2 = extract_embedding_from_image(flipped, onnx_model_path);
            if (!emb2.empty()) embeddings.push_back(emb2);
            
            if (embeddings.empty()) {
                std::cerr << "  [Error] Failed to extract embedding for face " << (i+1) << std::endl;
                continue;
            }
            
            // Average embeddings for more robust recognition
            std::vector<float> final_embedding = average_embeddings(embeddings);
            std::cout << "  [Debug] Extracted embedding size: " << final_embedding.size() 
                      << " (from " << embeddings.size() << " variations)" << std::endl;
            std::string person_name = g_database->identify(final_embedding);
        std::cout << "  Face " << (i+1) << ": (" << (int)x << "," << (int)y << ") " 
                  << (int)w << "x" << (int)h << " -> " << person_name << std::endl;
        
        cv::rectangle(result_image, cv::Rect((int)x, (int)y, (int)w, (int)h), cv::Scalar(0, 255, 0), 2);
        cv::putText(result_image, person_name, cv::Point((int)x, (int)y - 10),
                    cv::FONT_HERSHEY_SIMPLEX, 0.9, cv::Scalar(0, 255, 0), 2);
    }
    
    cv::imwrite("recognition_result.jpg", result_image);
    std::cout << "\n[Output] Result saved to: recognition_result.jpg" << std::endl;
    return 0;
}

int mode_register(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " register <image_path> <person_name> [onnx_model_path]" << std::endl;
        return 1;
    }

    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    std::cout << "\n=== Face Registration Mode ===" << std::endl;
    auto db = std::make_shared<FaceDatabase>(argv[0]);
    
    if (argc >= 5) {
        db->set_model_path(argv[4]);
    }
    
    std::string resolved_image_path = resolve_path(argv[0], argv[2]);
    bool success = db->register_face_from_image(resolved_image_path, argv[3]);
    
    if (success) {
        db->list_all();
        std::cout << "\n✓ Registration completed!" << std::endl;
        return 0;
    }
    std::cerr << "\n✗ Registration failed!" << std::endl;
    return 1;
}

bool is_image_file(const std::string& path) {
    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || 
            ext == ".bmp" || ext == ".tiff" || ext == ".webp");
}

int mode_recognize(int argc, char* argv[]) {
    std::string input_path = (argc >= 3) ? argv[2] : "./cvedix_data/test_video/face.mp4";
    std::string onnx_model_path = (argc >= 4) ? argv[3] : "";

    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();

    std::cout << "\n=== Face Recognition Mode ===" << std::endl;
    std::string resolved_input_path = resolve_path(argv[0], input_path);
    
    if (!std::filesystem::exists(resolved_input_path)) {
        std::cerr << "\n[Error] Input file not found: " << resolved_input_path << std::endl;
        return 1;
    }
    
    bool is_image = is_image_file(resolved_input_path);
    g_database = std::make_shared<FaceDatabase>(argv[0], "./face_database.txt");
    
    if (!onnx_model_path.empty()) {
        g_database->set_model_path(onnx_model_path);
    }
    
    g_database->list_all();

    if (g_database->size() == 0) {
        std::cerr << "\n[Error] Database is empty! Register faces first." << std::endl;
        std::cerr << "Usage: " << argv[0] << " register <image_path> <person_name>" << std::endl;
        return 1;
    }

    std::cout << "\nConfig: Input=" << resolved_input_path 
              << ", Type=" << (is_image ? "Image" : "Video")
              << ", DB=" << g_database->size() << " faces, Threshold=" << g_database->threshold() << "\n" << std::endl;
    
    if (is_image) {
        std::string model_path = onnx_model_path.empty() 
            ? "./cvedix_data/models/face/face_recognition_sface_2021dec.onnx"
            : onnx_model_path;
        return recognize_image(argv[0], resolved_input_path, resolve_path(argv[0], model_path));
    }

    std::string project_root = get_project_root(argv[0]);
    std::vector<std::string> detector_paths = {
        resolve_path(argv[0], "build/bin/cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx"),
        resolve_path(argv[0], "cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx"),
        (std::filesystem::path(project_root) / "build/bin/cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx").string(),
        (std::filesystem::path(project_root) / "cvedix_data/models/face/face_detection_yunet_2023mar_int8.onnx").string()
    };
    
    std::vector<std::string> model_paths = {
        resolve_path(argv[0], "build/bin/cvedix_data/models/face/face_recognition_sface_2021dec.onnx"),
        resolve_path(argv[0], "cvedix_data/models/face/face_recognition_sface_2021dec.onnx"),
        (std::filesystem::path(project_root) / "build/bin/cvedix_data/models/face/face_recognition_sface_2021dec.onnx").string(),
        (std::filesystem::path(project_root) / "cvedix_data/models/face/face_recognition_sface_2021dec.onnx").string()
    };
    
    if (!onnx_model_path.empty()) {
        model_paths.insert(model_paths.begin(), resolve_path(argv[0], onnx_model_path));
    }
    
    std::string detector_model = detector_paths[0];
    bool found_detector = false;
    for (const auto& path : detector_paths) {
        if (std::filesystem::exists(path)) {
            detector_model = path;
            found_detector = true;
            break;
        }
    }
    
    if (!found_detector) {
        std::cerr << "[Error] Face detector model not found. Tried:" << std::endl;
        for (const auto& path : detector_paths) {
            std::cerr << "  - " << path << std::endl;
        }
        return 1;
    }
    
    std::string onnx_model = model_paths[0];
    bool found_model = false;
    for (const auto& path : model_paths) {
        if (std::filesystem::exists(path)) {
            onnx_model = path;
            found_model = true;
            break;
        }
    }
    
    if (!found_model) {
        std::cerr << "[Error] Face recognition model not found. Tried:" << std::endl;
        for (const auto& path : model_paths) {
            std::cerr << "  - " << path << std::endl;
        }
        return 1;
    }
    
    std::cout << "[Video Recognition] Using detector: " << detector_model << std::endl;
    std::cout << "[Video Recognition] Using recognizer: " << onnx_model << std::endl;

    auto file_src = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src", 0, resolved_input_path, 0.6);
    auto detector = std::make_shared<cvedix_nodes::cvedix_yunet_face_detector_node>("detector", detector_model, 0.9f, 0.3f, 5000);
    auto recognizer = std::make_shared<cvedix_nodes::cvedix_insight_face_recognition_node>("recognizer", onnx_model, 112, 112, true);
    auto osd = std::make_shared<cvedix_nodes::cvedix_face_osd_node_v2>("osd");
    auto screen = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen", 0);

    recognizer->set_meta_handled_hooker(face_recognition_callback);
    detector->attach_to({file_src});
    recognizer->attach_to({detector});
    osd->attach_to({recognizer});
    screen->attach_to({osd});

    std::cout << "Pipeline: file_src → detector → recognizer → osd → screen" << std::endl;
    std::cout << "Starting... Press ENTER to stop\n" << std::endl;

    file_src->start();
    std::string wait;
    std::getline(std::cin, wait);
    file_src->detach_recursively();
    std::cout << "Stopped." << std::endl;
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage:" << std::endl;
        std::cout << "  Register: " << argv[0] << " register <image_path> <person_name> [onnx_model_path]" << std::endl;
        std::cout << "  Recognize: " << argv[0] << " recognize [video_path|image_path] [onnx_model_path]" << std::endl;
        std::cout << "\nExamples:" << std::endl;
        std::cout << "  " << argv[0] << " register alice.jpg \"Alice\"" << std::endl;
        std::cout << "  " << argv[0] << " recognize face.mp4" << std::endl;
        std::cout << "  " << argv[0] << " recognize photo.jpg" << std::endl;
        return 1;
    }

    std::string mode = argv[1];
    if (mode == "register") return mode_register(argc, argv);
    if (mode == "recognize") return mode_recognize(argc, argv);
    
    std::cerr << "Unknown mode: " << mode << std::endl;
    std::cerr << "Valid modes: register, recognize" << std::endl;
    return 1;
}
}