# Face Database Connection - Quick Start

Hướng dẫn nhanh để cấu hình Face Database Connection với MySQL local server.

## 🚀 Quick Setup (5 phút)

### Bước 1: Kiểm Tra Database

```bash
mysql -u root -p
```

```sql
USE face_recognition;
SHOW TABLES;
-- Phải có: face_libraries và face_log
```

### Bước 2: Cấu Hình Kết Nối

**Thay các giá trị sau:**
- `your_mysql_password` → Password MySQL của bạn
- `face_user` → Username MySQL (hoặc `root` nếu dùng root)

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

### Bước 3: Kiểm Tra

```bash
# Kiểm tra cấu hình
curl http://localhost:8080/v1/recognition/face-database/connection | jq

# Test đăng ký face
curl -X POST "http://localhost:8080/v1/recognition/faces?subject=test_user" \
  -F "file=@test_face.jpg"

# Kiểm tra trong database
mysql -u root -p -e "USE face_recognition; SELECT * FROM face_libraries;"
```

## ✅ Kết Quả Mong Đợi

### Response khi cấu hình thành công:

```json
{
  "message": "Face database connection configured successfully",
  "config": {
    "type": "mysql",
    "host": "localhost",
    "port": 3306,
    "database": "face_recognition",
    "username": "root",
    "charset": "utf8mb4"
  },
  "note": "Database connection will be used instead of face_database.txt file"
}
```

### Response khi kiểm tra cấu hình:

```json
{
  "enabled": true,
  "config": {
    "type": "mysql",
    "host": "localhost",
    "port": 3306,
    "database": "face_recognition",
    "username": "root",
    "charset": "utf8mb4"
  },
  "message": "Database connection is configured and enabled"
}
```

## 🔧 Troubleshooting

### Lỗi: "Field 'host' is required"
→ Đảm bảo request body có đầy đủ các trường: `type`, `host`, `database`, `username`, `password`

### Lỗi: "Failed to save configuration"
→ Kiểm tra quyền ghi file `config.json`:
```bash
ls -l config.json
chmod 644 config.json  # Nếu cần
```

### Database không có dữ liệu
→ Kiểm tra MySQL đang chạy:
```bash
sudo systemctl status mysql
```

## 📚 Tài Liệu Đầy Đủ

Xem [Face Database Connection Guide](./FACE_DATABASE_CONNECTION.md) để biết chi tiết.

## 🔄 Tắt Database Connection

Để quay lại dùng file `face_database.txt`:

```bash
curl -X POST http://localhost:8080/v1/recognition/face-database/connection \
  -H "Content-Type: application/json" \
  -d '{"enabled": false}'
```

