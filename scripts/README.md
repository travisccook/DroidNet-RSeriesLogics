# RSeriesLogic Build Scripts

This directory contains build automation scripts for the RSeriesLogic project.

## Scripts

### validate_javascript.py
Extracts and validates JavaScript code embedded in C++ files before building.

**Features:**
- Extracts JavaScript from WebPages.h and WifiWebServer.h
- Validates syntax using Node.js
- Reports errors with file and line numbers
- Runs automatically before each build

**Requirements:**
- Python 3.x
- Node.js (for JavaScript syntax validation)

**Manual usage:**
```bash
cd RSeriesLogic
python3 scripts/validate_javascript.py
```

### pre_build_validate.py
PlatformIO pre-build hook that runs JavaScript validation.

### post_build.py
PlatformIO post-build hook that copies firmware to build directory.

## Adding Validation

To add JavaScript validation to other files:
1. Edit `validate_javascript.py`
2. Add the file path to `files_to_check` list
3. Adjust extraction patterns if needed

## Disabling Validation

To temporarily disable JavaScript validation, comment out the pre-build script in `platformio.ini`:
```ini
extra_scripts = 
    # pre:scripts/pre_build_validate.py
    post:scripts/post_build.py
```