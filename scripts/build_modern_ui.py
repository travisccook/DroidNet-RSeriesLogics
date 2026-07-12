#!/usr/bin/env python3
"""
Build script for Modern UI files
This script prepares the data folder for SPIFFS upload
"""

import os
import sys

def check_modern_ui_files():
    """Check if modern UI files exist in data folder"""
    data_dir = os.path.join(os.path.dirname(os.path.dirname(__file__)), 'data', 'modern')
    
    if not os.path.exists(data_dir):
        print(f"ERROR: Modern UI directory not found: {data_dir}")
        return False
    
    required_files = ['index.html', 'wifi.html']
    missing_files = []
    
    for file in required_files:
        file_path = os.path.join(data_dir, file)
        if not os.path.exists(file_path):
            missing_files.append(file)
        else:
            size = os.path.getsize(file_path)
            print(f"✓ {file} ({size} bytes)")
    
    if missing_files:
        print(f"ERROR: Missing files: {', '.join(missing_files)}")
        return False
    
    return True

def main():
    """Main build function"""
    print("Modern UI Build Script")
    print("=" * 50)
    
    if not check_modern_ui_files():
        sys.exit(1)
    
    print("\nModern UI files are ready for SPIFFS upload!")
    print("\nTo upload to device:")
    print("  pio run --target uploadfs")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())