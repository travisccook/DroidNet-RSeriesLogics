/**
 * Flash Color animation - Flashing color pattern
 * Cycles through palette colors with configurable flash timing
 */

import { createAnimation } from '../core/animation-base.js';
import { DISPLAY_CONFIG, getColorFromPaletteWithHue } from '../core/config.js';

export default createAnimation({
    name: 'flash-color',
    displayName: 'Flash Color',
    description: 'Flashing color pattern',
    category: 'patterns',
    effectId: 6,
    interval: 50,
    
    init(params) {
        return {
            flashState: true,
            colorIndex: 0,
            flashTimer: 0
        };
    },
    
    frame(controller, state, params) {
        // Update flash timer based on speed
        state.flashTimer++;
        const flashInterval = Math.max(1, 10 - params.speed);
        
        if (state.flashTimer >= flashInterval) {
            state.flashTimer = 0;
            state.flashState = !state.flashState;
            
            // Change color when turning on
            if (state.flashState) {
                state.colorIndex = (state.colorIndex + 1) % 10;
            }
        }
        
        // Process FLD
        if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'FLD') {
            if (state.flashState) {
                const color = getColorFromPaletteWithHue(
                    params.fld.palette,
                    state.colorIndex,
                    params.fld.brightness,
                    params.fld.hue
                );
                controller.fill('FLD', color);
            } else {
                controller.fill('FLD', {r: 0, g: 0, b: 0});
            }
        }
        
        // Process RLD
        if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'RLD') {
            if (state.flashState) {
                const color = getColorFromPaletteWithHue(
                    params.rld.palette,
                    state.colorIndex,
                    params.rld.brightness,
                    params.rld.hue
                );
                controller.fill('RLD', color);
            } else {
                controller.fill('RLD', {r: 0, g: 0, b: 0});
            }
        }
        
        controller.render();
    }
});