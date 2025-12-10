# Hướng dẫn biên dịch và chạy MQTT JSON Receiver Sample

## Yêu cầu

1. **libmosquitto-dev** - Đã cài đặt ✓
2. **nlohmann/json** - Sẽ được tự động tải nếu chưa có
3. **CVEDIX SDK** - Phải được cài đặt trên hệ thống
4. **CMake** - Cần để build

## Cách 1: Build riêng trong thư mục samples (Khuyến nghị)

### Bước 1: Vào thư mục samples
```bash
cd /home/cvedix/project/edge_ai_api/samples
```

### Bước 2: Chạy script build
```bash
./build.sh
```

Hoặc build thủ công:
```bash
mkdir -p build
cd build
cmake .. -DAUTO_DOWNLOAD_DEPENDENCIES=ON
make -j$(nproc)
```

### Bước 3: Chạy sample

Executable sẽ được tạo tại: `samples/build/mqtt_json_receiver_sample`

```bash
cd build
./mqtt_json_receiver_sample [broker_url] [port] [topic] [username] [password]
```

## Cách 2: Build cùng với project chính

### Bước 1: Vào thư mục build của project
```bash
cd /home/cvedix/project/edge_ai_api
mkdir -p build
cd build
```

### Bước 2: Cấu hình CMake với MQTT support
```bash
cmake .. -DCVEDIX_WITH_MQTT=ON -DAUTO_DOWNLOAD_DEPENDENCIES=ON -DBUILD_SAMPLES=ON
```

### Bước 3: Biên dịch
```bash
make mqtt_json_receiver_sample -j$(nproc)
```

### Bước 4: Chạy sample

Executable sẽ được tạo tại: `build/samples/mqtt_json_receiver_sample`

**Cú pháp:**
```bash
./samples/mqtt_json_receiver_sample [broker_url] [port] [topic] [username] [password]
```

**Ví dụ:**
```bash
# Sử dụng broker mặc định
./samples/mqtt_json_receiver_sample

# Chỉ định broker và topic
./samples/mqtt_json_receiver_sample anhoidong.datacenter.cvedix.com 1883 events

# Với username và password
./samples/mqtt_json_receiver_sample localhost 1883 events user pass
```

## Thông số mặc định

- **Broker URL**: `anhoidong.datacenter.cvedix.com`
- **Port**: `1883`
- **Topic**: `events`
- **Username**: (trống)
- **Password**: (trống)

## Lưu ý

- Nhấn `Ctrl+C` để dừng chương trình
- Chương trình sẽ tự động reconnect nếu mất kết nối
- Thống kê sẽ được hiển thị mỗi 5 giây

