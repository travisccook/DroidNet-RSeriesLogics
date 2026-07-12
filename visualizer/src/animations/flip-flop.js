/**
 * Flip Flop animation - Alternating two-color pattern
 * Creates a flip-flop effect between two colors
 */

import { createAnimation } from '../core/animation-base.js';
import { DISPLAY_CONFIG, getColorFromPaletteWithHue } from '../core/config.js';

export default createAnimation({
    name: 'flip-flop',
    displayName: 'Flip Flop',
    description: 'Alternating two-color pattern',
    category: 'patterns',
    effectId: 7,
    interval: 100,
    
    init(params) {
        return {
            flipState: false,
            colorIndex1: 0,
            colorIndex2: 5,
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
            
            // Slowly advance colors
            if (state.flipState) {
                state.colorIndex1 = (state.colorIndex1 + 1) % 10;
                state.colorIndex2 = (state.colorIndex2 + 1) % 10;
            }
        }
        
        // Process FLD
        if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'FLD') {
            const colorIndex = state.flipState ? state.colorIndex1 : state.colorIndex2;
            const color = getColorFromPaletteWithHue(
                params.fld.palette,
                colorIndex,
                params.fld.brightness,
                params.fld.hue
            );
            controller.fill('FLD', color);
        }
        
        // Process RLD
        if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'RLD') {
            // RLD uses opposite state for visual interest
            const colorIndex = !state.flipState ? state.colorIndex1 : state.colorIndex2;
            const color = getColorFromPaletteWithHue(
                params.rld.palette,
                colorIndex,
                params.rld.brightness,
                params.rld.hue
            );
            controller.fill('RLD', color);
        }
        
        controller.render();
    }
});