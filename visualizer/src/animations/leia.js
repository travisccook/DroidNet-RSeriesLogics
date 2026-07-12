/**
 * Leia animation - Princess Leia hologram pattern
 * Creates a blue/white hologram shimmer effect with scan lines
 */

import { createAnimation } from '../core/animation-base.js';
import { DISPLAY_CONFIG } from '../core/config.js';

export default createAnimation({
    name: 'leia',
    displayName: 'Leia',
    description: 'Princess Leia hologram pattern',
    category: 'core',
    effectId: 3,
    interval: 40,
    
    init(params) {
        return {
            scanLine: 0,
            shimmerPhase: 0,
            staticNoise: new Array(DISPLAY_CONFIG.fldLedCount + DISPLAY_CONFIG.rldLedCount)
                .fill(0).map(() => Math.random())
        };
    },
    
    frame(controller, state, params) {
        // Process FLD
        if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'FLD') {
            for (let row = 0; row < DISPLAY_CONFIG.fldRows; row++) {
                for (let col = 0; col < DISPLAY_CONFIG.fldCols; col++) {
                    const index = row * DISPLAY_CONFIG.fldCols + col;
                    const color = calculateHologramColor(
                        row, col, state, index, 
                        params.fld.brightness, params.speed
                    );
                    controller.setLEDLogical('FLD', row, col, color);
                }
            }
        }
        
        // Process RLD
        if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'RLD') {
            for (let row = 0; row < DISPLAY_CONFIG.rldRows; row++) {
                for (let col = 0; col < DISPLAY_CONFIG.rldCols; col++) {
                    const index = DISPLAY_CONFIG.fldLedCount + 
                        row * DISPLAY_CONFIG.rldCols + col;
                    const color = calculateHologramColor(
                        row, col, state, index, 
                        params.rld.brightness, params.speed
                    );
                    controller.setLEDLogical('RLD', row, col, color);
                }
            }
        }
        
        // Update animation state
        state.scanLine = (state.scanLine + (params.speed / 5)) % 16;
        state.shimmerPhase = (state.shimmerPhase + 0.1) % (Math.PI * 2);
        
        // Occasionally regenerate some static
        for (let i = 0; i < 10; i++) {
            const idx = Math.floor(Math.random() * state.staticNoise.length);
            state.staticNoise[idx] = Math.random();
        }
        
        controller.render();
    }
});

function calculateHologramColor(row, col, state, noiseIndex, brightness, speed) {
    // Base blue color
    let r = 20;
    let g = 60;
    let b = 200;
    
    // Add scan line effect
    const scanDist = Math.abs(row - state.scanLine);
    if (scanDist < 2) {
        const scanIntensity = 1 - (scanDist / 2);
        r += 100 * scanIntensity;
        g += 140 * scanIntensity;
        b += 55 * scanIntensity;
    }
    
    // Add shimmer effect
    const shimmer = Math.sin(state.shimmerPhase + col * 0.3 + row * 0.5) * 0.3 + 0.7;
    
    // Add static noise
    const noise = state.staticNoise[noiseIndex];
    const noiseIntensity = noise > 0.9 ? noise : 0;
    
    // Combine effects
    r = Math.floor((r * shimmer + noiseIntensity * 255) * brightness / 255);
    g = Math.floor((g * shimmer + noiseIntensity * 255) * brightness / 255);
    b = Math.floor((b * shimmer + noiseIntensity * 255) * brightness / 255);
    
    // Clamp values
    r = Math.min(255, Math.max(0, r));
    g = Math.min(255, Math.max(0, g));
    b = Math.min(255, Math.max(0, b));
    
    return { r, g, b };
}