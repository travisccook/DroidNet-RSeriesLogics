/**
 * Scan Line animation - Horizontal scanning beam
 * Creates a horizontal scanning line that moves up and down
 */

import { createAnimation } from '../core/animation-base.js';
import { DISPLAY_CONFIG, getColorFromPaletteWithHue } from '../core/config.js';

export default createAnimation({
    name: 'scan-line',
    displayName: 'Horizontal Scan',
    description: 'Horizontal scanning beam',
    category: 'effects',
    effectId: 20,
    interval: 50,
    
    init(params) {
        return {
            fldScanRow: 0,
            rldScanRow: 0,
            fldDirection: 1,
            rldDirection: 1,
            colorIndex: 0,
            colorTimer: 0
        };
    },
    
    frame(controller, state, params) {
        // Clear displays
        controller.clear();
        
        // Update color
        state.colorTimer++;
        if (state.colorTimer >= 20) {
            state.colorTimer = 0;
            state.colorIndex = (state.colorIndex + 1) % 10;
        }
        
        // Process FLD
        if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'FLD') {
            // Draw scan line with fade effect
            for (let row = 0; row < DISPLAY_CONFIG.fldRows; row++) {
                const distance = Math.abs(row - state.fldScanRow);
                let intensity = 0;
                
                if (distance === 0) {
                    intensity = 1.0;
                } else if (distance === 1) {
                    intensity = 0.5;
                } else if (distance === 2) {
                    intensity = 0.2;
                }
                
                if (intensity > 0) {
                    for (let col = 0; col < DISPLAY_CONFIG.fldCols; col++) {
                        const color = getColorFromPaletteWithHue(
                            params.fld.palette,
                            state.colorIndex,
                            params.fld.brightness * intensity,
                            params.fld.hue
                        );
                        controller.setLEDLogical('FLD', row, col, color);
                    }
                }
            }
            
            // Update scan position
            state.fldScanRow += state.fldDirection * (params.speed / 5);
            
            // Bounce at edges
            if (state.fldScanRow >= DISPLAY_CONFIG.fldRows - 1) {
                state.fldScanRow = DISPLAY_CONFIG.fldRows - 1;
                state.fldDirection = -1;
            } else if (state.fldScanRow <= 0) {
                state.fldScanRow = 0;
                state.fldDirection = 1;
            }
        }
        
        // Process RLD
        if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'RLD') {
            // Draw scan line with fade effect
            for (let row = 0; row < DISPLAY_CONFIG.rldRows; row++) {
                const distance = Math.abs(row - state.rldScanRow);
                let intensity = 0;
                
                if (distance === 0) {
                    intensity = 1.0;
                } else if (distance === 1) {
                    intensity = 0.3;
                }
                
                if (intensity > 0) {
                    for (let col = 0; col < DISPLAY_CONFIG.rldCols; col++) {
                        const color = getColorFromPaletteWithHue(
                            params.rld.palette,
                            state.colorIndex,
                            params.rld.brightness * intensity,
                            params.rld.hue
                        );
                        controller.setLEDLogical('RLD', row, col, color);
                    }
                }
            }
            
            // Update scan position - RLD moves faster due to fewer rows
            state.rldScanRow += state.rldDirection * (params.speed / 3);
            
            // Bounce at edges
            if (state.rldScanRow >= DISPLAY_CONFIG.rldRows - 1) {
                state.rldScanRow = DISPLAY_CONFIG.rldRows - 1;
                state.rldDirection = -1;
            } else if (state.rldScanRow <= 0) {
                state.rldScanRow = 0;
                state.rldDirection = 1;
            }
        }
        
        controller.render();
    }
});