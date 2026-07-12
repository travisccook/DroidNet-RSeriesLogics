const esbuild = require('esbuild');
const fs = require('fs');
const path = require('path');

// Ensure dist directory exists
if (!fs.existsSync('dist')) {
    fs.mkdirSync('dist');
}

// Copy HTML and CSS files
fs.copyFileSync('src/index.html', 'dist/index.html');
fs.copyFileSync('src/styles.css', 'dist/styles.css');

// Build configuration
const buildOptions = {
    entryPoints: ['src/app.js'],
    bundle: true,
    outfile: 'dist/bundle.js',
    format: 'iife',
    target: 'es2020',
    sourcemap: true,
    minify: process.env.NODE_ENV === 'production',
    loader: {
        '.js': 'js'
    },
    define: {
        'process.env.NODE_ENV': '"development"'
    }
};

// Check if watch mode
const isWatch = process.argv.includes('--watch');

async function build() {
    try {
        if (isWatch) {
            // Watch mode
            const ctx = await esbuild.context(buildOptions);
            await ctx.watch();
            console.log('Watching for changes...');
            
            // Also watch HTML and CSS files
            fs.watchFile('src/index.html', () => {
                fs.copyFileSync('src/index.html', 'dist/index.html');
                console.log('Copied index.html');
            });
            
            fs.watchFile('src/styles.css', () => {
                fs.copyFileSync('src/styles.css', 'dist/styles.css');
                console.log('Copied styles.css');
            });
        } else {
            // Single build
            await esbuild.build(buildOptions);
            console.log('Build complete!');
        }
    } catch (error) {
        console.error('Build failed:', error);
        process.exit(1);
    }
}

build();