#!/usr/bin/env python3
"""
PlatformIO Pre-extra Script
Runs before anything else
"""

Import("env")
import subprocess
import sys
import os
from datetime import datetime

# Run JavaScript validation immediately
print("Running JavaScript validation...")

project_dir = env.get("PROJECT_DIR")
validate_script = os.path.join(project_dir, "scripts", "validate_javascript.py")

if os.path.exists(validate_script):
    try:
        result = subprocess.run(
            [sys.executable, validate_script],
            cwd=project_dir
        )
        
        if result.returncode != 0:
            print("❌ Build aborted due to JavaScript validation errors")
            sys.exit(1)
            
    except Exception as e:
        print(f"Warning: Could not run JavaScript validation: {e}")

# Generate compressed HTML headers
html_script = os.path.join(project_dir, "scripts", "html_to_header.py")
if os.path.exists(html_script):
    print("\nGenerating compressed HTML headers...")
    try:
        result = subprocess.run(
            [sys.executable, html_script],
            cwd=project_dir,
            capture_output=True,
            text=True
        )
        if result.returncode == 0:
            print("✓ HTML headers generated successfully")
        else:
            print(f"Warning: HTML header generation failed: {result.stderr}")
    except Exception as e:
        print(f"Warning: Could not generate HTML headers: {e}")

# Add firmware filename as a build flag
timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
firmware_filename = f"RSeriesLogic_{timestamp}.bin"
env.Append(CPPDEFINES=[("FIRMWARE_FILENAME", f'\\"{firmware_filename}\\"')])