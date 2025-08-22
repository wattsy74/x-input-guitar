#!/usr/bin/env python3
"""
Config JSON to C Header Converter
Converts config.json to a C header file for embedding in firmware
"""

import json
import sys
import os

def json_to_c_string(json_file_path, output_header_path):
    """Convert JSON file to C string literal header"""
    
    try:
        # Read and validate JSON
        with open(json_file_path, 'r') as f:
            config_data = json.load(f)
        
        # Re-serialize to ensure clean formatting
        json_string = json.dumps(config_data, indent=2)
        
        # Convert to C string literal
        c_string_lines = []
        for line in json_string.split('\n'):
            # Escape quotes and add line ending
            escaped_line = line.replace('\\', '\\\\').replace('"', '\\"')
            c_string_lines.append(f'"{escaped_line}\\n"')
        
        # Create header content
        header_content = '\n'.join(c_string_lines)
        
        # Write header file
        with open(output_header_path, 'w') as f:
            f.write(header_content)
        
        print(f"✅ Converted {json_file_path} -> {output_header_path}")
        print(f"📄 JSON size: {len(json_string)} bytes")
        
        return True
        
    except json.JSONDecodeError as e:
        print(f"❌ JSON parse error in {json_file_path}: {e}")
        return False
    except Exception as e:
        print(f"❌ Error converting config: {e}")
        return False

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    json_file = os.path.join(script_dir, "config.json")
    header_file = os.path.join(script_dir, "config_json_embedded.h")
    
    if not os.path.exists(json_file):
        print(f"❌ Config file not found: {json_file}")
        sys.exit(1)
    
    if json_to_c_string(json_file, header_file):
        print("🎉 Config embedding successful!")
        sys.exit(0)
    else:
        print("💥 Config embedding failed!")
        sys.exit(1)

if __name__ == "__main__":
    main()
