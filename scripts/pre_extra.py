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


# DroidNet WDP device announce: the build's commit, sent as "fw" (src/wdp/wdp_fork.h).
# The same short sha firmware/build-images.sh names the images by, plus "-dirty" when the tree
# has changes outside firmware/ (the prebuilt images there are outputs, not sources), or
# "unknown" when git cannot say. StringifyMacro is PlatformIO's helper for a string-valued
# define (docs: build_flags, "Stringification"); it adds the quotes and escapes.
def wdp_fw_version(cwd):
    def git(*args):
        result = subprocess.run(["git", *args], cwd=cwd, capture_output=True, text=True)
        if result.returncode != 0:
            raise RuntimeError(result.stderr.strip())
        return result.stdout.strip()

    try:
        sha = git("rev-parse", "--short=7", "HEAD")
        dirty = git("status", "--porcelain", "--", ".", ":(exclude)firmware")
    except Exception as e:
        print(f"Warning: WDP fw falls back to 'unknown' ({e})")
        return "unknown"
    return sha + "-dirty" if dirty else sha


wdp_fw = wdp_fw_version(project_dir)
print(f"WDP announce fw: {wdp_fw}")
env.Append(CPPDEFINES=[("WDP_FW", env.StringifyMacro(wdp_fw))])
