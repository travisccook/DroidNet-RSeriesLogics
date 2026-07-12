/**
 * RSeries Logic Display Visualizer - Main Application
 */

import { LEDController } from './core/led-controller.js';
import { DISPLAY_CONFIG, SEQUENCES } from './core/config.js';
import { animationRegistry } from './animation-registry.js';

// Application state
const state = {
    currentAnimation: null,
    animationInterval: null,
    controller: null,
    settings: {
        fld: {
            brightness: 255,
            hue: 0,
            fade: 128,
            delay: 10,
            palette: 0
        },
        rld: {
            brightness: 255,
            hue: 0,
            fade: 128,
            delay: 10,
            palette: 1  // Default to rear palette
        },
        speed: 5,
        targetDisplay: 'BOTH',
        duration: 0
    }
};

// Initialize application
async function init() {
    console.log('Initializing RSeries Logic Visualizer...');
    
    // Get canvas elements
    const fldCanvas = document.getElementById('fld-canvas');
    const rldCanvas = document.getElementById('rld-canvas');
    
    if (!fldCanvas || !rldCanvas) {
        console.error('Canvas elements not found!');
        return;
    }
    
    console.log('Creating LED controller...');
    // Create LED controller
    state.controller = new LEDController(fldCanvas, rldCanvas, DISPLAY_CONFIG);
    
    console.log('Loading animations...');
    // Load animations
    await animationRegistry.loadAll();
    console.log(`Loaded ${animationRegistry.getAll().length} animations`);
    
    console.log('Creating animation buttons...');
    // Create animation buttons
    createAnimationButtons();
    
    // Setup event handlers
    setupEventHandlers();
    
    // Load saved settings
    loadSettings();
    
    // Start with normal animation
    console.log('Starting normal animation...');
    startAnimation('normal');
}

// Create animation buttons
function createAnimationButtons() {
    const container = document.getElementById('animation-buttons');
    if (!container) {
        console.error('Animation buttons container not found!');
        return;
    }
    
    container.innerHTML = '';
    
    // Group animations by category
    const categories = ['core', 'patterns', 'effects', 'text'];
    
    categories.forEach(category => {
        const animations = animationRegistry.getByCategory(category);
        console.log(`Category ${category}: ${animations.length} animations`);
        if (animations.length === 0) return;
        
        // Add category header
        const header = document.createElement('div');
        header.className = 'animation-category';
        header.textContent = category.charAt(0).toUpperCase() + category.slice(1);
        container.appendChild(header);
        
        // Add animation buttons
        animations.forEach(animation => {
            console.log(`Creating button for ${animation.name}`);
            const button = document.createElement('button');
            button.className = 'animation-button';
            button.textContent = animation.displayName;
            button.title = animation.description;
            button.dataset.animation = animation.name;
            
            button.addEventListener('click', () => {
                startAnimation(animation.name);
            });
            
            container.appendChild(button);
        });
    });
    
    console.log(`Total buttons created: ${container.querySelectorAll('.animation-button').length}`);
}

// Start animation
function startAnimation(name) {
    // Stop current animation
    stopAnimation();
    
    // Get animation
    const animation = animationRegistry.get(name);
    if (!animation) {
        console.error(`Animation not found: ${name}`);
        return;
    }
    
    // Update UI
    document.querySelectorAll('.animation-button').forEach(btn => {
        btn.classList.toggle('active', btn.dataset.animation === name);
    });
    
    // Initialize animation
    state.currentAnimation = animation;
    animation.initialize(state.settings);
    
    // Generate MarcDuino commands
    updateCommands(animation);
    
    // Start animation loop
    state.animationInterval = setInterval(() => {
        animation.update(state.controller, state.settings);
    }, DISPLAY_CONFIG.frameInterval);
    
    // Handle duration
    if (state.settings.duration > 0) {
        setTimeout(() => {
            stopAnimation();
        }, state.settings.duration * 1000);
    }
}

// Stop animation
function stopAnimation() {
    if (state.animationInterval) {
        clearInterval(state.animationInterval);
        state.animationInterval = null;
    }
    
    if (state.currentAnimation) {
        state.currentAnimation.destroy();
        state.currentAnimation = null;
    }
    
    // Clear displays
    state.controller.clear();
    state.controller.render();
    
    // Clear active button
    document.querySelectorAll('.animation-button').forEach(btn => {
        btn.classList.remove('active');
    });
}

// Update MarcDuino commands
function updateCommands(animation) {
    const output = document.getElementById('command-output');
    const commands = [];
    
    // Effect command
    if (animation.effectId !== undefined) {
        commands.push(`:LE${animation.effectId.toString().padStart(2, '0')}`);
    }
    
    // Check which displays are targeted
    const targetDisplay = state.settings.targetDisplay;
    
    if (targetDisplay === 'BOTH' || targetDisplay === 'FLD') {
        // FLD Parameter commands
        commands.push('// Front Logic Display');
        commands.push(`:LEFB${state.settings.fld.brightness}`);
        commands.push(`:LEFF${state.settings.fld.fade}`);
        commands.push(`:LEFD${state.settings.fld.delay}`);
        commands.push(`:LEFH${state.settings.fld.hue}`);
        commands.push(`:LEFP${state.settings.fld.palette}`);
    }
    
    if (targetDisplay === 'BOTH' || targetDisplay === 'RLD') {
        // RLD Parameter commands
        commands.push('// Rear Logic Display');
        commands.push(`:LERB${state.settings.rld.brightness}`);
        commands.push(`:LERF${state.settings.rld.fade}`);
        commands.push(`:LERD${state.settings.rld.delay}`);
        commands.push(`:LERH${state.settings.rld.hue}`);
        commands.push(`:LERP${state.settings.rld.palette}`);
    }
    
    if (state.settings.duration > 0) {
        commands.push(`:LET${state.settings.duration.toString().padStart(2, '0')}`);
    }
    
    output.value = commands.join('\n');
}

// Setup event handlers
function setupEventHandlers() {
    // Global parameter sliders
    const globalParams = ['speed'];
    globalParams.forEach(param => {
        const slider = document.getElementById(param);
        const value = document.getElementById(`${param}-value`);
        
        if (slider && value) {
            slider.addEventListener('input', (e) => {
                const val = parseInt(e.target.value);
                state.settings[param] = val;
                value.textContent = val;
                saveSettings();
                
                if (state.currentAnimation) {
                    updateCommands(state.currentAnimation);
                }
            });
        }
    });
    
    // Per-display parameter sliders
    const displays = ['fld', 'rld'];
    const displayParams = ['brightness', 'hue', 'fade', 'delay'];
    
    displays.forEach(display => {
        displayParams.forEach(param => {
            const slider = document.getElementById(`${display}-${param}`);
            const value = document.getElementById(`${display}-${param}-value`);
            
            if (slider && value) {
                slider.addEventListener('input', (e) => {
                    const val = parseInt(e.target.value);
                    state.settings[display][param] = val;
                    value.textContent = val;
                    saveSettings();
                    
                    if (state.currentAnimation) {
                        updateCommands(state.currentAnimation);
                    }
                });
            }
        });
        
        // Palette selector for each display
        const palette = document.getElementById(`${display}-palette`);
        if (palette) {
            palette.addEventListener('change', (e) => {
                state.settings[display].palette = parseInt(e.target.value);
                saveSettings();
                if (state.currentAnimation) {
                    updateCommands(state.currentAnimation);
                }
            });
        }
    });
    
    // Target display selector
    const targetDisplay = document.getElementById('target-display');
    if (targetDisplay) {
        targetDisplay.addEventListener('change', (e) => {
            state.settings.targetDisplay = e.target.value;
            saveSettings();
        });
    }
    
    // Duration input
    const duration = document.getElementById('duration');
    if (duration) {
        duration.addEventListener('change', (e) => {
            state.settings.duration = parseInt(e.target.value) || 0;
            saveSettings();
            if (state.currentAnimation) {
                updateCommands(state.currentAnimation);
            }
        });
    }
    
    // Stop button
    document.getElementById('stop-animation').addEventListener('click', stopAnimation);
    
    // Reset button
    document.getElementById('reset-settings').addEventListener('click', resetSettings);
    
    // Copy commands button
    document.getElementById('copy-commands').addEventListener('click', () => {
        const output = document.getElementById('command-output');
        output.select();
        document.execCommand('copy');
        
        // Visual feedback
        const button = document.getElementById('copy-commands');
        const originalText = button.textContent;
        button.textContent = 'Copied!';
        setTimeout(() => {
            button.textContent = originalText;
        }, 1000);
    });
}

// Save settings to localStorage
function saveSettings() {
    localStorage.setItem('rseries-visualizer-settings', JSON.stringify(state.settings));
}

// Load settings from localStorage
function loadSettings() {
    const saved = localStorage.getItem('rseries-visualizer-settings');
    if (saved) {
        try {
            const savedSettings = JSON.parse(saved);
            
            // Handle migration from old flat structure to new nested structure
            if (savedSettings.brightness !== undefined && !savedSettings.fld) {
                // Old format, migrate to new format
                state.settings.fld = {
                    brightness: savedSettings.brightness || 255,
                    hue: savedSettings.hue || 0,
                    fade: savedSettings.fade || 128,
                    delay: savedSettings.delay || 10,
                    palette: savedSettings.palette || 0
                };
                state.settings.rld = {
                    brightness: savedSettings.brightness || 255,
                    hue: savedSettings.hue || 0,
                    fade: savedSettings.fade || 128,
                    delay: savedSettings.delay || 10,
                    palette: savedSettings.palette || 1
                };
                state.settings.speed = savedSettings.speed || 5;
                state.settings.targetDisplay = savedSettings.targetDisplay || 'BOTH';
                state.settings.duration = savedSettings.duration || 0;
            } else {
                // New format
                Object.assign(state.settings, savedSettings);
            }
            
            // Update UI - Global settings
            ['speed', 'targetDisplay', 'duration'].forEach(key => {
                const element = document.getElementById(key);
                if (element) {
                    element.value = state.settings[key];
                    const valueElement = document.getElementById(`${key}-value`);
                    if (valueElement) {
                        valueElement.textContent = state.settings[key];
                    }
                }
            });
            
            // Update UI - Per-display settings
            ['fld', 'rld'].forEach(display => {
                Object.keys(state.settings[display]).forEach(key => {
                    const element = document.getElementById(`${display}-${key}`);
                    if (element) {
                        element.value = state.settings[display][key];
                        const valueElement = document.getElementById(`${display}-${key}-value`);
                        if (valueElement) {
                            valueElement.textContent = state.settings[display][key];
                        }
                    }
                });
            });
        } catch (error) {
            console.error('Failed to load settings:', error);
        }
    }
}

// Reset settings to defaults
function resetSettings() {
    state.settings = {
        fld: {
            brightness: DISPLAY_CONFIG.defaultBrightness,
            hue: 0,
            fade: DISPLAY_CONFIG.defaultFade,
            delay: DISPLAY_CONFIG.defaultDelay,
            palette: 0  // Front default palette
        },
        rld: {
            brightness: DISPLAY_CONFIG.defaultBrightness,
            hue: 0,
            fade: DISPLAY_CONFIG.defaultFade,
            delay: DISPLAY_CONFIG.defaultDelay,
            palette: 1  // Rear default palette
        },
        speed: DISPLAY_CONFIG.defaultSpeed,
        targetDisplay: 'BOTH',
        duration: 0
    };
    
    // Update UI - Global settings
    ['speed', 'targetDisplay', 'duration'].forEach(key => {
        const element = document.getElementById(key);
        if (element) {
            element.value = state.settings[key];
            const valueElement = document.getElementById(`${key}-value`);
            if (valueElement) {
                valueElement.textContent = state.settings[key];
            }
        }
    });
    
    // Update UI - Per-display settings
    ['fld', 'rld'].forEach(display => {
        Object.keys(state.settings[display]).forEach(key => {
            const element = document.getElementById(`${display}-${key}`);
            if (element) {
                element.value = state.settings[display][key];
                const valueElement = document.getElementById(`${display}-${key}-value`);
                if (valueElement) {
                    valueElement.textContent = state.settings[display][key];
                }
            }
        });
    });
    
    saveSettings();
    
    if (state.currentAnimation) {
        updateCommands(state.currentAnimation);
    }
}

// Start application when DOM is ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
} else {
    init();
}