# RSeriesLogic Visualizer Integration Guide

## Overview

This visualizer provides a web-based implementation for testing and developing animations that will run on the RSeries Logic Display hardware (ESP32-based).

## Mapping to Arduino Implementation

### Display Configurations

The visualizer supports the same display configurations as the Arduino code:

- **Front Logic Display (FLD)**:
  - DeathStar configuration: 8x10 matrix, 80 LEDs
  - Matches `LogicEngineDeathStarFLD` in Arduino code

- **Rear Logic Display (RLD)**:
  - DeathStar configuration: 24x4 matrix, 96 LEDs  
  - Matches `LogicEngineDeathStarRLD` in Arduino code

### Effect IDs

The visualizer uses the same effect IDs as defined in `LogicEngineDefaults`:

```javascript
// JavaScript (visualizer)
export const effects = {
    NORMAL: 0,
    ALARM: 1,
    FAILURE: 2,
    LEIA: 3,
    MARCH: 4,
    // ... etc
};
```

```cpp
// C++ (Arduino)
static constexpr byte NORMAL = 0;
static constexpr byte ALARM = 1;
static constexpr byte FAILURE = 2;
static constexpr byte LEIA = 3;
static constexpr byte MARCH = 4;
// ... etc
```

### Color Palettes

The visualizer implements the same 6 color palettes as the hardware:
- Palette 0: Front default (blue/purple)
- Palette 1: Rear default (green/yellow/red)
- Palette 2: Monotone (black to red)
- Palette 3: Dual color (red and yellow)
- Palette 4: Blue and red
- Palette 5: Yellow and green

### Porting Animations

To port a visualizer animation to Arduino:

1. **JavaScript animation structure**:
```javascript
export default {
    name: 'myEffect',
    interval: 100,
    init() {
        return { position: 0 };
    },
    frame(controller, state) {
        controller.setPixel(state.position, 0, 'blue', 'FLD');
        state.position = (state.position + 1) % 8;
    }
};
```

2. **Arduino implementation**:
```cpp
// In logic-sequences.h or similar
case MY_EFFECT:
    if (frameCounter % 10 == 0) {  // interval equivalent
        static byte position = 0;
        clear();
        setPixel(position, 0, BLUE);
        position = (position + 1) % 8;
    }
    break;
```

### Key Differences

1. **State Management**: 
   - Visualizer: State object passed to frame()
   - Arduino: Static variables or class members

2. **Timing**:
   - Visualizer: setInterval with milliseconds
   - Arduino: Frame counter with logic in animate()

3. **Color Handling**:
   - Visualizer: Color names ('red', 'blue', etc.)
   - Arduino: HSV values via setHSV()

## Testing Workflow

1. Develop and test animations in the visualizer
2. Verify visual appearance and timing
3. Port logic to Arduino C++ code
4. Test on actual hardware

## Web Interface Integration

The existing web interface at `/web-visualizer/` can be enhanced to communicate with this new modular visualizer for live preview of hardware state.