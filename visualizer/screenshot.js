const puppeteer = require('puppeteer');
const path = require('path');

(async () => {
    const browser = await puppeteer.launch();
    const page = await browser.newPage();
    
    // Set viewport to 1920x1080
    await page.setViewport({ width: 1920, height: 1080 });
    
    // Load the local HTML file
    const filePath = `file://${path.join(__dirname, 'dist', 'index.html')}`;
    await page.goto(filePath, { waitUntil: 'networkidle0' });
    
    // Wait a bit for any animations to complete
    await new Promise(resolve => setTimeout(resolve, 1000));
    
    // Take screenshot
    await page.screenshot({ 
        path: 'visualizer-screenshot.png',
        fullPage: false
    });
    
    console.log('Screenshot saved as visualizer-screenshot.png');
    
    await browser.close();
})();