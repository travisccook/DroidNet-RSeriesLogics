/**
 * Base animation class/factory for RSeries Logic Display animations
 */

export class AnimationBase {
    constructor(config) {
        this.name = config.name;
        this.displayName = config.displayName;
        this.description = config.description || '';
        this.category = config.category || 'effects';
        this.interval = config.interval || 100;
        this.effectId = config.effectId || 0;
        
        // Animation methods
        this.init = config.init || (() => ({}));
        this.frame = config.frame || (() => {});
        this.cleanup = config.cleanup || (() => {});
        
        // Animation state
        this.state = {};
        this.frameCount = 0;
        this.lastFrameTime = 0;
    }
    
    // Initialize animation
    initialize(params = {}) {
        this.state = this.init(params);
        this.frameCount = 0;
        this.lastFrameTime = Date.now();
        return this;
    }
    
    // Update animation frame
    update(controller, params = {}) {
        const now = Date.now();
        if (now - this.lastFrameTime >= this.interval) {
            this.frame(controller, this.state, params);
            this.frameCount++;
            this.lastFrameTime = now;
        }
    }
    
    // Clean up animation
    destroy() {
        this.cleanup(this.state);
        this.state = {};
        this.frameCount = 0;
    }
}

// Factory function for creating animations
export function createAnimation(config) {
    return new AnimationBase(config);
}

// Base state for LED tracking
export class LEDState {
    constructor(ledCount) {
        this.colorNum = new Array(ledCount).fill(0);
        this.colorPause = new Array(ledCount).fill(0);
        this.lastColor = new Array(ledCount).fill(0);
    }
    
    reset() {
        this.colorNum.fill(0);
        this.colorPause.fill(0);
        this.lastColor.fill(0);
    }
}