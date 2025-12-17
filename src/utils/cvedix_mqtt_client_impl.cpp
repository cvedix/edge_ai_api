#include "cvedix/utils/mqtt_client/cvedix_mqtt_client.h"
#include <chrono>
#include <cstring>
#include <ctime>
#include <mosquitto.h>
#include <thread>

namespace cvedix_utils {

cvedix_mqtt_client::cvedix_mqtt_client(const std::string &broker_url, int port,
                                       const std::string &client_id,
                                       int keepalive)
    : broker_url_(broker_url), port_(port),
      client_id_(client_id.empty()
                     ? "cvedix_mqtt_client_" + std::to_string(time(nullptr))
                     : client_id),
      keepalive_(keepalive), connected_(false), connecting_(false),
      auto_reconnect_enabled_(false), reconnect_interval_ms_(5000),
      should_stop_reconnect_(false), mosq_(nullptr) {
  mosquitto_lib_init();
  mosq_ = mosquitto_new(client_id_.c_str(), true, this);
  if (mosq_) {
    mosquitto_connect_callback_set(mosq_, on_connect_wrapper);
    mosquitto_disconnect_callback_set(mosq_, on_disconnect_wrapper);
    mosquitto_publish_callback_set(mosq_, on_publish_wrapper);
    mosquitto_message_callback_set(mosq_, on_message_wrapper);
  }
}

cvedix_mqtt_client::~cvedix_mqtt_client() {
  should_stop_reconnect_ = true;
  if (reconnect_thread_.joinable()) {
    reconnect_thread_.join();
  }
  disconnect();
  if (mosq_) {
    mosquitto_destroy(mosq_);
  }
  mosquitto_lib_cleanup();
}

bool cvedix_mqtt_client::connect(const std::string &username,
                                 const std::string &password) {
  if (!mosq_) {
    last_error_ = "Mosquitto client not initialized";
    return false;
  }

  username_ = username;
  password_ = password;

  if (!username.empty() && !password.empty()) {
    mosquitto_username_pw_set(mosq_, username.c_str(), password.c_str());
  }

  connecting_ = true;
  int rc =
      mosquitto_connect_async(mosq_, broker_url_.c_str(), port_, keepalive_);
  if (rc != MOSQ_ERR_SUCCESS) {
    connecting_ = false;
    last_error_ =
        "Failed to initiate connection: " + std::string(mosquitto_strerror(rc));
    return false;
  }

  // Start loop
  mosquitto_loop_start(mosq_);

  // Wait for connection (with timeout)
  int timeout = 5000; // 5 seconds
  int elapsed = 0;
  while (connecting_ && elapsed < timeout) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    elapsed += 100;
  }

  if (auto_reconnect_enabled_ && !reconnect_thread_.joinable()) {
    should_stop_reconnect_ = false;
    reconnect_thread_ = std::thread(&cvedix_mqtt_client::reconnect_loop, this);
  }

  return connected_;
}

void cvedix_mqtt_client::disconnect() {
  if (mosq_ && connected_) {
    mosquitto_disconnect(mosq_);
    mosquitto_loop_stop(mosq_, false);
    connected_ = false;
  }
}

int cvedix_mqtt_client::publish(const std::string &topic,
                                const std::string &payload, int qos,
                                bool retain) {
  if (!mosq_ || !connected_) {
    last_error_ = "Not connected";
    return -1;
  }

  std::lock_guard<std::mutex> lock(publish_mutex_);
  int mid = 0;
  int rc = mosquitto_publish(mosq_, &mid, topic.c_str(), payload.size(),
                             payload.c_str(), qos, retain);
  if (rc != MOSQ_ERR_SUCCESS) {
    last_error_ = "Publish failed: " + std::string(mosquitto_strerror(rc));
    return -1;
  }
  return mid;
}

bool cvedix_mqtt_client::subscribe(const std::string &topic, int qos) {
  if (!mosq_ || !connected_) {
    last_error_ = "Not connected";
    return false;
  }

  int rc = mosquitto_subscribe(mosq_, nullptr, topic.c_str(), qos);
  if (rc != MOSQ_ERR_SUCCESS) {
    last_error_ = "Subscribe failed: " + std::string(mosquitto_strerror(rc));
    return false;
  }
  return true;
}

bool cvedix_mqtt_client::unsubscribe(const std::string &topic) {
  if (!mosq_ || !connected_) {
    last_error_ = "Not connected";
    return false;
  }

  int rc = mosquitto_unsubscribe(mosq_, nullptr, topic.c_str());
  if (rc != MOSQ_ERR_SUCCESS) {
    last_error_ = "Unsubscribe failed: " + std::string(mosquitto_strerror(rc));
    return false;
  }
  return true;
}

bool cvedix_mqtt_client::is_connected() const { return connected_; }

bool cvedix_mqtt_client::is_ready() const { return connected_ && mosq_; }

void cvedix_mqtt_client::set_on_connect_callback(on_connect_callback callback) {
  on_connect_cb_ = std::move(callback);
}

void cvedix_mqtt_client::set_on_disconnect_callback(
    on_disconnect_callback callback) {
  on_disconnect_cb_ = std::move(callback);
}

void cvedix_mqtt_client::set_on_publish_callback(on_publish_callback callback) {
  on_publish_cb_ = std::move(callback);
}

void cvedix_mqtt_client::set_on_message_callback(on_message_callback callback) {
  on_message_cb_ = std::move(callback);
}

void cvedix_mqtt_client::set_auto_reconnect(bool enable,
                                            int reconnect_interval_ms) {
  auto_reconnect_enabled_ = enable;
  reconnect_interval_ms_ = reconnect_interval_ms;

  if (enable && !reconnect_thread_.joinable() && mosq_) {
    should_stop_reconnect_ = false;
    reconnect_thread_ = std::thread(&cvedix_mqtt_client::reconnect_loop, this);
  } else if (!enable) {
    should_stop_reconnect_ = true;
    if (reconnect_thread_.joinable()) {
      reconnect_thread_.join();
    }
  }
}

std::string cvedix_mqtt_client::get_last_error() const { return last_error_; }

void cvedix_mqtt_client::reconnect_loop() {
  while (!should_stop_reconnect_) {
    std::this_thread::sleep_for(
        std::chrono::milliseconds(reconnect_interval_ms_));

    if (should_stop_reconnect_) {
      break;
    }

    if (!connected_ && !connecting_ && mosq_) {
      connecting_ = true;
      int rc = mosquitto_reconnect_async(mosq_);
      if (rc == MOSQ_ERR_SUCCESS) {
        // Wait a bit for connection
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
      } else {
        connecting_ = false;
        last_error_ =
            "Reconnect failed: " + std::string(mosquitto_strerror(rc));
      }
    }
  }
}

void cvedix_mqtt_client::on_connect_wrapper(struct mosquitto *mosq, void *obj,
                                            int rc) {
  auto *client = static_cast<cvedix_mqtt_client *>(obj);
  if (rc == 0) {
    client->connected_ = true;
    client->connecting_ = false;
    if (client->on_connect_cb_) {
      client->on_connect_cb_(true);
    }
  } else {
    client->connected_ = false;
    client->connecting_ = false;
    client->last_error_ =
        "Connection failed: " + std::string(mosquitto_strerror(rc));
    if (client->on_connect_cb_) {
      client->on_connect_cb_(false);
    }
  }
}

void cvedix_mqtt_client::on_disconnect_wrapper(struct mosquitto *mosq,
                                               void *obj, int rc) {
  auto *client = static_cast<cvedix_mqtt_client *>(obj);
  client->connected_ = false;
  if (client->on_disconnect_cb_) {
    client->on_disconnect_cb_();
  }
}

void cvedix_mqtt_client::on_publish_wrapper(struct mosquitto *mosq, void *obj,
                                            int mid) {
  auto *client = static_cast<cvedix_mqtt_client *>(obj);
  if (client->on_publish_cb_) {
    client->on_publish_cb_(mid);
  }
}

void cvedix_mqtt_client::on_message_wrapper(
    struct mosquitto *mosq, void *obj,
    const struct mosquitto_message *message) {
  auto *client = static_cast<cvedix_mqtt_client *>(obj);
  if (client->on_message_cb_ && message) {
    std::string topic(message->topic ? message->topic : "");
    std::string payload(static_cast<const char *>(message->payload),
                        message->payloadlen);
    client->on_message_cb_(topic, payload);
  }
}

} // namespace cvedix_utils
