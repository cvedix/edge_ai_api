# Sửa lỗi CMake bị treo khi download nlohmann/json

## Vấn đề
CMake đang bị treo khi download nlohmann/json từ GitHub.

## Giải pháp nhanh

### Bước 1: Dừng CMake đang chạy
Nhấn `Ctrl+C` trong terminal để dừng quá trình cmake.

### Bước 2: Cài đặt nlohmann/json từ package (KHUYẾN NGHỊ)
```bash
sudo apt-get update
sudo apt-get install nlohmann-json3-dev
```

### Bước 3: Chạy lại CMake
```bash
cd /home/cvedix/project/edge_ai_api/samples/build
cmake .. -DAUTO_DOWNLOAD_DEPENDENCIES=ON
```

Hoặc nếu không muốn auto-download:
```bash
cmake .. -DAUTO_DOWNLOAD_DEPENDENCIES=OFF
```

## Lý do
- Cài từ package nhanh hơn (vài giây)
- Download từ GitHub có thể mất thời gian hoặc bị treo nếu mạng chậm
- Package đã được test và ổn định

## Sau khi cài xong
CMake sẽ tự động tìm thấy nlohmann/json trong `/usr/include/nlohmann/json.hpp` và không cần download nữa.

