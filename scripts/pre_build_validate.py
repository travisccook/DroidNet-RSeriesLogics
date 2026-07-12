#!/usr/bin/env python3
"""
PlatformIO Pre-build Script
Validates JavaScript before building
"""

Import("env")
import subprocess
import sys
import os

def validate_javascript_prebuild(source, target, env):
    """Run JavaScript validation before build"""
    print("Running JavaScript validation...")
    
    # Get the path to the validation script
    project_dir = env.get("PROJECT_DIR")
    validate_script = os.path.join(project_dir, "scripts", "validate_javascript.py")
    
    if not os.path.exists(validate_script):
        print("Warning: JavaScript validation script not found")
        return
        
    try:
        # Run the validation script
        result = subprocess.run(
            [sys.executable, validate_script],
            cwd=project_dir,
            capture_output=True,
            text=True
        )
        
        # Print the output
        if result.stdout:
            print(result.stdout)
        if result.stderr:
            print(result.stderr, file=sys.stderr)
            
        # If validation failed, stop the build
        if result.returncode != 0:
            print("❌ Build aborted due to JavaScript validation errors")
            env.Exit(1)
            
    except Exception as e:
        print(f"Warning: Could not run JavaScript validation: {e}")

# Register the pre-build script to run before compilation
env.AddPreAction("$BUILD_DIR/src/main.cpp.o", validate_javascript_prebuild)