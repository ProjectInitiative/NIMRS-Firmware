#ifndef WEB_ASSETS_H
#define WEB_ASSETS_H

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>NIMRS Decoder</title>
    <link rel="stylesheet" href="style.css">
    <script src="lame.min.js"></script>
</head>
<body>
    <div class="container">
        <header>
            <h1>NIMRS Decoder</h1>
            <div id="connection-status" class="status-indicator disconnected"></div>
        </header>

        <nav>
            <button class="nav-btn active" onclick="showTab('dashboard')">Dashboard</button>
            <button class="nav-btn" onclick="showTab('cvs')">CVs</button>
            <button class="nav-btn" onclick="showTab('files')">Files</button>
            <button class="nav-btn" onclick="showTab('logs')">Logs</button>
            <button class="nav-btn" onclick="showTab('system')">System</button>
        </nav>

        <main>
            <!-- Dashboard Tab -->
            <section id="dashboard" class="tab-content active">
                <div class="card-grid">
                    <div class="card">
                        <h3>DCC Address</h3>
                        <div class="value" id="dcc-address">--</div>
                    </div>
                    <div class="card">
                        <h3>Speed</h3>
                        <div class="value" id="dcc-speed">0</div>
                    </div>
                    <div class="card">
                        <h3>Direction</h3>
                        <div class="value" id="dcc-direction">--</div>
                    </div>
                    <div class="card">
                        <h3>Uptime</h3>
                        <div class="value" id="uptime">0s</div>
                    </div>
                </div>

                <div class="control-panel">
                    <h3>Controls</h3>
                    <div class="button-group">
                        <button class="btn danger" onclick="sendAction('stop')">STOP</button>
                        <button class="btn primary" onclick="sendAction('toggle_lights')">Toggle Lights</button>
                    </div>
                    <br>
                    <div class="card" style="margin-top: 10px;">
                        <h3>Motor Control</h3>
                        <label>Speed Step: <span id="speed-display">0</span>/128</label>
                        <input type="range" id="speed-slider" min="0" max="128" value="0" style="width:100%" onchange="setSpeed(this.value)" oninput="document.getElementById('speed-display').innerText=this.value">
                        <br><br>
                        <label>Direction: 
                            <button id="dir-btn" class="btn" onclick="toggleDir()">FWD</button>
                        </label>
                    </div>

                    <div class="card" style="margin-top: 10px;">
                        <h3>Functions</h3>
                        <div id="func-grid" style="display: grid; grid-template-columns: repeat(auto-fill, minmax(60px, 1fr)); gap: 8px;">
                            <!-- Populated by JS -->
                        </div>
                    </div>
                </div>
            </section>

            <!-- CV Tab -->
            <section id="cvs" class="tab-content">
                <div class="card">
                    <h3>CV Configuration <button class="btn small" onclick="loadAllCVs()">Refresh</button></h3>
                    <table id="cv-table">
                        <thead><tr><th>CV</th><th>Name</th><th>Description</th><th>Value</th><th>Action</th></tr></thead>
                        <tbody></tbody>
                    </table>
                    <br>
                    <h4>Custom CV</h4>
                    <input type="number" id="custom-cv" placeholder="CV #" style="width:60px">
                    <input type="number" id="custom-val" placeholder="Value" style="width:60px">
                    <button class="btn small" onclick="rwCustomCV('read')">Read</button>
                    <button class="btn small" onclick="rwCustomCV('write')">Write</button>
                </div>
            </section>

            <!-- File Manager Tab -->
            <section id="files" class="tab-content">
                <div class="card" style="margin-bottom: 20px;">
                    <h3>Storage Usage</h3>
                    <div class="quota-container">
                        <div id="quota-bar" style="height: 20px; background: #333; border-radius: 10px; overflow: hidden;">
                            <div id="quota-fill" style="height: 100%; width: 0%; background: var(--primary-color); transition: width 0.5s;"></div>
                        </div>
                        <p id="quota-text">Loading storage info...</p>
                    </div>
                </div>

                <div class="file-upload">
                    <h3>Upload Files</h3>
                    <form id="upload-form">
                        <input type="file" id="file-input" multiple required>
                        <div style="margin: 10px 0;">
                            <label><input type="checkbox" id="compress-mp3"> Compress .wav to .mp3 (saves space)</label>
                        </div>
                        <button type="submit" class="btn primary">Upload Selected</button>
                    </form>
                    <div id="upload-status"></div>
                </div>

                <div class="file-list-container">
                    <h3>Filesystem</h3>
                    <div class="table-controls">
                        <button class="btn small" onclick="loadFiles()">Refresh</button>
                        <button class="btn small danger" onclick="deleteSelected()">Delete Selected</button>
                    </div>
                    <table id="file-table">
                        <thead>
                            <tr>
                                <th><input type="checkbox" onclick="toggleAll(this)"></th>
                                <th>Name</th>
                                <th>Size</th>
                                <th>Actions</th>
                            </tr>
                        </thead>
                        <tbody>
                            <!-- Files injected here -->
                        </tbody>
                    </table>
                </div>
            </section>

            <!-- Logs Tab -->
            <section id="logs" class="tab-content">
                <div class="log-controls">
                    <label><input type="checkbox" id="auto-scroll" checked> Auto-scroll</label>
                    <label><input type="checkbox" id="debug-logs" onchange="toggleDebugLogs(this)"> Debug Mode</label>
                    <button class="btn small" onclick="clearLogs()">Clear View</button>
                </div>
                <div id="log-viewer" class="terminal"></div>
            </section>

            <!-- System Tab -->
            <section id="system" class="tab-content">
                <div class="card">
                    <h3>Firmware Update</h3>
                    <p>Upload a new .bin file to update the firmware.</p>
                    <form method="POST" action="/update" enctype="multipart/form-data">
                        <input type="file" name="update">
                        <button type="submit" class="btn warning">Update Firmware</button>
                    </form>
                </div>
                
                 <div class="card">
                    <h3>WiFi Status</h3>
                    <div id="wifi-details">Loading...</div>
                </div>

                <div class="card">
                    <h3>WiFi Settings</h3>
                    <p>Update WiFi credentials. The device will restart.</p>
                    <form id="wifi-form" onsubmit="saveWifi(event)">
                        <label>SSID: 
                            <div style="display:flex; gap:5px;">
                                <input type="text" id="wifi-ssid" required style="flex-grow:1;">
                                <button type="button" class="btn small" onclick="scanWifi()">Scan</button>
                            </div>
                        </label>
                        <div id="scan-results" style="display:none; margin: 10px 0; border: 1px solid #444; padding: 5px;"></div>
                        <br>
                        <label>Pass: 
                            <div style="display:flex; gap:5px;">
                                <input type="password" id="wifi-pass" required style="flex-grow:1;">
                                <button type="button" class="btn small" onclick="togglePass(this)">Show</button>
                            </div>
                        </label><br><br>
                        <button type="submit" class="btn primary">Save & Connect</button>
                    </form>
                    <br>
                    <button class="btn danger" onclick="resetWifi()">Reset to Access Point</button>
                </div>

                <div class="card">
                   <h3>System Info</h3>
                   <p>Version: <span id="sys-version">--</span></p>
                   <p>Build: <span id="sys-hash">--</span></p>
                   <p>Device Name: <span id="sys-hostname">--</span></p>
                </div>

                <div class="card">
                    <h3>Device Settings</h3>
                    <label>Device Name:
                        <div style="display:flex; gap:5px;">
                             <input type="text" id="config-hostname" placeholder="NIMRS-Decoder" style="flex-grow:1;">
                             <button class="btn primary" onclick="saveHostname()">Save</button>
                        </div>
                    </label>
                    <p><small>Changes require restart.</small></p>
                </div>
            </section>
        </main>
    </div>
    <script src="app.js"></script>
</body>
</html>
)rawliteral";

const char STYLE_CSS[] PROGMEM = R"rawliteral(
:root {
    --bg-color: #121212;
    --card-bg: #1e1e1e;
    --text-color: #e0e0e0;
    --primary-color: #bb86fc;
    --secondary-color: #03dac6;
    --danger-color: #cf6679;
    --success-color: #4caf50;
    --font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
}

body {
    background-color: var(--bg-color);
    color: var(--text-color);
    font-family: var(--font-family);
    margin: 0;
    padding: 0;
    display: flex;
    justify-content: center;
}

.container {
    max-width: 800px;
    width: 100%;
    padding: 20px;
}

header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin-bottom: 20px;
    border-bottom: 1px solid #333;
    padding-bottom: 10px;
}

h1, h2, h3 { margin: 0 0 10px 0; }

.status-indicator {
    width: 15px;
    height: 15px;
    border-radius: 50%;
    background-color: #555;
    transition: background-color 0.3s;
}
.status-indicator.connected { background-color: var(--success-color); box-shadow: 0 0 10px var(--success-color); }
.status-indicator.disconnected { background-color: var(--danger-color); }

/* Navigation */
nav {
    display: flex;
    gap: 10px;
    margin-bottom: 20px;
    border-bottom: 1px solid #333;
}

.nav-btn {
    background: none;
    border: none;
    color: #888;
    padding: 10px 20px;
    cursor: pointer;
    font-size: 1rem;
    border-bottom: 2px solid transparent;
}

.nav-btn:hover { color: #fff; }
.nav-btn.active {
    color: var(--primary-color);
    border-bottom-color: var(--primary-color);
}

/* Tabs */
.tab-content { display: none; }
.tab-content.active { display: block; animation: fadeIn 0.3s; }

@keyframes fadeIn { from { opacity: 0; } to { opacity: 1; } }

/* Cards */
.card-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
    gap: 15px;
    margin-bottom: 20px;
}

.card {
    background-color: var(--card-bg);
    padding: 15px;
    border-radius: 8px;
    box-shadow: 0 2px 4px rgba(0,0,0,0.3);
}

.card .value {
    font-size: 1.5rem;
    font-weight: bold;
    color: var(--primary-color);
}

/* Controls */
.button-group {
    display: flex;
    gap: 10px;
    flex-wrap: wrap;
}

.btn {
    padding: 10px 20px;
    border: none;
    border-radius: 4px;
    cursor: pointer;
    font-size: 1rem;
    font-weight: bold;
    color: #000;
    transition: filter 0.2s;
}
.btn:hover { filter: brightness(1.1); }
.btn.primary { background-color: var(--primary-color); }
.btn.warning { background-color: #ff9800; }
.btn.danger { background-color: var(--danger-color); color: #fff; }
.btn.small { padding: 5px 10px; font-size: 0.8rem; background-color: #444; color: #fff; }

/* File Table */
table {
    width: 100%;
    border-collapse: collapse;
    margin-top: 10px;
}
th, td { text-align: left; padding: 10px; border-bottom: 1px solid #333; }
th { color: #888; }

/* Logs */
.terminal {
    background-color: #000;
    color: #0f0;
    font-family: monospace;
    height: 400px;
    min-height: 200px;
    overflow-y: auto;
    padding: 10px;
    border-radius: 4px;
    border: 1px solid #333;
    white-space: pre-wrap;
    resize: vertical;
}
.log-controls { margin-bottom: 5px; display: flex; justify-content: space-between; }
)rawliteral";

const char APP_JS[] PROGMEM = R"rawliteral(
// State
let currentTab = 'dashboard';
let logInterval = null;
let statusInterval = null;
let cvListRendered = false;
let trackableCVS = []; // Fetched from backend

// Initialization
document.addEventListener('DOMContentLoaded', () => {
    // Determine initial tab (maybe from hash)
    showTab('dashboard');
    renderFunctions();
    fetchCVDefs(); // Populate trackableCVS from Registry

    // Start polling status
    pollStatus();
    statusInterval = setInterval(pollStatus, 1000);

    // Setup File Upload
    document.getElementById('upload-form').addEventListener('submit', handleUpload);
});

async function fetchCVDefs() {
    try {
        const r = await fetch('/api/cv/defs');
        trackableCVS = await r.json();
        if (currentTab === 'cvs') renderCVTable();
    } catch (e) { console.error("Failed to fetch CV defs", e); }
}

function renderFunctions() {
    const grid = document.getElementById('func-grid');
    if(!grid) return;
    grid.innerHTML = '';
    for(let i=0; i<=28; i++) {
        const btn = document.createElement('button');
        btn.className = 'btn small';
        btn.id = `f${i}-btn`;
        btn.innerText = `F${i}`;
        btn.style.backgroundColor = '#444';
        btn.onclick = () => toggleFunc(i);
        grid.appendChild(btn);
    }
}

function toggleFunc(i) {
    const btn = document.getElementById(`f${i}-btn`);
    const newState = !btn.classList.contains('active-func');
    // Optimistic update
    updateFuncBtn(i, newState);
    
    sendAction('set_function', newState, i);
}

function updateFuncBtn(i, active) {
    const btn = document.getElementById(`f${i}-btn`);
    if(!btn) return;
    if(active) {
        btn.classList.add('active-func');
        btn.style.backgroundColor = 'var(--primary-color)';
        btn.style.color = '#000';
    } else {
        btn.classList.remove('active-func');
        btn.style.backgroundColor = '#444';
        btn.style.color = '#fff';
    }
}

function showTab(tabId) {
    // Update Nav
    document.querySelectorAll('.nav-btn').forEach(b => b.classList.remove('active'));
    const navBtn = document.querySelector(`.nav-btn[onclick="showTab('${tabId}')"]`);
    if (navBtn) navBtn.classList.add('active');

    // Update Content
    document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
    const tabEl = document.getElementById(tabId);
    if (tabEl) tabEl.classList.add('active');

    currentTab = tabId;

    // Specific logic per tab
    if (tabId === 'logs') {
        if (!logInterval) {
            pollLogs(); // Immediate fetch
            logInterval = setInterval(pollLogs, 1000);
        }
    } else {
        if (logInterval) {
            clearInterval(logInterval);
            logInterval = null;
        }
    }

    if (tabId === 'files') loadFiles();
    if (tabId === 'cvs') renderCVTable();
}

// --- API Interactions ---

function pollStatus() {
    fetch('/api/status')
        .then(r => r.json())
        .then(data => {
            const indicator = document.getElementById('connection-status');
            indicator.classList.remove('disconnected');
            indicator.classList.add('connected');
            
            document.getElementById('dcc-address').innerText = data.address || '--';
            document.getElementById('dcc-speed').innerText = data.speed || '0';
            
            const dirStr = data.direction; 
            document.getElementById('dcc-direction').innerText = dirStr;
            
            document.getElementById('uptime').innerText = formatUptime(data.uptime);
            
            // Sync controls
            if (document.activeElement.id !== 'speed-slider') {
                document.getElementById('speed-slider').value = data.speed;
                document.getElementById('speed-display').innerText = data.speed;
            }
            
            const isFwd = (dirStr === "forward");
            const btn = document.getElementById('dir-btn');
            if (btn) {
                btn.innerText = isFwd ? "FWD" : "REV";
                btn.dataset.dir = isFwd; 
            }
            
            // Sync Functions
            if (data.functions) {
                data.functions.forEach((state, idx) => {
                    updateFuncBtn(idx, state);
                });
            }
            
            // System tab detail
            document.getElementById('wifi-details').innerText = `Connected: ${data.wifi ? 'Yes' : 'No'}`;
            if(document.getElementById('sys-version')) document.getElementById('sys-version').innerText = data.version || 'Unknown';
            if(document.getElementById('sys-hash')) document.getElementById('sys-hash').innerText = data.hash || 'Unknown';
            if(document.getElementById('sys-hostname')) {
                document.getElementById('sys-hostname').innerText = data.hostname || 'NIMRS-Decoder';
                // Only update input if not focused to avoid typing interruption
                const hnInput = document.getElementById('config-hostname');
                if (hnInput && document.activeElement !== hnInput && !hnInput.value) {
                    hnInput.value = data.hostname || 'NIMRS-Decoder';
                }
            }

            // Quota
            if (data.fs_total) {
                const used = data.fs_used || 0;
                const total = data.fs_total;
                const perc = Math.round((used / total) * 100);
                const fill = document.getElementById('quota-fill');
                if (fill) fill.style.width = perc + '%';
                const text = document.getElementById('quota-text');
                if (text) text.innerText = `${formatBytes(used)} / ${formatBytes(total)} (${perc}%)`;
            }
        })
        .catch(e => {
            const indicator = document.getElementById('connection-status');
            indicator.classList.remove('connected');
            indicator.classList.add('disconnected');
        });
}

function sendAction(action, value, index) {
    const payload = { action };
    if (value !== undefined) payload.value = value;
    if (index !== undefined) payload.index = index;

    fetch('/api/control', { 
        method: 'POST', 
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(payload) 
    })
    .then(r => {
        if (!r.ok) console.error("Control failed");
        else pollStatus(); 
    });
}

function setSpeed(val) {
    sendAction('set_speed', parseInt(val));
}

function toggleDir() {
    const btn = document.getElementById('dir-btn');
    const currentFwd = (btn.innerText === "FWD");
    sendAction('set_direction', !currentFwd);
}

// --- CV Management ---

function renderCVTable() {
    const tbody = document.querySelector('#cv-table tbody');
    if (!tbody) return;

    // Capture existing values
    const existingValues = {};
    document.querySelectorAll('[id^="cv-val-"]').forEach(input => {
        existingValues[input.id.replace('cv-val-', '')] = input.value;
    });

    tbody.innerHTML = '';
    
    // Sort CVs by number
    trackableCVS.sort((a, b) => a.cv - b.cv);

    trackableCVS.forEach(item => {
        const tr = document.createElement('tr');
        const val = existingValues[item.cv] || '';
        tr.innerHTML = `
            <td>${item.cv}</td>
            <td><b>${item.name || 'Custom'}</b></td>
            <td><small>${item.desc || '-'}</small></td>
            <td><input type="number" id="cv-val-${item.cv}" style="width:60px" value="${val}" placeholder="?"></td>
            <td>
                <button class="btn small" onclick="writeCV(${item.cv})">Save</button>
            </td>
        `;
        tbody.appendChild(tr);
    });
    cvListRendered = true;
}

async function loadAllCVs() {
    for (const item of trackableCVS) {
        await readCV(item.cv);
    }
}

function readCV(cv) {
    return fetch('/api/cv', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({ cmd: 'read', cv: cv })
    })
    .then(r => r.json())
    .then(data => {
        // If this CV isn't in our list, add it
        if (!trackableCVS.find(c => c.cv === cv)) {
            trackableCVS.push({cv: cv, name: "Custom", desc: "User added CV"});
            renderCVTable();
        }
        const input = document.getElementById(`cv-val-${cv}`);
        if (input) input.value = data.value;
        
        // Also update custom fields if they match
        if (cv === parseInt(document.getElementById('custom-cv').value)) {
             document.getElementById('custom-val').value = data.value;
        }
    });
}

function writeCV(cv) {
    const input = document.getElementById(`cv-val-${cv}`);
    if (!input || input.value === '') return;
    doWriteCV(cv, parseInt(input.value));
}

function doWriteCV(cv, val) {
    fetch('/api/cv', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({ cmd: 'write', cv: cv, value: val })
    })
    .then(r => {
        if (r.ok) {
            if (!trackableCVS.find(c => c.cv === cv)) {
                trackableCVS.push({cv: cv, name: "Custom", desc: "User added CV"});
                renderCVTable();
            }
            alert(`CV${cv} Saved`); 
        } else alert("Write Failed");
    });
}

function rwCustomCV(mode) {
    const cv = parseInt(document.getElementById('custom-cv').value);
    if (!cv) return;
    if (mode === 'read') readCV(cv);
    else {
        const val = parseInt(document.getElementById('custom-val').value);
        if (isNaN(val)) return;
        doWriteCV(cv, val);
    }
}

// --- File Manager ---

function loadFiles() {
    fetch('/api/files/list')
        .then(r => r.json())
        .then(files => {
            const tbody = document.querySelector('#file-table tbody');
            tbody.innerHTML = '';
            files.forEach(f => {
                const tr = document.createElement('tr');
                const isAudio = f.name.toLowerCase().endsWith('.wav') || f.name.toLowerCase().endsWith('.mp3');
                tr.innerHTML = `
                    <td><input type="checkbox" class="file-check" value="${f.name}"></td>
                    <td>${f.name}</td>
                    <td>${formatBytes(f.size)}</td>
                    <td>
                        ${isAudio ? `<button class="btn small primary" onclick="playAudio('${f.name}')">Play</button>` : ''}
                        <a href="/${f.name}" class="btn small" download>DL</a>
                        <button class="btn small danger" onclick="deleteFile('${f.name}')">Del</button>
                    </td>
                `;
                tbody.appendChild(tr);
            });
        });
}

function formatBytes(bytes, decimals = 2) {
    if (!+bytes) return '0 Bytes';
    const k = 1024;
    const dm = decimals < 0 ? 0 : decimals;
    const sizes = ['Bytes', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return `${parseFloat((bytes / Math.pow(k, i)).toFixed(dm))} ${sizes[i]}`;
}

function playAudio(filename) {
    fetch('/api/audio/play?file=' + encodeURIComponent(filename), { method: 'POST' })
    .then(r => r.text())
    .then(msg => console.log("Audio:", msg))
    .catch(e => alert("Audio Error: " + e));
}

function toggleAll(source) {
    document.querySelectorAll('.file-check').forEach(c => c.checked = source.checked);
}

async function handleUpload(e) {
    e.preventDefault();
    const input = document.getElementById('file-input');
    const files = input.files;
    const compress = document.getElementById('compress-mp3').checked;
    if (!files.length) return;

    const statusDiv = document.getElementById('upload-status');
    statusDiv.innerText = "Starting upload...";

    for (let i = 0; i < files.length; i++) {
        let file = files[i];
        let filename = file.name;

        if (compress && filename.toLowerCase().endsWith('.wav')) {
            statusDiv.innerText = `Compressing ${filename}...`;
            try {
                const mp3Data = await compressToMp3(file);
                file = new File([mp3Data], filename.replace(/\.wav$/i, '.mp3'), { type: 'audio/mpeg' });
                filename = file.name;
            } catch (err) {
                console.error("Compression failed", err);
                statusDiv.innerText = `Compression failed for ${filename}, uploading original...`;
            }
        }

        statusDiv.innerText = `Uploading ${i+1}/${files.length}: ${filename}...`;
        const formData = new FormData();
        formData.append("file", file);
        try {
            const r = await fetch('/api/files/upload', {
                method: 'POST',
                body: formData
            });
            if (!r.ok) throw new Error(r.statusText);
        } catch (err) {
            statusDiv.innerText = `Error uploading ${filename}: ${err}`;
            return;
        }
    }
    statusDiv.innerText = "All uploads complete!";
    input.value = ''; 
    loadFiles();
    pollStatus(); // Refresh quota
}

async function compressToMp3(file) {
    if (typeof lamejs === 'undefined') {
        throw new Error("lamejs library not loaded. Check /lame.min.js");
    }
    return new Promise((resolve, reject) => {
        const reader = new FileReader();
        reader.onload = async (e) => {
            try {
                const audioCtx = new (window.AudioContext || window.webkitAudioContext)();
                const arrayBuffer = e.target.result;
                const audioBuffer = await audioCtx.decodeAudioData(arrayBuffer);
                
                const channels = audioBuffer.numberOfChannels;
                const sampleRate = audioBuffer.sampleRate;
                const mp3encoder = new lamejs.Mp3Encoder(channels, sampleRate, 128);
                const mp3Data = [];

                const samplesL = audioBuffer.getChannelData(0);
                const samplesR = channels > 1 ? audioBuffer.getChannelData(1) : samplesL;
                
                // Convert Float32 samples to Int16
                const convert = (samples) => {
                    const int16 = new Int16Array(samples.length);
                    for (let i = 0; i < samples.length; i++) {
                        let s = Math.max(-1, Math.min(1, samples[i]));
                        int16[i] = s < 0 ? s * 0x8000 : s * 0x7FFF;
                    }
                    return int16;
                };

                const blockSide = 1152;
                const int16L = convert(samplesL);
                const int16R = channels > 1 ? convert(samplesR) : null;

                for (let i = 0; i < int16L.length; i += blockSide) {
                    const leftChunk = int16L.subarray(i, i + blockSide);
                    let mp3buf;
                    if (channels > 1) {
                        const rightChunk = int16R.subarray(i, i + blockSide);
                        mp3buf = mp3encoder.encodeBuffer(leftChunk, rightChunk);
                    } else {
                        mp3buf = mp3encoder.encodeBuffer(leftChunk);
                    }
                    if (mp3buf.length > 0) mp3Data.push(mp3buf);
                }

                const end = mp3encoder.flush();
                if (end.length > 0) mp3Data.push(end);

                console.log(`Compressed ${file.name}: ${file.size} -> ${new Blob(mp3Data).size} bytes`);
                resolve(new Blob(mp3Data, { type: 'audio/mpeg' }));
            } catch (err) {
                console.error("Compression error:", err);
                reject(err);
            }
        };
        reader.onerror = reject;
        reader.readAsArrayBuffer(file);
    });
}

function deleteFile(filename) {
    if (!confirm(`Delete ${filename}?`)) return;
    performDelete(filename).then(() => loadFiles());
}

async function deleteSelected() {
    const checked = document.querySelectorAll('.file-check:checked');
    if (!checked.length) return alert("No files selected");
    if (!confirm(`Delete ${checked.length} files?`)) return;
    for (const c of checked) await performDelete(c.value);
    loadFiles();
}

function performDelete(filename) {
    return fetch('/api/files/delete', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: `path=/${filename}`
    });
}

// --- Logs ---
let clearedTimestamp = 0;

function pollLogs() {
    fetch('/api/logs')
        .then(r => r.json())
        .then(lines => {
            const viewer = document.getElementById('log-viewer');
            const newLines = lines.filter(line => {
                const match = line.match(/^\\[(\d+)\\]/);
                if (match) return parseInt(match[1]) > clearedTimestamp;
                return true; 
            });
            viewer.innerHTML = newLines.join('\n');
            if (document.getElementById('auto-scroll').checked) {
                viewer.scrollTop = viewer.scrollHeight;
            }
        });
}

function clearLogs() {
    fetch('/api/logs')
        .then(r => r.json())
        .then(lines => {
            if (lines.length > 0) {
                const lastLine = lines[lines.length - 1];
                const match = lastLine.match(/^\\[(\d+)\\]/);
                if (match) clearedTimestamp = parseInt(match[1]);
            }
            document.getElementById('log-viewer').innerHTML = '';
        });
}

function toggleDebugLogs(el) {
    sendAction('set_log_level', el.checked ? 0 : 1);
}

// Utils
function formatUptime(s) {
    const h = Math.floor(s / 3600);
    const m = Math.floor((s % 3600) / 60);
    const sec = s % 60;
    return `${h}h ${m}m ${sec}s`;
}

// --- WiFi Management ---

function togglePass(btn) {
    const el = document.getElementById('wifi-pass');
    if (el.type === "password") {
        el.type = "text";
        btn.innerText = "Hide";
    } else {
        el.type = "password";
        btn.innerText = "Show";
    }
}

function scanWifi() {
    const res = document.getElementById('scan-results');
    res.style.display = 'block';
    res.innerText = 'Scanning...';
    fetch('/api/wifi/scan')
        .then(r => r.json())
        .then(nets => {
            res.innerHTML = '';
            if (nets.length === 0) {
                res.innerText = "No networks found";
                return;
            }
            nets.forEach(n => {
                const div = document.createElement('div');
                div.innerHTML = `<a href="#" onclick="selectNetwork('${n.ssid}')">${n.ssid}</a> (${n.rssi} dBm)`;
                div.style.padding = "5px";
                div.style.cursor = "pointer";
                div.style.borderBottom = "1px solid #333";
                res.appendChild(div);
            });
        })
        .catch(e => res.innerText = "Scan failed: " + e);
}

function selectNetwork(ssid) {
    document.getElementById('wifi-ssid').value = ssid;
    document.getElementById('scan-results').style.display = 'none';
}

function saveWifi(e) {
    e.preventDefault();
    const ssid = document.getElementById('wifi-ssid').value;
    const pass = document.getElementById('wifi-pass').value;
    if (!confirm("Device will restart and try to connect. Continue?")) return;
    fetch('/api/wifi/save', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: `ssid=${encodeURIComponent(ssid)}&pass=${encodeURIComponent(pass)}`
    })
    .then(() => alert("Saved! Restarting..."))
    .catch(e => alert("Error: " + e));
}

function saveHostname() {
    const name = document.getElementById('config-hostname').value;
    if (!name || name.length < 1) return alert("Invalid name");
    if (!confirm(`Rename device to '${name}' and restart?`)) return;
    
    fetch('/api/config/hostname', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: `name=${encodeURIComponent(name)}`
    })
    .then(r => r.text())
    .then(msg => alert(msg))
    .catch(e => alert("Error: " + e));
}

function resetWifi() {
    if (!confirm("Forget saved WiFi and restart into AP mode?")) return;
    fetch('/api/wifi/reset', { method: 'POST' })
    .then(() => alert("Resetting... Connect to 'NIMRS-Decoder' AP."))
    .catch(e => alert("Error: " + e));
}
)rawliteral";

#endif