/**
 * Sparkle animation - Random twinkling sparkles
 * Creates random sparkling effects across the display
 */

import { createAnimation } from '../core/animation-base.js';
import { DISPLAY_CONFIG, getColorFromPaletteWithHue } from '../core/config.js';

class Sparkle {
    constructor(row, col, colorIndex, maxBrightness) {
        this.row = row;
        this.col = col;
        this.colorIndex = colorIndex;
        this.maxBrightness = maxBrightness;
        this.phase = 0;
        this.speed = 0.1 + Math.random() * 0.2;
    }
    
    update() {
        this.phase += this.speed;
        return this.phase < Math.PI;
    }
    
    getBrightness() {
        return Math.sin(this.phase) * this.maxBrightness;
    }
}

export default createAnimation({
    name: 'sparkle',
    displayName: 'Sparkle',
    description: 'Random twinkling sparkles',
    category: 'effects',
    effectId: 28,
    interval: 50,
    
    init(params) {
        return {
            fldSparkles: [],
            rldSparkles: [],
            backgroundDim: 0.1
        };
    },
    
    frame(controller, state, params) {
        // Draw dim background
        if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'FLD') {
            for (let row = 0; row < DISPLAY_CONFIG.fldRows; row++) {
                for (let col = 0; col < DISPLAY_CONFIG.fldCols; col++) {
                    const color = getColorFromPaletteWithHue(
                        params.fld.palette,
                        Math.floor((row + col) / 3) % 10,
                        params.fld.brightness * state.backgroundDim,
                        params.fld.hue
                    );
                    controller.setLEDLogical('FLD', row, col, color);
                }
            }
        }
        
        if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'RLD') {
            for (let row = 0; row < DISPLAY_CONFIG.rldRows; row++) {
                for (let col = 0; col < DISPLAY_CONFIG.rldCols; col++) {
                    const color = getColorFromPaletteWithHue(
                        params.rld.palette,
                        Math.floor((row * 2 + col) / 4) % 10,
                        params.rld.brightness * state.backgroundDim,
                        params.rld.hue
                    );
                    controller.setLEDLogical('RLD', row, col, color);
                }
            }
        }
        
        // Process FLD sparkles
        if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'FLD') {
            // Add new sparkles
            const sparkleChance = 0.05 * (params.speed / 5);
            for (let i = 0; i < 3; i++) {
                if (Math.random() < sparkleChance) {
                    const row = Math.floor(Math.random() * DISPLAY_CONFIG.fldRows);
                    const col = Math.floor(Math.random() * DISPLAY_CONFIG.fldCols);
                    const colorIndex = Math.floor(Math.random() * 10);
                    state.fldSparkles.push(
                        new Sparkle(row, col, colorIndex, params.fld.brightness)
                    );
                }
            }
            
            // Update and render sparkles
            state.fldSparkles = state.fldSparkles.filter(sparkle => {
                const active = sparkle.update();
                
                if (active) {
                    const brightness = sparkle.getBrightness();
                    const color = getColorFromPaletteWithHue(
                        params.fld.palette,
                        sparkle.colorIndex,
                        brightness,
                        params.fld.hue
                    );
                    controller.setLEDLogical('FLD', sparkle.row, sparkle.col, color);
                }
                
                return active;
            });
        }
        
        // Process RLD sparkles
        if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'RLD') {
            // Add new sparkles
            const sparkleChance = 0.03 * (params.speed / 5);
            for (let i = 0; i < 5; i++) {
                if (Math.random() < sparkleChance) {
                    const row = Math.floor(Math.random() * DISPLAY_CONFIG.rldRows);
                    const col = Math.floor(Math.random() * DISPLAY_CONFIG.rldCols);
                    const colorIndex = Math.floor(Math.random() * 10);
                    state.rldSparkles.push(
                        new Sparkle(row, col, colorIndex, params.rld.brightness)
                    );
                }
            }
            
            // Update and render sparkles
            state.rldSparkles = state.rldSparkles.filter(sparkle => {
                const active = sparkle.update();
                
                if (active) {
                    const brightness = sparkle.getBrightness();
                    const color = getColorFromPaletteWithHue(
                        params.rld.palette,
                        sparkle.colorIndex,
                        brightness,
                        params.rld.hue
                    );
                    controller.setLEDLogical('RLD', sparkle.row, sparkle.col, color);
                }
                
                return active;
            });
        }
        
        controller.render();
    }
});