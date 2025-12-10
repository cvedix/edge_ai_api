#include "cvedix/nodes/src/cvedix_file_src_node.h"
#include "cvedix/nodes/infers/cvedix_yunet_face_detector_node.h"
#include "cvedix/nodes/infers/cvedix_sface_feature_encoder_node.h"
#include "cvedix/nodes/track/cvedix_sort_track_node.h"
#include "cvedix/nodes/broker/cvedix_json_console_broker_node.h"

#include "cvedix/nodes/des/cvedix_screen_des_node.h"
#include "cvedix/nodes/des/cvedix_file_des_node.h"
#include "cvedix/nodes/des/cvedix_fake_des_node.h"
#include "cvedix/nodes/mid/cvedix_split_node.h"
#include <cvedix/objects/shapes/cvedix_size.h>
#include "cvedix/nodes/osd/cvedix_face_osd_node.h"
#include <sstream>
#include <iomanip>

#include "cvedix/utils/analysis_board/cvedix_analysis_board.h"
#include <mosquitto.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <cstdlib>
#include <csignal>
#include <fstream>
#include <iostream>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <regex>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

/*
* ## face tracking sample ##
* track for multi faces using cvedix_sort_track_node.
* Tự gửi kết quả tracking lên MQTT bằng code, không sử dụng cvedix_json_mqtt_broker_node.
*/

// Global flag for signal handling
volatile sig_atomic_t stop_flag = 0;

void signal_handler(int /*signal*/) {
    stop_flag = 1;
}

int main(int argc, char* argv[]) {
    CVEDIX_SET_LOG_INCLUDE_CODE_LOCATION(false);
    CVEDIX_SET_LOG_INCLUDE_THREAD_ID(false);
    CVEDIX_SET_LOG_LEVEL(cvedix_utils::cvedix_log_level::INFO);
    CVEDIX_LOGGER_INIT();
    
    // Signal handling
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // MQTT configuration với giá trị mặc định
    std::string mqtt_broker = "mqtt.goads.com.vn";
    int mqtt_port = 1883;
    std::string mqtt_topic = "face_tracking/events";
    std::string mqtt_username = "";
    std::string mqtt_password = "";

    // Parse command line arguments
    // Format: ./face_tracking_sample [mqtt_broker] [mqtt_port] [mqtt_topic] [username] [password]
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

    std::cout << "=== Face Tracking Sample with MQTT ===" << std::endl;
    std::cout << "MQTT Broker: " << mqtt_broker << ":" << mqtt_port << std::endl;
    std::cout << "MQTT Topic: " << mqtt_topic << std::endl;

    // Initialize mosquitto library
    mosquitto_lib_init();
    
    // Create mosquitto client
    struct mosquitto* mosq = mosquitto_new("face_tracking_sample", true, nullptr);
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
            mosquitto_loop_start(mosq);
        } else {
            std::cerr << "[MQTT] Failed to connect: " << mosquitto_strerror(rc) << std::endl;
            std::cerr << "[MQTT] Continuing without MQTT publishing..." << std::endl;
            mosquitto_destroy(mosq);
            mosq = nullptr;
        }
    } else {
        std::cerr << "[MQTT] Failed to create mosquitto client" << std::endl;
    }

    // MQTT publish function - Non-blocking với mosquitto_loop để xử lý network I/O
    std::atomic<int> sent_count(0);
    std::atomic<int> failed_count(0);
    std::atomic<int> publish_count(0);
    
    auto mqtt_publish_json = [mosq, mqtt_topic, &mqtt_connected, &sent_count, &failed_count, &publish_count](const std::string& json_data) {
        int pub_count = publish_count.fetch_add(1);
        if (pub_count < 10) {
            std::cerr << "[MQTT] Publishing #" << (pub_count + 1) << " - data: " 
                     << json_data.length() << " bytes" << std::endl;
        } else if (pub_count == 10) {
            std::cerr << "[MQTT] Published 10+ messages. Reducing log verbosity..." << std::endl;
        }
        
        if (!mosq) {
            if (pub_count < 5) {
                std::cerr << "[MQTT] MQTT client not initialized, skipping publish" << std::endl;
            }
            return;
        }
        
        // Kiểm tra connection trước khi publish
        if (!mqtt_connected) {
            if (pub_count < 5) {
                std::cerr << "[MQTT] MQTT not connected, skipping publish" << std::endl;
            }
            return;
        }
        
        // Gọi mosquitto_loop() để xử lý network I/O (non-blocking)
        // Điều này đảm bảo messages được gửi đi và không block
        mosquitto_loop(mosq, 0, 1);  // Non-blocking, max 1 packet
        
        // Publish message
        int rc = mosquitto_publish(mosq, nullptr, mqtt_topic.c_str(), 
                                  json_data.length(), json_data.c_str(), 1, false);
        
        if (rc == MOSQ_ERR_SUCCESS) {
            int count = sent_count.fetch_add(1) + 1;
            if (count % 10 == 0) {
                std::cerr << "[MQTT] Sent " << count << " messages" << std::endl;
            }
        } else {
            int fail_count = failed_count.fetch_add(1) + 1;
            if (fail_count <= 5 || fail_count % 100 == 0) {
                std::cerr << "[MQTT] Publish failed (code " << rc << "): " << mosquitto_strerror(rc) << std::endl;
            }
            // Nếu lỗi là "not connected", cập nhật flag
            if (rc == MOSQ_ERR_NO_CONN) {
                mqtt_connected = false;
            }
        }
    };
    
    // Shared buffer để lưu JSON từ console broker
    // Console broker sẽ in JSON ra stdout, ta sẽ sử dụng một thread để đọc và gửi MQTT
    // Giải pháp: Sử dụng một thread để đọc JSON từ stdout và parse, sau đó gửi MQTT
    // Tuy nhiên, vì không thể dễ dàng redirect stdout, ta sẽ sử dụng một cách khác:
    // Sử dụng một thread để đọc JSON từ một buffer shared hoặc sử dụng một cách khác
    
    // Giải pháp đơn giản: Sử dụng một thread để đọc JSON từ stdout
    // Ta sẽ sử dụng một cách đơn giản: Đọc từ một buffer hoặc sử dụng một cách khác
    
    // Thread để đọc JSON từ stdout và gửi MQTT
    // Vì console_broker_node in JSON ra stdout, ta sẽ cần redirect stdout để đọc
    // Tuy nhiên, cách này phức tạp vì cần redirect stdout của toàn bộ process
    
    // Giải pháp tạm thời: Sử dụng một thread để parse JSON từ stdout và gửi MQTT
    // Ta sẽ sử dụng một cách đơn giản: Đọc từ một buffer hoặc sử dụng một cách khác
    
    // Tạm thời, ta sẽ không implement thread để đọc từ stdout
    // Thay vào đó, ta sẽ sử dụng console broker và sẽ tìm cách khác để lấy JSON

    // Helper function to resolve path relative to project root
    auto resolve_path = [](const std::string& relative_path) -> std::string {
        // Try absolute path first
        if (relative_path[0] == '/') {
            return relative_path;
        }
        
        // Try from current directory
        if (std::filesystem::exists(relative_path)) {
            return relative_path;
        }
        
        // Try from project root (assuming we're in samples/build)
        std::string project_root = "/home/cvedix/project/edge_ai_api";
        std::string full_path = project_root + "/" + relative_path;
        if (std::filesystem::exists(full_path)) {
            return full_path;
        }
        
        // Try removing leading ./
        if (relative_path.find("./") == 0) {
            std::string without_dot = relative_path.substr(2);
            std::string full_path2 = project_root + "/" + without_dot;
            if (std::filesystem::exists(full_path2)) {
                return full_path2;
            }
        }
        
        // Return original if not found
        return relative_path;
    };

    // Resolve paths
    std::string video_path = resolve_path("/home/cvedix/project/edge_ai_api/cvedix_data/test_video/face.mp4");
    std::string yunet_model_path = resolve_path("/home/cvedix/project/edge_ai_api/cvedix_data/models/face/face_detection_yunet_2022mar.onnx");
    std::string sface_model_path = resolve_path("/home/cvedix/project/edge_ai_api/cvedix_data/models/face/face_recognition_sface_2021dec.onnx");

    std::cout << "Video path: " << video_path << std::endl;
    std::cout << "YuNet model path: " << yunet_model_path << std::endl;
    std::cout << "SFace model path: " << sface_model_path << std::endl;

    // create nodes
    auto file_src_0 = std::make_shared<cvedix_nodes::cvedix_file_src_node>("file_src_0", 0, video_path);
    auto yunet_face_detector_0 = std::make_shared<cvedix_nodes::cvedix_yunet_face_detector_node>("yunet_face_detector_0", yunet_model_path);
    auto sface_face_encoder_0 = std::make_shared<cvedix_nodes::cvedix_sface_feature_encoder_node>("sface_face_encoder_0", sface_model_path);
    auto track_0 = std::make_shared<cvedix_nodes::cvedix_sort_track_node>("track_0", cvedix_nodes::cvedix_track_for::FACE);   // track for face
    
    auto osd_0 = std::make_shared<cvedix_nodes::cvedix_face_osd_node>("osd_0");
    auto screen_des_0 = std::make_shared<cvedix_nodes::cvedix_screen_des_node>("screen_des_0", 0);

    // Sử dụng console broker để lấy JSON từ track node
    // JSON sẽ được in ra stdout, ta sẽ sử dụng một thread để đọc và gửi MQTT
    auto json_broker_0 = std::make_shared<cvedix_nodes::cvedix_json_console_broker_node>(
        "json_broker_0",
        cvedix_nodes::cvedix_broke_for::FACE,
        1000,   // broking_cache_warn_threshold
        5000    // broking_cache_ignore_threshold
    );
    
    // Split node: Chia luồng thành 2 nhánh (MQTT và OSD)
    auto split_node_0 = std::make_shared<cvedix_nodes::cvedix_split_node>("split_node_0");
    
    // Fake DES node cho nhánh MQTT (broker node không cần DES node thật, nhưng pipeline yêu cầu DES node ở cuối)
    auto fake_des_0 = std::make_shared<cvedix_nodes::cvedix_fake_des_node>("fake_des_0", 0);
    
    yunet_face_detector_0->attach_to({file_src_0});
    track_0->attach_to({yunet_face_detector_0});
    
    // Split node: Chia luồng từ track_0 thành 2 nhánh (MQTT và OSD)
    split_node_0->attach_to({track_0});
    
    // Nhánh 1: JSON broker -> fake_des (lấy JSON từ track node)
    // JSON sẽ được in ra stdout bởi console broker
    json_broker_0->attach_to({split_node_0});
    fake_des_0->attach_to({json_broker_0});
    
    // Nhánh 2: OSD -> screen_des (hiển thị kết quả trên màn hình)
    osd_0->attach_to({split_node_0});
    screen_des_0->attach_to({osd_0});
    
    // Thread để đọc JSON từ stdout và gửi MQTT
    // Vì console_broker_node in JSON ra stdout, ta sẽ sử dụng một thread để đọc và parse JSON
    // Giải pháp: Sử dụng một thread để đọc JSON từ stdout và parse, sau đó gửi MQTT
    
    // Tạo pipe để đọc từ stdout
    // Tuy nhiên, redirect stdout của toàn bộ process là phức tạp
    // Giải pháp đơn giản: Sử dụng một thread để đọc từ stdout bằng cách sử dụng một buffer shared
    // Hoặc sử dụng một cách khác để intercept stdout
    
    // Redirect stdout để đọc JSON
    // Tạo pipe để đọc từ stdout
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        std::cerr << "[Error] Failed to create pipe" << std::endl;
        return 1;
    }
    
    // Lưu stdout gốc
    int stdout_backup = dup(STDOUT_FILENO);
    if (stdout_backup == -1) {
        std::cerr << "[Error] Failed to backup stdout" << std::endl;
        return 1;
    }
    
    // Redirect stdout đến pipe
    if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
        std::cerr << "[Error] Failed to redirect stdout" << std::endl;
        return 1;
    }
    close(pipefd[1]);
    
    // Thread để đọc JSON từ pipe và gửi MQTT
    std::atomic<bool> stop_json_reader(false);
    std::thread json_reader_thread([&stop_json_reader, &mqtt_publish_json, pipefd]() {
        char buffer[4096];
        std::string line_buffer;
        std::string json_candidate;
        bool in_json = false;
        int brace_count = 0;
        
        while (!stop_json_reader) {
            ssize_t n = read(pipefd[0], buffer, sizeof(buffer) - 1);
            if (n > 0) {
                buffer[n] = '\0';
                line_buffer += buffer;
                
                // Parse JSON từ line_buffer
                for (size_t i = 0; i < line_buffer.length(); i++) {
                    char c = line_buffer[i];
                    
                    if (c == '{') {
                        if (!in_json) {
                            in_json = true;
                            json_candidate = "{";
                            brace_count = 1;
                        } else {
                            json_candidate += c;
                            brace_count++;
                        }
                    } else if (c == '}') {
                        if (in_json) {
                            json_candidate += c;
                            brace_count--;
                            if (brace_count == 0) {
                                // Tìm thấy JSON hoàn chỉnh
                                // Kiểm tra xem có phải JSON hợp lệ không (bắt đầu bằng { và kết thúc bằng })
                                if (json_candidate.length() > 10 && 
                                    json_candidate[0] == '{' && 
                                    json_candidate.back() == '}') {
                                    // Gửi MQTT
                                    mqtt_publish_json(json_candidate);
                                }
                                json_candidate.clear();
                                in_json = false;
                            }
                        }
                    } else if (in_json) {
                        json_candidate += c;
                    }
                    
                    // Nếu gặp newline và không trong JSON, xóa buffer
                    if (c == '\n' && !in_json) {
                        // Giữ lại phần cuối của buffer (có thể là phần đầu của JSON)
                        if (line_buffer.length() > i + 1) {
                            line_buffer = line_buffer.substr(i + 1);
                            i = -1; // Reset để xử lý từ đầu
                        } else {
                            line_buffer.clear();
                        }
                    }
                }
                
                // Nếu buffer quá lớn, xóa phần đầu
                if (line_buffer.length() > 8192) {
                    line_buffer = line_buffer.substr(line_buffer.length() - 4096);
                }
            } else if (n == 0) {
                // EOF
                break;
            } else {
                // Error
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    break;
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        close(pipefd[0]);
    });
    
    // Đợi một chút để đảm bảo MQTT connection ổn định trước khi redirect stdout
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Kiểm tra lại MQTT connection trước khi redirect stdout
    if (mosq) {
        int rc = mosquitto_loop(mosq, 0, 1);
        if (rc != MOSQ_ERR_SUCCESS && rc != MOSQ_ERR_INVAL) {
            std::cerr << "[MQTT] Warning: Connection check before stdout redirect: " 
                     << mosquitto_strerror(rc) << std::endl;
        }
    }
    
    file_src_0->start();
    
    // Đợi một chút để pipeline khởi động
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Kiểm tra lại MQTT connection sau khi start pipeline
    if (mosq) {
        int rc = mosquitto_loop(mosq, 0, 1);
        if (rc != MOSQ_ERR_SUCCESS && rc != MOSQ_ERR_INVAL) {
            std::cerr << "[MQTT] Warning: Connection check after pipeline start: " 
                     << mosquitto_strerror(rc) << std::endl;
            // Thử reconnect nếu cần
            if (rc == MOSQ_ERR_NO_CONN) {
                std::cerr << "[MQTT] Attempting to reconnect..." << std::endl;
                mqtt_connected = false;
                int reconnect_rc = mosquitto_reconnect(mosq);
                if (reconnect_rc == MOSQ_ERR_SUCCESS) {
                    mqtt_connected = true;
                    std::cerr << "[MQTT] Reconnected successfully!" << std::endl;
                } else {
                    std::cerr << "[MQTT] Reconnect failed: " << mosquitto_strerror(reconnect_rc) << std::endl;
                }
            }
        }
    }

    std::cerr << "[Main] Pipeline started. Press Ctrl+C to stop..." << std::endl;

    // Chờ pipeline chạy cho đến khi nhận signal dừng (Ctrl+C)
    while (!stop_flag) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cerr << "\n[Main] Received Ctrl+C signal, shutting down..." << std::endl;

    // Dừng JSON reader thread
    stop_json_reader = true;
    if (json_reader_thread.joinable()) {
        json_reader_thread.join();
    }
    
    // Restore stdout
    dup2(stdout_backup, STDOUT_FILENO);
    close(stdout_backup);
    close(pipefd[0]);

    // Dừng pipeline
    file_src_0->detach_recursively();

    // Cleanup MQTT
    if (mosq) {
        mosquitto_disconnect(mosq);
        mosquitto_loop_stop(mosq, false);
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
    }

    std::cout << "[Main] Total messages published via MQTT: " << publish_count.load() << std::endl;
    if (sent_count.load() > 0) {
        std::cout << "[Main] Total messages sent via MQTT: " << sent_count.load() << std::endl;
    }
    if (failed_count.load() > 0) {
        std::cout << "[Main] Total failed MQTT publishes: " << failed_count.load() << std::endl;
    }

    
}