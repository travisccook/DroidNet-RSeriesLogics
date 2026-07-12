#!/usr/bin/env python3
"""
PlatformIO post-build script to copy .bin files to build folder
Creates uniquely named firmware files with board name and timestamp
"""

import os
import shutil
from datetime import datetime
Import("env")

def copy_bin_to_build(source, target, env):
    """Copy the generated .bin file to the build folder with a unique name"""
    
    # Get the environment name (board name)
    env_name = env.get("PIOENV")
    
    # Get project directory
    project_dir = env.get("PROJECT_DIR")
    
    # Create build directory path - go up one level to main project
    parent_dir = os.path.dirname(project_dir)
    build_dir = os.path.join(parent_dir, "build")
    
    # Ensure build directory exists
    os.makedirs(build_dir, exist_ok=True)
    
    # Get the source bin file path
    bin_file = str(target[0])
    
    # Create timestamp
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    
    # Create unique filename: RSeriesLogic_YYYYMMDD_HHMMSS.bin
    new_filename = f"RSeriesLogic_{timestamp}.bin"
    dest_path = os.path.join(build_dir, new_filename)
    
    # Copy the bin file
    if os.path.exists(bin_file):
        shutil.copy2(bin_file, dest_path)
        print(f"\nFirmware copied to: ../build/{new_filename}")
        
        # Also create a "latest" symlink for convenience
        latest_link = os.path.join(build_dir, "RSeriesLogic_latest.bin")
        if os.path.exists(latest_link):
            os.remove(latest_link)
        # Use relative path for the symlink
        os.symlink(new_filename, latest_link)
        print(f"Latest symlink updated: ../build/RSeriesLogic_latest.bin")
    else:
        print(f"\nWarning: Bin file not found at {bin_file}")

# Register the post-build action on the bin file
env.AddPostAction("$BUILD_DIR/firmware.bin", copy_bin_to_build)