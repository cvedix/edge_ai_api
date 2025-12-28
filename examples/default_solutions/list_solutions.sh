#!/bin/bash

# Script để liệt kê các default solutions có sẵn

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INDEX_FILE="$SCRIPT_DIR/index.json"

if [ ! -f "$INDEX_FILE" ]; then
    echo "Error: index.json not found at $INDEX_FILE"
    exit 1
fi

# Parse và hiển thị danh sách solutions
echo "=========================================="
echo "  Default Solutions Catalog"
echo "=========================================="
echo ""

# Sử dụng jq nếu có, nếu không dùng python
if command -v jq &> /dev/null; then
    # Hiển thị theo category
    echo "📁 Categories:"
    jq -r '.categories | to_entries[] | "  - \(.value.name): \(.value.description)"' "$INDEX_FILE"
    echo ""

    echo "📋 Solutions:"
    jq -r '.solutions[] | "  [\(.category)] \(.id) - \(.name)\n      \(.description)\n      Use case: \(.useCase)\n      Difficulty: \(.difficulty)\n"' "$INDEX_FILE"

    echo ""
    echo "💡 Usage:"
    echo "  curl -X POST http://localhost:8080/v1/core/solution \\"
    echo "    -H \"Content-Type: application/json\" \\"
    echo "    -d @$SCRIPT_DIR/<solution_file>.json"

elif command -v python3 &> /dev/null; then
    python3 << EOF
import json
import sys

with open("$INDEX_FILE", "r") as f:
    data = json.load(f)

print("📁 Categories:")
for cat_id, cat_info in data["categories"].items():
    print(f"  - {cat_info['name']}: {cat_info['description']}")

print("\n📋 Solutions:")
for sol in data["solutions"]:
    difficulty_stars = "⭐" * (["beginner", "intermediate", "advanced"].index(sol["difficulty"]) + 1)
    print(f"  [{sol['category']}] {sol['id']} - {sol['name']}")
    print(f"      {sol['description']}")
    print(f"      Use case: {sol['useCase']}")
    print(f"      Difficulty: {difficulty_stars} {sol['difficulty']}")
    print()

print("💡 Usage:")
print("  curl -X POST http://localhost:8080/v1/core/solution \\")
print("    -H \"Content-Type: application/json\" \\")
print(f"    -d @$SCRIPT_DIR/<solution_file>.json")
EOF
else
    echo "Error: Need jq or python3 to parse JSON"
    echo "Install jq: sudo apt-get install jq"
    exit 1
fi
