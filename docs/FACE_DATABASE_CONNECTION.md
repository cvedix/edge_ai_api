# Face Database Connection Guide

Hướng dẫn chi tiết về cách cấu hình và sử dụng Face Database Connection với MySQL/PostgreSQL.

## 📋 Mục Lục

1. [Tổng Quan](#tổng-quan)
2. [Kiểm Tra Database Schema](#kiểm-tra-database-schema)
3. [Cấu Hình Kết Nối](#cấu-hình-kết-nối)
4. [Kiểm Tra Cấu Hình](#kiểm-tra-cấu-hình)
5. [Tắt Database Connection](#tắt-database-connection)
6. [Troubleshooting](#troubleshooting)

---

## Tổng Quan

Face Database Connection cho phép bạn lưu trữ dữ liệu face recognition vào MySQL hoặc PostgreSQL thay vì sử dụng file `face_database.txt` mặc định.

### Database Schema Yêu Cầu

Hệ thống yêu cầu 2 bảng trong database:

1. **face_libraries** - Lưu trữ thông tin khuôn mặt
2. **face_log** - Lưu trữ log các request

### Endpoints

- `POST /v1/recognition/face-database/connection` - Cấu hình kết nối database
- `GET /v1/recognition/face-database/connection` - Lấy cấu hình hiện tại

---

## Kiểm Tra Database Schema

Trước khi cấu hình, hãy đảm bảo database và các bảng đã được tạo đúng cấu trúc.

### 1. Kiểm Tra Database

```bash
mysql -u root -p
```

```sql
SHOW DATABASES;
USE face_recognition;
SHOW TABLES;
```

### 2. Kiểm Tra Cấu Trúc Bảng `face_libraries`

```sql
DESCRIBE face_libraries;
```

Kết quả mong đợi:
```
+-------------+--------------+------+-----+-------------------+-------------------+
| Field       | Type         | Null | Key | Default           | Extra             |
+-------------+--------------+------+-----+-------------------+-------------------+
| id          | int          | NO   | PRI | NULL              | auto_increment    |
| image_id    | varchar(36)  | YES  |     | NULL              |                   |
| subject     | varchar(255) | YES  |     | NULL              |                   |
| base64_image| longtext     | YES  |     | NULL              |                   |
| embedding   | text         | YES  |     | NULL              |                   |
| create_at   | timestamp    | YES  |     | CURRENT_TIMESTAMP | DEFAULT_GENERATED |
| machine_id  | varchar(255) | YES  |     | NULL              |                   |
| mac_address | varchar(255) | YES  |     | NULL              |                   |
+-------------+--------------+------+-----+-------------------+-------------------+
```

### 3. Kiểm Tra Cấu Trúc Bảng `face_log`

```sql
DESCRIBE face_log;
```

Kết quả mong đợi:
```
+--------------+--------------+------+-----+-------------------+-------------------+
| Field        | Type         | Null | Key | Default           | Extra             |
+--------------+--------------+------+-----+-------------------+-------------------+
| id           | int          | NO   | PRI | NULL              | auto_increment    |
| request_type | varchar(50)  | YES  |     | NULL              |                   |
| timestamp    | datetime     | YES  |     | NULL              |                   |
| client_ip    | varchar(45)  | YES  |     | NULL              |                   |
| request_body | longtext     | YES  |     | NULL              |                   |
| response_body| longtext     | YES  |     | NULL              |                   |
| response_code| int          | YES  |     | NULL              |                   |
| notes        | text         | YES  |     | NULL              |                   |
| mac_address  | varchar(255) | YES  |     | NULL              |                   |
| machine_id   | varchar(255) | YES  |     | NULL              |                   |
+--------------+--------------+------+-----+-------------------+-------------------+
```

### 4. Tạo Bảng Nếu Chưa Có

Nếu các bảng chưa tồn tại, bạn có thể tạo bằng các lệnh sau:

```sql
-- Tạo bảng face_libraries
CREATE TABLE IF NOT EXISTS face_libraries (
    id INT AUTO_INCREMENT PRIMARY KEY,
    image_id VARCHAR(36),
    subject VARCHAR(255),
    base64_image LONGTEXT,
    embedding TEXT,
    create_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    machine_id VARCHAR(255),
    mac_address VARCHAR(255),
    INDEX idx_image_id (image_id),
    INDEX idx_subject (subject)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Tạo bảng face_log
CREATE TABLE IF NOT EXISTS face_log (
    id INT AUTO_INCREMENT PRIMARY KEY,
    request_type VARCHAR(50),
    timestamp DATETIME,
    client_ip VARCHAR(45),
    request_body LONGTEXT,
    response_body LONGTEXT,
    response_code INT,
    notes TEXT,
    mac_address VARCHAR(255),
    machine_id VARCHAR(255),
    INDEX idx_timestamp (timestamp),
    INDEX idx_request_type (request_type)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
```

### 5. Tạo User và Cấp Quyền (Nếu Cần)

```sql
-- Tạo user mới (thay 'face_user' và 'your_password' bằng giá trị của bạn)
CREATE USER IF NOT EXISTS 'face_user'@'localhost' IDENTIFIED BY 'your_password';

-- Cấp quyền cho database face_recognition
GRANT ALL PRIVILEGES ON face_recognition.* TO 'face_user'@'localhost';
FLUSH PRIVILEGES;

-- Kiểm tra quyền
SHOW GRANTS FOR 'face_user'@'localhost';
```

---

## Cấu Hình Kết Nối

### Bước 1: Kiểm Tra Server API Đang Chạy

```bash
curl http://localhost:8080/v1/core/health
```

Kết quả mong đợi:
```json
{
  "status": "ok",
  "timestamp": "2024-01-01T00:00:00Z"
}
```

### Bước 2: Cấu Hình Kết Nối MySQL

Sử dụng endpoint `POST /v1/recognition/face-database/connection` để cấu hình:

```bash
curl -X POST http://localhost:8080/v1/recognition/face-database/connection \
  -H "Content-Type: application/json" \
  -d '{
    "type": "mysql",
    "host": "localhost",
    "port": 3306,
    "database": "face_recognition",
    "username": "face_user",
    "password": "your_password",
    "charset": "utf8mb4"
  }'
```

**Lưu ý:** Thay các giá trị sau bằng thông tin thực tế của bạn:
- `host`: Địa chỉ MySQL server (thường là `localhost` hoặc `127.0.0.1`)
- `port`: Port MySQL (mặc định: `3306`)
- `database`: Tên database (`face_recognition`)
- `username`: Username MySQL của bạn
- `password`: Password MySQL của bạn
- `charset`: Character set (mặc định: `utf8mb4`)

### Bước 3: Kiểm Tra Response

Response thành công sẽ có dạng:

```json
{
  "message": "Face database connection configured successfully",
  "config": {
    "type": "mysql",
    "host": "localhost",
    "port": 3306,
    "database": "face_recognition",
    "username": "face_user",
    "charset": "utf8mb4"
  },
  "note": "Database connection will be used instead of face_database.txt file"
}
```

### Ví Dụ Với Thông Tin Thực Tế

Nếu bạn đang sử dụng:
- **Host:** `localhost`
- **Port:** `3306` (mặc định MySQL)
- **Database:** `face_recognition`
- **Username:** `root` (hoặc user khác)
- **Password:** `your_mysql_password`

```bash
curl -X POST http://localhost:8080/v1/recognition/face-database/connection \
  -H "Content-Type: application/json" \
  -d '{
    "type": "mysql",
    "host": "localhost",
    "port": 3306,
    "database": "face_recognition",
    "username": "root",
    "password": "your_mysql_password",
    "charset": "utf8mb4"
  }'
```

### Sử Dụng File JSON (Tùy Chọn)

Bạn cũng có thể tạo file JSON và sử dụng:

**file: `db_config.json`**
```json
{
  "type": "mysql",
  "host": "localhost",
  "port": 3306,
  "database": "face_recognition",
  "username": "face_user",
  "password": "your_password",
  "charset": "utf8mb4"
}
```

```bash
curl -X POST http://localhost:8080/v1/recognition/face-database/connection \
  -H "Content-Type: application/json" \
  -d @db_config.json
```

---

## Kiểm Tra Cấu Hình

### 1. Lấy Cấu Hình Hiện Tại

```bash
curl http://localhost:8080/v1/recognition/face-database/connection
```

Response khi database đã được cấu hình:

```json
{
  "enabled": true,
  "config": {
    "type": "mysql",
    "host": "localhost",
    "port": 3306,
    "database": "face_recognition",
    "username": "face_user",
    "charset": "utf8mb4"
  },
  "message": "Database connection is configured and enabled"
}
```

Response khi chưa cấu hình database:

```json
{
  "enabled": false,
  "message": "No database connection configured. Using default face_database.txt file",
  "default_file": "/opt/edge_ai_api/data/face_database.txt"
}
```

### 2. Kiểm Tra Trong config.json

Cấu hình được lưu trong `config.json` dưới section `face_database`:

```bash
cat config.json | jq '.face_database'
```

Kết quả:
```json
{
  "enabled": true,
  "connection": {
    "type": "mysql",
    "host": "localhost",
    "port": 3306,
    "database": "face_recognition",
    "username": "face_user",
    "charset": "utf8mb4"
  }
}
```

### 3. Test Kết Nối Database

Sau khi cấu hình, bạn có thể test bằng cách:

**a. Đăng ký một face mới:**

```bash
curl -X POST "http://localhost:8080/v1/recognition/faces?subject=test_user&det_prob_threshold=0.5" \
  -F "file=@/path/to/face_image.jpg"
```

**b. Kiểm tra trong database:**

```sql
USE face_recognition;
SELECT * FROM face_libraries ORDER BY id DESC LIMIT 5;
SELECT * FROM face_log ORDER BY id DESC LIMIT 5;
```

Nếu dữ liệu xuất hiện trong database, nghĩa là kết nối đã hoạt động!

---

## Tắt Database Connection

Để tắt database connection và quay lại sử dụng file `face_database.txt`:

```bash
curl -X POST http://localhost:8080/v1/recognition/face-database/connection \
  -H "Content-Type: application/json" \
  -d '{"enabled": false}'
```

Response:

```json
{
  "message": "Database connection disabled. Using default face_database.txt file",
  "enabled": false,
  "default_file": "/opt/edge_ai_api/data/face_database.txt"
}
```

---

## Troubleshooting

### Lỗi: "Field 'type' (mysql/postgresql) is required"

**Nguyên nhân:** Thiếu trường `type` trong request body.

**Giải pháp:** Đảm bảo request body có đầy đủ các trường bắt buộc:
- `type`: `"mysql"` hoặc `"postgresql"`
- `host`: Địa chỉ database server
- `database`: Tên database
- `username`: Username
- `password`: Password

### Lỗi: "Field 'type' must be either 'mysql' or 'postgresql'"

**Nguyên nhân:** Giá trị `type` không đúng.

**Giải pháp:** Sử dụng `"mysql"` hoặc `"postgresql"` (chữ thường).

### Lỗi: "Failed to save configuration"

**Nguyên nhân:** Không thể ghi vào `config.json`.

**Giải pháp:**
1. Kiểm tra quyền ghi file: `ls -l config.json`
2. Đảm bảo thư mục tồn tại
3. Kiểm tra log: `tail -f logs/api.log`

### Lỗi Kết Nối Database

**Nguyên nhân:** Thông tin kết nối không đúng hoặc database không khả dụng.

**Giải pháp:**

1. **Kiểm tra MySQL đang chạy:**
   ```bash
   sudo systemctl status mysql
   # hoặc
   sudo service mysql status
   ```

2. **Kiểm tra kết nối từ command line:**
   ```bash
   mysql -u face_user -p -h localhost face_recognition
   ```

3. **Kiểm tra firewall:**
   ```bash
   sudo ufw status
   # Nếu cần, mở port MySQL:
   sudo ufw allow 3306
   ```

4. **Kiểm tra MySQL bind address:**
   ```bash
   sudo grep bind-address /etc/mysql/mysql.conf.d/mysqld.cnf
   ```
   
   Nếu là `127.0.0.1`, chỉ chấp nhận kết nối localhost. Nếu cần kết nối từ xa, đổi thành `0.0.0.0` và restart MySQL:
   ```bash
   sudo systemctl restart mysql
   ```

### Database Không Có Dữ Liệu

**Nguyên nhân:** Có thể hệ thống vẫn đang sử dụng file `face_database.txt`.

**Giải pháp:**

1. Kiểm tra cấu hình hiện tại:
   ```bash
   curl http://localhost:8080/v1/recognition/face-database/connection
   ```

2. Đảm bảo `enabled: true` trong response

3. Restart API server sau khi cấu hình:
   ```bash
   # Nếu chạy từ terminal
   # Ctrl+C để dừng và chạy lại
   ./build/edge_ai_api
   
   # Nếu chạy từ systemd
   sudo systemctl restart edge-ai-api
   ```

### Kiểm Tra Logs

Xem logs để debug:

```bash
# Logs API
tail -f logs/api.log

# Hoặc nếu dùng systemd
sudo journalctl -u edge-ai-api -f
```

Tìm các dòng có chứa `[FaceDatabase]` hoặc `[API] POST /v1/recognition/face-database/connection`.

---

## Ví Dụ Hoàn Chỉnh

### Scenario: Cấu Hình MySQL Local Server

**1. Kiểm tra MySQL:**
```bash
mysql -u root -p
```

```sql
SHOW DATABASES;
USE face_recognition;
SHOW TABLES;
```

**2. Cấu hình kết nối:**
```bash
curl -X POST http://localhost:8080/v1/recognition/face-database/connection \
  -H "Content-Type: application/json" \
  -d '{
    "type": "mysql",
    "host": "localhost",
    "port": 3306,
    "database": "face_recognition",
    "username": "root",
    "password": "your_mysql_password"
  }'
```

**3. Verify cấu hình:**
```bash
curl http://localhost:8080/v1/recognition/face-database/connection | jq
```

**4. Test bằng cách đăng ký face:**
```bash
curl -X POST "http://localhost:8080/v1/recognition/faces?subject=test_user" \
  -F "file=@test_face.jpg"
```

**5. Kiểm tra trong database:**
```sql
SELECT * FROM face_libraries WHERE subject = 'test_user';
SELECT * FROM face_log ORDER BY id DESC LIMIT 1;
```

---

## Lưu Ý Quan Trọng

1. **Bảo Mật:** Không commit password vào git. Sử dụng environment variables hoặc config file riêng.

2. **Backup:** Luôn backup database trước khi thay đổi cấu hình.

3. **Performance:** Database connection có thể chậm hơn file-based nếu network latency cao.

4. **Migration:** Dữ liệu từ `face_database.txt` không tự động migrate sang database. Bạn cần migrate thủ công nếu cần.

5. **Schema:** Đảm bảo database schema đúng với yêu cầu trước khi cấu hình.

---

## Tài Liệu Tham Khảo

- [API Reference](./API.md) - Tài liệu API đầy đủ
- [OpenAPI Specification](../openapi.yaml) - OpenAPI spec chi tiết
- [Architecture](./ARCHITECTURE.md) - Kiến trúc hệ thống

