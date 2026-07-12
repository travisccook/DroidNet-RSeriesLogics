/**
 * Lights Out animation - All LEDs off
 */

import { createAnimation } from '../core/animation-base.js';

export default createAnimation({
    name: 'lights-out',
    displayName: 'Lights Out',
    description: 'All LEDs turned off',
    category: 'core',
    effectId: 14,
    interval: 1000, // Slow interval since nothing changes
    
    init() {
        return {};
    },
    
    frame(controller, state, params) {
        // Clear both displays
        if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'FLD') {
            controller.clear('FLD');
        }
        if (params.targetDisplay === 'BOTH' || params.targetDisplay === 'RLD') {
            controller.clear('RLD');
        }
        controller.render();
    }
});