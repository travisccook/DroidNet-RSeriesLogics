/**
 * Flip Flop Alt Color animation - Alternating color pattern variation
 * Creates a flip-flop effect with alternating colors between odd/even LEDs
 */

import { createAnimation } from '../core/animation-base.js';
import { DISPLAY_CONFIG, getColorFromPaletteWithHue } from '../core/config.js';

export default createAnimation({
    name: 'flip-flop-alt',
    displayName: 'Flip Flop Alt',
    description: 'Alternating flip-flop color pattern',
    category: 'patterns',
    effectId: 8,
    interval: 100,
    
    init(params) {
        return {
            flipState: false,
            colorIndex1: 0,
            colorIndex2: 4,
            timer: 0
        };
    },
    
    frame(controller, state, params) {
        // Update timer based on speed
        state.timer++;
        const flipInterval = Math.max(1, 10 - params.speed);
        
        if (state.timer >= flipInterval) {
            state.timer = 0;
            state.flipState = !state.flipState;
            
            // Advance colors
            state.colorIndex1 = (state.colorIndex1 + 1) % 10;
            state.colorIndex2 = (state.colorIndex2 + 1) % 10;
        }
        
        // Process FLD
        if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'FLD') {
            for (let row = 0; row < DISPLAY_CONFIG.fldRows; row++) {
                for (let col = 0; col < DISPLAY_CONFIG.fldCols; col++) {
                    // Create checkerboard pattern
                    const isEven = ((row + col) % 2) === 0;
                    const useFirstColor = state.flipState ? isEven : !isEven;
                    
                    const colorIndex = useFirstColor ? state.colorIndex1 : state.colorIndex2;
                    const color = getColorFromPaletteWithHue(
                        params.fld.palette,
                        colorIndex,
                        params.fld.brightness,
                        params.fld.hue
                    );
                    
                    controller.setLEDLogical('FLD', row, col, color);
                }
            }
        }
        
        // Process RLD - Different pattern for variety
        if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'RLD') {
            for (let row = 0; row < DISPLAY_CONFIG.rldRows; row++) {
                for (let col = 0; col < DISPLAY_CONFIG.rldCols; col++) {
                    // Vertical stripes instead of checkerboard
                    const isEvenCol = (col % 4) < 2;
                    const useFirstColor = state.flipState ? isEvenCol : !isEvenCol;
                    
                    const colorIndex = useFirstColor ? state.colorIndex1 : state.colorIndex2;
                    const color = getColorFromPaletteWithHue(
                        params.rld.palette,
                        colorIndex,
                        params.rld.brightness,
                        params.rld.hue
                    );
                    
                    controller.setLEDLogical('RLD', row, col, color);
                }
            }
        }
        
        controller.render();
    }
});