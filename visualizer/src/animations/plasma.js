/**
 * Plasma animation - Plasma field effect
 * Creates a flowing plasma effect using sine wave interference patterns
 */

import { createAnimation } from '../core/animation-base.js';
import { DISPLAY_CONFIG, hsvToRgb } from '../core/config.js';

export default createAnimation({
    name: 'plasma',
    displayName: 'Plasma',
    description: 'Flowing plasma field effect',
    category: 'effects',
    effectId: 25,
    interval: 50,
    
    init(params) {
        return {
            time: 0,
            hueShift: 0
        };
    },
    
    frame(controller, state, params) {
        // Process FLD
        if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'FLD') {
            for (let row = 0; row < DISPLAY_CONFIG.fldRows; row++) {
                for (let col = 0; col < DISPLAY_CONFIG.fldCols; col++) {
                    const color = calculatePlasmaColor(
                        col, row, 
                        DISPLAY_CONFIG.fldCols, DISPLAY_CONFIG.fldRows,
                        state.time, params.fld.brightness, 
                        params.fld.hue + state.hueShift
                    );
                    controller.setLEDLogical('FLD', row, col, color);
                }
            }
        }
        
        // Process RLD
        if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'RLD') {
            for (let row = 0; row < DISPLAY_CONFIG.rldRows; row++) {
                for (let col = 0; col < DISPLAY_CONFIG.rldCols; col++) {
                    const color = calculatePlasmaColor(
                        col, row,
                        DISPLAY_CONFIG.rldCols, DISPLAY_CONFIG.rldRows,
                        state.time * 1.2, params.rld.brightness,
                        params.rld.hue + state.hueShift
                    );
                    controller.setLEDLogical('RLD', row, col, color);
                }
            }
        }
        
        // Update animation state
        state.time += 0.05 * (params.speed / 5);
        state.hueShift = (state.hueShift + 1) % 256;
        
        controller.render();
    }
});

function calculatePlasmaColor(x, y, width, height, time, brightness, hueOffset) {
    // Normalize coordinates
    const nx = x / width;
    const ny = y / height;
    
    // Create multiple sine wave patterns
    const wave1 = Math.sin(nx * 10 + time);
    const wave2 = Math.sin(10 * (nx * Math.sin(time / 2) + ny * Math.cos(time / 3)) + time);
    const cx = nx + 0.5 * Math.sin(time / 5);
    const cy = ny + 0.5 * Math.cos(time / 3);
    const wave3 = Math.sin(Math.sqrt(50 * (cx * cx + cy * cy) + 1) + time);
    
    // Combine waves
    const value = (wave1 + wave2 + wave3) / 3;
    
    // Convert to hue (0-255 range)
    const hue = ((value + 1) * 128 + hueOffset) % 256;
    
    // High saturation, variable brightness
    const saturation = 255;
    const adjustedBrightness = brightness * (0.7 + 0.3 * ((value + 1) / 2));
    
    return hsvToRgb(hue, saturation, adjustedBrightness);
}