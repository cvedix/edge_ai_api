# Face Database Connection Examples

Ví dụ và scripts để cấu hình Face Database Connection.

## 📁 Files

- `configure_mysql_local.sh` - Script tự động cấu hình MySQL local server

## 🚀 Sử Dụng Script

### Cách 1: Sử dụng với thông tin mặc định

```bash
cd examples/face_database
./configure_mysql_local.sh
```

Script sẽ hỏi MySQL password.

### Cách 2: Sử dụng với environment variables

```bash
export DB_HOST=localhost
export DB_PORT=3306
export DB_NAME=face_recognition
export DB_USER=root
export DB_PASSWORD=your_password
export API_URL=http://localhost:8080

./configure_mysql_local.sh
```

### Cách 3: Sử dụng với user khác

```bash
DB_USER=face_user DB_PASSWORD=face_password ./configure_mysql_local.sh
```

## ✅ Script Sẽ:

1. ✓ Kiểm tra API server đang chạy
2. ✓ Kiểm tra MySQL connection
3. ✓ Kiểm tra database tables
4. ✓ Cấu hình database connection
5. ✓ Verify cấu hình

## 📚 Tài Liệu

- [Face Database Connection Guide](../../docs/FACE_DATABASE_CONNECTION.md) - Hướng dẫn chi tiết
- [Quick Start Guide](../../docs/FACE_DATABASE_QUICK_START.md) - Hướng dẫn nhanh
- [API Reference](../../docs/API.md) - Tài liệu API

## 🔧 Manual Configuration

Nếu không muốn dùng script, có thể cấu hình thủ công:

```bash
curl -X POST http://localhost:8080/v1/recognition/face-database/connection \
  -H "Content-Type: application/json" \
  -d '{
    "type": "mysql",
    "host": "localhost",
    "port": 3306,
    "database": "face_recognition",
    "username": "root",
    "password": "your_password",
    "charset": "utf8mb4"
  }'
```

