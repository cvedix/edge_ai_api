#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yunet_face_detector_node.h"
#include "cvedix/nodes/track/cvedix_sort_track_node.h"
#include "cvedix/nodes/osd/cvedix_face_osd_node_v2.h"
#include "cvedix/nodes/des/cvedix_screen_des_node.h"
// #include "cvedix/nodes/des/cvedix_rtmp_des_node.h"
#include "cvedix/nodes/mid/cvedix_split_node.h"
#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include "cvedix/nodes/broker/cvedix_json_enhanced_console_broker_node.h"
#include "cvedix/nodes/broker/cvedix_json_mqtt_broker_node.h"
#include "cvedix/nodes/broker/cereal_archive/cvedix_objects_cereal_archive.h"
#include "cpp_base64/base64.h"
#include <opencv2/imgcodecs.hpp>
#include <csignal>
#include <thread>
#include <atomic>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <mosquitto.h>

/*
 * ## Simple MQTT Sample ##
 * 
 * Sample đơn giản gửi MQTT event publishing
 * // Sample đơn giản kết hợp RTMP streaming và MQTT event publishing
 * 
 * Tính năng:
 * - Đọc video từ file (có thể thay bằng RTSP)
 * - Phát hiện khuôn mặt với YuNet
 * - Theo dõi khuôn mặt với SORT tracker
 * - Gửi message GIẢ qua MQTT để test (không gửi message thật từ detection)
 * // - Gửi video stream lên RTMP server
 * - Hiển thị trên màn hình
 * 
 * Yêu cầu:
 * - Build với: cmake -DCVEDIX_WITH_MQTT=ON ..
 * - MQTT broker (mosquitto)
 * // - RTMP server (nginx-rtmp hoặc tương tự)
 * 
 * Cấu hình:
 * - Chỉnh sửa MQTT_BROKER trong code
 * // - Chỉnh sửa RTMP_URL và MQTT_BROKER trong code
 * 
 * Sử dụng:
 *   ./simple_rtmp_mqtt_sample [video_file] [mqtt_broker] [mqtt_port] [mqtt_topic]
 * //   ./simple_rtmp_mqtt_sample [video_file] [rtmp_url] [mqtt_broker] [mqtt_port] [mqtt_topic]
 * 
 * Ví dụ:
 *   ./simple_rtmp_mqtt_sample ./test_video.mp4 localhost 1883 events
 * //   ./simple_rtmp_mqtt_sample ./test_video.mp4 rtmp://localhost:1935/live/test localhost 1883 events
 */

// Global flag for signal handling
volatile sig_atomic_t stop_flag = 0;

void signal_handler(int signal) {
    stop_flag = 1;
}

// Helper function to crop image
cv::Mat crop_image(const cv::Mat& frame, int x, int y, int width, int height) {
    if (frame.empty()) return cv::Mat();
    int x1 = std::max(0, x);
    int y1 = std::max(0, y);
    int x2 = std::min(frame.cols, x + width);
    int y2 = std::min(frame.rows, y + height);
    if (x2 <= x1 || y2 <= y1) return cv::Mat();
    cv::Rect roi(x1, y1, x2 - x1, y2 - y1);
    return frame(roi).clone();
}

// Helper function to encode image to base64
std::string mat_to_base64(const cv::Mat& img) {
    if (img.empty()) return "";
    std::vector<uchar> buf;
    cv::imencode(".jpg", img, buf);
    std::string encoded = base64_encode(buf.data(), buf.size());
    return encoded;
}

// Helper function to get current timestamp
std::string get_current_timestamp() {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return std::to_string(now);
}

// Helper function to create fake JSON message for testing
std::string create_fake_json_message(int frame_index) {
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    std::string json = R"({
  "frame_index": )" + std::to_string(frame_index) + R"(,
  "width": 1280,
  "height": 720,
  "timestamp": )" + std::to_string(timestamp) + R"(,
  "targets": [
    {
      "track_id": 1,
      "class_id": 0,
      "class_name": "face",
      "confidence": 0.95,
      "bbox": {
        "x": 100,
        "y": 150,
        "width": 200,
        "height": 250
      }
    },
    {
      "track_id": 2,
      "class_id": 0,
      "class_name": "face",
      "confidence": 0.88,
      "bbox": {
        "x": 500,
        "y": 200,
        "width": 180,
        "height": 220
      }
    }
  ],
  "events": [
    {
      "event_type": "face_detected",
      "track_id": 1,
      "timestamp": )" + std::to_string(timestamp) + R"(
    }
  ]
})";
    return json;
}

// Helper function to resolve path relative to project root
std::string resolve_path(const std::string& relative_path) {
    // Try absolute path first
    if (relative_path[0] == '/') {
        return relative_path;
    }
    
    // Try from current directory
    if (std::ifstream(relative_path).good()) {
        return relative_path;
    }
    
    // Try from project root (assuming we're in samples/build)
    std::string project_root = "/home/cvedix/project/edge_ai_api";
    std::string full_path = project_root + "/" + relative_path;
    if (std::ifstream(full_path).good()) {
        return full_path;
    }
    
    // Try removing leading ./
    if (relative_path.find("./") == 0) {
        std::string without_dot = relative_path.substr(2);
        std::string full_path2 = project_root + "/" + without_dot;
        if (std::ifstream(full_path2).good()) {
            return full_path2;
        }
    }
    
    // Return original if not found
    return relative_path;
}

int main(int argc, char* argv[]) {
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();
    // Signal handling
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Configuration với giá trị mặc định
    std::string mqtt_broker = "mqtt.goads.com.vn";
    int mqtt_port = 1883;
    std::string mqtt_topic = "ba_crossline/events";
    std::string mqtt_username = "";
    std::string mqtt_password = "";

    // Parse command line arguments (không cần video_file nữa)
    // Format: ./simple_rtmp_mqtt_sample [mqtt_broker] [mqtt_port] [mqtt_topic] [username] [password]
    if (argc > 1) mqtt_broker = argv[1];
    if (argc > 2) {
        try {
            mqtt_port = std::stoi(argv[2]);
        } catch (const std::exception& e) {
            std::cerr << "[Error] Invalid port number: " << argv[2] << std::endl;
            return 1;
        }
    }
    if (argc > 3) mqtt_topic = argv[3];
    if (argc > 4) mqtt_username = argv[4];
    if (argc > 5) mqtt_password = argv[5];

    std::cout << "=== Simple MQTT Sample (FAKE MESSAGES - NO IMAGE PROCESSING) ===" << std::endl;
    std::cout << "MQTT Broker: " << mqtt_broker << ":" << mqtt_port << std::endl;
    std::cout << "MQTT Topic: " << mqtt_topic << std::endl;
    std::cout << "NOTE: Using ONLY mqtt_broker_node to send FAKE messages" << std::endl;
    std::cout << "NOTE: NO other nodes (file_src, face_detector, tracker, osd, etc.)" << std::endl;
    std::cout << "===============================================================" << std::endl;

    try {
        // ============================================
        // CHỈ TẠO 1 NODE: mqtt_broker_node
        // KHÔNG có node nào khác (file_src, face_detector, tracker, osd, screen_des, split, etc.)
        // ============================================
        // MQTT Client - Sử dụng mosquitto trực tiếp
        // Initialize mosquitto library
        mosquitto_lib_init();
        
        // Create mosquitto client
        struct mosquitto* mosq = mosquitto_new("simple_rtmp_mqtt_sample", true, nullptr);
        bool mqtt_connected = false;
        
        if (mosq) {
            // Set username/password if provided
            if (!mqtt_username.empty() && !mqtt_password.empty()) {
                mosquitto_username_pw_set(mosq, mqtt_username.c_str(), mqtt_password.c_str());
            }
            
            // Connect to broker
            std::cout << "[MQTT] Connecting to broker " << mqtt_broker << ":" << mqtt_port << "..." << std::endl;
            int rc = mosquitto_connect(mosq, mqtt_broker.c_str(), mqtt_port, 60);
            if (rc == MOSQ_ERR_SUCCESS) {
                mqtt_connected = true;
                std::cout << "[MQTT] Connected successfully!" << std::endl;
            } else {
                std::cerr << "[MQTT] Failed to connect: " << mosquitto_strerror(rc) << std::endl;
                std::cerr << "[MQTT] Exiting..." << std::endl;
                mosquitto_destroy(mosq);
                mosquitto_lib_cleanup();
                return 1;
            }
        } else {
            std::cerr << "[MQTT] Failed to create mosquitto client" << std::endl;
            return 1;
        }

        // Counter for fake messages
        std::atomic<int> fake_frame_counter(0);
        std::atomic<int> sent_count(0);
        
        // MQTT publish function - Gửi message GIẢ qua mqtt_broker_node
        // Function này được gọi bởi mqtt_broker_node khi có data (hoặc được gọi trực tiếp để test)
        auto mqtt_publish_func = [mosq, mqtt_topic, mqtt_connected, &fake_frame_counter, &sent_count](const std::string& json_data) {
            // Ignore json_data thật, tạo message giả để test
            if (mosq && mqtt_connected) {
                int frame_index = fake_frame_counter.fetch_add(1);
                std::string fake_json = create_fake_json_message(frame_index);
                
                int rc = mosquitto_publish(mosq, nullptr, mqtt_topic.c_str(), 
                                          fake_json.length(), fake_json.c_str(), 1, false);
                if (rc == MOSQ_ERR_SUCCESS) {
                    int count = sent_count.fetch_add(1) + 1;
                    if (count % 10 == 0) {
                        std::cout << "[mqtt_broker_node] Sent " << count << " fake messages (via mqtt_broker_node callback)" << std::endl;
                    }
                }
            }
        };

        // ============================================
        // CHỈ TẠO 1 NODE DUY NHẤT: mqtt_broker_node
        // ============================================
        std::cout << "[Main] Creating ONLY ONE node: mqtt_broker_node" << std::endl;
        std::cout << "[Main] NO file_src, NO face_detector, NO tracker, NO osd, NO screen_des, NO split" << std::endl;
        
        auto mqtt_broker_node = std::make_shared<cvedix_nodes::cvedix_json_mqtt_broker_node>(
            "mqtt_broker_0",
            cvedix_nodes::cvedix_broke_for::FACE,
            1000,   // broking_cache_warn_threshold
            5000,   // broking_cache_ignore_threshold
            nullptr, // json_transformer
            mqtt_publish_func); // mqtt_publisher

        std::cout << "[Main] ✓ mqtt_broker_node created successfully" << std::endl;
        std::cout << "[Main] ✓ Total nodes created: 1 (ONLY mqtt_broker_node)" << std::endl;
        std::cout << "[Main] ✓ mqtt_broker_node is NOT attached to any pipeline" << std::endl;
        std::cout << "[Main] Starting thread to send fake messages via mqtt_broker_node..." << std::endl;

        // Thread riêng để gửi message giả định kỳ qua mqtt_broker_node
        // Gọi trực tiếp publish function của mqtt_broker_node (simulate callback từ pipeline)
        // NOTE: Đây là cách test mqtt_broker_node mà không cần pipeline
        std::thread mqtt_thread([mqtt_publish_func, &stop_flag]() {
            std::cout << "[MQTT Thread] Started, calling mqtt_broker_node publish function every 1 second..." << std::endl;
            while (!stop_flag) {
                // Tạo fake JSON message và gọi publish function của mqtt_broker_node
                // (Trong thực tế, mqtt_broker_node sẽ tự gọi callback này khi nhận data từ pipeline)
                std::string fake_json = "{}"; // Dummy JSON, sẽ bị ignore trong mqtt_publish_func
                mqtt_publish_func(fake_json); // Gọi publish function của mqtt_broker_node
                
                // Gửi message mỗi 1 giây
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            std::cout << "[MQTT Thread] Stopped" << std::endl;
        });

        std::cout << "[Main] Running. Press Ctrl+C to stop..." << std::endl;

        // Main loop với signal handling
        while (!stop_flag) {
            // Process MQTT network events
            if (mosq && mqtt_connected) {
                mosquitto_loop(mosq, 0, 1);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Dừng
        std::cout << "[Main] Stopping..." << std::endl;
        
        // Đợi MQTT thread kết thúc
        if (mqtt_thread.joinable()) {
            std::cout << "[Main] Waiting for MQTT thread to finish..." << std::endl;
            mqtt_thread.join();
        }
        
        // Disconnect MQTT
        if (mosq) {
            mosquitto_disconnect(mosq);
            mosquitto_destroy(mosq);
            mosquitto_lib_cleanup();
        }
        
        if (sent_count.load() > 0) {
            std::cout << "[Main] Total fake messages sent via mqtt_broker_node: " << sent_count.load() << std::endl;
        }

        std::cout << "[Main] Program stopped." << std::endl;
        std::cout << "[Main] Summary: Only 1 node was used: mqtt_broker_node" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[Error] " << e.what() << std::endl;
        return 1;
    }

    return 0;
}




