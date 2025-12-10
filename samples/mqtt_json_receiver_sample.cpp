#ifdef CVEDIX_WITH_MQTT

#include "cvedix/utils/mqtt_json_receiver/cvedix_mqtt_json_receiver.h"
#include "third_party/nlohmann/json.hpp"
#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>

using json = nlohmann::json;

/*
* ## Ví dụ nhận và xử lý JSON từ MQTT ##
* 
* Tính năng:
* - Subscribe đến MQTT topics
* - Nhận và parse JSON messages
* - Xử lý JSON với custom callback
* - Auto-reconnect khi mất kết nối
*
* Yêu cầu:
* - libmosquitto-dev (sudo apt-get install libmosquitto-dev)
* - Build với flag: -DCVEDIX_WITH_MQTT=ON
*
* Biên dịch:
*   cmake -DCVEDIX_WITH_MQTT=ON ..
*
* Sử dụng:
*   ./mqtt_json_receiver_sample [broker_url] [port] [topic] [username] [password]
*
* Ví dụ:
*   ./mqtt_json_receiver_sample anhoidong.datacenter.cvedix.com 1883 events
*   ./mqtt_json_receiver_sample localhost 1883 events user pass
*/

// Cờ toàn cục để xử lý tín hiệu
volatile sig_atomic_t stop_flag = 0;

void signal_handler(int signal) {
    stop_flag = 1;
}

int main(int argc, char** argv) {
    // Xử lý Ctrl+C
    std::signal(SIGINT, signal_handler);
    
    // MQTT configuration
    std::string broker_url = "mqtt.goads.com.vn";
    int port = 1883;
    std::string topic = "ba_crossline/events";
    std::string username = "";
    std::string password = "";
    
    // Parse arguments
    if (argc > 1) {
        broker_url = argv[1];
    }
    if (argc > 2) {
        port = std::stoi(argv[2]);
    }
    if (argc > 3) {
        topic = argv[3];
    }
    if (argc > 4) {
        username = argv[4];
    }
    if (argc > 5) {
        password = argv[5];
    }
    
    std::cout << "[Main] MQTT JSON Receiver Sample" << std::endl;
    std::cout << "[Main] Broker: " << broker_url << ":" << port << std::endl;
    std::cout << "[Main] Topic: " << topic << std::endl;
    
    // Tạo MQTT JSON receiver
    auto receiver = std::make_unique<cvedix_utils::cvedix_mqtt_json_receiver>(
        broker_url,
        port,
        "cvedix_json_receiver_sample"
    );
    
    // Set callbacks
    int json_message_count = 0;
    int raw_message_count = 0;
    
    // Raw callback: Nhận mọi message (kể cả không phải JSON)
    receiver->set_raw_callback([&raw_message_count](const std::string& topic, const std::string& payload) {
        raw_message_count++;
        if (raw_message_count % 10 == 0) {
            std::cout << "[Raw Callback] Received " << raw_message_count 
                      << " raw messages on topic: " << topic << std::endl;
        }
    });
    
    // JSON callback: Chỉ nhận JSON hợp lệ
    receiver->set_json_callback([&json_message_count](const std::string& topic, const std::string& json_data) {
        json_message_count++;
        
        try {
            // Parse JSON
            json j = json::parse(json_data);
            
            // Xử lý JSON theo cấu trúc
            if (j.is_array()) {
                // Nếu là array
                std::cout << "\n[JSON Callback] Received JSON array with " << j.size() << " elements" << std::endl;
                
                for (size_t i = 0; i < j.size() && i < 3; i++) { // Chỉ hiển thị 3 phần tử đầu
                    if (j[i].contains("events")) {
                        std::cout << "  Element " << i << ": Contains events array" << std::endl;
                        if (j[i]["events"].is_array()) {
                            std::cout << "    Events count: " << j[i]["events"].size() << std::endl;
                        }
                    }
                    if (j[i].contains("frame_id")) {
                        std::cout << "    Frame ID: " << j[i]["frame_id"] << std::endl;
                    }
                }
            } else if (j.is_object()) {
                // Nếu là object
                std::cout << "\n[JSON Callback] Received JSON object" << std::endl;
                
                if (j.contains("frame_index")) {
                    std::cout << "  Frame Index: " << j["frame_index"] << std::endl;
                }
                if (j.contains("targets") && j["targets"].is_array()) {
                    std::cout << "  Targets count: " << j["targets"].size() << std::endl;
                }
                if (j.contains("width") && j.contains("height")) {
                    std::cout << "  Resolution: " << j["width"] << "x" << j["height"] << std::endl;
                }
            }
            
            // Log mỗi 10 messages để tránh spam
            if (json_message_count % 10 == 0) {
                std::cout << "[JSON Callback] Processed " << json_message_count 
                          << " JSON messages from topic: " << topic << std::endl;
            }
            
        } catch (const json::parse_error& e) {
            std::cerr << "[JSON Callback] Parse error: " << e.what() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[JSON Callback] Error: " << e.what() << std::endl;
        }
    });
    
    // Bật auto-reconnect
    receiver->set_auto_reconnect(true, 5000);
    
    // Kết nối đến broker
    std::cout << "[Main] Connecting to MQTT broker..." << std::endl;
    if (!receiver->connect(username, password)) {
        std::cerr << "[Main] Failed to connect: " << receiver->get_last_error() << std::endl;
        return 1;
    }
    
    // Đợi kết nối
    int retry_count = 0;
    while (!receiver->is_connected() && retry_count < 10) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        retry_count++;
    }
    
    if (!receiver->is_connected()) {
        std::cerr << "[Main] Connection timeout" << std::endl;
        return 1;
    }
    
    std::cout << "[Main] Connected successfully!" << std::endl;
    
    // Subscribe đến topic
    std::cout << "[Main] Subscribing to topic: " << topic << std::endl;
    if (!receiver->subscribe(topic, 1)) {
        std::cerr << "[Main] Failed to subscribe: " << receiver->get_last_error() << std::endl;
        return 1;
    }
    
    std::cout << "[Main] Subscribed successfully!" << std::endl;
    std::cout << "[Main] Waiting for messages... (Press Ctrl+C to stop)" << std::endl;
    
    // Main loop
    while (!stop_flag) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Hiển thị thống kê mỗi 5 giây
        static auto last_stats_time = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_stats_time).count() >= 5) {
            std::cout << "\n[Stats] Raw messages: " << raw_message_count 
                      << ", JSON messages: " << json_message_count << std::endl;
            last_stats_time = now;
        }
    }
    
    // Cleanup
    std::cout << "\n[Main] Disconnecting..." << std::endl;
    receiver->disconnect();
    
    std::cout << "[Main] Final stats - Raw: " << raw_message_count 
              << ", JSON: " << json_message_count << std::endl;
    
    return 0;
}

#else
#include <iostream>
int main() {
    std::cerr << "MQTT support not enabled. Build with -DCVEDIX_WITH_MQTT=ON" << std::endl;
    return 1;
}
#endif // CVEDIX_WITH_MQTT
