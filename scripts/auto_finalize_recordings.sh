#!/bin/bash
# ============================================
# Edge AI API - Auto Finalize Recordings Script
# ============================================
#
# Script để tự động finalize các file MP4 trong thư mục record
# Có thể chạy định kỳ hoặc sau khi recording dừng
#
# Usage:
#   ./scripts/auto_finalize_recordings.sh [record_directory]
#   ./scripts/auto_finalize_recordings.sh /opt/edge_ai_api/record
#
# ============================================

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default record directory
RECORD_DIR="${1:-/opt/edge_ai_api/record}"

# Get script directory for calling convert script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Check if directory exists
if [ ! -d "$RECORD_DIR" ]; then
    echo -e "${RED}Error: Directory not found: $RECORD_DIR${NC}" >&2
    exit 1
fi

# Check if ffmpeg is available
if ! command -v ffmpeg &> /dev/null; then
    echo -e "${RED}Error: ffmpeg is not installed${NC}" >&2
    echo "Please install ffmpeg: sudo apt-get install ffmpeg" >&2
    exit 1
fi

echo -e "${BLUE}============================================${NC}"
echo -e "${BLUE}Auto Finalize Recordings${NC}"
echo -e "${BLUE}============================================${NC}"
echo "Directory: $RECORD_DIR"
echo

# Find all MP4 files
MP4_FILES=$(find "$RECORD_DIR" -maxdepth 1 -type f -name "*.mp4" 2>/dev/null | sort)

if [ -z "$MP4_FILES" ]; then
    echo -e "${YELLOW}No MP4 files found in $RECORD_DIR${NC}"
    exit 0
fi

# Count files
FILE_COUNT=$(echo "$MP4_FILES" | wc -l)
echo "Found $FILE_COUNT MP4 file(s)"
echo

SUCCESS_COUNT=0
FAILED_COUNT=0
SKIPPED_COUNT=0

# Process each file
while IFS= read -r mp4_file; do
    # Check if file is being written to
    if command -v lsof &> /dev/null; then
        if lsof "$mp4_file" &> /dev/null; then
            echo -e "${YELLOW}⏭ Skipping (file is being written): $(basename "$mp4_file")${NC}"
            SKIPPED_COUNT=$((SKIPPED_COUNT + 1))
            continue
        fi
    fi
    
    # Check file size stability
    PREV_SIZE=$(stat -f%z "$mp4_file" 2>/dev/null || stat -c%s "$mp4_file" 2>/dev/null)
    sleep 1
    CURR_SIZE=$(stat -f%z "$mp4_file" 2>/dev/null || stat -c%s "$mp4_file" 2>/dev/null)
    
    if [ "$PREV_SIZE" != "$CURR_SIZE" ]; then
        echo -e "${YELLOW}⏭ Skipping (file size changed): $(basename "$mp4_file")${NC}"
        SKIPPED_COUNT=$((SKIPPED_COUNT + 1))
        continue
    fi
    
    # Try to finalize
    echo -e "${BLUE}Processing: $(basename "$mp4_file")${NC}"
    
    TEMP_FILE="${mp4_file}.tmp"
    
    # First try faststart (fastest, preserves quality)
    if ffmpeg -i "$mp4_file" -c copy -movflags +faststart "$TEMP_FILE" -y 2>/dev/null; then
        mv "$TEMP_FILE" "$mp4_file"
        echo -e "${GREEN}✓ Finalized: $(basename "$mp4_file")${NC}"
        SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
    else
        rm -f "$TEMP_FILE"
        echo -e "${YELLOW}⚠ Faststart failed (file may use incompatible encoding settings)${NC}"
        echo -e "${BLUE}  Checking file encoding...${NC}"
        
        # Check if file uses incompatible settings
        PROFILE=$(ffprobe -v quiet -select_streams v:0 -show_entries stream=profile -of default=noprint_wrappers=1:nokey=1 "$mp4_file" 2>/dev/null || echo "")
        PIX_FMT=$(ffprobe -v quiet -select_streams v:0 -show_entries stream=pix_fmt -of default=noprint_wrappers=1:nokey=1 "$mp4_file" 2>/dev/null || echo "")
        
        if [[ "$PROFILE" == *"High"* ]] || [[ "$PIX_FMT" != "yuv420p" ]]; then
            echo -e "${YELLOW}  File uses incompatible encoding (Profile: ${PROFILE:-unknown}, Pixel: ${PIX_FMT:-unknown})${NC}"
            echo -e "${BLUE}  Converting to compatible format...${NC}"
            
            # Use convert script to create compatible version
            COMPATIBLE_FILE="${mp4_file%.*}_compatible.mp4"
            if "$SCRIPT_DIR/convert_mp4_compatible.sh" "$mp4_file" "$COMPATIBLE_FILE" 2>/dev/null; then
                echo -e "${GREEN}✓ Converted to compatible format: $(basename "$COMPATIBLE_FILE")${NC}"
                SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
            else
                echo -e "${RED}✗ Conversion failed: $(basename "$mp4_file")${NC}"
                FAILED_COUNT=$((FAILED_COUNT + 1))
            fi
        else
            echo -e "${YELLOW}⚠ Faststart failed but encoding seems compatible${NC}"
            FAILED_COUNT=$((FAILED_COUNT + 1))
        fi
    fi
    
    echo
done <<< "$MP4_FILES"

# Summary
echo -e "${BLUE}============================================${NC}"
echo -e "${BLUE}Summary:${NC}"
echo "  Successfully finalized: $SUCCESS_COUNT"
echo "  Failed: $FAILED_COUNT"
echo "  Skipped (being written): $SKIPPED_COUNT"
echo -e "${BLUE}============================================${NC}"

if [ $FAILED_COUNT -gt 0 ]; then
    echo
    echo -e "${YELLOW}Note: Some files failed to finalize with faststart.${NC}"
    echo "You may need to convert them manually:"
    echo "  ./scripts/convert_mp4_compatible.sh <file>"
fi

exit 0

