#!/usr/bin/env python3
"""
Script to update example JSON files to use new input/output structure in additionalParams.
Maintains backward compatibility by supporting both formats.
"""

import json
import os
import sys
from pathlib import Path

# Define parameter categories
INPUT_PARAMS = {
    "FILE_PATH", "RTSP_SRC_URL", "RTSP_URL", "RTMP_SRC_URL", "RTMP_URL",  # RTMP_URL can be input if RTMP_DES_URL not present
    "HLS_URL", "HTTP_URL", "MODEL_PATH", "SFACE_MODEL_PATH", "WEIGHTS_PATH",
    "CONFIG_PATH", "LABELS_PATH", "RESIZE_RATIO", "MODEL_NAME", "SFACE_MODEL_NAME",
    "FACE_DETECTION_MODEL_PATH", "BUFFALO_L_FACE_ENCODING_MODEL", "EMAP_FILE_FOR_EMBEDDINGS",
    "FACE_SWAP_MODEL_PATH", "SWAP_SOURCE_IMAGE", "INPUT_WIDTH", "INPUT_HEIGHT", "SCORE_THRESHOLD",
    "CROSSLINE_START_X", "CROSSLINE_START_Y", "CROSSLINE_END_X", "CROSSLINE_END_Y",
    "LINE_CHANNEL", "LINE_START_X", "LINE_START_Y", "LINE_END_X", "LINE_END_Y"
}

OUTPUT_PARAMS = {
    "RTMP_URL", "RTMP_DES_URL", "MQTT_BROKER_URL", "MQTT_PORT", "MQTT_TOPIC",
    "MQTT_USERNAME", "MQTT_PASSWORD", "MQTT_RATE_LIMIT_MS", "ENABLE_SCREEN_DES",
    "RECORD_PATH", "BROKE_FOR", "PROCESSING_DELAY_MS"
}

def classify_params(params):
    """Classify parameters into input and output."""
    input_params = {}
    output_params = {}
    other_params = {}
    
    # Check if RTMP_URL is used for output (if RTMP_DES_URL exists)
    rtmp_used_for_output = "RTMP_DES_URL" in params
    
    for key, value in params.items():
        if key in INPUT_PARAMS:
            # Special case: RTMP_URL can be input or output
            if key == "RTMP_URL" and rtmp_used_for_output:
                output_params[key] = value
            else:
                input_params[key] = value
        elif key in OUTPUT_PARAMS:
            output_params[key] = value
        else:
            # Unknown parameter - try to infer from name
            if any(x in key.upper() for x in ["PATH", "MODEL", "WEIGHT", "CONFIG", "LABEL", "URL", "SRC", "RATIO", "INPUT"]):
                input_params[key] = value
            elif any(x in key.upper() for x in ["OUTPUT", "DES", "MQTT", "SCREEN", "RECORD", "BROKE"]):
                output_params[key] = value
            else:
                other_params[key] = value
    
    return input_params, output_params, other_params

def update_json_file(file_path):
    """Update a single JSON file to use input/output structure."""
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
        
        # Check if already using new format
        if "additionalParams" in data:
            if isinstance(data["additionalParams"], dict):
                if "input" in data["additionalParams"] or "output" in data["additionalParams"]:
                    print(f"  ✓ Already using new format: {file_path}")
                    return False
        
        # Convert old format to new format
        if "additionalParams" in data and isinstance(data["additionalParams"], dict):
            old_params = data["additionalParams"]
            input_params, output_params, other_params = classify_params(old_params)
            
            # Merge other_params into input_params (safer default)
            input_params.update(other_params)
            
            # Create new structure
            new_additional_params = {}
            if input_params:
                new_additional_params["input"] = input_params
            if output_params:
                new_additional_params["output"] = output_params
            
            data["additionalParams"] = new_additional_params
            
            # Write back
            with open(file_path, 'w', encoding='utf-8') as f:
                json.dump(data, f, indent=2, ensure_ascii=False)
            
            print(f"  ✓ Updated: {file_path}")
            return True
        
        return False
    except Exception as e:
        print(f"  ✗ Error updating {file_path}: {e}")
        return False

def main():
    """Main function to update all example files."""
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    examples_dir = project_root / "examples" / "instances"
    
    if not examples_dir.exists():
        print(f"Error: Examples directory not found: {examples_dir}")
        sys.exit(1)
    
    print(f"Updating example files in: {examples_dir}")
    print("-" * 60)
    
    updated_count = 0
    skipped_count = 0
    error_count = 0
    
    # Find all JSON files
    json_files = list(examples_dir.rglob("*.json"))
    
    for json_file in sorted(json_files):
        # Skip report_body_example.json files (they're not instance configs)
        if "report_body" in json_file.name.lower():
            skipped_count += 1
            continue
        
        if update_json_file(json_file):
            updated_count += 1
        else:
            skipped_count += 1
    
    print("-" * 60)
    print(f"Summary:")
    print(f"  Updated: {updated_count} files")
    print(f"  Skipped: {skipped_count} files")
    print(f"  Errors: {error_count} files")
    print("Done!")

if __name__ == "__main__":
    main()

