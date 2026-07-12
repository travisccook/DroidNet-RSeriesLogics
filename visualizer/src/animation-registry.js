/**
 * Animation Registry for RSeries Logic Display Visualizer
 * Statically imports and manages all available animations
 */

// Import all animations
import normal from './animations/normal.js';
import alarm from './animations/alarm.js';
import failure from './animations/failure.js';
import leia from './animations/leia.js';
import march from './animations/march.js';
import redAlert from './animations/red-alert.js';
import lightsOut from './animations/lights-out.js';
import solidColor from './animations/solid-color.js';
import flashColor from './animations/flash-color.js';
import flipFlop from './animations/flip-flop.js';
import flipFlopAlt from './animations/flip-flop-alt.js';
import colorSwap from './animations/color-swap.js';
import rainbow from './animations/rainbow.js';
import colorWipe from './animations/color-wipe.js';
import scanLine from './animations/scan-line.js';
import pulse from './animations/pulse.js';
import roamingPixel from './animations/roaming-pixel.js';
import fire from './animations/fire.js';
import textScroll from './animations/text-scroll.js';
import plasma from './animations/plasma.js';
import meteor from './animations/meteor.js';
import sparkle from './animations/sparkle.js';

export class AnimationRegistry {
    constructor() {
        this.animations = new Map();
        this.categories = {
            core: [],
            patterns: [],
            effects: [],
            text: [],
            experimental: []
        };
    }
    
    // Register an animation
    register(animation) {
        this.animations.set(animation.name, animation);
        
        // Add to category
        const category = this.categories[animation.category];
        if (category) {
            category.push(animation);
        }
    }
    
    // Get animation by name
    get(name) {
        return this.animations.get(name);
    }
    
    // Get all animations in a category
    getByCategory(category) {
        return this.categories[category] || [];
    }
    
    // Get all animations
    getAll() {
        return Array.from(this.animations.values());
    }
    
    // Load all animations
    async loadAll() {
        // Register all animations
        const animations = [
            normal, alarm, failure, leia, march, redAlert, lightsOut,
            solidColor, flashColor, flipFlop, flipFlopAlt, colorSwap, rainbow, colorWipe,
            scanLine, pulse, roamingPixel, fire, textScroll,
            plasma, meteor, sparkle
        ];
        
        animations.forEach(animation => {
            if (animation) {
                this.register(animation);
            }
        });
        
        return this;
    }
}

// Create and export singleton instance
export const animationRegistry = new AnimationRegistry();