#!/bin/bash

# Script test kết nối với database cụ thể
# Thông tin từ mysql_config.json.example

# Màu sắc
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Thông tin database từ config
DB_HOST="localhost"
DB_PORT="3306"
DB_NAME="face_recognition"
DB_USER="face_user"
DB_PASSWORD="Admin@123"
DB_CHARSET="utf8mb4"

echo -e "${BLUE}=== Test Kết Nối Database ===${NC}\n"
echo -e "${YELLOW}Thông tin kết nối:${NC}"
echo "  Host: $DB_HOST"
echo "  Port: $DB_PORT"
echo "  Database: $DB_NAME"
echo "  Username: $DB_USER"
echo "  Charset: $DB_CHARSET"
echo ""

echo -e "${GREEN}[1/4] Test kết nối MySQL...${NC}"
if mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" -p"$DB_PASSWORD" -e "SELECT 1;" 2>/dev/null; then
    echo -e "${GREEN}✓ Kết nối thành công!${NC}\n"
else
    echo -e "${RED}❌ Kết nối thất bại!${NC}"
    echo -e "${YELLOW}Kiểm tra:${NC}"
    echo "  - MySQL đang chạy: sudo systemctl status mysql"
    echo "  - User có quyền: mysql -u root -p -e \"SHOW GRANTS FOR '$DB_USER'@'localhost';\""
    echo "  - Password đúng: mysql -u $DB_USER -p"
    exit 1
fi

echo -e "${GREEN}[2/4] Kiểm tra database '$DB_NAME'...${NC}"
DB_EXISTS=$(mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" -p"$DB_PASSWORD" -e "SHOW DATABASES LIKE '$DB_NAME';" 2>/dev/null | grep -c "$DB_NAME")

if [ "$DB_EXISTS" -eq 1 ]; then
    echo -e "${GREEN}✓ Database '$DB_NAME' tồn tại${NC}\n"
else
    echo -e "${RED}❌ Database '$DB_NAME' không tồn tại${NC}"
    echo -e "${YELLOW}Tạo database...${NC}"
    mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" -p"$DB_PASSWORD" -e "CREATE DATABASE IF NOT EXISTS $DB_NAME CHARACTER SET $DB_CHARSET COLLATE utf8mb4_unicode_ci;"
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ Đã tạo database '$DB_NAME'${NC}\n"
    else
        echo -e "${RED}❌ Không thể tạo database (có thể user không có quyền)${NC}"
        exit 1
    fi
fi

echo -e "${GREEN}[3/4] Kiểm tra tables...${NC}"
TABLES=$(mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" -p"$DB_PASSWORD" "$DB_NAME" -e "SHOW TABLES;" 2>/dev/null | grep -E "(face_libraries|face_log)" | wc -l)

if [ "$TABLES" -eq 2 ]; then
    echo -e "${GREEN}✓ Cả 2 tables đều tồn tại${NC}\n"
elif [ "$TABLES" -eq 1 ]; then
    echo -e "${YELLOW}⚠ Chỉ có 1 table (thiếu 1 table)${NC}\n"
else
    echo -e "${YELLOW}⚠ Không có tables nào, tạo tables...${NC}"
    mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" -p"$DB_PASSWORD" "$DB_NAME" <<EOF
CREATE TABLE IF NOT EXISTS face_libraries (
    id INT AUTO_INCREMENT PRIMARY KEY,
    image_id VARCHAR(36),
    subject VARCHAR(255),
    base64_image LONGTEXT,
    embedding TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    machine_id VARCHAR(255),
    mac_address VARCHAR(255),
    INDEX idx_image_id (image_id),
    INDEX idx_subject (subject)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

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
EOF
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ Đã tạo tables${NC}\n"
    else
        echo -e "${RED}❌ Không thể tạo tables${NC}"
        exit 1
    fi
fi

echo -e "${GREEN}[4/4] Kiểm tra dữ liệu...${NC}"
FACE_COUNT=$(mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" -p"$DB_PASSWORD" "$DB_NAME" -e "SELECT COUNT(*) FROM face_libraries;" 2>/dev/null | tail -1)
LOG_COUNT=$(mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" -p"$DB_PASSWORD" "$DB_NAME" -e "SELECT COUNT(*) FROM face_log;" 2>/dev/null | tail -1)

echo -e "${BLUE}Records:${NC}"
echo "  face_libraries: $FACE_COUNT"
echo "  face_log: $LOG_COUNT"
echo ""

echo -e "${GREEN}=== Kết nối thành công! ===${NC}\n"
echo -e "${YELLOW}Để cấu hình API với thông tin này:${NC}"
echo "curl -X POST http://localhost:8080/v1/recognition/face-database/connection \\"
echo "  -H \"Content-Type: application/json\" \\"
echo "  -d '{"
echo "    \"type\": \"mysql\","
echo "    \"host\": \"$DB_HOST\","
echo "    \"port\": $DB_PORT,"
echo "    \"database\": \"$DB_NAME\","
echo "    \"username\": \"$DB_USER\","
echo "    \"password\": \"$DB_PASSWORD\","
echo "    \"charset\": \"$DB_CHARSET\""
echo "  }'"
echo ""

