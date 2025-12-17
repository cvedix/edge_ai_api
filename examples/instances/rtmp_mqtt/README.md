# RTMP/MQTT Integration - Hướng Dẫn Test

## 📋 Tổng Quan

Tài liệu này hướng dẫn cách tích hợp RTMP streaming và MQTT event publishing trong các instances.

## 🎯 Tính Năng

- ✅ RTMP streaming output
- ✅ MQTT event publishing
- ✅ Kết hợp cả hai trong một instance
- ✅ Rate limiting cho MQTT events
- ✅ Custom JSON transformer cho MQTT

## 📁 Cấu Trúc Files

```
rtmp_mqtt/
├── README.md                    # File này
├── rtmp_setup_guide.md          # Hướng dẫn setup RTMP server
├── mqtt_setup_guide.md          # Hướng dẫn setup MQTT broker
└── integration_examples.md      # Ví dụ tích hợp
```

## 🔧 RTMP Streaming

### Cấu Hình RTMP Output

**Tham số trong `additionalParams`:**
```json
{
  "RTMP_URL": "rtmp://server:1935/live/stream_key",
  "RESIZE_RATIO": "1.0",
  "ENABLE_SCREEN_DES": "false"
}
```

### Test RTMP Stream

```bash
# Publish test stream
ffmpeg -re -i test.mp4 -c copy -f flv rtmp://server:1935/live/test

# Play stream
ffplay rtmp://server:1935/live/test
```

### Troubleshooting RTMP

**Lỗi: Connection failed**
```bash
# Kiểm tra RTMP server
netstat -tlnp | grep 1935

# Kiểm tra firewall
sudo ufw allow 1935/tcp

# Test với ffmpeg
ffmpeg -re -i test.mp4 -c copy -f flv rtmp://server:1935/live/test
```

## 🔧 MQTT Event Publishing

### Cấu Hình MQTT

**Tham số trong `additionalParams`:**
```json
{
  "MQTT_BROKER_URL": "localhost",
  "MQTT_PORT": "1883",
  "MQTT_TOPIC": "events",
  "MQTT_USERNAME": "",
  "MQTT_PASSWORD": "",
  "MQTT_RATE_LIMIT_MS": "1000",
  "BROKE_FOR": "FACE"  // hoặc "NORMAL"
}
```

### Subscribe MQTT Events

```bash
# Subscribe với mosquitto
mosquitto_sub -h localhost -t events -v

# Subscribe với authentication
mosquitto_sub -h localhost -t events -u username -P password -v
```

### Troubleshooting MQTT

**Lỗi: Connection failed**
```bash
# Kiểm tra MQTT broker
sudo systemctl status mosquitto

# Test connection
mosquitto_sub -h localhost -t test -v

# Kiểm tra port
netstat -tlnp | grep 1883
```

## 📝 Integration Examples

### Example 1: Face Detection + RTMP + MQTT

Xem: `face_detection/test_mqtt_events.json` và `face_detection/test_rtmp_output.json`

### Example 2: BA Crossline + RTMP + MQTT

Xem: `ba_crossline/test_rtsp_source_rtmp_mqtt.json`

### Example 3: Simple RTMP + MQTT Sample

Xem: `sample/simple_rtmp_mqtt_sample.cpp`

## 📚 Tài Liệu Tham Khảo

- RTMP setup: `sample/SELECTED_SAMPLES_RTMP_MQTT.md`
- MQTT transformer: `sample/README_MQTT_JSON_TRANSFORMER.md`
- Sample code: `sample/simple_rtmp_mqtt_sample.cpp`
