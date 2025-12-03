# Hardware Information API Documentation

## Tổng quan

Hardware Information API cung cấp các endpoint để truy vấn thông tin phần cứng và trạng thái hệ thống của thiết bị Edge AI. API này sử dụng thư viện [hwinfo](https://github.com/lfreist/hwinfo) để thu thập thông tin chi tiết về CPU, GPU, RAM, Disk, Mainboard, OS và Battery.

## Endpoints

### 1. GET /v1/core/system/info

Lấy thông tin phần cứng tĩnh của hệ thống.

**Request:**
```bash
curl http://localhost:8080/v1/core/system/info
```

**Response (200 OK):**
```json
{
  "cpu": {
    "vendor": "GenuineIntel",
    "model": "Intel(R) Core(TM) i7-10700K CPU @ 3.80GHz",
    "physical_cores": 8,
    "logical_cores": 16,
    "max_frequency": 3792,
    "regular_frequency": 3792,
    "min_frequency": -1,
    "current_frequency": 3792,
    "cache_size": 16777216
  },
  "ram": {
    "vendor": "Corsair",
    "model": "CMK32GX4M2Z3600C18",
    "name": "Physical Memory",
    "serial_number": "N/A",
    "size_mib": 65437,
    "free_mib": 54405,
    "available_mib": 54405
  },
  "gpu": [
    {
      "vendor": "NVIDIA",
      "model": "NVIDIA GeForce RTX 3070 Ti",
      "driver_version": "31.0.15.2698",
      "memory_mib": 8190,
      "current_frequency": 0,
      "min_frequency": -1,
      "max_frequency": -1
    }
  ],
  "disk": [
    {
      "vendor": "(Standard disk drives)",
      "model": "WD_BLACK SN850 Heatsink 1TB",
      "serial_number": "***",
      "size_bytes": 1000202273280,
      "free_size_bytes": 500000000000,
      "volumes": ["/dev/sda1", "/dev/sda2"]
    }
  ],
  "mainboard": {
    "vendor": "ASUSTeK COMPUTER INC.",
    "name": "PRIME Z490-A",
    "version": "Rev 1.xx",
    "serial_number": "N/A"
  },
  "os": {
    "name": "Ubuntu 22.04 LTS",
    "short_name": "Linux",
    "version": "22.04",
    "kernel": "5.15.0-72-generic",
    "architecture": "64 bit",
    "endianess": "little endian"
  },
  "battery": []
}
```

**Chi tiết các trường:**

#### CPU
- `vendor`: Nhà sản xuất CPU (GenuineIntel, AuthenticAMD, ...)
- `model`: Tên model CPU đầy đủ
- `physical_cores`: Số lõi vật lý
- `logical_cores`: Số lõi logic (threads)
- `max_frequency`: Tần số tối đa (MHz)
- `regular_frequency`: Tần số thông thường (MHz)
- `min_frequency`: Tần số tối thiểu (MHz, -1 nếu không hỗ trợ)
- `current_frequency`: Tần số hiện tại (MHz, -1 nếu không hỗ trợ)
- `cache_size`: Kích thước cache (bytes)

#### RAM
- `vendor`: Nhà sản xuất RAM
- `model`: Model RAM
- `name`: Tên RAM
- `serial_number`: Số serial (có thể "N/A" trên Linux)
- `size_mib`: Tổng dung lượng (MiB)
- `free_mib`: Dung lượng trống (MiB)
- `available_mib`: Dung lượng khả dụng (MiB)

#### GPU (Array)
- `vendor`: Nhà sản xuất GPU (NVIDIA, AMD, Intel, ...)
- `model`: Tên model GPU
- `driver_version`: Phiên bản driver
- `memory_mib`: Dung lượng bộ nhớ GPU (MiB)
- `current_frequency`: Tần số hiện tại (MHz)
- `min_frequency`: Tần số tối thiểu (MHz, -1 nếu không hỗ trợ)
- `max_frequency`: Tần số tối đa (MHz, -1 nếu không hỗ trợ)

**Lưu ý:** Một số thông tin GPU có thể không khả dụng trên Linux nếu không có OpenCL hoặc vendor-specific APIs.

#### Disk (Array)
- `vendor`: Nhà sản xuất ổ đĩa
- `model`: Model ổ đĩa
- `serial_number`: Số serial
- `size_bytes`: Dung lượng tổng (bytes)
- `free_size_bytes`: Dung lượng trống (bytes)
- `volumes`: Danh sách các phân vùng/volumes

#### Mainboard
- `vendor`: Nhà sản xuất mainboard
- `name`: Tên mainboard
- `version`: Phiên bản mainboard
- `serial_number`: Số serial (có thể "N/A" trên Linux)

#### OS
- `name`: Tên hệ điều hành đầy đủ
- `short_name`: Tên ngắn (Windows, Linux, macOS)
- `version`: Phiên bản OS
- `kernel`: Phiên bản kernel (có thể "unknown" trên Windows)
- `architecture`: Kiến trúc (32 bit, 64 bit)
- `endianess`: Endianess (little endian, big endian)

#### Battery (Array)
- `vendor`: Nhà sản xuất pin
- `model`: Model pin
- `serial_number`: Số serial
- `technology`: Công nghệ pin
- `capacity_mwh`: Dung lượng (mWh)
- `charging`: Trạng thái sạc (true/false)

**Lưu ý:** Array có thể rỗng `[]` trên desktop/server không có pin.

---

### 2. GET /v1/core/system/status

Lấy trạng thái hiện tại của hệ thống (CPU usage, RAM usage, load average, uptime).

**Request:**
```bash
curl http://localhost:8080/v1/core/system/status
```

**Response (200 OK):**
```json
{
  "cpu": {
    "usage_percent": 25.5,
    "current_frequency_mhz": 3792,
    "temperature_celsius": 45.5
  },
  "ram": {
    "total_mib": 65437,
    "used_mib": 11032,
    "free_mib": 54405,
    "available_mib": 54405,
    "usage_percent": 16.85,
    "cached_mib": 5000,
    "buffers_mib": 1000
  },
  "load_average": {
    "1min": 0.75,
    "5min": 0.82,
    "15min": 0.88
  },
  "uptime_seconds": 86400
}
```

**Chi tiết các trường:**

#### CPU Status
- `usage_percent`: Mức sử dụng CPU tổng thể (0-100%)
- `current_frequency_mhz`: Tần số CPU hiện tại (MHz, -1 nếu không hỗ trợ)
- `temperature_celsius`: Nhiệt độ CPU (Celsius, chỉ có nếu hệ thống hỗ trợ)

#### RAM Status
- `total_mib`: Tổng dung lượng RAM (MiB)
- `used_mib`: Dung lượng RAM đã sử dụng (MiB)
- `free_mib`: Dung lượng RAM trống (MiB)
- `available_mib`: Dung lượng RAM khả dụng (MiB)
- `usage_percent`: Tỷ lệ sử dụng RAM (0-100%)
- `cached_mib`: Dung lượng cached (MiB, optional)
- `buffers_mib`: Dung lượng buffers (MiB, optional)

#### Load Average
- `1min`: Load average 1 phút
- `5min`: Load average 5 phút
- `15min`: Load average 15 phút

#### System Uptime
- `uptime_seconds`: Thời gian hoạt động của hệ thống (giây)

---

## Cách sử dụng

### 1. Kiểm tra thông tin phần cứng

```bash
# Lấy thông tin phần cứng đầy đủ
curl http://localhost:8080/v1/core/system/info | jq

# Lấy thông tin CPU
curl http://localhost:8080/v1/core/system/info | jq '.cpu'

# Lấy danh sách GPU
curl http://localhost:8080/v1/core/system/info | jq '.gpu'

# Lấy thông tin RAM
curl http://localhost:8080/v1/core/system/info | jq '.ram'
```

### 2. Kiểm tra trạng thái hệ thống

```bash
# Lấy trạng thái hệ thống
curl http://localhost:8080/v1/core/system/status | jq

# Lấy CPU usage
curl http://localhost:8080/v1/core/system/status | jq '.cpu.usage_percent'

# Lấy RAM usage
curl http://localhost:8080/v1/core/system/status | jq '.ram.usage_percent'

# Lấy load average
curl http://localhost:8080/v1/core/system/status | jq '.load_average'
```

### 3. Sử dụng với Python

```python
import requests
import json

# Lấy thông tin phần cứng
response = requests.get('http://localhost:8080/v1/core/system/info')
hardware_info = response.json()

print(f"CPU: {hardware_info['cpu']['model']}")
print(f"Physical Cores: {hardware_info['cpu']['physical_cores']}")
print(f"Logical Cores: {hardware_info['cpu']['logical_cores']}")
print(f"RAM: {hardware_info['ram']['size_mib']} MiB")

# Lấy trạng thái hệ thống
response = requests.get('http://localhost:8080/v1/core/system/status')
system_status = response.json()

print(f"CPU Usage: {system_status['cpu']['usage_percent']}%")
print(f"RAM Usage: {system_status['ram']['usage_percent']}%")
print(f"Load Average (1min): {system_status['load_average']['1min']}")
```

### 4. Sử dụng với JavaScript/Node.js

```javascript
const axios = require('axios');

// Lấy thông tin phần cứng
async function getHardwareInfo() {
  try {
    const response = await axios.get('http://localhost:8080/v1/core/system/info');
    const info = response.data;
    
    console.log(`CPU: ${info.cpu.model}`);
    console.log(`Cores: ${info.cpu.physical_cores} physical, ${info.cpu.logical_cores} logical`);
    console.log(`RAM: ${info.ram.size_mib} MiB`);
    console.log(`GPUs: ${info.gpu.length}`);
  } catch (error) {
    console.error('Error:', error.message);
  }
}

// Lấy trạng thái hệ thống
async function getSystemStatus() {
  try {
    const response = await axios.get('http://localhost:8080/v1/core/system/status');
    const status = response.data;
    
    console.log(`CPU Usage: ${status.cpu.usage_percent}%`);
    console.log(`RAM Usage: ${status.ram.usage_percent}%`);
    console.log(`Uptime: ${status.uptime_seconds} seconds`);
  } catch (error) {
    console.error('Error:', error.message);
  }
}

getHardwareInfo();
getSystemStatus();
```

---

## Hỗ trợ nền tảng

### Linux
- CPU: Đầy đủ thông tin
- RAM: Đầy đủ thông tin
- Disk: Đầy đủ thông tin
- Mainboard: Đầy đủ (không có serial number)
- OS: Đầy đủ thông tin
- GPU: Cần OpenCL hoặc vendor-specific APIs để có thông tin đầy đủ
- Battery: Hỗ trợ nếu có

### Windows
- Tất cả components đều được hỗ trợ đầy đủ

### macOS
- CPU, RAM, Disk, OS: Đầy đủ
- GPU, Battery: Có thể hạn chế

---

## Lưu ý và Troubleshooting

### 1. Giá trị -1 hoặc null

Một số trường có thể trả về `-1` hoặc `null` nếu:
- Hệ thống không hỗ trợ thông tin đó
- Không có quyền truy cập (cần root/admin)
- Driver hoặc kernel không hỗ trợ

**Ví dụ:**
- `min_frequency: -1` - Hệ thống không hỗ trợ đọc tần số tối thiểu
- `temperature_celsius: null` - Không có cảm biến nhiệt độ hoặc không thể đọc

### 2. GPU Information

Thông tin GPU trên Linux có thể hạn chế nếu:
- Không có OpenCL được cài đặt
- Không có vendor-specific tools (nvidia-smi, rocm-smi)
- GPU không được nhận diện bởi hệ thống

**Giải pháp:**
- Cài đặt OpenCL: `sudo apt-get install opencl-headers ocl-icd-opencl-dev`
- Với NVIDIA: Cài đặt nvidia-smi
- Với AMD: Cài đặt rocm-smi

### 3. CPU Usage Calculation

CPU usage được tính toán dựa trên 2 mẫu thời gian cách nhau ít nhất 100ms. Lần gọi đầu tiên có thể trả về `0.0` vì chưa có dữ liệu để so sánh.

**Khuyến nghị:** Gọi API 2 lần cách nhau 1 giây để có kết quả chính xác.

### 4. Memory Information

- `free_mib`: RAM hoàn toàn trống
- `available_mib`: RAM có thể sử dụng ngay (bao gồm cả cached có thể giải phóng)
- `cached_mib`: RAM đang được cache (có thể giải phóng khi cần)

### 5. Disk Information

- `size_bytes`: Dung lượng thực tế của ổ đĩa
- `free_size_bytes`: Dung lượng trống trên tất cả các phân vùng
- `volumes`: Danh sách các phân vùng/volumes trên ổ đĩa

### 6. Temperature Reading

Nhiệt độ CPU được đọc từ `/sys/class/thermal/thermal_zone*/temp` trên Linux. Nếu không có cảm biến nhiệt độ hoặc không thể đọc, trường này sẽ không xuất hiện trong response.

---

## Error Handling

API sẽ trả về lỗi trong các trường hợp sau:

### 500 Internal Server Error

```json
{
  "error": "Internal server error",
  "message": "Error details here"
}
```

**Nguyên nhân có thể:**
- hwinfo library không thể đọc thông tin phần cứng
- Lỗi khi đọc file system (/proc, /sys)
- Lỗi cấu hình

**Giải pháp:**
- Kiểm tra quyền truy cập file system
- Kiểm tra log của application
- Đảm bảo hwinfo submodule đã được khởi tạo đúng

---

## Performance

- **GET /v1/core/system/info**: Thông tin tĩnh, có thể cache nếu cần
- **GET /v1/core/system/status**: Thông tin động, nên gọi real-time

**Khuyến nghị:**
- Không gọi quá thường xuyên (tối đa 1 lần/giây cho status)
- Cache thông tin phần cứng nếu không thay đổi

---

## Ví dụ sử dụng thực tế

### Monitoring Dashboard

```python
import requests
import time
from datetime import datetime

def monitor_system(interval=5):
    """Monitor system status mỗi interval giây"""
    while True:
        try:
            # Lấy trạng thái
            status = requests.get('http://localhost:8080/v1/core/system/status').json()
            
            # In thông tin
            print(f"[{datetime.now()}] CPU: {status['cpu']['usage_percent']:.1f}% | "
                  f"RAM: {status['ram']['usage_percent']:.1f}% | "
                  f"Load: {status['load_average']['1min']:.2f}")
            
            time.sleep(interval)
        except Exception as e:
            print(f"Error: {e}")
            time.sleep(interval)

monitor_system()
```

### Health Check với Hardware Info

```bash
#!/bin/bash
# Script kiểm tra health và hardware

echo "=== System Health Check ==="

# Health check
HEALTH=$(curl -s http://localhost:8080/v1/core/health | jq -r '.status')
echo "Health Status: $HEALTH"

# Hardware info
CPU_MODEL=$(curl -s http://localhost:8080/v1/core/system/info | jq -r '.cpu.model')
RAM_SIZE=$(curl -s http://localhost:8080/v1/core/system/info | jq -r '.ram.size_mib')
echo "CPU: $CPU_MODEL"
echo "RAM: ${RAM_SIZE} MiB"

# System status
CPU_USAGE=$(curl -s http://localhost:8080/v1/core/system/status | jq -r '.cpu.usage_percent')
RAM_USAGE=$(curl -s http://localhost:8080/v1/core/system/status | jq -r '.ram.usage_percent')
echo "CPU Usage: ${CPU_USAGE}%"
echo "RAM Usage: ${RAM_USAGE}%"
```

---

## Tài liệu tham khảo

- [hwinfo Library](https://github.com/lfreist/hwinfo) - Thư viện phần cứng information
- [OpenAPI Specification](../openapi.yaml) - Full API specification
- [Edge AI API README](../README.md) - Tài liệu chính của project

---


