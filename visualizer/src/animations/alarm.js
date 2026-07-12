/**
 * Alarm animation - Fast red/yellow flashing
 */

import { createAnimation } from '../core/animation-base.js';
import { hsvToRgb, DISPLAY_CONFIG } from '../core/config.js';

export default createAnimation({
    name: 'alarm',
    displayName: 'Alarm',
    description: 'Fast red and yellow flashing alarm pattern',
    category: 'core',
    effectId: 1,
    interval: 100,
    
    init(params) {
        return {
            phase: 0,
            brightness: params.brightness || 255
        };
    },
    
    frame(controller, state, params) {
        state.brightness = params.brightness || state.brightness;
        
        // Alternate between red and yellow
        const color = state.phase % 2 === 0 
            ? hsvToRgb(0, 255, state.brightness)    // Red
            : hsvToRgb(42, 255, state.brightness);  // Yellow
        
        // Fill both displays
        if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'FLD') {
            controller.fill('FLD', color);
        }
        if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'RLD') {
            controller.fill('RLD', color);
        }
        
        controller.render();
        state.phase++;
    }
});