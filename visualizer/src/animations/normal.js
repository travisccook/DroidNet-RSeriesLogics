/**
 * Normal animation - Default twinkling effect
 */

import { createAnimation, LEDState } from '../core/animation-base.js';
import { getColorFromPaletteWithHue, getNextColorNum, DISPLAY_CONFIG } from '../core/config.js';

export default createAnimation({
    name: 'normal',
    displayName: 'Normal',
    description: 'Default twinkling effect with random color changes',
    category: 'core',
    effectId: 0,
    interval: 50,
    
    init(params) {
        return {
            fldState: new LEDState(DISPLAY_CONFIG.fldLedCount),
            rldState: new LEDState(DISPLAY_CONFIG.rldLedCount)
        };
    },
    
    frame(controller, state, params) {
        // Process FLD
        if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'FLD') {
            const fldParams = {
                fade: params.fld.fade,
                delay: params.fld.delay,
                palette: params.fld.palette,
                brightness: params.fld.brightness,
                hue: params.fld.hue
            };
            updateDisplay(controller, 'FLD', state.fldState, fldParams);
        }
        
        // Process RLD
        if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'RLD') {
            const rldParams = {
                fade: params.rld.fade,
                delay: params.rld.delay,
                palette: params.rld.palette,
                brightness: params.rld.brightness,
                hue: params.rld.hue
            };
            updateDisplay(controller, 'RLD', state.rldState, rldParams);
        }
    }
});

function updateDisplay(controller, display, ledState, params) {
    const ledCount = display === 'FLD' ? DISPLAY_CONFIG.fldLedCount : DISPLAY_CONFIG.rldLedCount;
    
    for (let i = 0; i < ledCount; i++) {
        // Check if LED should change color
        if (ledState.colorPause[i] > 0) {
            ledState.colorPause[i]--;
        } else {
            // Random chance to change color
            if (Math.random() < 0.1) {
                // Move to next color
                ledState.colorNum[i] = getNextColorNum(ledState.colorNum[i], params.fade);
                
                // Set random pause
                ledState.colorPause[i] = Math.floor(Math.random() * params.delay * 10);
            }
        }
        
        // Get color from palette with hue adjustment
        const colorIndex = Math.floor(ledState.colorNum[i] / 3);
        const color = getColorFromPaletteWithHue(params.palette, colorIndex, params.brightness, params.hue);
        
        // Apply tweening between colors
        const tween = (ledState.colorNum[i] % 3) / 3;
        if (tween > 0 && ledState.lastColor[i]) {
            const lastColor = getColorFromPaletteWithHue(params.palette, 
                Math.floor(ledState.lastColor[i] / 3), params.brightness, params.hue);
            
            color.r = Math.floor(lastColor.r * (1 - tween) + color.r * tween);
            color.g = Math.floor(lastColor.g * (1 - tween) + color.g * tween);
            color.b = Math.floor(lastColor.b * (1 - tween) + color.b * tween);
        }
        
        // Set LED color
        controller.setLED(display, i, color);
        
        // Update last color
        if (Math.floor(ledState.colorNum[i] / 3) !== Math.floor(ledState.lastColor[i] / 3)) {
            ledState.lastColor[i] = ledState.colorNum[i];
        }
    }
    
    controller.render();
}