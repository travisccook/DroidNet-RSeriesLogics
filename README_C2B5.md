# C2B5 RSeriesLogic

This README documents the C2B5-specific enhancements and modifications to the standard RSeriesLogic firmware.

## Overview

The C2B5 version of RSeriesLogic includes custom features and optimizations tailored for the C2B5 astromech droid build.

## Enhanced Features

### Simplified mDNS Hostname
- Default hostname: `RSeries.local`
- Automatic fallback to unique identifier if network conflict detected (RSeries-XXXX.local where XXXX is the last 4 characters of the board's MAC address)
- Maintains compatibility with custom hostname configuration

### Improved WiFi Initialization
- Smart AP mode detection after factory reset
- Automatic AP mode when no networks are configured
- Preserves user's preferred WiFi mode across reboots

### WiFi Management
- Automatic AP mode when no networks are configured
- Smart WiFi fallback with configurable timeout (10-300 seconds)
- Client/AP mode selector with persistence across reboots
- Editable network priorities for connection order
- Direct "Connect" button for immediate network switching
- Protection against client mode without saved networks
- Real-time network management with add/remove/prioritize functions
- WiFi status display showing connection details and saved networks
- Proper handling of SSIDs containing spaces and special characters
- **Automatic URL decoding of SSIDs** - Fixes issues where network names with spaces were stored as URL-encoded (e.g., "Imperial%20Archives" → "Imperial Archives")

## Configuration

### Factory Reset Behavior
- Command: `#LEZERO`
- Clears all stored preferences
- Automatically starts in Access Point mode for initial setup
- Default AP credentials maintained for easy access

### Network Access
- Web Interface: `http://RSeries.local` (when connected to same network)
- Access Point Mode: Connect to "RSeries" WiFi network
- Configure via web interface at `http://192.168.4.1`

## Compatibility

- Fully compatible with standard RSeriesLogic commands
- Supports all original MarcDuino commands
- Maintains compatibility with existing astromech control systems

## Modern UI (C2B5 UI)

### Overview
The C2B5 UI is a modern, responsive web interface designed to replace the legacy web interface. It features:
- Clean, light-themed design inspired by droidnet.local
- Mobile-friendly responsive layout
- Real-time status updates
- Improved WiFi management interface

### Access
- Click the "C2B5 UI" button from the classic interface
- Direct access: `http://RSeries.local/m/`

### Features
- **System Status**: Real-time display of WiFi connection status
- **WiFi Management**: Enhanced interface for network configuration
- **Firmware Info**: Displays current firmware filename for easy version tracking

## Development Files and Structure

### Folder Organization
The following folders contain C2B5-specific additions:

#### `/data/modern/`
Contains the modern UI HTML files:
- `index.html` - Main C2B5 UI landing page
- `wifi.html` - Enhanced WiFi configuration page

#### `/include/`
Auto-generated header files for compressed HTML:
- `modern_ui_html.h` - Master include file
- `modern_index_html.h` - Compressed index.html
- `modern_wifi_html.h` - Compressed wifi.html

#### `/scripts/`
Build and development scripts:
- `html_to_header.py` - Compresses HTML files for firmware inclusion
- `build_modern_ui.py` - Validates modern UI files
- `validate_javascript.py` - JavaScript validation tool
- `pre_extra.py` - PlatformIO pre-build script

#### `/tests/`
Test files for development (not included in firmware):
- `test_modern_ui.html` - Standalone test of modern UI
- `test_wifi_debug.html` - WiFi page debugging
- `test_modern_ui_puppeteer.js` - Automated browser testing

### Build Process
1. HTML files in `/data/modern/` are automatically compressed during build
2. Compressed files are stored as C++ headers in `/include/`
3. JavaScript validation runs before each build
4. Firmware filename is embedded for version tracking

### Debug Features
When troubleshooting, the firmware includes debug output:
- `[MODERN_UI]` - Route handling and content serving
- `[MODERN_UI_DEBUG]` - Periodic status messages (every 30 seconds)
- Includes firmware filename, free heap, and server status

## Future Enhancements

This document will be updated as new C2B5-specific features are added.