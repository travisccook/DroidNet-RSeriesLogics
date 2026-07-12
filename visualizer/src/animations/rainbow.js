/**
 * Rainbow animation - Smooth color wheel rotation
 */

import { createAnimation } from '../core/animation-base.js';
import { hsvToRgb, DISPLAY_CONFIG } from '../core/config.js';

export default createAnimation({
    name: 'rainbow',
    displayName: 'Rainbow',
    description: 'Smooth rainbow color wheel effect',
    category: 'patterns',
    effectId: 10,
    interval: 50,
    
    init(params) {
        return {
            hueOffset: 0,
            brightness: params.brightness || 255,
            speed: params.speed || 5
        };
    },
    
    frame(controller, state, params) {
        state.brightness = params.brightness || state.brightness;
        state.speed = params.speed || state.speed;
        
        // Update FLD
        if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'FLD') {
            for (let row = 0; row < DISPLAY_CONFIG.fldRows; row++) {
                for (let col = 0; col < DISPLAY_CONFIG.fldCols; col++) {
                    const hue = (state.hueOffset + col * 25 + row * 10) % 256;
                    const color = hsvToRgb(hue, 255, state.brightness);
                    controller.setLEDLogical('FLD', row, col, color);
                }
            }
        }
        
        // Update RLD
        if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'RLD') {
            for (let row = 0; row < DISPLAY_CONFIG.rldRows; row++) {
                for (let col = 0; col < DISPLAY_CONFIG.rldCols; col++) {
                    const hue = (state.hueOffset + col * 9 + row * 20) % 256;
                    const color = hsvToRgb(hue, 255, state.brightness);
                    controller.setLEDLogical('RLD', row, col, color);
                }
            }
        }
        
        controller.render();
        
        // Advance hue
        state.hueOffset = (state.hueOffset + state.speed) % 256;
    }
});