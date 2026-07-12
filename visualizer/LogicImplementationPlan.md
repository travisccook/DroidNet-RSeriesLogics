# RSeriesLogic Visualizer Implementation Plan

## Overview
This document provides a comprehensive implementation plan for creating a web-based visualizer for the RSeriesLogic Arduino project. The visualizer will simulate the Front Logic Display (FLD) and Rear Logic Display (RLD) animations without requiring physical hardware.

## Technical Specifications

### Supported Hardware Configurations

#### Front Logic Display (FLD)
- **Board Type**: RSeries 40 LED (DeathStar/Curved)
- **Physical Layout**: 2 PCBs with 40 LEDs each
- **Matrix Dimensions**: 8 rows × 10 columns
- **Total LEDs**: 80
- **LED Type**: SK6812 (RGBW capable, but we'll use RGB mode)
- **Data Pin**: Configurable (default: pin 15 on ESP32)

#### Rear Logic Display (RLD)
- **Board Type**: Imahara 112 LED (Grant Imahara commemorative)
- **Physical Layout**: Single PCB
- **Matrix Dimensions**: 4 rows × 28 columns
- **Total LEDs**: 112
- **LED Type**: SK9822 (APA102 compatible with clock pin)
- **Data Pin**: 33, Clock Pin: 32 (ESP32 defaults)

### LED Mapping Arrays

The physical LEDs are wired in a serpentine pattern. The mapping arrays translate logical positions to physical LED indices:

#### FLD Mapping (80 LEDs)
```javascript
const FLD_MAP = [
     0,  1,  2,  3,  4,  5,  6,  7,
    15, 14, 13, 12, 11, 10,  9,  8,
    16, 17, 18, 19, 20, 21, 22, 23,
    31, 30, 29, 28, 27, 26, 25, 24,
    32, 33, 34, 35, 36, 37, 38, 39,
    79, 78, 77, 76, 75, 74, 73, 72,
    64, 65, 66, 67, 68, 69, 70, 71,
    63, 62, 61, 60, 59, 58, 57, 56,
    48, 49, 50, 51, 52, 53, 54, 55,
    47, 46, 45, 44, 43, 42, 41, 40
];
```

#### RLD Mapping (112 LEDs)
```javascript
const RLD_MAP = [
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,
     55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28,
     56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83,
    111,110,109,108,107,106,105,104,103,102,101,100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87, 86, 85, 84
];
```

## Color System

### HSV Color Model
The LogicEngine uses HSV (Hue, Saturation, Value) for smooth color transitions:
- **Hue**: 0-255 (mapped from 0-360 degrees)
- **Saturation**: 0-255 (0 = white, 255 = full color)
- **Value**: 0-255 (brightness)

### Color Palettes
Six built-in palettes (PAL_COUNT = 6):
1. **Palette 0**: Front default (varied colors)
2. **Palette 1**: Rear default (cooler tones)
3. **Palette 2**: Red/Orange/Yellow
4. **Palette 3**: Green/Cyan
5. **Palette 4**: Blue/Purple
6. **Palette 5**: Custom/User defined

### Color Values (ColorVal enum)
```javascript
const ColorVal = {
    kDefault: 0,
    kRed: 1,
    kOrange: 2,
    kYellow: 3,
    kGreen: 4,
    kCyan: 5,
    kBlue: 6,
    kPurple: 7,
    kMagenta: 8,
    kPink: 9
};
```

### Color Mapping to Hue
```javascript
const colorHueMap = [0, 0, 26, 42, 85, 128, 170, 202, 213, 228];
```

## Animation System

### Core Parameters
- **Fade**: Speed of color transitions (0-255)
- **Delay**: Pause between color changes (0-255)
- **Hue**: Global hue adjustment (0-255)
- **Brightness**: Global brightness (0-255)
- **Speed Scale**: Animation speed modifier (0-9)
- **Duration**: Time-limited effects (0-99 seconds)

### Animation Sequences
Each animation is identified by a sequence number:

```javascript
const Sequences = {
    NORMAL: 0,
    ALARM: 1,
    FAILURE: 2,
    LEIA: 3,
    MARCH: 4,
    SOLIDCOLOR: 5,
    FLASHCOLOR: 6,
    FLIPFLOPCOLOR: 7,
    FLIPFLOPALTCOLOR: 8,
    COLORSWAP: 9,
    RAINBOW: 10,
    REDALERT: 11,
    MICBRIGHT: 12,
    MICRAINBOW: 13,
    LIGHTSOUT: 14,
    TEXT: 15,
    TEXTSCROLLLEFT: 16,
    TEXTSCROLLRIGHT: 17,
    TEXTSCROLLUP: 18,
    ROAMINGPIXEL: 19,
    HORIZONTALSCANLINE: 20,
    VERTICALSCANLINE: 21,
    FIRE: 22,
    PSICOLORWIPE: 23,
    PULSE: 24,
    RANDOM: 99
};
```

### Animation State Management
Each LED maintains:
```javascript
class LEDStatus {
    constructor() {
        this.colorNum = 0;    // Current color index (0-30)
        this.colorPause = 0;  // Frames until next color
    }
}
```

### Color Number System
- Total colors: 16 key colors
- With transitions: 31 total states
  - 0-15: Colors fading up
  - 16-30: Colors fading back down ("bizarro" colors)

## Text Rendering System

### Font Specifications
Two font sizes are supported:

#### 5-Point Font (FLD)
- Character width: 4 pixels
- Character height: 5 pixels
- Spacing: 1 pixel between characters

#### 4-Point Font (RLD)
- Character width: 3 pixels
- Character height: 4 pixels
- Spacing: 1 pixel between characters
- Stagger support for even/odd rows

### Supported Characters
- Full ASCII alphabet (A-Z, a-z)
- Numbers (0-9)
- Common symbols and punctuation
- Aurebesh alphabet (Star Wars)

## Implementation Steps

### Phase 1: Core Infrastructure
1. **Project Setup**
   - Create directory structure
   - Initialize package.json with dependencies
   - Set up esbuild configuration

2. **LED Controller**
   ```javascript
   class LEDController {
       constructor() {
           this.fldLeds = new Array(80);
           this.rldLeds = new Array(112);
           this.fldStatus = new Array(80);
           this.rldStatus = new Array(112);
       }
       
       setLED(display, index, color) { }
       clear(display) { }
       mapLogicalToPhysical(display, row, col) { }
   }
   ```

3. **Color System**
   ```javascript
   class ColorSystem {
       constructor() {
           this.palettes = this.initializePalettes();
       }
       
       hsvToRgb(h, s, v) { }
       calculateAllColors(paletteNum, brightness) { }
       getNextColor(currentColor, fade) { }
   }
   ```

### Phase 2: Animation Engine
1. **Base Animation Class**
   ```javascript
   class Animation {
       constructor(name, sequence) {
           this.name = name;
           this.sequence = sequence;
           this.interval = 50; // milliseconds
       }
       
       init(controller) { }
       frame(controller, state) { }
       cleanup() { }
   }
   ```

2. **Core Animations**
   - Normal: Random twinkling effect
   - Alarm: Fast red/yellow flashing
   - Failure: Downward red cascade
   - Leia: Blue/white pattern
   - March: Imperial March sync

3. **Effect Animations**
   - Solid Color: Static color display
   - Flash: On/off blinking
   - Rainbow: Color wheel rotation
   - Fire: Flame simulation
   - Text Scroll: Message display

### Phase 3: User Interface
1. **HTML Structure**
   ```html
   <div class="display-container">
       <div class="fld-display">
           <div class="led-matrix" id="fld-matrix"></div>
       </div>
       <div class="rld-display">
           <div class="led-matrix" id="rld-matrix"></div>
       </div>
   </div>
   ```

2. **Control Panel**
   - Animation selection buttons
   - Parameter sliders (brightness, speed, etc.)
   - Text input for scrolling messages
   - Color picker for solid colors

3. **CSS Styling**
   - LED appearance with glow effects
   - Responsive layout
   - Dark theme to match hardware

### Phase 4: Advanced Features
1. **Timing System**
   - Frame-based animation (10ms ticks)
   - Duration limits for effects
   - Smooth transitions between animations

2. **State Persistence**
   - Save/load animation settings
   - Remember user preferences
   - Export/import configurations

3. **Performance Optimization**
   - RequestAnimationFrame usage
   - Efficient DOM updates
   - WebGL rendering (optional)

## Code Organization

### Directory Structure
```
visualizer/
├── src/
│   ├── index.html
│   ├── styles.css
│   ├── app.js
│   ├── core/
│   │   ├── led-controller.js
│   │   ├── color-system.js
│   │   ├── config.js
│   │   └── fonts.js
│   ├── animations/
│   │   ├── base-animation.js
│   │   ├── normal.js
│   │   ├── alarm.js
│   │   ├── text-scroll.js
│   │   └── [other animations]
│   └── ui/
│       ├── controls.js
│       └── display.js
├── dist/
├── build.js
├── package.json
└── README.md
```

### Build System
Using esbuild for bundling:
```javascript
// build.js
const esbuild = require('esbuild');

esbuild.build({
    entryPoints: ['src/app.js'],
    bundle: true,
    outfile: 'dist/bundle.js',
    sourcemap: true,
    format: 'esm'
});
```

## Testing Strategy

### Unit Tests
- Color conversion accuracy
- LED mapping correctness
- Animation state management
- Text rendering verification

### Visual Tests
- Animation smoothness
- Color accuracy
- Timing precision
- Cross-browser compatibility

### Performance Benchmarks
- Frame rate maintenance (60 FPS target)
- Memory usage monitoring
- CPU utilization checks

## Future Enhancements

### Planned Features
1. Audio-reactive animations (microphone input)
2. Custom animation editor
3. Arduino code export
4. Network synchronization
5. Mobile app companion

### Integration Options
1. Serial communication with actual hardware
2. MQTT/WebSocket live control
3. RESTful API for remote management
4. Integration with droid control systems

## References

### Source Files
- `/lib/Reeltwo/src/dome/LogicEngine.h` - Core logic engine implementation
- `/src/main.cpp` - Hardware initialization and configuration
- `/logic-sequences.h` - Animation sequence definitions
- `/WebPages.h` - Web interface implementation

### External Documentation
- FastLED Library: https://fastled.io/
- SK6812 Datasheet: LED specifications
- SK9822 Datasheet: APA102 compatible LEDs
- R-Series Builders Forum: Community resources

## Conclusion

This implementation plan provides a complete roadmap for creating a functional RSeriesLogic visualizer. The modular architecture allows for easy extension and modification while maintaining compatibility with the Arduino hardware implementation.

Next steps:
1. Create the basic project structure
2. Implement the LED controller with proper mapping
3. Port the color system from the Arduino code
4. Implement core animations one by one
5. Build the user interface
6. Test and optimize performance