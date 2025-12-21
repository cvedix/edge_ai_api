#!/bin/bash

# Test MySQL INSERT command với password có ký tự đặc biệt

DB_HOST="localhost"
DB_PORT="3306"
DB_NAME="face_recognition"
DB_USER="face_user"
DB_PASSWORD="Admin@123"

echo "=== Test MySQL INSERT Command ==="
echo ""

# Test 1: Simple connection test
echo "[1/3] Test connection..."
MYSQL_PWD="$DB_PASSWORD" mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" "$DB_NAME" -e "SELECT 1;" 2>&1
if [ $? -eq 0 ]; then
    echo "✓ Connection successful"
else
    echo "❌ Connection failed"
    exit 1
fi
echo ""

# Test 2: Test INSERT với simple data
echo "[2/3] Test INSERT với simple data..."
cat > /tmp/test_insert.sql <<EOF
INSERT INTO face_libraries (image_id, subject, base64_image, embedding, created_at) 
VALUES ('test-123', 'test_subject', 'test_base64', '1.0,2.0,3.0', NOW());
EOF

MYSQL_PWD="$DB_PASSWORD" mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" "$DB_NAME" < /tmp/test_insert.sql 2>&1
if [ $? -eq 0 ]; then
    echo "✓ INSERT successful"
    # Clean up test data
    MYSQL_PWD="$DB_PASSWORD" mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" "$DB_NAME" -e "DELETE FROM face_libraries WHERE image_id='test-123';" 2>&1
else
    echo "❌ INSERT failed"
    cat /tmp/test_insert.sql
fi
rm -f /tmp/test_insert.sql
echo ""

# Test 3: Test với data có ký tự đặc biệt
echo "[3/3] Test INSERT với ký tự đặc biệt..."
cat > /tmp/test_insert2.sql <<'EOF'
INSERT INTO face_libraries (image_id, subject, base64_image, embedding, created_at) 
VALUES ('test-456', 'test''subject', 'base64''data', '1.0,2.0,3.0', NOW());
EOF

MYSQL_PWD="$DB_PASSWORD" mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" "$DB_NAME" < /tmp/test_insert2.sql 2>&1
if [ $? -eq 0 ]; then
    echo "✓ INSERT with special chars successful"
    # Clean up
    MYSQL_PWD="$DB_PASSWORD" mysql -h "$DB_HOST" -P "$DB_PORT" -u "$DB_USER" "$DB_NAME" -e "DELETE FROM face_libraries WHERE image_id='test-456';" 2>&1
else
    echo "❌ INSERT with special chars failed"
fi
rm -f /tmp/test_insert2.sql
echo ""

echo "=== Test Complete ==="

