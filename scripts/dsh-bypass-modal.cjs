#!/usr/bin/env node
// Click the "继续" modal button on the dsh web UI via CDP, then print "ready".
const fetch_ = globalThis.fetch;
const WebSocket_ = globalThis.WebSocket;

const port = process.argv[2] || '18777';
const cdpPort = 9223;

async function main() {
    // Launch chrome with remote debugging
    const { spawn } = require('node:child_process');
    const path = require('node:path');
    const fs = require('node:fs');

    const userDataDir = path.join(process.env.TEMP || '/tmp', 'cdp-chrome-' + Date.now());
    fs.mkdirSync(userDataDir, {recursive: true});

    const chrome = spawn(
        'D:/shared/work/WorkBuddy/Temp/chrome-win64/chrome.exe',
        [
            '--headless=new',
            '--no-sandbox', '--disable-gpu',
            '--remote-debugging-port=' + cdpPort,
            '--user-data-dir=' + userDataDir,
            '--window-size=1440,900',
            `http://127.0.0.1:${port}`,
        ],
        {stdio: 'ignore', detached: true}
    );
    chrome.unref();

    // Wait for CDP to be ready
    let tabs;
    for (let i = 0; i < 30; i++) {
        await new Promise(r => setTimeout(r, 500));
        try {
            const r = await fetch_(`http://127.0.0.1:${cdpPort}/json`);
            tabs = await r.json();
            if (tabs.length) break;
        } catch {}
    }
    if (!tabs || !tabs.length) { console.error('CDP not ready'); process.exit(1); }

    const ws = new WebSocket_(tabs[0].webSocketDebuggerUrl);
    await new Promise(r => ws.onopen = r);
    let id = 0;
    const send = (method, params={}) => new Promise(r => {
        const myId = ++id;
        const h = (e) => { if (JSON.parse(e.data).id === myId) { ws.removeEventListener('message', h); r(JSON.parse(e.data)); } };
        ws.addEventListener('message', h);
        ws.send(JSON.stringify({id: myId, method, params}));
    });

    // Wait for the page to render the modal, then click "继续".
    await new Promise(r => setTimeout(r, 4000));
    await send('Runtime.evaluate', { expression: `
        (() => {
            const btns = [...document.querySelectorAll('button')];
            const target = btns.find(b => b.textContent.trim() === '继续');
            if (target) target.click();
            return target ? 'clicked' : 'not-found';
        })()
    `, returnByValue: true });
    await new Promise(r => setTimeout(r, 1500));
    console.log('ready');
}

main().catch(e => { console.error(e); process.exit(1); });