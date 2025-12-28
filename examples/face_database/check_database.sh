#!/bin/bash

# Script kiểm tra MySQL local server và database face_recognition

# Màu sắc
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== Kiểm Tra MySQL Local Server ===${NC}\n"

# Thông tin mặc định
DB_HOST="${DB_HOST:-localhost}"
DB_PORT="${DB_PORT:-3306}"
DB_NAME="${DB_NAME:-face_recognition}"
DB_USER="${DB_USER:-root}"

# Nếu không có password từ env, hỏi user
if [ -z "$DB_PASSWORD" ]; then
    echo -e "${YELLOW}Nhập MySQL password cho user '$DB_USER' (hoặc Enter nếu không có password):${NC}"
    read -s DB_PASSWORD
    echo
fi

echo -e "${GREEN}[1/5] Kiểm tra MySQL service đang chạy...${NC}"
if systemctl is-active --quiet mysql 2>/dev/null || systemctl is-active --quiet mysqld 2>/dev/null; then
    echo -e "${GREEN}✓ MySQL service đang chạy${NC}\n"
elif pgrep -x mysqld > /dev/null 2>&1; then
    echo -e "${GREEN}✓ MySQL process đang chạy${NC}\n"
else
    echo -e "${RED}❌ MySQL service không chạy${NC}"
    echo -e "${YELLOW}Khởi động MySQL:${NC}"
    echo "  sudo systemctl start mysql"
    echo "  # hoặc"
    echo "  sudo systemctl start mysqld"
    exit 1
fi

echo -e "${GREEN}[2/5] Kiểm tra kết nối MySQL...${NC}"
if [ -z "$DB_PASSWORD" ]; then
    if mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" -e "SELECT 1;" 2>/dev/null; then
        echo -e "${GREEN}✓ Kết nối MySQL thành công (không cần password)${NC}\n"
    else
        echo -e "${RED}❌ Không thể kết nối MySQL${NC}"
        exit 1
    fi
else
    if mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" -p"$DB_PASSWORD" -e "SELECT 1;" 2>/dev/null; then
        echo -e "${GREEN}✓ Kết nối MySQL thành công${NC}\n"
    else
        echo -e "${RED}❌ Không thể kết nối MySQL (sai password hoặc user không có quyền)${NC}"
        exit 1
    fi
fi

echo -e "${GREEN}[3/5] Kiểm tra database '$DB_NAME'...${NC}"
if [ -z "$DB_PASSWORD" ]; then
    DB_EXISTS=$(mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" -e "SHOW DATABASES LIKE '$DB_NAME';" 2>/dev/null | grep -c "$DB_NAME")
else
    DB_EXISTS=$(mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" -p"$DB_PASSWORD" -e "SHOW DATABASES LIKE '$DB_NAME';" 2>/dev/null | grep -c "$DB_NAME")
fi

if [ "$DB_EXISTS" -eq 1 ]; then
    echo -e "${GREEN}✓ Database '$DB_NAME' tồn tại${NC}\n"
else
    echo -e "${RED}❌ Database '$DB_NAME' không tồn tại${NC}"
    echo -e "${YELLOW}Tạo database:${NC}"
    if [ -z "$DB_PASSWORD" ]; then
        mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" -e "CREATE DATABASE IF NOT EXISTS $DB_NAME;"
    else
        mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" -p"$DB_PASSWORD" -e "CREATE DATABASE IF NOT EXISTS $DB_NAME;"
    fi
    echo -e "${GREEN}✓ Đã tạo database '$DB_NAME'${NC}\n"
fi

echo -e "${GREEN}[4/5] Kiểm tra tables...${NC}"
if [ -z "$DB_PASSWORD" ]; then
    TABLES=$(mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" "$DB_NAME" -e "SHOW TABLES;" 2>/dev/null | grep -E "(face_libraries|face_log)" | wc -l)
else
    TABLES=$(mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" -p"$DB_PASSWORD" "$DB_NAME" -e "SHOW TABLES;" 2>/dev/null | grep -E "(face_libraries|face_log)" | wc -l)
fi

if [ "$TABLES" -eq 2 ]; then
    echo -e "${GREEN}✓ Cả 2 tables đều tồn tại (face_libraries, face_log)${NC}\n"
elif [ "$TABLES" -eq 1 ]; then
    echo -e "${YELLOW}⚠ Chỉ có 1 table (thiếu 1 table)${NC}\n"
else
    echo -e "${YELLOW}⚠ Không có tables nào${NC}"
    echo -e "${YELLOW}Tạo tables...${NC}"
    
    if [ -z "$DB_PASSWORD" ]; then
        mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" "$DB_NAME" <<EOF
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
    else
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
    fi
    echo -e "${GREEN}✓ Đã tạo tables${NC}\n"
fi

echo -e "${GREEN}[5/5] Kiểm tra cấu trúc tables...${NC}"
if [ -z "$DB_PASSWORD" ]; then
    echo -e "${BLUE}--- face_libraries table structure ---${NC}"
    mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" "$DB_NAME" -e "DESCRIBE face_libraries;" 2>/dev/null
    echo
    echo -e "${BLUE}--- face_log table structure ---${NC}"
    mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" "$DB_NAME" -e "DESCRIBE face_log;" 2>/dev/null
    echo
    echo -e "${BLUE}--- Số lượng records ---${NC}"
    echo "face_libraries: $(mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" "$DB_NAME" -e "SELECT COUNT(*) FROM face_libraries;" 2>/dev/null | tail -1)"
    echo "face_log: $(mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" "$DB_NAME" -e "SELECT COUNT(*) FROM face_log;" 2>/dev/null | tail -1)"
else
    echo -e "${BLUE}--- face_libraries table structure ---${NC}"
    mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" -p"$DB_PASSWORD" "$DB_NAME" -e "DESCRIBE face_libraries;" 2>/dev/null
    echo
    echo -e "${BLUE}--- face_log table structure ---${NC}"
    mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" -p"$DB_PASSWORD" "$DB_NAME" -e "DESCRIBE face_log;" 2>/dev/null
    echo
    echo -e "${BLUE}--- Số lượng records ---${NC}"
    echo "face_libraries: $(mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" -p"$DB_PASSWORD" "$DB_NAME" -e "SELECT COUNT(*) FROM face_libraries;" 2>/dev/null | tail -1)"
    echo "face_log: $(mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" -p"$DB_PASSWORD" "$DB_NAME" -e "SELECT COUNT(*) FROM face_log;" 2>/dev/null | tail -1)"
fi

echo -e "\n${GREEN}=== Hoàn thành! ===${NC}"
echo -e "${YELLOW}Để xem dữ liệu chi tiết:${NC}"
if [ -z "$DB_PASSWORD" ]; then
    echo "mysql -u $DB_USER -h $DB_HOST -P $DB_PORT $DB_NAME"
    echo "  SELECT * FROM face_libraries ORDER BY id DESC LIMIT 5;"
    echo "  SELECT * FROM face_log ORDER BY id DESC LIMIT 5;"
else
    echo "mysql -u $DB_USER -p -h $DB_HOST -P $DB_PORT $DB_NAME"
    echo "  SELECT * FROM face_libraries ORDER BY id DESC LIMIT 5;"
    echo "  SELECT * FROM face_log ORDER BY id DESC LIMIT 5;"
fi

