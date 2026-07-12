/**
 * LED Controller for RSeries Logic Display Visualizer
 * Manages rendering of LED matrices to HTML canvas elements
 */

export class LEDController {
    constructor(fldCanvas, rldCanvas, config) {
        this.fldCanvas = fldCanvas;
        this.rldCanvas = rldCanvas;
        this.config = config;
        
        // Get canvas contexts
        this.fldCtx = fldCanvas.getContext('2d');
        this.rldCtx = rldCanvas.getContext('2d');
        
        // LED states - array of color objects {r, g, b}
        this.fldLeds = new Array(80).fill(null).map(() => ({r: 0, g: 0, b: 0}));
        this.rldLeds = new Array(112).fill(null).map(() => ({r: 0, g: 0, b: 0}));
        
        // Canvas dimensions
        this.setupCanvas();
    }
    
    setupCanvas() {
        // FLD: 10 columns x 8 rows
        const fldWidth = 10 * (this.config.ledSize + this.config.ledSpacing) - this.config.ledSpacing;
        const fldHeight = 8 * (this.config.ledSize + this.config.ledSpacing) - this.config.ledSpacing;
        
        // RLD: 28 columns x 4 rows
        const rldWidth = 28 * (this.config.ledSize + this.config.ledSpacing) - this.config.ledSpacing;
        const rldHeight = 4 * (this.config.ledSize + this.config.ledSpacing) - this.config.ledSpacing;
        
        // Set canvas sizes
        this.fldCanvas.width = fldWidth + 2 * this.config.padding;
        this.fldCanvas.height = fldHeight + 2 * this.config.padding;
        this.rldCanvas.width = rldWidth + 2 * this.config.padding;
        this.rldCanvas.height = rldHeight + 2 * this.config.padding;
        
        // Initial clear
        this.clear();
    }
    
    /**
     * Set LED color by physical index
     * @param {string} display - 'FLD' or 'RLD'
     * @param {number} index - Physical LED index
     * @param {Object} color - {r, g, b} color object (0-255)
     */
    setLED(display, index, color) {
        if (display === 'FLD' && index >= 0 && index < 80) {
            this.fldLeds[index] = {...color};
        } else if (display === 'RLD' && index >= 0 && index < 112) {
            this.rldLeds[index] = {...color};
        }
    }
    
    /**
     * Set LED by logical position (row, col)
     * @param {string} display - 'FLD' or 'RLD'
     * @param {number} row - Row index
     * @param {number} col - Column index
     * @param {Object} color - {r, g, b} color object
     */
    setLEDLogical(display, row, col, color) {
        const index = this.getPhysicalIndex(display, row, col);
        if (index !== -1) {
            this.setLED(display, index, color);
        }
    }
    
    /**
     * Get physical LED index from logical position
     * @param {string} display - 'FLD' or 'RLD'
     * @param {number} row - Row index
     * @param {number} col - Column index
     * @returns {number} Physical LED index or -1 if invalid
     */
    getPhysicalIndex(display, row, col) {
        if (display === 'FLD') {
            if (row < 0 || row >= 8 || col < 0 || col >= 10) return -1;
            const logicalIndex = row * 10 + col;
            return this.config.FLD_MAP[logicalIndex];
        } else if (display === 'RLD') {
            if (row < 0 || row >= 4 || col < 0 || col >= 28) return -1;
            const logicalIndex = row * 28 + col;
            return this.config.RLD_MAP[logicalIndex];
        }
        return -1;
    }
    
    /**
     * Clear all LEDs
     * @param {string} display - 'FLD', 'RLD', or 'BOTH'
     */
    clear(display = 'BOTH') {
        const black = {r: 0, g: 0, b: 0};
        
        if (display === 'FLD' || display === 'BOTH') {
            this.fldLeds.fill(null).forEach((_, i) => this.fldLeds[i] = {...black});
        }
        if (display === 'RLD' || display === 'BOTH') {
            this.rldLeds.fill(null).forEach((_, i) => this.rldLeds[i] = {...black});
        }
    }
    
    /**
     * Render LEDs to canvas
     */
    render() {
        this.renderDisplay(this.fldCtx, this.fldLeds, 10, 8, 0);
        this.renderDisplay(this.rldCtx, this.rldLeds, 28, 4, this.rldVerticalOffset || 0);
    }
    
    /**
     * Render a single display
     */
    renderDisplay(ctx, leds, cols, rows, verticalOffset = 0) {
        // Clear canvas
        ctx.fillStyle = '#000';
        ctx.fillRect(0, 0, ctx.canvas.width, ctx.canvas.height);
        
        // Draw LEDs
        for (let row = 0; row < rows; row++) {
            for (let col = 0; col < cols; col++) {
                const logicalIndex = row * cols + col;
                let physicalIndex;
                
                // Get the physical index based on mapping
                if (cols === 10) { // FLD
                    physicalIndex = this.config.FLD_MAP[logicalIndex];
                } else { // RLD
                    physicalIndex = this.config.RLD_MAP[logicalIndex];
                }
                
                const led = leds[physicalIndex];
                if (!led) continue;
                
                const x = this.config.padding + col * (this.config.ledSize + this.config.ledSpacing);
                const y = this.config.padding + verticalOffset + row * (this.config.ledSize + this.config.ledSpacing);
                
                // Draw LED with glow effect
                const brightness = Math.max(led.r, led.g, led.b) / 255;
                
                // Glow
                if (brightness > 0.1) {
                    const gradient = ctx.createRadialGradient(
                        x + this.config.ledSize/2, y + this.config.ledSize/2, 0,
                        x + this.config.ledSize/2, y + this.config.ledSize/2, this.config.ledSize
                    );
                    gradient.addColorStop(0, `rgba(${led.r}, ${led.g}, ${led.b}, ${brightness})`);
                    gradient.addColorStop(1, `rgba(${led.r}, ${led.g}, ${led.b}, 0)`);
                    ctx.fillStyle = gradient;
                    ctx.fillRect(x - this.config.ledSize/2, y - this.config.ledSize/2, 
                                this.config.ledSize * 2, this.config.ledSize * 2);
                }
                
                // LED body
                ctx.fillStyle = `rgb(${led.r}, ${led.g}, ${led.b})`;
                ctx.fillRect(x, y, this.config.ledSize, this.config.ledSize);
                
                // LED border
                ctx.strokeStyle = '#333';
                ctx.lineWidth = 1;
                ctx.strokeRect(x, y, this.config.ledSize, this.config.ledSize);
            }
        }
    }
    
    /**
     * Set all LEDs to a single color
     * @param {string} display - 'FLD', 'RLD', or 'BOTH'
     * @param {Object} color - {r, g, b} color object
     */
    fill(display, color) {
        if (display === 'FLD' || display === 'BOTH') {
            this.fldLeds.forEach((_, i) => this.fldLeds[i] = {...color});
        }
        if (display === 'RLD' || display === 'BOTH') {
            this.rldLeds.forEach((_, i) => this.rldLeds[i] = {...color});
        }
    }
}