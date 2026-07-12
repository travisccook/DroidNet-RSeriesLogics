/**
 * Red Alert animation - Emergency alert pattern
 * Creates an alternating red strobe effect with varying patterns
 */

import { createAnimation } from '../core/animation-base.js';
import { DISPLAY_CONFIG } from '../core/config.js';

export default createAnimation({
    name: 'red-alert',
    displayName: 'Red Alert',
    description: 'Emergency red alert pattern',
    category: 'core',
    effectId: 11,
    interval: 50,
    
    init(params) {
        return {
            phase: 0,
            pattern: 0,
            strobeState: false,
            strobeCount: 0,
            waveOffset: 0
        };
    },
    
    frame(controller, state, params) {
        // Different patterns for variety
        switch (state.pattern) {
            case 0:
                // Double strobe
                renderDoubleStrobe(controller, state, params);
                break;
            case 1:
                // Alternating halves
                renderAlternatingHalves(controller, state, params);
                break;
            case 2:
                // Wave pattern
                renderWavePattern(controller, state, params);
                break;
            case 3:
                // Random sparkle
                renderSparklePattern(controller, state, params);
                break;
        }
        
        // Update phase
        state.phase++;
        
        // Change pattern periodically
        if (state.phase > 100) {
            state.phase = 0;
            state.pattern = (state.pattern + 1) % 4;
        }
        
        controller.render();
    }
});

function renderDoubleStrobe(controller, state, params) {
    // Double flash pattern: on-off-on-off-pause
    const sequence = [1, 0, 1, 0, 0, 0, 0, 0];
    const seqIndex = Math.floor(state.phase / 3) % sequence.length;
    const on = sequence[seqIndex];
    
    if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'FLD') {
        const color = on ? 
            {r: params.fld.brightness, g: 0, b: 0} : 
            {r: 0, g: 0, b: 0};
        controller.fill('FLD', color);
    }
    
    if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'RLD') {
        const color = on ? 
            {r: params.rld.brightness, g: 0, b: 0} : 
            {r: 0, g: 0, b: 0};
        controller.fill('RLD', color);
    }
}

function renderAlternatingHalves(controller, state, params) {
    const alternate = Math.floor(state.phase / 10) % 2;
    
    if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'FLD') {
        for (let row = 0; row < DISPLAY_CONFIG.fldRows; row++) {
            for (let col = 0; col < DISPLAY_CONFIG.fldCols; col++) {
                const isLeft = col < DISPLAY_CONFIG.fldCols / 2;
                const on = (isLeft && alternate) || (!isLeft && !alternate);
                const color = on ? 
                    {r: params.fld.brightness, g: 0, b: 0} : 
                    {r: 0, g: 0, b: 0};
                controller.setLEDLogical('FLD', row, col, color);
            }
        }
    }
    
    if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'RLD') {
        for (let row = 0; row < DISPLAY_CONFIG.rldRows; row++) {
            for (let col = 0; col < DISPLAY_CONFIG.rldCols; col++) {
                const isLeft = col < DISPLAY_CONFIG.rldCols / 2;
                const on = (isLeft && !alternate) || (!isLeft && alternate);
                const color = on ? 
                    {r: params.rld.brightness, g: 0, b: 0} : 
                    {r: 0, g: 0, b: 0};
                controller.setLEDLogical('RLD', row, col, color);
            }
        }
    }
}

function renderWavePattern(controller, state, params) {
    state.waveOffset += 0.3 * (params.speed / 5);
    
    if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'FLD') {
        for (let row = 0; row < DISPLAY_CONFIG.fldRows; row++) {
            for (let col = 0; col < DISPLAY_CONFIG.fldCols; col++) {
                const wave = Math.sin(col * 0.5 + state.waveOffset) * 0.5 + 0.5;
                const intensity = wave * params.fld.brightness;
                controller.setLEDLogical('FLD', row, col, 
                    {r: intensity, g: 0, b: 0});
            }
        }
    }
    
    if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'RLD') {
        for (let row = 0; row < DISPLAY_CONFIG.rldRows; row++) {
            for (let col = 0; col < DISPLAY_CONFIG.rldCols; col++) {
                const wave = Math.sin(col * 0.3 - state.waveOffset) * 0.5 + 0.5;
                const intensity = wave * params.rld.brightness;
                controller.setLEDLogical('RLD', row, col, 
                    {r: intensity, g: 0, b: 0});
            }
        }
    }
}

function renderSparklePattern(controller, state, params) {
    // Start with dim red background
    const dimRed = 0.2;
    
    if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'FLD') {
        controller.fill('FLD', {
            r: params.fld.brightness * dimRed, 
            g: 0, 
            b: 0
        });
        
        // Add random bright spots
        for (let i = 0; i < 5; i++) {
            const row = Math.floor(Math.random() * DISPLAY_CONFIG.fldRows);
            const col = Math.floor(Math.random() * DISPLAY_CONFIG.fldCols);
            controller.setLEDLogical('FLD', row, col, 
                {r: params.fld.brightness, g: 0, b: 0});
        }
    }
    
    if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'RLD') {
        controller.fill('RLD', {
            r: params.rld.brightness * dimRed, 
            g: 0, 
            b: 0
        });
        
        // Add random bright spots
        for (let i = 0; i < 8; i++) {
            const row = Math.floor(Math.random() * DISPLAY_CONFIG.rldRows);
            const col = Math.floor(Math.random() * DISPLAY_CONFIG.rldCols);
            controller.setLEDLogical('RLD', row, col, 
                {r: params.rld.brightness, g: 0, b: 0});
        }
    }
}