/**
 * Meteor animation - Falling meteor trails
 * Creates falling meteors with glowing trails
 */

import { createAnimation } from '../core/animation-base.js';
import { DISPLAY_CONFIG, getColorFromPaletteWithHue } from '../core/config.js';

class Meteor {
    constructor(col, maxRows, speed = 1) {
        this.col = col;
        this.row = -5;
        this.speed = speed;
        this.maxRows = maxRows;
        this.trailLength = 8;
        this.colorIndex = Math.floor(Math.random() * 10);
    }
    
    update() {
        this.row += this.speed;
        return this.row < this.maxRows + this.trailLength;
    }
    
    getIntensity(row) {
        const distance = this.row - row;
        if (distance < 0 || distance > this.trailLength) return 0;
        return Math.pow(1 - (distance / this.trailLength), 2);
    }
}

export default createAnimation({
    name: 'meteor',
    displayName: 'Meteor',
    description: 'Falling meteor trails',
    category: 'effects',
    effectId: 27,
    interval: 50,
    
    init(params) {
        return {
            fldMeteors: [],
            rldMeteors: [],
            fldSpawnTimer: 0,
            rldSpawnTimer: 0
        };
    },
    
    frame(controller, state, params) {
        // Clear displays
        controller.clear();
        
        // Process FLD
        if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'FLD') {
            // Spawn new meteors
            state.fldSpawnTimer--;
            if (state.fldSpawnTimer <= 0) {
                const col = Math.floor(Math.random() * DISPLAY_CONFIG.fldCols);
                const speed = 0.3 + Math.random() * 0.4 * (params.speed / 5);
                state.fldMeteors.push(new Meteor(col, DISPLAY_CONFIG.fldRows, speed));
                state.fldSpawnTimer = Math.floor(Math.random() * 20) + 10;
            }
            
            // Update and render meteors
            state.fldMeteors = state.fldMeteors.filter(meteor => {
                const active = meteor.update();
                
                if (active) {
                    for (let row = 0; row < DISPLAY_CONFIG.fldRows; row++) {
                        const intensity = meteor.getIntensity(row);
                        if (intensity > 0) {
                            const color = getColorFromPaletteWithHue(
                                params.fld.palette,
                                meteor.colorIndex,
                                params.fld.brightness * intensity,
                                params.fld.hue
                            );
                            
                            // Blend with existing color
                            const existing = controller.fldLeds[
                                controller.getPhysicalIndex('FLD', row, meteor.col)
                            ];
                            if (existing) {
                                color.r = Math.min(255, color.r + existing.r);
                                color.g = Math.min(255, color.g + existing.g);
                                color.b = Math.min(255, color.b + existing.b);
                            }
                            
                            controller.setLEDLogical('FLD', row, meteor.col, color);
                        }
                    }
                }
                
                return active;
            });
        }
        
        // Process RLD
        if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'RLD') {
            // Spawn new meteors
            state.rldSpawnTimer--;
            if (state.rldSpawnTimer <= 0) {
                const col = Math.floor(Math.random() * DISPLAY_CONFIG.rldCols);
                const speed = 0.2 + Math.random() * 0.3 * (params.speed / 5);
                state.rldMeteors.push(new Meteor(col, DISPLAY_CONFIG.rldRows, speed));
                state.rldSpawnTimer = Math.floor(Math.random() * 10) + 5;
            }
            
            // Update and render meteors
            state.rldMeteors = state.rldMeteors.filter(meteor => {
                const active = meteor.update();
                
                if (active) {
                    for (let row = 0; row < DISPLAY_CONFIG.rldRows; row++) {
                        const intensity = meteor.getIntensity(row);
                        if (intensity > 0) {
                            const color = getColorFromPaletteWithHue(
                                params.rld.palette,
                                meteor.colorIndex,
                                params.rld.brightness * intensity,
                                params.rld.hue
                            );
                            
                            // Blend with existing color
                            const existing = controller.rldLeds[
                                controller.getPhysicalIndex('RLD', row, meteor.col)
                            ];
                            if (existing) {
                                color.r = Math.min(255, color.r + existing.r);
                                color.g = Math.min(255, color.g + existing.g);
                                color.b = Math.min(255, color.b + existing.b);
                            }
                            
                            controller.setLEDLogical('RLD', row, meteor.col, color);
                        }
                    }
                }
                
                return active;
            });
        }
        
        controller.render();
    }
});