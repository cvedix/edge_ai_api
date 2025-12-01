#!/bin/bash
# ============================================
# Load Environment Variables from .env file
# ============================================
# 
# This script loads environment variables from .env file
# and runs the edge_ai_api server.
#
# Usage:
#   ./scripts/load_env.sh
#   ./scripts/load_env.sh /path/to/.env
#
# ============================================

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Default .env file location
ENV_FILE="${1:-$PROJECT_ROOT/.env}"

# Check if .env file exists
if [ ! -f "$ENV_FILE" ]; then
    echo "Warning: .env file not found at $ENV_FILE"
    echo "Creating from .env.example..."
    
    if [ -f "$PROJECT_ROOT/.env.example" ]; then
        cp "$PROJECT_ROOT/.env.example" "$ENV_FILE"
        echo "Created .env file from .env.example"
        echo "Please edit $ENV_FILE and set your values"
        exit 1
    else
        echo "Error: .env.example not found. Cannot create .env file."
        exit 1
    fi
fi

# Load environment variables
echo "Loading environment variables from $ENV_FILE"
set -a  # Automatically export all variables
source "$ENV_FILE"
set +a  # Stop automatically exporting

# Change to project root
cd "$PROJECT_ROOT" || exit 1

# Check if build directory exists
if [ ! -d "build" ]; then
    echo "Error: build directory not found. Please build the project first."
    exit 1
fi

# Check if executable exists (try both locations)
EXECUTABLE=""
if [ -f "build/bin/edge_ai_api" ]; then
    EXECUTABLE="build/bin/edge_ai_api"
elif [ -f "build/edge_ai_api" ]; then
    EXECUTABLE="build/edge_ai_api"
else
    echo "Error: edge_ai_api executable not found."
    echo "Searched in: build/bin/edge_ai_api and build/edge_ai_api"
    echo "Please build the project first."
    exit 1
fi

# Run the server
echo "Starting Edge AI API Server..."
echo "Using environment variables from $ENV_FILE"
echo "Executable: $EXECUTABLE"
echo ""

# Show loaded API_PORT for debugging
if [ -n "$API_PORT" ]; then
    echo "API_PORT=$API_PORT (from .env)"
else
    echo "Warning: API_PORT not set, using default 8080"
fi
echo ""

cd "$(dirname "$EXECUTABLE")" || exit 1
./$(basename "$EXECUTABLE")

