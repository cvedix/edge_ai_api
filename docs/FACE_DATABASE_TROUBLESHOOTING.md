# Face Database Troubleshooting Guide

Hướng dẫn debug và sửa lỗi khi face database không lưu vào MySQL/PostgreSQL.

## 🔍 Vấn Đề: Dữ Liệu Vẫn Lưu Vào File Thay Vì Database

### Triệu Chứng
- ✅ Cấu hình database connection thành công (200 OK)
- ✅ POST register face thành công
- ✅ GET list faces thành công
- ❌ Nhưng dữ liệu chỉ có trong `face_database.txt`, không có trong database

### Nguyên Nhân Có Thể

1. **Server chưa được restart sau khi cấu hình**
   - Config được lưu vào `config.json` nhưng server cần restart để load config mới

2. **Database connection test fail**
   - MySQL command không có trong PATH
   - Password có ký tự đặc biệt gây lỗi
   - MySQL connection fail nhưng không báo lỗi rõ

3. **Config không được load đúng**
   - `face_database.enabled` không phải `true`
   - Config path không đúng

## 🛠️ Cách Debug

### Bước 1: Chạy Debug Script

```bash
cd examples/face_database
./debug_database_issue.sh
```

Script sẽ kiểm tra:
- Config trong `config.json`
- API response về database config
- MySQL connection
- INSERT test
- Logs

### Bước 2: Kiểm Tra Config

```bash
# Kiểm tra config.json
cat config.json | jq '.face_database'

# Hoặc nếu ở production path
cat /opt/edge_ai_api/config/config.json | jq '.face_database'
```

Kết quả mong đợi:
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

### Bước 3: Kiểm Tra API Response

```bash
curl http://localhost:8080/v1/recognition/face-database/connection | jq
```

Nếu `enabled: false`, có nghĩa là:
- Config chưa được lưu đúng
- Hoặc server chưa restart

### Bước 4: Kiểm Tra Logs

```bash
tail -f logs/api.log | grep -i "FaceDatabaseHelper\|RecognitionHandler.*Database"
```

Tìm các dòng:
- `[FaceDatabaseHelper] Database connection enabled: ...` - Config được load
- `[RecognitionHandler] Database enabled: yes/no` - Database check
- `[RecognitionHandler] Database connection test successful` - Connection OK
- `[FaceDatabaseHelper] MySQL error: ...` - Lỗi MySQL

### Bước 5: Test MySQL Command

```bash
# Kiểm tra mysql command có trong PATH
which mysql

# Test connection
MYSQL_PWD='Admin@123' mysql -h localhost -P 3306 -u face_user face_recognition -e "SELECT 1;"

# Test INSERT
MYSQL_PWD='Admin@123' mysql -h localhost -P 3306 -u face_user face_recognition <<EOF
INSERT INTO face_libraries (image_id, subject, base64_image, embedding, created_at) 
VALUES ('test-123', 'test', 'test', '1.0,2.0', NOW());
EOF
```

## ✅ Giải Pháp

### Giải Pháp 1: Restart API Server

**QUAN TRỌNG:** Sau khi cấu hình database connection, **PHẢI RESTART** API server!

```bash
# Nếu chạy từ terminal
# Ctrl+C để stop, sau đó:
./build/edge_ai_api

# Nếu chạy từ systemd
sudo systemctl restart edge-ai-api
```

### Giải Pháp 2: Kiểm Tra MySQL Command

Đảm bảo `mysql` command có trong PATH:

```bash
which mysql
# Phải trả về: /usr/bin/mysql hoặc tương tự

# Nếu không có, cài đặt:
sudo apt-get install mysql-client
```

### Giải Pháp 3: Kiểm Tra Permissions

Đảm bảo user `face_user` có quyền INSERT:

```bash
mysql -u root -p
```

```sql
USE face_recognition;
SHOW GRANTS FOR 'face_user'@'localhost';
-- Phải có: INSERT, SELECT, UPDATE, DELETE
```

Nếu thiếu quyền:
```sql
GRANT ALL PRIVILEGES ON face_recognition.* TO 'face_user'@'localhost';
FLUSH PRIVILEGES;
```

### Giải Pháp 4: Kiểm Tra Password

Nếu password có ký tự đặc biệt (`@`, `#`, `$`, v.v.), đảm bảo được escape đúng trong config.json.

## 📋 Checklist Debug

- [ ] Config đã được lưu vào `config.json` với `enabled: true`
- [ ] API server đã được **RESTART** sau khi cấu hình
- [ ] API response cho thấy `enabled: true`
- [ ] MySQL command có trong PATH (`which mysql`)
- [ ] MySQL connection test thành công
- [ ] User có quyền INSERT vào database
- [ ] Logs không có lỗi MySQL
- [ ] Column name đúng (`created_at` không phải `create_at`)

## 🔧 Test Thủ Công

### Test 1: Cấu hình lại database

```bash
curl -X POST http://localhost:8080/v1/recognition/face-database/connection \
  -H "Content-Type: application/json" \
  -d '{
    "type": "mysql",
    "host": "localhost",
    "port": 3306,
    "database": "face_recognition",
    "username": "face_user",
    "password": "Admin@123",
    "charset": "utf8mb4"
  }'
```

### Test 2: Verify config

```bash
curl http://localhost:8080/v1/recognition/face-database/connection | jq
```

### Test 3: RESTART server

```bash
# Stop server (Ctrl+C)
# Start lại
./build/edge_ai_api
```

### Test 4: Register face mới

```bash
curl -X POST "http://localhost:8080/v1/recognition/faces?subject=test_debug" \
  -F "file=@test_face.jpg"
```

### Test 5: Kiểm tra database

```bash
mysql -u face_user -p'Admin@123' -e "USE face_recognition; SELECT * FROM face_libraries ORDER BY id DESC LIMIT 5;"
```

### Test 6: Kiểm tra logs

```bash
tail -f logs/api.log | grep -i "database\|FaceDatabaseHelper"
```

Tìm các dòng:
- `Database enabled: yes` ✓
- `Database connection test successful` ✓
- `Face saved to database successfully` ✓
- Hoặc `Database save FAILED` nếu có lỗi

## 🚨 Lỗi Thường Gặp

### Lỗi: "Database enabled: no"

**Nguyên nhân:** Config chưa được load hoặc server chưa restart

**Giải pháp:**
1. Kiểm tra `config.json` có `face_database.enabled = true`
2. **RESTART API server**

### Lỗi: "MySQL command failed"

**Nguyên nhân:** 
- MySQL command không có trong PATH
- Password sai
- Connection fail

**Giải pháp:**
1. Cài đặt `mysql-client`: `sudo apt-get install mysql-client`
2. Test connection thủ công
3. Kiểm tra password và permissions

### Lỗi: "Unknown column 'create_at'"

**Nguyên nhân:** Column name sai (phải là `created_at`)

**Giải pháp:** Đã được sửa trong code, rebuild project

## 📞 Cần Trợ Giúp?

Nếu vẫn gặp vấn đề, cung cấp:
1. Output của `debug_database_issue.sh`
2. Logs từ `logs/api.log`
3. Output của `curl http://localhost:8080/v1/recognition/face-database/connection`
4. Kết quả `mysql -u face_user -p -e "SELECT 1;"`


