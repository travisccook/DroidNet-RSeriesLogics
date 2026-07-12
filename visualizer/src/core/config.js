/**
 * Configuration for RSeries Logic Display Visualizer
 * Contains LED mappings, color palettes, and display settings
 */

// LED Mappings - translates logical position to physical LED index
export const FLD_MAP = [
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

export const RLD_MAP = [
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,
     55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28,
     56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83,
    111,110,109,108,107,106,105,104,103,102,101,100, 99, 98, 97, 96, 95, 94, 93, 92, 91, 90, 89, 88, 87, 86, 85, 84
];

// Color value enum
export const ColorVal = {
    kDefault: 0,
    kRed: 1,
    kOrange: 2,
    kYellow: 3,
    kGreen: 4,
    kCyan: 5,
    kBlue: 6,
    kPurple: 7,
    kMagenta: 8,
    kPink: 9,
    kCustom: 10
};

// Hue values for each color (0-255 scale)
export const COLOR_HUE_MAP = [0, 0, 26, 42, 85, 128, 170, 202, 213, 228];

// Color palettes - each palette has 10 colors
export const PALETTES = [
    // Palette 0 - Front Default (varied)
    [ColorVal.kRed, ColorVal.kOrange, ColorVal.kYellow, ColorVal.kGreen, ColorVal.kCyan, 
     ColorVal.kBlue, ColorVal.kPurple, ColorVal.kMagenta, ColorVal.kPink, ColorVal.kDefault],
    
    // Palette 1 - Rear Default (cooler)
    [ColorVal.kGreen, ColorVal.kCyan, ColorVal.kBlue, ColorVal.kPurple, ColorVal.kMagenta,
     ColorVal.kGreen, ColorVal.kCyan, ColorVal.kBlue, ColorVal.kPurple, ColorVal.kDefault],
    
    // Palette 2 - Red/Orange/Yellow
    [ColorVal.kRed, ColorVal.kOrange, ColorVal.kYellow, ColorVal.kRed, ColorVal.kOrange,
     ColorVal.kYellow, ColorVal.kRed, ColorVal.kOrange, ColorVal.kYellow, ColorVal.kDefault],
    
    // Palette 3 - Green/Cyan
    [ColorVal.kGreen, ColorVal.kCyan, ColorVal.kGreen, ColorVal.kCyan, ColorVal.kGreen,
     ColorVal.kCyan, ColorVal.kGreen, ColorVal.kCyan, ColorVal.kGreen, ColorVal.kDefault],
    
    // Palette 4 - Blue/Purple
    [ColorVal.kBlue, ColorVal.kPurple, ColorVal.kBlue, ColorVal.kPurple, ColorVal.kBlue,
     ColorVal.kPurple, ColorVal.kBlue, ColorVal.kPurple, ColorVal.kBlue, ColorVal.kDefault],
    
    // Palette 5 - Custom (user defined)
    [ColorVal.kRed, ColorVal.kGreen, ColorVal.kBlue, ColorVal.kYellow, ColorVal.kCyan,
     ColorVal.kMagenta, ColorVal.kOrange, ColorVal.kPurple, ColorVal.kPink, ColorVal.kDefault]
];

// Display configuration
export const DISPLAY_CONFIG = {
    ledSize: 24,        // Size of each LED square in pixels
    ledSpacing: 1,      // Space between LEDs
    padding: 6,         // Canvas padding
    
    // FLD dimensions
    fldRows: 8,
    fldCols: 10,
    fldLedCount: 80,
    
    // RLD dimensions
    rldRows: 4,
    rldCols: 28,
    rldLedCount: 112,
    
    // Animation defaults
    defaultBrightness: 255,
    defaultSpeed: 5,
    defaultPalette: 0,
    defaultFade: 128,
    defaultDelay: 10,
    
    // Frame timing
    frameInterval: 10,  // milliseconds (100Hz like Arduino)
    
    // LED mappings
    FLD_MAP: FLD_MAP,
    RLD_MAP: RLD_MAP,
    
    // Board types
    boards: {
        FLD: {
            DEATHSTAR_40: { name: "DeathStar 40 LED", count: 80 },
            CURVED_40: { name: "Curved 40 LED", count: 80 },
            TEECES_V4: { name: "Teeces V4", count: 80 }
        },
        RLD: {
            IMAHARA_112: { name: "Imahara 112 LED", count: 112 },
            KENNY_96: { name: "Kenny 96 LED", count: 96 },
            TEECES_V4: { name: "Teeces V4", count: 80 }
        }
    }
};

// Animation sequences
export const SEQUENCES = {
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
    PLASMA: 25,
    SINELON: 26,
    METEOR: 27,
    SPARKLE: 28,
    CYCLON: 29,
    RANDOM: 99
};

// Color system helpers
export function hsvToRgb(h, s, v) {
    // h is 0-255, s is 0-255, v is 0-255
    h = (h / 255) * 360;
    s = s / 255;
    v = v / 255;
    
    const c = v * s;
    const x = c * (1 - Math.abs(((h / 60) % 2) - 1));
    const m = v - c;
    
    let r, g, b;
    
    if (h >= 0 && h < 60) {
        [r, g, b] = [c, x, 0];
    } else if (h >= 60 && h < 120) {
        [r, g, b] = [x, c, 0];
    } else if (h >= 120 && h < 180) {
        [r, g, b] = [0, c, x];
    } else if (h >= 180 && h < 240) {
        [r, g, b] = [0, x, c];
    } else if (h >= 240 && h < 300) {
        [r, g, b] = [x, 0, c];
    } else {
        [r, g, b] = [c, 0, x];
    }
    
    return {
        r: Math.round((r + m) * 255),
        g: Math.round((g + m) * 255),
        b: Math.round((b + m) * 255)
    };
}

// Get color from palette
export function getColorFromPalette(paletteNum, colorIndex, brightness = 255) {
    const palette = PALETTES[paletteNum] || PALETTES[0];
    const colorVal = palette[colorIndex % palette.length];
    let hue = COLOR_HUE_MAP[colorVal];
    
    // For default color, use a rainbow effect
    if (colorVal === ColorVal.kDefault) {
        hue = (colorIndex * 25) % 256;
    }
    
    return hsvToRgb(hue, 255, brightness);
}

// Get color from palette with hue adjustment
export function getColorFromPaletteWithHue(paletteNum, colorIndex, brightness = 255, hueAdjust = 0) {
    const palette = PALETTES[paletteNum] || PALETTES[0];
    const colorVal = palette[colorIndex % palette.length];
    let hue = COLOR_HUE_MAP[colorVal];
    
    // For default color, use a rainbow effect
    if (colorVal === ColorVal.kDefault) {
        hue = (colorIndex * 25) % 256;
    }
    
    // Apply hue adjustment
    hue = (hue + hueAdjust) % 256;
    
    return hsvToRgb(hue, 255, brightness);
}

// Get next color number with tweening
export function getNextColorNum(currentNum, speed) {
    // Colors 0-15 are forward, 16-30 are reverse (bizarro)
    let nextNum = currentNum;
    
    if (currentNum < 15) {
        nextNum = currentNum + (speed / 10);
        if (nextNum > 15) nextNum = 15;
    } else if (currentNum < 30) {
        nextNum = currentNum + (speed / 10);
        if (nextNum > 30) nextNum = 0;
    }
    
    return Math.floor(nextNum);
}

// Export all configurations
export default {
    FLD_MAP,
    RLD_MAP,
    ColorVal,
    COLOR_HUE_MAP,
    PALETTES,
    DISPLAY_CONFIG,
    SEQUENCES,
    hsvToRgb,
    getColorFromPalette,
    getNextColorNum,
    ledSize: DISPLAY_CONFIG.ledSize,
    ledSpacing: DISPLAY_CONFIG.ledSpacing,
    padding: DISPLAY_CONFIG.padding
};