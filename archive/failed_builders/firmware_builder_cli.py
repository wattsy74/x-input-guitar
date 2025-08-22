#!/usr/bin/env python3
"""
CLI version of firmware builder that doesn't require tkinter
"""
import argparse
import firmware_builder

def main():
    parser = argparse.ArgumentParser(description='BGG Firmware Builder CLI')
    parser.add_argument('action', choices=['compile', 'upload'], help='Action to perform')
    parser.add_argument('--type', choices=['X-Input', 'HID'], default='X-Input', help='Controller type')
    parser.add_argument('--port', help='Serial port (required for upload, e.g., COM3)')
    
    args = parser.parse_args()
    
    try:
        if args.action == 'compile':
            print(f"Compiling {args.type} firmware...")
            result = firmware_builder.compile_firmware(args.type)
            print(result)
        elif args.action == 'upload':
            if not args.port:
                print("Error: --port is required for upload")
                return 1
            print(f"Uploading {args.type} firmware to {args.port}...")
            result = firmware_builder.upload_firmware(args.type, args.port)
            print(result)
    except Exception as e:
        print(f"Error: {e}")
        return 1
    
    return 0

if __name__ == '__main__':
    exit(main())
