// Placeholder animation
import { createAnimation } from '../core/animation-base.js';
export default createAnimation({
    name: 'text-scroll',
    displayName: 'Text Scroll',
    description: 'Placeholder',
    category: 'text',
    interval: 100,
    init() { return {}; },
    frame(controller) { controller.render(); }
});
