// Placeholder animation
import { createAnimation } from '../core/animation-base.js';
export default createAnimation({
    name: 'color-swap',
    displayName: 'Color Swap',
    description: 'Placeholder',
    category: 'patterns',
    interval: 100,
    init() { return {}; },
    frame(controller) { controller.render(); }
});
