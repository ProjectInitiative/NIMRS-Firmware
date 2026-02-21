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
            <div class="header-content">
                <h1>NIMRS Decoder</h1>
                <div id="connection-status" class="status-indicator disconnected" title="Connection Status"></div>
            </div>
        </header>

        <nav>
            <button class="nav-btn" onclick="showTab('dashboard')">Dashboard</button>
            <button class="nav-btn" onclick="showTab('cvs')">CVs</button>
            <button class="nav-btn" onclick="showTab('files')">Files</button>
            <button class="nav-btn" onclick="showTab('logs')">Logs</button>
            <button class="nav-btn" onclick="showTab('debug')">Debug</button>
            <button class="nav-btn" onclick="showTab('system')">System</button>
        </nav>

        <main>
            <!-- Dashboard Tab -->
            <section id="dashboard" class="tab-content">
                <div class="dashboard-grid">
                    <!-- Status Cards -->
                    <div class="card-group">
                        <div class="card mini-card">
                            <h3>Addr</h3>
                            <div class="value" id="dcc-address">--</div>
                        </div>
                        <div class="card mini-card">
                            <h3>Speed</h3>
                            <div class="value" id="dcc-speed">0</div>
                        </div>
                        <div class="card mini-card">
                            <h3>Dir</h3>
                            <div class="value" id="dcc-direction">--</div>
                        </div>
                         <div class="card mini-card">
                            <h3>Uptime</h3>
                            <div class="value" id="uptime">0s</div>
                        </div>
                    </div>

                    <!-- Main Controls -->
                    <div class="card control-card">
                        <h3>Throttle</h3>
                        <div class="slider-container">
                            <input type="range" id="speed-slider" min="0" max="126" value="0" class="slider" onchange="setSpeed(this.value)" oninput="updateSpeedDisplay(this.value)">
                            <div class="slider-labels">
                                <span>STOP</span>
                                <span id="speed-display" class="highlight">0</span>
                                <span>MAX</span>
                            </div>
                        </div>
                        <div class="control-actions">
                            <button id="dir-btn" class="btn large" onclick="toggleDir()">FWD</button>
                            <button class="btn danger large" onclick="sendAction('stop')">STOP</button>
                            <button class="btn primary large" onclick="sendAction('toggle_lights')">Lights</button>
                        </div>
                    </div>

                    <!-- Functions -->
                    <div class="card">
                        <h3>Functions</h3>
                        <div id="func-grid" class="func-grid">
                            <!-- Populated by JS -->
                        </div>
                    </div>
                </div>
            </section>

            <!-- CV Tab -->
            <section id="cvs" class="tab-content">
                <div class="card">
                    <div class="card-header">
                        <h3>CV Configuration</h3>
                        <div class="search-box">
                            <input type="text" id="cv-search" placeholder="Search CVs..." onkeyup="renderCVTable()">
                            <button class="btn small" onclick="loadAllCVs()">Reload</button>
                        </div>
                    </div>

                    <div class="table-container">
                        <table id="cv-table">
                            <thead><tr><th style="width:50px">CV</th><th>Name</th><th>Value</th><th>Description</th></tr></thead>
                            <tbody></tbody>
                        </table>
                    </div>

                    <div class="custom-cv-box">
                        <h4>Custom Access</h4>
                        <div style="display:flex; gap:10px;">
                            <input type="number" id="custom-cv" placeholder="CV #" style="width:80px">
                            <input type="number" id="custom-val" placeholder="Value" style="width:80px">
                            <button class="btn small" onclick="rwCustomCV('read')">Read</button>
                            <button class="btn small primary" onclick="rwCustomCV('write')">Write</button>
                        </div>
                    </div>
                </div>
            </section>

            <!-- File Manager Tab -->
            <section id="files" class="tab-content">
                <div class="card quota-card">
                    <h3>Storage Usage</h3>
                    <div class="quota-container">
                        <div id="quota-bar" class="quota-bar"><div id="quota-fill"></div></div>
                        <p id="quota-text">Loading...</p>
                    </div>
                </div>

                <div class="file-manager">
                    <div class="card upload-card">
                        <h3>Upload</h3>
                        <form id="upload-form">
                            <input type="file" id="file-input" multiple required>
                            <label class="checkbox-label"><input type="checkbox" id="compress-mp3"> Compress .wav to .mp3</label>
                            <button type="submit" class="btn primary full-width">Upload</button>
                        </form>
                        <div id="upload-status"></div>
                    </div>

                    <div class="card file-list-card">
                        <div class="card-header">
                            <h3>Files</h3>
                            <div class="table-controls">
                                <button class="btn small" onclick="loadFiles()">Refresh</button>
                                <button class="btn small danger" onclick="deleteSelected()">Delete Selected</button>
                            </div>
                        </div>
                        <div class="table-container">
                            <table id="file-table">
                                <thead>
                                    <tr>
                                        <th style="width:30px"><input type="checkbox" onclick="toggleAll(this)"></th>
                                        <th>Name</th>
                                        <th>Size</th>
                                        <th>Actions</th>
                                    </tr>
                                </thead>
                                <tbody></tbody>
                            </table>
                        </div>
                    </div>
                </div>
            </section>

            <!-- Logs Tab -->
            <section id="logs" class="tab-content">
                <div class="log-controls">
                    <div class="control-group">
                        <label><input type="checkbox" id="auto-scroll" checked> Auto-scroll</label>
                        <select id="log-type-filter" onchange="pollLogs()">
                            <option value="">System Logs</option>
                            <option value="data">Telemetry Data</option>
                            <option value="debug">DCC Debug</option>
                        </select>
                    </div>
                    <div class="control-group">
                        <label><input type="checkbox" id="debug-logs" onchange="toggleDebugLogs(this)"> Debug Level</label>
                        <button class="btn small" onclick="clearLogs()">Clear</button>
                    </div>
                </div>
                <div id="log-viewer" class="terminal"></div>
            </section>

            <!-- Debug Tab -->
            <section id="debug" class="tab-content">
                <div class="card">
                    <h3>Motor Self-Test</h3>
                    <p>Runs a 3-second motor profile. Ensure track is clear.</p>
                    <button class="btn warning" onclick="runMotorTest()">Run Test (3s)</button>
                    <div class="result-box">
                        <textarea id="test-results" readonly placeholder="Results will appear here..."></textarea>
                        <button class="btn small" onclick="copyTestResults()">Copy JSON</button>
                    </div>
                </div>
            </section>

            <!-- System Tab -->
            <section id="system" class="tab-content">
                <div class="card">
                    <h3>System Information</h3>
                    <div class="info-grid">
                        <div><strong>Version:</strong> <span id="sys-version">--</span></div>
                        <div><strong>Build:</strong> <span id="sys-hash">--</span></div>
                        <div><strong>WiFi:</strong> <span id="wifi-details">--</span></div>
                        <div><strong>Hostname:</strong> <span id="sys-hostname">--</span></div>
                    </div>
                </div>

                <details class="settings-group">
                    <summary>WiFi Settings</summary>
                    <div class="settings-content">
                        <form id="wifi-form" onsubmit="saveWifi(event)">
                            <div class="form-group">
                                <label>SSID</label>
                                <div class="input-group">
                                    <input type="text" id="wifi-ssid" required>
                                    <button type="button" class="btn secondary" onclick="scanWifi()">Scan</button>
                                </div>
                            </div>
                            <div id="scan-results" class="scan-results"></div>
                            <div class="form-group">
                                <label>Password</label>
                                <div class="input-group">
                                    <input type="password" id="wifi-pass" required>
                                    <button type="button" class="btn secondary" onclick="togglePass(this)">Show</button>
                                </div>
                            </div>
                            <div class="actions">
                                <button type="submit" class="btn primary">Save & Connect</button>
                                <button type="button" class="btn danger" onclick="resetWifi()">Reset to AP</button>
                            </div>
                        </form>
                    </div>
                </details>

                <details class="settings-group">
                    <summary>Device Settings</summary>
                    <div class="settings-content">
                        <label>Device Name</label>
                        <div class="input-group">
                             <input type="text" id="config-hostname" placeholder="NIMRS-Decoder">
                             <button class="btn primary" onclick="saveHostname()">Save</button>
                        </div>
                        <p class="hint">Changes require restart.</p>
                    </div>
                </details>

                <details class="settings-group">
                    <summary>Security</summary>
                    <div class="settings-content">
                        <form id="auth-form" onsubmit="saveAuth(event)">
                            <div class="form-group">
                                <label>Username</label>
                                <input type="text" id="web-user" placeholder="admin">
                            </div>
                            <div class="form-group">
                                <label>Password</label>
                                <div class="input-group">
                                    <input type="password" id="web-pass" placeholder="admin">
                                    <button type="button" class="btn secondary" onclick="togglePass(this, 'web-pass')">Show</button>
                                </div>
                            </div>
                            <button type="submit" class="btn primary">Update Credentials</button>
                        </form>
                        <p class="hint">Leave blank to disable authentication.</p>
                    </div>
                </details>

                <details class="settings-group">
                    <summary>Firmware Update</summary>
                    <div class="settings-content">
                        <form method="POST" action="/update" enctype="multipart/form-data">
                            <input type="file" name="update" class="file-input-btn">
                            <button type="submit" class="btn warning">Update Firmware</button>
                        </form>
                    </div>
                </details>
            </section>
        </main>
    </div>
    <div id="toast-container"></div>
    <script src="app.js"></script>
</body>
</html>
)rawliteral";

const char STYLE_CSS[] PROGMEM = R"rawliteral(
:root {
    --bg-color: #121212;
    --card-bg: #1e1e1e;
    --text-color: #e0e0e0;
    --text-muted: #a0a0a0;
    --primary-color: #bb86fc;
    --primary-dark: #3700b3;
    --secondary-color: #03dac6;
    --danger-color: #cf6679;
    --success-color: #4caf50;
    --warning-color: #ff9800;
    --border-color: #333;
    --input-bg: #2c2c2c;
    --font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
}

body {
    background-color: var(--bg-color);
    color: var(--text-color);
    font-family: var(--font-family);
    margin: 0;
    padding: 0;
    line-height: 1.6;
}

.container {
    max-width: 900px;
    margin: 0 auto;
    padding: 20px;
}

/* Header */
header {
    margin-bottom: 20px;
    border-bottom: 1px solid var(--border-color);
    padding-bottom: 10px;
}

.header-content {
    display: flex;
    justify-content: space-between;
    align-items: center;
}

h1, h2, h3, h4 { margin: 0 0 10px 0; color: #fff; }

.status-indicator {
    width: 12px;
    height: 12px;
    border-radius: 50%;
    background-color: #555;
    transition: background-color 0.3s, box-shadow 0.3s;
}
.status-indicator.connected { background-color: var(--success-color); box-shadow: 0 0 8px var(--success-color); }
.status-indicator.disconnected { background-color: var(--danger-color); }

/* Navigation */
nav {
    display: flex;
    gap: 5px;
    margin-bottom: 20px;
    overflow-x: auto;
    padding-bottom: 5px;
    border-bottom: 1px solid var(--border-color);
}

.nav-btn {
    background: none;
    border: none;
    color: var(--text-muted);
    padding: 10px 15px;
    cursor: pointer;
    font-size: 1rem;
    font-weight: 500;
    border-bottom: 2px solid transparent;
    transition: color 0.2s, border-color 0.2s;
    white-space: nowrap;
}

.nav-btn:hover { color: #fff; }
.nav-btn.active {
    color: var(--primary-color);
    border-bottom-color: var(--primary-color);
}

/* Tabs */
.tab-content { display: none; }
.tab-content.active { display: block; animation: fadeIn 0.2s; }

@keyframes fadeIn { from { opacity: 0; transform: translateY(5px); } to { opacity: 1; transform: translateY(0); } }

/* Cards & Layout */
.card {
    background-color: var(--card-bg);
    padding: 20px;
    border-radius: 8px;
    box-shadow: 0 4px 6px rgba(0,0,0,0.2);
    margin-bottom: 20px;
}

.card-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 15px;
    flex-wrap: wrap;
    gap: 10px;
}

.dashboard-grid {
    display: grid;
    gap: 20px;
    grid-template-columns: 1fr;
}

@media (min-width: 768px) {
    .dashboard-grid {
        grid-template-columns: repeat(2, 1fr);
    }
    .control-card { grid-column: span 2; }
    .card-group { grid-column: span 2; display: flex; gap: 20px; }
    .mini-card { flex: 1; margin-bottom: 0; }
}

.mini-card {
    text-align: center;
    padding: 15px;
}

.mini-card .value {
    font-size: 1.8rem;
    font-weight: bold;
    color: var(--primary-color);
}

/* Controls */
.slider-container {
    padding: 20px 0;
}

.slider {
    -webkit-appearance: none;
    width: 100%;
    height: 10px;
    border-radius: 5px;
    background: #444;
    outline: none;
    transition: background 0.2s;
}

.slider::-webkit-slider-thumb {
    -webkit-appearance: none;
    appearance: none;
    width: 24px;
    height: 24px;
    border-radius: 50%;
    background: var(--primary-color);
    cursor: pointer;
    box-shadow: 0 0 5px rgba(0,0,0,0.5);
}

.slider-labels {
    display: flex;
    justify-content: space-between;
    margin-top: 10px;
    color: var(--text-muted);
    font-size: 0.9rem;
}

.highlight { color: #fff; font-weight: bold; }

.control-actions {
    display: flex;
    gap: 15px;
    justify-content: center;
    margin-top: 20px;
    flex-wrap: wrap;
}

.func-grid {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(60px, 1fr));
    gap: 10px;
}

/* Buttons */
.btn {
    padding: 8px 16px;
    border: none;
    border-radius: 4px;
    cursor: pointer;
    font-size: 0.95rem;
    font-weight: 600;
    color: #fff;
    background-color: #444;
    transition: filter 0.2s, transform 0.1s;
}
.btn:active { transform: scale(0.98); }
.btn:hover { filter: brightness(1.1); }
.btn.primary { background-color: var(--primary-color); color: #000; }
.btn.secondary { background-color: #555; }
.btn.danger { background-color: var(--danger-color); }
.btn.warning { background-color: var(--warning-color); color: #000; }
.btn.large { padding: 12px 24px; font-size: 1.1rem; }
.btn.small { padding: 4px 8px; font-size: 0.8rem; }
.btn.full-width { width: 100%; }

.active-func { background-color: var(--primary-color) !important; color: #000 !important; }

/* Inputs & Forms */
input[type="text"], input[type="number"], input[type="password"], select, textarea {
    background-color: var(--input-bg);
    border: 1px solid var(--border-color);
    color: #fff;
    padding: 8px 12px;
    border-radius: 4px;
    font-size: 1rem;
    width: 100%;
    box-sizing: border-box;
}

input:focus { outline: 2px solid var(--primary-color); border-color: transparent; }

.input-group {
    display: flex;
    gap: 5px;
}

/* CV Table */
.table-container {
    overflow-x: auto;
}
table {
    width: 100%;
    border-collapse: collapse;
    font-size: 0.9rem;
}
th, td {
    text-align: left;
    padding: 10px;
    border-bottom: 1px solid var(--border-color);
}
th { color: var(--text-muted); font-weight: 600; }
tr:hover { background-color: rgba(255,255,255,0.05); }

/* Input States */
input.dirty { background-color: #ffb74d !important; color: #000 !important; }
@keyframes flashSuccess { 0% { background-color: var(--success-color); } 100% { background-color: var(--input-bg); } }
@keyframes flashError { 0% { background-color: var(--danger-color); } 100% { background-color: var(--input-bg); } }
.flash-success { animation: flashSuccess 1s ease-out; }
.flash-error { animation: flashError 1s ease-out; }

/* Settings Details */
details.settings-group {
    background-color: var(--card-bg);
    border-radius: 8px;
    margin-bottom: 15px;
    overflow: hidden;
}
summary {
    padding: 15px 20px;
    cursor: pointer;
    font-weight: bold;
    background-color: rgba(255,255,255,0.03);
    list-style: none;
    position: relative;
}
summary::-webkit-details-marker { display: none; }
summary::after {
    content: '+';
    position: absolute;
    right: 20px;
    font-size: 1.2rem;
}
details[open] summary::after { content: '-'; }
.settings-content { padding: 20px; border-top: 1px solid var(--border-color); }

.form-group { margin-bottom: 15px; }
.form-group label { display: block; margin-bottom: 5px; color: var(--text-muted); font-size: 0.9rem; }
.hint { font-size: 0.8rem; color: var(--text-muted); margin-top: 5px; }

/* File Manager */
.quota-container { margin-top: 10px; }
.quota-bar { height: 10px; background: #333; border-radius: 5px; overflow: hidden; }
#quota-fill { height: 100%; width: 0%; background: var(--primary-color); transition: width 0.5s; }
#quota-text { font-size: 0.8rem; color: var(--text-muted); text-align: right; margin-top: 5px; }

.file-manager { display: flex; flex-direction: column; gap: 20px; }
@media(min-width: 768px) {
    .file-manager { flex-direction: row; }
    .upload-card { flex: 1; }
    .file-list-card { flex: 2; }
}
.checkbox-label { display: flex; align-items: center; gap: 10px; margin: 10px 0; cursor: pointer; }
.checkbox-label input { width: auto; }

/* Logs */
.terminal {
    background-color: #000;
    color: #0f0;
    font-family: 'Courier New', Courier, monospace;
    height: 400px;
    overflow-y: auto;
    padding: 10px;
    border-radius: 4px;
    border: 1px solid var(--border-color);
    white-space: pre-wrap;
    font-size: 0.85rem;
}
.log-controls {
    display: flex;
    justify-content: space-between;
    margin-bottom: 10px;
    flex-wrap: wrap;
    gap: 10px;
}
.control-group { display: flex; align-items: center; gap: 10px; }

/* Toast */
#toast-container {
    position: fixed;
    bottom: 20px;
    right: 20px;
    z-index: 1000;
}
.toast {
    background-color: #333;
    color: #fff;
    padding: 10px 20px;
    margin-top: 10px;
    border-radius: 4px;
    box-shadow: 0 2px 10px rgba(0,0,0,0.5);
    animation: slideIn 0.3s ease-out;
    border-left: 4px solid var(--primary-color);
}
@keyframes slideIn { from { transform: translateX(100%); opacity: 0; } to { transform: translateX(0); opacity: 1; } }

.result-box textarea { height: 150px; background: #000; color: #0f0; margin-bottom: 10px; resize: vertical; }

)rawliteral";

const char APP_JS[] PROGMEM = R"rawliteral(
// State
let currentTab = localStorage.getItem('activeTab') || 'dashboard';
let logInterval = null;
let statusInterval = null;
let cvDefs = [];
let cvValues = {};
let lastSeenTimestamp = { "": 0, "data": 0, "debug": 0 };
let sessionLogs = { "": [], "data": [], "debug": [] };
let clearedMarkers = { "": 0, "data": 0, "debug": 0 };

document.addEventListener('DOMContentLoaded', () => {
    // Restore tab
    showTab(currentTab);

    // Setup
    renderFunctions();
    pollStatus();
    statusInterval = setInterval(pollStatus, 1000);

    document.getElementById('upload-form').addEventListener('submit', handleUpload);
    
    // Initial data fetch (lazy)
    if (currentTab === 'cvs') loadAllCVs();
    if (currentTab === 'files') loadFiles();
});

// --- Tab Management ---
function showTab(tabId) {
    document.querySelectorAll('.nav-btn').forEach(b => {
        b.classList.toggle('active', b.getAttribute('onclick').includes(tabId));
    });

    document.querySelectorAll('.tab-content').forEach(c => {
        c.classList.remove('active');
    });

    const tabEl = document.getElementById(tabId);
    if (tabEl) {
        tabEl.classList.add('active');
        localStorage.setItem('activeTab', tabId);
        currentTab = tabId;

        // Tab specific triggers
        if (tabId === 'logs') {
            if (!logInterval) {
                pollLogs();
                logInterval = setInterval(pollLogs, 1000);
            }
        } else {
            if (logInterval) { clearInterval(logInterval); logInterval = null; }
        }

        if (tabId === 'cvs' && cvDefs.length === 0) loadAllCVs();
        if (tabId === 'files') loadFiles();
    }
}

// --- Status & Control ---
function pollStatus() {
    fetch('/api/status')
        .then(r => r.json())
        .then(data => {
            const ind = document.getElementById('connection-status');
            ind.classList.remove('disconnected');
            ind.classList.add('connected');
            ind.title = "Connected";
            
            updateText('dcc-address', data.address);
            updateText('dcc-speed', data.speed);
            updateText('dcc-direction', data.direction === 'forward' ? 'FWD' : 'REV');
            updateText('uptime', formatUptime(data.uptime));
            
            // Sync Slider if not being dragged
            const slider = document.getElementById('speed-slider');
            if (document.activeElement !== slider) {
                slider.value = data.speed;
                updateText('speed-display', data.speed);
            }
            
            // Sync Dir Button
            const dirBtn = document.getElementById('dir-btn');
            dirBtn.innerText = (data.direction === 'forward') ? "FWD" : "REV";
            
            // Sync Functions
            if (data.functions) {
                data.functions.forEach((s, i) => updateFuncBtn(i, s));
            }
            
            // System Info
            updateText('wifi-details', data.wifi ? "Connected" : "Disconnected");
            updateText('sys-version', data.version);
            updateText('sys-hash', data.hash);
            updateText('sys-hostname', data.hostname);
            if(document.activeElement.id !== 'config-hostname') {
                const hn = document.getElementById('config-hostname');
                if(hn && !hn.value) hn.value = data.hostname;
            }

            // Quota
            if (data.fs_total) {
                const used = data.fs_used || 0;
                const total = data.fs_total;
                const perc = Math.min(100, Math.round((used/total)*100));
                const fill = document.getElementById('quota-fill');
                if(fill) fill.style.width = perc + '%';
                updateText('quota-text', `${formatBytes(used)} / ${formatBytes(total)} (${perc}%)`);
            }
        })
        .catch(() => {
            const ind = document.getElementById('connection-status');
            ind.classList.remove('connected');
            ind.classList.add('disconnected');
            ind.title = "Disconnected";
        });
}

function updateText(id, txt) {
    const el = document.getElementById(id);
    if(el && el.innerText !== String(txt)) el.innerText = txt;
}

function updateSpeedDisplay(val) {
    document.getElementById('speed-display').innerText = val;
}

function setSpeed(val) {
    sendAction('set_speed', parseInt(val));
}

function toggleDir() {
    const btn = document.getElementById('dir-btn');
    const isFwd = (btn.innerText === "FWD");
    sendAction('set_direction', !isFwd);
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
    .then(() => setTimeout(pollStatus, 50)) // Quick refresh
    .catch(console.error);
}

function renderFunctions() {
    const grid = document.getElementById('func-grid');
    if(!grid) return;
    grid.innerHTML = '';
    for(let i=0; i<=28; i++) {
        const btn = document.createElement('button');
        btn.className = 'btn';
        btn.id = `f${i}-btn`;
        btn.innerText = `F${i}`;
        btn.onclick = () => toggleFunc(i);
        grid.appendChild(btn);
    }
}

function toggleFunc(i) {
    const btn = document.getElementById(`f${i}-btn`);
    const newState = !btn.classList.contains('active-func');
    updateFuncBtn(i, newState); // Optimistic
    sendAction('set_function', newState, i);
}

function updateFuncBtn(i, active) {
    const btn = document.getElementById(`f${i}-btn`);
    if(btn) btn.classList.toggle('active-func', active);
}

// --- CV Management ---
async function loadAllCVs() {
    try {
        // Parallel Fetch
        const [defsRes, valsRes] = await Promise.all([
            fetch('/api/cv/defs'),
            fetch('/api/cv/all')
        ]);

        cvDefs = await defsRes.json();
        const vals = await valsRes.json();

        // Merge values into simple map
        cvValues = {};
        for(let k in vals) cvValues[k] = vals[k];

        renderCVTable();
        showToast("CVs Loaded");
    } catch(e) {
        console.error(e);
        showToast("Failed to load CVs");
    }
}

function renderCVTable() {
    const tbody = document.querySelector('#cv-table tbody');
    if(!tbody) return;
    tbody.innerHTML = '';
    
    const filter = (document.getElementById('cv-search').value || '').toLowerCase();

    // Sort defs by CV ID
    const sorted = [...cvDefs].sort((a,b) => a.cv - b.cv);

    sorted.forEach(def => {
        // Filter logic
        const txt = `${def.cv} ${def.name} ${def.desc}`.toLowerCase();
        if(filter && !txt.includes(filter)) return;

        const tr = document.createElement('tr');
        const val = (cvValues[def.cv] !== undefined) ? cvValues[def.cv] : '?';

        tr.innerHTML = `
            <td><b>${def.cv}</b></td>
            <td>${def.name}</td>
            <td>
                <input type="number"
                       id="cv-input-${def.cv}"
                       value="${val}"
                       style="width:70px"
                       oninput="onCvInput(${def.cv})"
                       onchange="onCvChange(${def.cv})">
            </td>
            <td><small style="color:var(--text-muted)">${def.desc}</small></td>
        `;
        tbody.appendChild(tr);
    });
}

function onCvInput(cv) {
    const el = document.getElementById(`cv-input-${cv}`);
    el.classList.add('dirty');
}

function onCvChange(cv) {
    const el = document.getElementById(`cv-input-${cv}`);
    const val = parseInt(el.value);
    if(isNaN(val)) return;

    // Write
    fetch('/api/cv', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({ cmd: 'write', cv: cv, value: val })
    }).then(r => {
        if(r.ok) {
            el.classList.remove('dirty');
            el.classList.add('flash-success');
            setTimeout(() => el.classList.remove('flash-success'), 1000);
            cvValues[cv] = val; // Update local cache
        } else {
            el.classList.add('flash-error');
            setTimeout(() => el.classList.remove('flash-error'), 1000);
            showToast("Write Failed");
        }
    }).catch(e => {
        el.classList.add('flash-error');
        showToast("Network Error");
    });
}

function rwCustomCV(mode) {
    const cvInput = document.getElementById('custom-cv');
    const valInput = document.getElementById('custom-val');
    const cv = parseInt(cvInput.value);
    if(!cv) return;

    if(mode === 'read') {
        fetch('/api/cv', {
            method: 'POST',
            body: JSON.stringify({ cmd: 'read', cv })
        })
        .then(r => r.json())
        .then(d => {
            valInput.value = d.value;
            // Also update main table if visible
            const mainInput = document.getElementById(`cv-input-${cv}`);
            if(mainInput) {
                mainInput.value = d.value;
                mainInput.classList.add('flash-success');
            }
        });
    } else {
        const val = parseInt(valInput.value);
        if(isNaN(val)) return;
        // Reuse write logic by simulating event if exists, or direct call
        const mainInput = document.getElementById(`cv-input-${cv}`);
        if(mainInput) {
            mainInput.value = val;
            onCvChange(cv);
        } else {
            // Write blindly
            fetch('/api/cv', { method: 'POST', body: JSON.stringify({ cmd: 'write', cv, value: val }) })
            .then(r => r.ok ? showToast(`CV${cv} Saved`) : showToast("Error"));
        }
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
                const isAudio = f.name.toLowerCase().match(/\.(wav|mp3)$/);
                tr.innerHTML = `
                    <td><input type="checkbox" class="file-check" value="${f.name}"></td>
                    <td><a href="${f.name}" download style="color:#fff;text-decoration:none">${f.name}</a></td>
                    <td>${formatBytes(f.size)}</td>
                    <td>
                        ${isAudio ? `<button class="btn small primary" onclick="playAudio('${f.name}')">Play</button>` : ''}
                        <button class="btn small danger" onclick="deleteFile('${f.name}')">Del</button>
                    </td>
                `;
                tbody.appendChild(tr);
            });
        });
}

function toggleAll(source) {
    document.querySelectorAll('.file-check').forEach(c => c.checked = source.checked);
}

function deleteFile(name) {
    if(confirm(`Delete ${name}?`)) {
        fetch('/api/files/delete', { method: 'POST', body: `path=${encodeURIComponent(name)}`, headers: {'Content-Type': 'application/x-www-form-urlencoded'} })
        .then(() => loadFiles());
    }
}

async function deleteSelected() {
    const checked = document.querySelectorAll('.file-check:checked');
    if(!checked.length) return;
    if(!confirm(`Delete ${checked.length} files?`)) return;

    for(const c of checked) {
        await fetch('/api/files/delete', { method: 'POST', body: `path=${encodeURIComponent(c.value)}`, headers: {'Content-Type': 'application/x-www-form-urlencoded'} });
    }
    loadFiles();
}

async function handleUpload(e) {
    e.preventDefault();
    const input = document.getElementById('file-input');
    const compress = document.getElementById('compress-mp3').checked;
    const status = document.getElementById('upload-status');

    if(!input.files.length) return;

    for (let file of input.files) {
        let name = file.name;
        status.innerText = `Uploading ${name}...`;

        if (compress && name.toLowerCase().endsWith('.wav')) {
             try {
                file = await compressToMp3(file);
                name = file.name;
                status.innerText = `Compressed to ${name}. Uploading...`;
             } catch(err) {
                 console.error(err);
             }
        }

        const fd = new FormData();
        fd.append("file", file, name);
        await fetch('/api/files/upload', { method: 'POST', body: fd });
    }
    status.innerText = "Done!";
    input.value = '';
    loadFiles();
}

// Reuse LameJS logic from before (simplified here for brevity, assume global lamejs)
async function compressToMp3(file) {
    if (typeof lamejs === 'undefined') throw new Error("No lamejs");
    return new Promise((resolve, reject) => {
        const reader = new FileReader();
        reader.onload = async (e) => {
            const ctx = new (window.AudioContext || window.webkitAudioContext)();
            const buf = await ctx.decodeAudioData(e.target.result);
            const mp3enc = new lamejs.Mp3Encoder(buf.numberOfChannels, buf.sampleRate, 128);

            // Simplified conversion...
            const samples = buf.getChannelData(0);
            const int16 = new Int16Array(samples.length);
            for(let i=0; i<samples.length; i++) int16[i] = samples[i] * (samples[i]<0 ? 0x8000 : 0x7FFF);

            const mp3 = mp3enc.encodeBuffer(int16);
            const end = mp3enc.flush();
            resolve(new File([new Blob([mp3, end], {type: 'audio/mpeg'})], file.name.replace('.wav','.mp3')));
        };
        reader.readAsArrayBuffer(file);
    });
}

function playAudio(f) {
    fetch('/api/audio/play', { method: 'POST', body: `file=${encodeURIComponent(f)}`, headers: {'Content-Type': 'application/x-www-form-urlencoded'} });
}

// --- Logs & Utils ---
function pollLogs() {
    const type = document.getElementById('log-type-filter').value;
    const url = type ? `/api/logs?type=${type}` : '/api/logs';
    
    fetch(url).then(r => r.json()).then(lines => {
        const viewer = document.getElementById('log-viewer');
        if(!viewer) return;

        // Append new lines logic (simplified)
        const newText = lines.join('\n');
        if(viewer.innerText !== newText) {
             viewer.innerText = newText;
             if(document.getElementById('auto-scroll').checked) viewer.scrollTop = viewer.scrollHeight;
        }
    });
}

function clearLogs() {
    document.getElementById('log-viewer').innerText = '';
    // Server clear not implemented in API, so just clear view
}

function showToast(msg) {
    const container = document.getElementById('toast-container');
    const el = document.createElement('div');
    el.className = 'toast';
    el.innerText = msg;
    container.appendChild(el);
    setTimeout(() => {
        el.style.opacity = '0';
        setTimeout(() => el.remove(), 300);
    }, 3000);
}

function formatBytes(a,b=2){if(!+a)return"0 B";const c=0>b?0:b,d=Math.floor(Math.log(a)/Math.log(1024));return`${parseFloat((a/Math.pow(1024,d)).toFixed(c))} ${["B","KB","MB","GB"][d]}`}
function formatUptime(s){const h=Math.floor(s/3600),m=Math.floor((s%3600)/60);return`${h}h ${m}m ${s%60}s`}

// WiFi / Sys
function saveWifi(e) {
    e.preventDefault();
    if(!confirm("Restart?")) return;
    const s = document.getElementById('wifi-ssid').value;
    const p = document.getElementById('wifi-pass').value;
    fetch('/api/wifi/save', { method: 'POST', body: `ssid=${encodeURIComponent(s)}&pass=${encodeURIComponent(p)}`, headers: {'Content-Type': 'application/x-www-form-urlencoded'} });
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

function saveAuth(e) {
    e.preventDefault();
    const user = document.getElementById('web-user').value;
    const pass = document.getElementById('web-pass').value;
    const isDisabling = (user === "");
    const confirmMsg = isDisabling
        ? "Disable web authentication?"
        : "Update web credentials?";

    if (!confirm(confirmMsg)) return;

    fetch('/api/config/webauth', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: `user=${encodeURIComponent(user)}&pass=${encodeURIComponent(pass)}`
    })
    .then(r => r.text())
    .then(msg => {
        alert(msg);
        location.reload();
    })
    .catch(e => alert("Error: " + e));
}

function scanWifi() {
    const res = document.getElementById('scan-results');
    res.innerHTML = 'Scanning...';
    fetch('/api/wifi/scan').then(r=>r.json()).then(l => {
        res.innerHTML = l.map(n => `<div onclick="document.getElementById('wifi-ssid').value='${n.ssid}'" style="cursor:pointer;padding:5px;border-bottom:1px solid #444">${n.ssid} (${n.rssi})</div>`).join('');
    });
}
function togglePass(btn, id='wifi-pass') {
    const el = document.getElementById(id);
    el.type = el.type === 'password' ? 'text' : 'password';
    btn.innerText = el.type === 'password' ? 'Show' : 'Hide';
}

function runMotorTest() {
    fetch('/api/motor/test', { method: 'POST' }).then(() => {
        setTimeout(() => {
            fetch('/api/motor/test').then(r=>r.text()).then(t => document.getElementById('test-results').value = t);
        }, 4000);
    });
}
function copyTestResults() {
    document.getElementById('test-results').select();
    document.execCommand('copy');
    showToast("Copied");
}
)rawliteral";

#endif
