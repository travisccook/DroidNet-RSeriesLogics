// Placeholder animation
import { createAnimation } from '../core/animation-base.js';
export default createAnimation({
    name: 'pulse',
    displayName: 'Pulse',
    description: 'Placeholder',
    category: 'effects',
    interval: 100,
    init() { return {}; },
    frame(controller) { controller.render(); }
});
