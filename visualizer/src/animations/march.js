/**
 * March animation - Imperial march synchronized pattern
 * Creates rhythmic patterns synchronized to the Imperial March beat
 */

import { createAnimation } from '../core/animation-base.js';
import { DISPLAY_CONFIG, getColorFromPaletteWithHue } from '../core/config.js';

// Imperial March beat pattern (simplified)
// 1 = beat, 0 = rest, values represent relative intensity
const MARCH_PATTERN = [
    1.0, 0, 1.0, 0, 1.0, 0, 0, 0,  // DUN DUN DUN
    0.8, 0, 0, 0, 0.9, 0, 0, 0,    // DUN   DUN
    1.0, 0, 1.0, 0, 1.0, 0, 0, 0,  // DUN DUN DUN
    0.8, 0, 0, 0, 0.9, 0, 0, 0,    // DUN   DUN
];

export default createAnimation({
    name: 'march',
    displayName: 'March',
    description: 'Imperial march synchronized pattern',
    category: 'core',
    effectId: 4,
    interval: 125, // ~120 BPM
    
    init(params) {
        return {
            beatIndex: 0,
            colorIndex: 0,
            pulsePhase: 0,
            marchColumns: new Array(DISPLAY_CONFIG.fldCols).fill(0),
            rldWave: 0
        };
    },
    
    frame(controller, state, params) {
        const beat = MARCH_PATTERN[state.beatIndex];
        const nextBeat = MARCH_PATTERN[(state.beatIndex + 1) % MARCH_PATTERN.length];
        
        // Interpolate between beats for smooth transitions
        const t = state.pulsePhase;
        const intensity = beat * (1 - t) + nextBeat * t;
        
        // Process FLD - Column marching effect
        if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'FLD') {
            // Update marching columns
            if (beat > 0 && state.pulsePhase === 0) {
                // Shift columns
                for (let i = DISPLAY_CONFIG.fldCols - 1; i > 0; i--) {
                    state.marchColumns[i] = state.marchColumns[i - 1] * 0.8;
                }
                state.marchColumns[0] = beat;
                
                // Change color on strong beats
                if (beat >= 1.0) {
                    state.colorIndex = (state.colorIndex + 1) % 10;
                }
            }
            
            // Render columns
            for (let col = 0; col < DISPLAY_CONFIG.fldCols; col++) {
                const colIntensity = state.marchColumns[col];
                const color = getColorFromPaletteWithHue(
                    params.fld.palette, 
                    state.colorIndex,
                    params.fld.brightness * colIntensity,
                    params.fld.hue
                );
                
                for (let row = 0; row < DISPLAY_CONFIG.fldRows; row++) {
                    // Add vertical gradient
                    const rowFade = 1 - (row / DISPLAY_CONFIG.fldRows) * 0.3;
                    const fadedColor = {
                        r: Math.floor(color.r * rowFade),
                        g: Math.floor(color.g * rowFade),
                        b: Math.floor(color.b * rowFade)
                    };
                    controller.setLEDLogical('FLD', row, col, fadedColor);
                }
            }
        }
        
        // Process RLD - Wave effect synchronized to beat
        if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'RLD') {
            for (let row = 0; row < DISPLAY_CONFIG.rldRows; row++) {
                for (let col = 0; col < DISPLAY_CONFIG.rldCols; col++) {
                    // Create wave effect
                    const waveOffset = Math.sin((col / DISPLAY_CONFIG.rldCols) * Math.PI * 2 + 
                        state.rldWave) * 0.5 + 0.5;
                    
                    // Combine with beat intensity
                    const ledIntensity = intensity * (0.5 + waveOffset * 0.5);
                    
                    const color = getColorFromPaletteWithHue(
                        params.rld.palette,
                        state.colorIndex + Math.floor(waveOffset * 3),
                        params.rld.brightness * ledIntensity,
                        params.rld.hue
                    );
                    
                    controller.setLEDLogical('RLD', row, col, color);
                }
            }
            
            // Update wave position on beats
            if (beat > 0 && state.pulsePhase === 0) {
                state.rldWave += Math.PI / 4;
            }
        }
        
        // Update animation state
        state.pulsePhase += 0.1 * (params.speed / 5);
        if (state.pulsePhase >= 1) {
            state.pulsePhase = 0;
            state.beatIndex = (state.beatIndex + 1) % MARCH_PATTERN.length;
        }
        
        controller.render();
    }
});