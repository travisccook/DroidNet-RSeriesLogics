// Placeholder animation
import { createAnimation } from '../core/animation-base.js';
export default createAnimation({
    name: 'roaming-pixel',
    displayName: 'Roaming Pixel',
    description: 'Placeholder',
    category: 'effects',
    interval: 100,
    init() { return {}; },
    frame(controller) { controller.render(); }
});
