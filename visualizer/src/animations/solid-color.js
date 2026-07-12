/**
 * Solid Color animation - Display a single solid color
 * Shows a solid color from the selected palette
 */

import { createAnimation } from '../core/animation-base.js';
import { DISPLAY_CONFIG, getColorFromPaletteWithHue } from '../core/config.js';

export default createAnimation({
    name: 'solid-color',
    displayName: 'Solid Color',
    description: 'Display a single solid color',
    category: 'patterns',
    effectId: 5,
    interval: 100,
    
    init(params) {
        return {
            colorIndex: 0,
            timer: 0
        };
    },
    
    frame(controller, state, params) {
        // Update color index based on speed
        state.timer++;
        const changeInterval = Math.max(1, 20 - params.speed * 2);
        
        if (state.timer >= changeInterval) {
            state.timer = 0;
            state.colorIndex = (state.colorIndex + 1) % 10;
        }
        
        // Process FLD
        if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'FLD') {
            const color = getColorFromPaletteWithHue(
                params.fld.palette,
                state.colorIndex,
                params.fld.brightness,
                params.fld.hue
            );
            controller.fill('FLD', color);
        }
        
        // Process RLD
        if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'RLD') {
            const color = getColorFromPaletteWithHue(
                params.rld.palette,
                state.colorIndex,
                params.rld.brightness,
                params.rld.hue
            );
            controller.fill('RLD', color);
        }
        
        controller.render();
    }
});