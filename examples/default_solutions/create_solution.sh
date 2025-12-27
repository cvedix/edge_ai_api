#!/bin/bash

# Script để tạo solution từ default solutions

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
API_BASE_URL="${API_BASE_URL:-http://localhost:8080}"

if [ $# -eq 0 ]; then
    echo "Usage: $0 <solution_id> [API_BASE_URL]"
    echo ""
    echo "Available solutions:"
    if command -v jq &> /dev/null && [ -f "$SCRIPT_DIR/index.json" ]; then
        jq -r '.solutions[] | "  - \(.id) (\(.file))"' "$SCRIPT_DIR/index.json"
    else
        for file in "$SCRIPT_DIR"/*.json; do
            basename=$(basename "$file")
            if [ "$basename" != "index.json" ]; then
                echo "  - $basename"
            fi
        done
    fi
    exit 1
fi

SOLUTION_ID="$1"
if [ -n "$2" ]; then
    API_BASE_URL="$2"
fi

# Tìm file solution
SOLUTION_FILE=""
if [ -f "$SCRIPT_DIR/index.json" ]; then
    if command -v jq &> /dev/null; then
        SOLUTION_FILE=$(jq -r ".solutions[] | select(.id == \"$SOLUTION_ID\") | .file" "$SCRIPT_DIR/index.json")
    elif command -v python3 &> /dev/null; then
        SOLUTION_FILE=$(python3 << EOF
import json
with open("$SCRIPT_DIR/index.json") as f:
    data = json.load(f)
    for sol in data["solutions"]:
        if sol["id"] == "$SOLUTION_ID":
            print(sol["file"])
            break
EOF
)
    fi
fi

# Nếu không tìm thấy trong index, thử tìm trực tiếp
if [ -z "$SOLUTION_FILE" ] || [ ! -f "$SCRIPT_DIR/$SOLUTION_FILE" ]; then
    # Thử với tên file trực tiếp
    if [ -f "$SCRIPT_DIR/${SOLUTION_ID}.json" ]; then
        SOLUTION_FILE="${SOLUTION_ID}.json"
    else
        echo "Error: Solution '$SOLUTION_ID' not found"
        echo ""
        echo "Available solutions:"
        for file in "$SCRIPT_DIR"/*.json; do
            basename=$(basename "$file")
            if [ "$basename" != "index.json" ]; then
                echo "  - $basename"
            fi
        done
        exit 1
    fi
fi

SOLUTION_PATH="$SCRIPT_DIR/$SOLUTION_FILE"

echo "Creating solution from: $SOLUTION_FILE"
echo "API URL: $API_BASE_URL"
echo ""

# Tạo solution
RESPONSE=$(curl -s -w "\n%{http_code}" -X POST "$API_BASE_URL/v1/core/solution" \
    -H "Content-Type: application/json" \
    -d @"$SOLUTION_PATH")

HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
BODY=$(echo "$RESPONSE" | sed '$d')

if [ "$HTTP_CODE" -eq 200 ] || [ "$HTTP_CODE" -eq 201 ]; then
    echo "✅ Solution created successfully!"
    echo "$BODY" | jq . 2>/dev/null || echo "$BODY"
else
    echo "❌ Failed to create solution (HTTP $HTTP_CODE)"
    echo "$BODY"
    exit 1
fi
