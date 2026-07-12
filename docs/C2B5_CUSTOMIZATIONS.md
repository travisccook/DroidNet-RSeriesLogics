# C2B5 RSeries Logic Engine

This is a customized fork of [reeltwo/WLogicEngine32](https://github.com/reeltwo/WLogicEngine32) with enhanced features for the C2B5 astromech droid control system.

## Key Differences from Stock RSeries

### 1. Modern Web Interface
Access a cleaner, mobile-friendly interface at `http://192.168.4.1/m/`
- **Enhanced WiFi Management**: Intuitive network configuration with priority-based connections
- **Responsive Design**: Works seamlessly on phones, tablets, and desktops
- **No Extra Steps**: HTML is compressed and embedded in firmware - just flash and go

### 2. Multi-Network WiFi Support
- Save multiple WiFi networks with priority levels (1-10)
- Automatically connects to the strongest available saved network
- Falls back to AP mode when no networks are available
- Easy network management through the Modern UI

### 3. Unified Build System
- All components share a single `platformio.ini` in the project root
- Consistent configuration across all boards
- Simplified build commands for all targets

## Installation

1. **Clone this repository**:
   ```bash
   git clone https://github.com/your-repo/C2B5.git
   cd C2B5/RSeriesLogic
   ```

2. **Build and upload**:
   ```bash
   pio run -e wlogicengine32 --target upload
   ```

## Usage

### Accessing the Web Interface

After powering on, connect to the WiFi access point:
- **SSID**: RSeries (default)
- **Password**: Astromech (default)

Then navigate to:
- **Classic Interface**: `http://192.168.4.1/`
- **Modern Interface**: `http://192.168.4.1/m/` (recommended)

### WiFi Configuration

1. Go to the Modern UI: `http://192.168.4.1/m/`
2. Click on "WiFi Settings"
3. Add your home network(s) with appropriate priorities
4. The device will automatically connect on next boot

### Switching Between AP and Client Mode

The device intelligently manages WiFi modes:
- **Client Mode**: Connects to saved networks when available
- **AP Mode**: Creates a hotspot when no networks are found or connection fails
- Configure the timeout and AP settings in the WiFi page

## Hardware Compatibility

This firmware is designed for the WReactor32 board but is compatible with standard ESP32 development boards. The Modern UI features work with:
- ESP32 DevKit
- WReactor32 (2020-2023 versions)
- Any ESP32 with at least 4MB flash

## What's Included

- **Stock RSeries Features**: All original logic display patterns and controls
- **Modern WiFi Management**: Enhanced network configuration
- **API Endpoints**: JSON APIs for integration with other systems
- **Compressed Web UI**: Efficient storage using gzip compression

## Upstream Compatibility

This fork maintains compatibility with the upstream RSeries protocol. You can:
- Use standard MarcDuino commands
- Connect existing controllers
- Merge upstream updates (excluding WebPages.h)

## Support

For C2B5-specific features and issues, please use our issue tracker. For general RSeries questions, refer to the [original documentation](https://github.com/reeltwo/WLogicEngine32).

---

*C2B5 Customizations by [your name/team] • Based on RSeries by Reeltwo*