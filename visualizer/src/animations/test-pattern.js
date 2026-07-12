export default {
    name: 'testPattern',
    displayName: 'Test Pattern',
    description: 'LED test pattern',
    category: 'patterns',
    interval: 2000,
    
    init() {
        return {
            mode: 0
        };
    },
    
    frame(controller, state) {
        controller.clear();
        
        if (this.target === 'both' || this.target === 'FLD') {
            // Test different patterns on FLD
            switch (state.mode) {
                case 0: // All red
                    for (let i = 0; i < controller.fldMatrix.count; i++) {
                        controller.setLED(i, 'red', 'FLD');
                    }
                    break;
                case 1: // All green
                    for (let i = 0; i < controller.fldMatrix.count; i++) {
                        controller.setLED(i, 'green', 'FLD');
                    }
                    break;
                case 2: // All blue
                    for (let i = 0; i < controller.fldMatrix.count; i++) {
                        controller.setLED(i, 'blue', 'FLD');
                    }
                    break;
                case 3: // All white
                    for (let i = 0; i < controller.fldMatrix.count; i++) {
                        controller.setLED(i, 'white', 'FLD');
                    }
                    break;
            }
        }
        
        if (this.target === 'both' || this.target === 'RLD') {
            // Same pattern on RLD
            const colors = ['red', 'green', 'blue', 'white'];
            const color = colors[state.mode];
            for (let i = 0; i < controller.rldMatrix.count; i++) {
                controller.setLED(i, color, 'RLD');
            }
        }
        
        state.mode = (state.mode + 1) % 4;
    }
};