#include <mosquitto.h>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <csignal>
#include <cstring>

/*
 * ## Simple MQTT Test - Gửi message giả để test ##
 * 
 * Chương trình đơn giản chỉ gửi MQTT message giả để test kết nối
 * Không cần xử lý video, chỉ test MQTT publishing
 * 
 * Sử dụng:
 *   ./test_mqtt_simple [mqtt_broker] [mqtt_port] [mqtt_topic] [username] [password]
 * 
 * Ví dụ:
 *   ./test_mqtt_simple mqtt.goads.com.vn 1883 ba_crossline/events
 *   ./test_mqtt_simple localhost 1883 ba_crossline/events user pass
 */

volatile sig_atomic_t stop_flag = 0;

void signal_handler(int signal) {
    stop_flag = 1;
}

// Tạo JSON message giả
std::string create_fake_json_message(int frame_index) {
    // Format JSON giống như từ face detection
    std::string json = R"({
  "frame_index": )" + std::to_string(frame_index) + R"(,
  "width": 1280,
  "height": 720,
  "timestamp": )" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count()) + R"(,
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
      "timestamp": )" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count()) + R"(
    }
  ]
})";
    return json;
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Configuration
    std::string mqtt_broker = "mqtt.goads.com.vn";
    int mqtt_port = 1883;
    std::string mqtt_topic = "ba_crossline/events";
    std::string mqtt_username = "";
    std::string mqtt_password = "";

    // Parse arguments
    if (argc > 1) mqtt_broker = argv[1];
    if (argc > 2) mqtt_port = std::stoi(argv[2]);
    if (argc > 3) mqtt_topic = argv[3];
    if (argc > 4) mqtt_username = argv[4];
    if (argc > 5) mqtt_password = argv[5];

    std::cout << "=== Simple MQTT Test ===" << std::endl;
    std::cout << "MQTT Broker: " << mqtt_broker << ":" << mqtt_port << std::endl;
    std::cout << "MQTT Topic: " << mqtt_topic << std::endl;
    std::cout << "=========================" << std::endl;

    // Initialize mosquitto
    mosquitto_lib_init();
    
    // Create client
    struct mosquitto* mosq = mosquitto_new("test_mqtt_simple", true, nullptr);
    if (!mosq) {
        std::cerr << "[Error] Failed to create mosquitto client" << std::endl;
        return 1;
    }

    // Set username/password if provided
    if (!mqtt_username.empty() && !mqtt_password.empty()) {
        mosquitto_username_pw_set(mosq, mqtt_username.c_str(), mqtt_password.c_str());
    }

    // Connect
    std::cout << "[MQTT] Connecting to " << mqtt_broker << ":" << mqtt_port << "..." << std::endl;
    int rc = mosquitto_connect(mosq, mqtt_broker.c_str(), mqtt_port, 60);
    if (rc != MOSQ_ERR_SUCCESS) {
        std::cerr << "[Error] Failed to connect: " << mosquitto_strerror(rc) << std::endl;
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        return 1;
    }

    std::cout << "[MQTT] Connected successfully!" << std::endl;
    std::cout << "[MQTT] Sending test messages... (Press Ctrl+C to stop)" << std::endl;

    // Send messages
    int frame_index = 0;
    int message_count = 0;
    
    while (!stop_flag) {
        // Create fake message
        std::string json_message = create_fake_json_message(frame_index++);
        
        // Publish
        rc = mosquitto_publish(mosq, nullptr, mqtt_topic.c_str(), 
                              json_message.length(), json_message.c_str(), 1, false);
        
        if (rc == MOSQ_ERR_SUCCESS) {
            message_count++;
            if (message_count % 10 == 0) {
                std::cout << "[MQTT] Sent " << message_count << " messages" << std::endl;
            }
        } else {
            std::cerr << "[Error] Failed to publish: " << mosquitto_strerror(rc) << std::endl;
        }

        // Loop để xử lý network
        mosquitto_loop(mosq, 0, 1);

        // Sleep 1 giây giữa các message (để test, không spam)
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Cleanup
    std::cout << "\n[MQTT] Disconnecting..." << std::endl;
    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();

    std::cout << "[MQTT] Total messages sent: " << message_count << std::endl;
    std::cout << "[MQTT] Done!" << std::endl;

    return 0;
}

