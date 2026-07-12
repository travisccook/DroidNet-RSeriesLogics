/**
 * Fire animation - Flame simulation effect
 */

import { createAnimation } from '../core/animation-base.js';
import { hsvToRgb, DISPLAY_CONFIG } from '../core/config.js';

export default createAnimation({
    name: 'fire',
    displayName: 'Fire',
    description: 'Realistic flame simulation',
    category: 'effects',
    effectId: 22,
    interval: 50,
    
    init(params) {
        return {
            // Heat map for each display
            fldHeat: new Array(DISPLAY_CONFIG.fldLedCount).fill(0),
            rldHeat: new Array(DISPLAY_CONFIG.rldLedCount).fill(0),
            brightness: params.brightness || 255,
            cooling: 55,
            sparking: 120
        };
    },
    
    frame(controller, state, params) {
        state.brightness = params.brightness || state.brightness;
        
        // Update FLD
        if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'FLD') {
            updateFire(controller, 'FLD', state.fldHeat, state, 
                DISPLAY_CONFIG.fldCols, DISPLAY_CONFIG.fldRows);
        }
        
        // Update RLD
        if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'RLD') {
            updateFire(controller, 'RLD', state.rldHeat, state,
                DISPLAY_CONFIG.rldCols, DISPLAY_CONFIG.rldRows);
        }
        
        controller.render();
    }
});

function updateFire(controller, display, heat, state, cols, rows) {
    // Cool down every cell a little
    for (let i = 0; i < heat.length; i++) {
        const cooldown = Math.random() * ((state.cooling * 10) / heat.length) + 2;
        heat[i] = Math.max(0, heat[i] - cooldown);
    }
    
    // Heat from each cell drifts up and diffuses slightly
    for (let col = 0; col < cols; col++) {
        for (let row = rows - 1; row >= 2; row--) {
            const idx = controller.getPhysicalIndex(display, row, col);
            const below1 = controller.getPhysicalIndex(display, row - 1, col);
            const below2 = controller.getPhysicalIndex(display, row - 2, col);
            
            if (idx !== -1 && below1 !== -1 && below2 !== -1) {
                heat[idx] = (heat[below1] + heat[below2] + heat[below2]) / 3;
            }
        }
    }
    
    // Randomly ignite new sparks at the bottom
    for (let col = 0; col < cols; col++) {
        if (Math.random() * 255 < state.sparking) {
            const sparkRow = Math.floor(Math.random() * 2); // Bottom 2 rows
            const idx = controller.getPhysicalIndex(display, sparkRow, col);
            if (idx !== -1) {
                heat[idx] = Math.min(255, heat[idx] + Math.random() * 95 + 160);
            }
        }
    }
    
    // Convert heat to colors
    for (let i = 0; i < heat.length; i++) {
        const temperature = heat[i];
        let color;
        
        // Scale heat down for display
        const t192 = Math.round((temperature / 255) * 191);
        
        // Calculate ramp up from dark red to bright yellow
        if (t192 < 64) {
            // Dark red to red
            color = { r: t192 * 4, g: 0, b: 0 };
        } else if (t192 < 128) {
            // Red to orange
            const t = (t192 - 64) * 4;
            color = { r: 255, g: t, b: 0 };
        } else {
            // Orange to yellow
            const t = (t192 - 128) * 4;
            color = { r: 255, g: 255, b: t };
        }
        
        // Apply brightness
        color.r = Math.floor(color.r * state.brightness / 255);
        color.g = Math.floor(color.g * state.brightness / 255);
        color.b = Math.floor(color.b * state.brightness / 255);
        
        controller.setLED(display, i, color);
    }
}