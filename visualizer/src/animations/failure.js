/**
 * Failure animation - System failure cascade effect
 * Displays a cascading red pattern from top to bottom
 */

import { createAnimation } from '../core/animation-base.js';
import { DISPLAY_CONFIG } from '../core/config.js';

export default createAnimation({
    name: 'failure',
    displayName: 'Failure',
    description: 'System failure cascade effect',
    category: 'core',
    effectId: 2,
    interval: 50,
    
    init(params) {
        return {
            row: 0,
            phase: 0, // 0 = cascading down, 1 = full red, 2 = flashing
            flashState: false,
            flashCount: 0,
            holdTimer: 0
        };
    },
    
    frame(controller, state, params) {
        if (state.phase === 0) {
            // Cascading phase
            if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'FLD') {
                // Clear FLD
                for (let r = 0; r < DISPLAY_CONFIG.fldRows; r++) {
                    for (let c = 0; c < DISPLAY_CONFIG.fldCols; c++) {
                        controller.setLEDLogical('FLD', r, c, {r: 0, g: 0, b: 0});
                    }
                }
                
                // Draw cascade
                for (let r = 0; r <= state.row && r < DISPLAY_CONFIG.fldRows; r++) {
                    for (let c = 0; c < DISPLAY_CONFIG.fldCols; c++) {
                        const brightness = params.fld.brightness;
                        const intensity = (r === state.row) ? 
                            Math.floor(brightness * 0.8) : brightness;
                        controller.setLEDLogical('FLD', r, c, 
                            {r: intensity, g: 0, b: 0});
                    }
                }
            }
            
            if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'RLD') {
                // Clear RLD
                for (let r = 0; r < DISPLAY_CONFIG.rldRows; r++) {
                    for (let c = 0; c < DISPLAY_CONFIG.rldCols; c++) {
                        controller.setLEDLogical('RLD', r, c, {r: 0, g: 0, b: 0});
                    }
                }
                
                // Draw cascade - RLD has fewer rows, so cascade faster
                const rldRow = Math.floor(state.row / 2);
                for (let r = 0; r <= rldRow && r < DISPLAY_CONFIG.rldRows; r++) {
                    for (let c = 0; c < DISPLAY_CONFIG.rldCols; c++) {
                        const brightness = params.rld.brightness;
                        const intensity = (r === rldRow) ? 
                            Math.floor(brightness * 0.8) : brightness;
                        controller.setLEDLogical('RLD', r, c, 
                            {r: intensity, g: 0, b: 0});
                    }
                }
            }
            
            // Advance cascade
            state.row++;
            
            // Check if cascade complete
            if (state.row >= DISPLAY_CONFIG.fldRows) {
                state.phase = 1;
                state.holdTimer = 20; // Hold full red briefly
            }
            
        } else if (state.phase === 1) {
            // Full red hold phase
            if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'FLD') {
                for (let r = 0; r < DISPLAY_CONFIG.fldRows; r++) {
                    for (let c = 0; c < DISPLAY_CONFIG.fldCols; c++) {
                        controller.setLEDLogical('FLD', r, c, 
                            {r: params.fld.brightness, g: 0, b: 0});
                    }
                }
            }
            
            if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'RLD') {
                for (let r = 0; r < DISPLAY_CONFIG.rldRows; r++) {
                    for (let c = 0; c < DISPLAY_CONFIG.rldCols; c++) {
                        controller.setLEDLogical('RLD', r, c, 
                            {r: params.rld.brightness, g: 0, b: 0});
                    }
                }
            }
            
            state.holdTimer--;
            if (state.holdTimer <= 0) {
                state.phase = 2;
            }
            
        } else if (state.phase === 2) {
            // Flashing phase
            const color = state.flashState ? 
                {r: 0, g: 0, b: 0} : 
                {r: params.fld.brightness, g: 0, b: 0};
            const rldColor = state.flashState ? 
                {r: 0, g: 0, b: 0} : 
                {r: params.rld.brightness, g: 0, b: 0};
            
            if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'FLD') {
                controller.fill('FLD', color);
            }
            
            if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'RLD') {
                controller.fill('RLD', rldColor);
            }
            
            // Toggle flash every few frames
            if (++state.flashCount >= 10) {
                state.flashState = !state.flashState;
                state.flashCount = 0;
            }
        }
        
        controller.render();
    }
});