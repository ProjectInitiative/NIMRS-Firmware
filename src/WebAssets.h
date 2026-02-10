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
</head>
<body>
    <div class="container">
        <header>
            <h1>NIMRS Decoder</h1>
            <div id="connection-status" class="status-indicator disconnected"></div>
        </header>

        <nav>
            <button class="nav-btn active" onclick="showTab('dashboard')">Dashboard</button>
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
                </div>
            </section>

            <!-- File Manager Tab -->
            <section id="files" class="tab-content">
                <div class="file-upload">
                    <h3>Upload Files</h3>
                    <form id="upload-form">
                        <input type="file" id="file-input" multiple required>
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
    overflow-y: auto;
    padding: 10px;
    border-radius: 4px;
    border: 1px solid #333;
    white-space: pre-wrap;
}
.log-controls { margin-bottom: 5px; display: flex; justify-content: space-between; }
)rawliteral";

const char APP_JS[] PROGMEM = R"rawliteral(
// State
let currentTab = 'dashboard';
let logInterval = null;
let statusInterval = null;

// Initialization
document.addEventListener('DOMContentLoaded', () => {
    // Determine initial tab (maybe from hash)
    showTab('dashboard');

    // Start polling status
    pollStatus();
    statusInterval = setInterval(pollStatus, 1000);

    // Setup File Upload
    document.getElementById('upload-form').addEventListener('submit', handleUpload);
});

function showTab(tabId) {
    // Update Nav
    document.querySelectorAll('.nav-btn').forEach(b => b.classList.remove('active'));
    document.querySelector(`.nav-btn[onclick="showTab('${tabId}')"]`).classList.add('active');

    // Update Content
    document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
    document.getElementById(tabId).classList.add('active');

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

    if (tabId === 'files') {
        loadFiles();
    }
}

// --- API Interactions ---

function pollStatus() {
    fetch('/api/status')
        .then(r => r.json())
        .then(data => {
            document.getElementById('connection-status').classList.remove('disconnected');
            document.getElementById('connection-status').classList.add('connected');
            
            document.getElementById('dcc-address').innerText = data.address || '--';
            document.getElementById('dcc-speed').innerText = data.speed || '0';
            document.getElementById('dcc-direction').innerText = data.direction;
            document.getElementById('uptime').innerText = formatUptime(data.uptime);
            
            // System tab detail
            document.getElementById('wifi-details').innerText = `Connected: ${data.wifi ? 'Yes' : 'No'}`;
            if(document.getElementById('sys-version')) document.getElementById('sys-version').innerText = data.version || 'Unknown';
            if(document.getElementById('sys-hash')) document.getElementById('sys-hash').innerText = data.hash || 'Unknown';
        })
        .catch(e => {
            document.getElementById('connection-status').classList.remove('connected');
            document.getElementById('connection-status').classList.add('disconnected');
        });
}

function sendAction(action) {
    // For now, these are just placeholders as the backend API for controls isn't fully defined yet.
    // We'll use a generic POST endpoint if/when implemented.
    console.log("Action:", action);
    // fetch('/api/control', { method: 'POST', body: JSON.stringify({ action }) });
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
                tr.innerHTML = `
                    <td><input type="checkbox" class="file-check" value="${f.name}"></td>
                    <td>${f.name}</td>
                    <td>${f.size}</td>
                    <td>
                        <a href="/${f.name}" class="btn small" download>DL</a>
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

async function handleUpload(e) {
    e.preventDefault();
    const input = document.getElementById('file-input');
    const files = input.files;
    if (!files.length) return;

    const statusDiv = document.getElementById('upload-status');
    statusDiv.innerText = "Starting upload...";

    for (let i = 0; i < files.length; i++) {
        const file = files[i];
        statusDiv.innerText = `Uploading ${i+1}/${files.length}: ${file.name}...`;
        
        const formData = new FormData();
        formData.append("file", file);

        try {
            const r = await fetch('/api/files/upload', {
                method: 'POST',
                body: formData
            });
            if (!r.ok) throw new Error(r.statusText);
        } catch (err) {
            statusDiv.innerText = `Error uploading ${file.name}: ${err}`;
            return;
        }
    }

    statusDiv.innerText = "All uploads complete!";
    input.value = ''; 
    loadFiles();
}

function deleteFile(filename) {
    if (!confirm(`Delete ${filename}?`)) return;
    performDelete(filename).then(() => loadFiles());
}

async function deleteSelected() {
    const checked = document.querySelectorAll('.file-check:checked');
    if (!checked.length) return alert("No files selected");
    if (!confirm(`Delete ${checked.length} files?`)) return;

    for (const c of checked) {
        await performDelete(c.value);
    }
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

function pollLogs() {
    fetch('/api/logs')
        .then(r => r.json())
        .then(lines => {
            const viewer = document.getElementById('log-viewer');
            const wasAtBottom = viewer.scrollTop + viewer.clientHeight >= viewer.scrollHeight - 10;
            
            // Simple replace for now, could be optimized to append only new
            viewer.innerHTML = lines.join('\n');

            if (document.getElementById('auto-scroll').checked && wasAtBottom) {
                viewer.scrollTop = viewer.scrollHeight;
            }
        });
}

function clearLogs() {
    document.getElementById('log-viewer').innerHTML = '';
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

function resetWifi() {
    if (!confirm("Forget saved WiFi and restart into AP mode?")) return;

    fetch('/api/wifi/reset', { method: 'POST' })
    .then(() => alert("Resetting... Connect to 'NIMRS-Decoder' AP."))
    .catch(e => alert("Error: " + e));
}
)rawliteral";

#endif