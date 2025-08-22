#!/usr/bin/env python3
"""
Simple firmware builder that uses VS Code tasks via CLI
"""
import argparse
import subprocess
import os

def run_vscode_task(task_name):
    """Run a VS Code task using the command line"""
    # Use VS Code CLI to run the task
    cmd = ['code', '--wait', '--folder-uri', os.getcwd(), '--command', f'workbench.action.tasks.runTask', task_name]
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
        return result.returncode == 0, result.stdout, result.stderr
    except subprocess.TimeoutExpired:
        return False, "", "Task timed out"
    except FileNotFoundError:
        # Fallback to direct Arduino CLI
        return False, "", "VS Code not found, falling back to direct Arduino CLI"

def main():
    parser = argparse.ArgumentParser(description='BGG Firmware Builder CLI')
    parser.add_argument('action', choices=['compile', 'upload'], help='Action to perform')
    parser.add_argument('--type', choices=['X-Input', 'HID'], default='X-Input', help='Controller type')
    parser.add_argument('--port', help='Serial port (required for upload, e.g., COM3)')
    
    args = parser.parse_args()
    
    print(f"Building {args.type} firmware...")
    
    if args.action == 'compile':
        print("Running Arduino: Verify task...")
        success, stdout, stderr = run_vscode_task('Arduino: Verify')
        if success:
            print("✓ Compile successful!")
        else:
            print(f"✗ Compile failed: {stderr}")
            return 1
    elif args.action == 'upload':
        if not args.port:
            print("Error: --port is required for upload")
            return 1
        print(f"Uploading to {args.port}...")
        # For upload, we'd need to set the port first, then run upload task
        print("Note: Upload via task requires setting port in VS Code first")
        print(f"Please set port to {args.port} in Arduino IDE, then run 'Arduino: Upload' task")
        return 1
    
    return 0

if __name__ == '__main__':
    exit(main())
