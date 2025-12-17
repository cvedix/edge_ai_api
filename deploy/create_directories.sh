#!/bin/bash
# ============================================
# Edge AI API - Create Directories Helper
# ============================================
#
# Script helper để tạo thư mục từ directories.conf
# Có thể được dùng bởi:
#   - debian/rules (Makefile)
#   - debian/postinst (bash)
#   - deploy/build.sh (bash)
#   - deploy/install_directories.sh (bash)
#
# Usage:
#   source deploy/create_directories.sh
#   create_app_directories "/opt/edge_ai_api" "/path/to/deploy"
#
# ============================================

# Function to create directories from configuration
# Parameters:
#   $1: INSTALL_DIR (e.g., /opt/edge_ai_api)
#   $2: PROJECT_ROOT or path to deploy directory (default: current directory)
create_app_directories() {
    local INSTALL_DIR="$1"
    local PROJECT_ROOT="${2:-$(pwd)}"

    if [ -z "$INSTALL_DIR" ]; then
        echo "Error: INSTALL_DIR is required" >&2
        return 1
    fi

    # Find directories.conf
    local DIRS_CONF=""
    if [ -f "$PROJECT_ROOT/deploy/directories.conf" ]; then
        DIRS_CONF="$PROJECT_ROOT/deploy/directories.conf"
    elif [ -f "$(dirname "$PROJECT_ROOT")/deploy/directories.conf" ]; then
        DIRS_CONF="$(dirname "$PROJECT_ROOT")/deploy/directories.conf"
    elif [ -f "deploy/directories.conf" ]; then
        DIRS_CONF="deploy/directories.conf"
    else
        echo "Warning: directories.conf not found, using defaults" >&2
        # Fallback to default directories
        declare -A APP_DIRECTORIES=(
            ["instances"]="750"
            ["solutions"]="750"
            ["groups"]="750"
            ["nodes"]="750"
            ["models"]="750"
            ["videos"]="750"
            ["logs"]="750"
            ["data"]="750"
            ["config"]="750"
            ["fonts"]="750"
            ["uploads"]="755"
            ["lib"]="755"
        )
    fi

    # Load configuration if file exists
    if [ -n "$DIRS_CONF" ] && [ -f "$DIRS_CONF" ]; then
        # Source the config file in a subshell to avoid polluting current environment
        # We need to extract the APP_DIRECTORIES array
        local temp_script=$(mktemp)
        cat > "$temp_script" << 'TEMP_EOF'
source "$1"
declare -p APP_DIRECTORIES
TEMP_EOF
        local dirs_output=$(bash "$temp_script" "$DIRS_CONF" 2>/dev/null)
        rm -f "$temp_script"

        # Evaluate the output to get the array
        if [ -n "$dirs_output" ]; then
            eval "$dirs_output"
        fi
    fi

    # Create each directory
    for dir_name in "${!APP_DIRECTORIES[@]}"; do
        local dir_path="$INSTALL_DIR/$dir_name"
        local dir_perms="${APP_DIRECTORIES[$dir_name]}"

        # Create directory
        mkdir -p "$dir_path" 2>/dev/null || true

        # Set permissions if specified
        if [ -n "$dir_perms" ] && [ "$dir_perms" != "0" ]; then
            chmod "$dir_perms" "$dir_path" 2>/dev/null || true
        fi
    done

    return 0
}

# Function to get list of directories (for Makefile or other uses)
# Returns space-separated list of directory names
get_directory_list() {
    local PROJECT_ROOT="${1:-$(pwd)}"

    # Find directories.conf
    local DIRS_CONF=""
    if [ -f "$PROJECT_ROOT/deploy/directories.conf" ]; then
        DIRS_CONF="$PROJECT_ROOT/deploy/directories.conf"
    elif [ -f "deploy/directories.conf" ]; then
        DIRS_CONF="deploy/directories.conf"
    fi

    # Load configuration
    if [ -n "$DIRS_CONF" ] && [ -f "$DIRS_CONF" ]; then
        local temp_script=$(mktemp)
        cat > "$temp_script" << 'TEMP_EOF'
source "$1"
for dir in "${!APP_DIRECTORIES[@]}"; do
    echo -n "$dir "
done
TEMP_EOF
        local dirs_list=$(bash "$temp_script" "$DIRS_CONF" 2>/dev/null)
        rm -f "$temp_script"
        echo "$dirs_list"
    else
        # Fallback
        echo "instances solutions groups nodes models videos logs data config fonts uploads lib"
    fi
}

# If script is run directly (not sourced), create directories
if [ "${BASH_SOURCE[0]}" = "${0}" ]; then
    if [ $# -lt 1 ]; then
        echo "Usage: $0 <INSTALL_DIR> [PROJECT_ROOT]"
        echo "   or: source $0  (to use functions)"
        exit 1
    fi
    create_app_directories "$1" "${2:-$(pwd)}"
fi
