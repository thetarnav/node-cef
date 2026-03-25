import * as cef from '../src/index.ts'
import * as bun from 'bun'
import * as path from 'node:path'

async function main() {
    const app = cef.createApp();
 
    app.onReady = () => {
        console.log('App is ready');
    }

    await app.start();

    const html = await bun.file(path.join(import.meta.dir, 'index.html')).text()

    const win = await app.createWindow({
        html,
        width: 800,
        height: 600,
        devtools: true,
        title: 'Bun CEF Example',
    });
  
    console.log('Window created. Press Ctrl+C to quit.');

    win.onMessage = (msg) => {
        console.log('Message from window:', msg);
        win.postMessage('echo:' + msg);
    }

    win.onClose =  () => {
        console.log('Window Closed! Bye bye..');
        app.quit();
        process.exit(0);
    }

    // Keep process alive
    process.on('sigint', () => {
        console.log('quitting...');
        app.quit();
        process.exit(0);
    })
}

main().catch(console.error);
