let currentTab = localStorage.getItem('activeTab') || 'dashboard';
let logInterval = null;
let statusInterval = null;
let cvDefs = [];
let cvValues = {};
let lastSeenTimestamp = { "": 0, "data": 0, "debug": 0 };
let sessionLogs = { "": [], "data": [], "debug": [] };
let clearedMarkers = { "": 0, "data": 0, "debug": 0 };
let telemetryChart = null;

document.addEventListener('DOMContentLoaded', () => {
    showTab(currentTab);
    renderFunctions();
    pollStatus();
    statusInterval = setInterval(pollStatus, 1000);
    initTelemetryChart();

    document.getElementById('upload-form').addEventListener('submit', handleUpload);

    if (currentTab === 'cvs') loadAllCVs();
    if (currentTab === 'files') loadFiles();
});

function initTelemetryChart() {
    const el = document.getElementById('telemetryChart');
    if (!el) return;
    const ctx = el.getContext('2d');
    telemetryChart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [
                { label: 'RPM', borderColor: '#4bc0c0', yAxisID: 'yRpm', data: [], tension: 0.1, pointRadius: 0 },
                { label: 'Current (A)', borderColor: '#ff6384', yAxisID: 'yLow', data: [], tension: 0.1, pointRadius: 0 },
                { label: 'Zone', borderColor: '#ffce56', yAxisID: 'yLow', data: [], stepped: true, pointRadius: 0 },
                { label: 'Stall', borderColor: '#ff0000', yAxisID: 'yLow', data: [], stepped: true, pointRadius: 0 }
            ]
        },
        options: {
            animation: false,
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                x: { display: false },
                yRpm: { type: 'linear', position: 'left', title: { display: true, text: 'RPM', color: '#4bc0c0' }, grid: { color: '#333' } },
                yLow: { type: 'linear', position: 'right', max: 5, title: { display: true, text: 'Amps / Zone' }, grid: { drawOnChartArea: false } }
            },
            plugins: { legend: { labels: { color: '#fff' } } }
        }
    });
}

async function updateTelemetryData() {
    if (currentTab !== 'telemetry' || !telemetryChart) return;
    try {
        const response = await fetch('/api/logs?filter=[NIMRS_DATA]');
        const logArray = await response.json();

        let times = [], rpms = [], currents = [], zones = [], stalls = [];

        logArray.forEach(line => {
            const jsonStart = line.indexOf('{');
            if (jsonStart > -1) {
                try {
                    const data = JSON.parse(line.substring(jsonStart));
                    const tsMatch = line.match(/\[(\d+)\]/);
                    times.push(tsMatch ? tsMatch[1] : '');
                    rpms.push(data.rpm);
                    currents.push(data.cur);
                    zones.push(data.zone);
                    stalls.push(data.stall);
                } catch (e) {}
            }
        });

        const limit = 100;
        telemetryChart.data.labels = times.slice(-limit);
        telemetryChart.data.datasets[0].data = rpms.slice(-limit);
        telemetryChart.data.datasets[1].data = currents.slice(-limit);
        telemetryChart.data.datasets[2].data = zones.slice(-limit);
        telemetryChart.data.datasets[3].data = stalls.slice(-limit);
        telemetryChart.update();
    } catch (e) {}
}

setInterval(updateTelemetryData, 500);

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

            const slider = document.getElementById('speed-slider');
            if (document.activeElement !== slider) {
                slider.value = data.speed;
                updateText('speed-display', data.speed);
            }

            const dirBtn = document.getElementById('dir-btn');
            dirBtn.innerText = (data.direction === 'forward') ? "FWD" : "REV";

            if (data.functions) {
                data.functions.forEach((s, i) => updateFuncBtn(i, s));
            }

            updateText('wifi-details', data.wifi ? "Connected" : "Disconnected");
            updateText('sys-version', data.version);
            updateText('sys-hash', data.hash);
            updateText('sys-hostname', data.hostname);
            if(document.activeElement.id !== 'config-hostname') {
                const hn = document.getElementById('config-hostname');
                if(hn && !hn.value) hn.value = data.hostname;
            }

            if (data.rolled_back) {
                const warn = document.getElementById('rollback-warning');
                if(!warn) {
                    const div = document.createElement('div');
                    div.id = 'rollback-warning';
                    div.className = 'card';
                    div.style.borderLeft = '4px solid var(--warning-color)';
                    div.innerHTML = `
                        <div style="display:flex; justify-content:space-between; align-items:start;">
                            <div>
                                <h3 style="color:var(--warning-color)">System Rollback Detected</h3>
                                <p>The system recovered from a boot loop by rolling back.</p>
                                <p style="font-size:0.9rem; color:var(--text-muted)">
                                    Crashed Version: <b>${data.crashed_version || 'Unknown'}</b><br>
                                    Currently Running: <b>${data.running_version || 'Unknown'}</b>
                                </p>
                            </div>
                            <button class="btn small" onclick="sendAction('clear_rollback'); document.getElementById('rollback-warning').remove();">Dismiss</button>
                        </div>`;
                    const dashboard = document.querySelector('#dashboard .dashboard-grid');
                    if(dashboard) dashboard.insertBefore(div, dashboard.firstChild);
                }
            } else {
                const warn = document.getElementById('rollback-warning');
                if(warn) warn.remove();
            }

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
    .then(() => setTimeout(pollStatus, 50))
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
    updateFuncBtn(i, newState);
    sendAction('set_function', newState, i);
}

function updateFuncBtn(i, active) {
    const btn = document.getElementById(`f${i}-btn`);
    if(btn) btn.classList.toggle('active-func', active);
}

async function loadAllCVs() {
    try {
        const [defsRes, valsRes] = await Promise.all([
            fetch('/api/cv/defs'),
            fetch('/api/cv/all')
        ]);

        cvDefs = await defsRes.json();
        const vals = await valsRes.json();

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

    const sorted = [...cvDefs].sort((a,b) => a.cv - b.cv);

    sorted.forEach(def => {
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

    fetch('/api/cv', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({ cmd: 'write', cv: cv, value: val })
    }).then(r => {
        if(r.ok) {
            el.classList.remove('dirty');
            el.classList.add('flash-success');
            setTimeout(() => el.classList.remove('flash-success'), 1000);
            cvValues[cv] = val;
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
            const mainInput = document.getElementById(`cv-input-${cv}`);
            if(mainInput) {
                mainInput.value = d.value;
                mainInput.classList.add('flash-success');
            }
        });
    } else {
        const val = parseInt(valInput.value);
        if(isNaN(val)) return;
        const mainInput = document.getElementById(`cv-input-${cv}`);
        if(mainInput) {
            mainInput.value = val;
            onCvChange(cv);
        } else {
            fetch('/api/cv', { method: 'POST', body: JSON.stringify({ cmd: 'write', cv, value: val }) })
            .then(r => r.ok ? showToast(`CV${cv} Saved`) : showToast("Error"));
        }
    }
}

function loadFiles() {
    fetch('/api/files/list?_=' + Date.now())
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
                        <a href="${f.name}" download class="btn small" style="text-decoration:none; display:inline-block; line-height:1.2;">Down</a>
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

function formatFileSystem() {
    if(!confirm("Format filesystem? This will delete ALL files and cannot be undone.")) return;

    showToast("Formatting...");
    fetch('/api/files/format', { method: 'POST' })
    .then(r => {
        if(r.ok) {
            showToast("Format Started. Please wait...");
            setTimeout(loadFiles, 5000);
        } else {
            showToast("Format Failed");
        }
    })
    .catch(() => showToast("Network Error"));
}

async function handleUpload(e) {
    e.preventDefault();
    const input = document.getElementById('file-input');
    const compress = document.getElementById('compress-mp3').checked;
    const status = document.getElementById('upload-status');

    if(!input.files.length) return;

    for (let i = 0; i < input.files.length; i++) {
        let file = input.files[i];
        let name = file.name;

        const lowerName = name.toLowerCase();
        if (!(lowerName.endsWith(".json") || lowerName.endsWith(".wav") || lowerName.endsWith(".mp3"))) {
            console.log(`[Upload] Rejected: ${name}`);
            status.innerText = `Rejected: ${name} (Invalid extension)`;
            continue;
        }

        console.log(`[Upload] Processing ${i+1}/${input.files.length}: ${name}`);
        status.innerText = `Processing ${name}...`;

        if (compress && lowerName.endsWith('.wav')) {
             try {
                console.log(`[Upload] Compressing ${name}...`);
                status.innerText = `Compressing ${name}...`;
                file = await compressToMp3(file);
                name = file.name;
                console.log(`[Upload] Compression done: ${name}`);
             } catch(err) {
                 console.error(err);
                 status.innerText = `Compression failed: ${err}`;
                 continue;
             }
        }

        const fd = new FormData();
        fd.append("file", file, name);
        try {
            console.log(`[Upload] Sending ${name}...`);
            status.innerText = `Uploading ${name}...`;
            const res = await fetch('/api/files/upload', { method: 'POST', body: fd });
            if (!res.ok) {
                const txt = await res.text();
                status.innerText = `Error: ${txt}`;
                console.error(`[Upload] Failed: ${txt}`);
            } else {
                console.log(`[Upload] Success: ${name}`);
            }
        } catch (e) {
            status.innerText = "Network Error";
            console.error(`[Upload] Network Error:`, e);
        }
    }
    status.innerText = "Done!";
    input.value = '';
    loadFiles();
}

async function compressToMp3(file) {
    if (typeof lamejs === 'undefined') throw new Error("No lamejs");
    return new Promise((resolve, reject) => {
        const reader = new FileReader();
        reader.onload = async (e) => {
            try {
                const ctx = new (window.AudioContext || window.webkitAudioContext)();
                const buf = await ctx.decodeAudioData(e.target.result);

                const channels = buf.numberOfChannels;
                const sampleRate = buf.sampleRate;
                const kbps = 128;

                console.log(`[LameJS] Init Encoder: ${channels}ch ${sampleRate}Hz ${kbps}kbps`);
                const mp3enc = new lamejs.Mp3Encoder(channels, sampleRate, kbps);

                const samplesL = buf.getChannelData(0);
                const samplesR = channels > 1 ? buf.getChannelData(1) : samplesL;

                const convert = (s) => s < 0 ? s * 0x8000 : s * 0x7FFF;

                const mp3Chunks = [];
                const blockSize = 1152;

                const len = samplesL.length;
                const int16L = new Int16Array(blockSize);
                const int16R = new Int16Array(blockSize);

                for (let i = 0; i < len; i += blockSize) {
                    const chunkLen = Math.min(blockSize, len - i);

                    for(let j=0; j<chunkLen; j++) int16L[j] = convert(samplesL[i+j]);

                    let mp3buf;
                    if (channels > 1) {
                        for(let j=0; j<chunkLen; j++) int16R[j] = convert(samplesR[i+j]);

                        const leftChunk = (chunkLen < blockSize) ? int16L.subarray(0, chunkLen) : int16L;
                        const rightChunk = (chunkLen < blockSize) ? int16R.subarray(0, chunkLen) : int16R;

                        mp3buf = mp3enc.encodeBuffer(leftChunk, rightChunk);
                    } else {
                        const leftChunk = (chunkLen < blockSize) ? int16L.subarray(0, chunkLen) : int16L;
                        mp3buf = mp3enc.encodeBuffer(leftChunk);
                    }

                    if (mp3buf.length > 0) mp3Chunks.push(mp3buf);
                }

                const end = mp3enc.flush();
                if (end.length > 0) mp3Chunks.push(end);

                console.log(`[LameJS] Done. Chunks: ${mp3Chunks.length}`);
                const blob = new Blob(mp3Chunks, {type: 'audio/mpeg'});
                resolve(new File([blob], file.name.replace(/\.[^/.]+$/, ".mp3"), {type: "audio/mpeg"}));
            } catch (err) {
                reject(err);
            }
        };
        reader.onerror = reject;
        reader.readAsArrayBuffer(file);
    });
}

function playAudio(f) {
    fetch('/api/audio/play', { method: 'POST', body: `file=${encodeURIComponent(f)}`, headers: {'Content-Type': 'application/x-www-form-urlencoded'} });
}

function pollLogs() {
    const type = document.getElementById('log-type-filter').value;
    const url = type ? `/api/logs?type=${type}` : '/api/logs';

    fetch(url).then(r => r.json()).then(lines => {
        const viewer = document.getElementById('log-viewer');
        if(!viewer) return;

        const newText = lines.join('\n');
        if(viewer.innerText !== newText) {
             viewer.innerText = newText;
             if(document.getElementById('auto-scroll').checked) viewer.scrollTop = viewer.scrollHeight;
        }
    });
}

function clearLogs() {
    const type = document.getElementById('log-type-filter').value;
    const markerKey = type || "";

    fetch('/api/logs', { method: 'DELETE' })
    .then(() => {
        sessionLogs = { "": [], "data": [], "debug": [] };
        lastSeenTimestamp = { "": 0, "data": 0, "debug": 0 };
        clearedMarkers = { "": 0, "data": 0, "debug": 0 };

        const viewer = document.getElementById('log-viewer');
        if (viewer) viewer.innerHTML = '';

        showToast("Logs Cleared");
    })
    .catch(e => showToast("Clear Failed"));
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

function measureResistance() {
    const status = document.getElementById('res-status');
    status.innerText = "Starting...";

    fetch('/api/motor/calibrate', { method: 'POST' })
    .then(r => r.json())
    .then(() => {
        status.innerText = "Measuring...";
        const poll = setInterval(() => {
            fetch('/api/motor/calibrate')
            .then(r => r.json())
            .then(d => {
                if (d.state === 'DONE') {
                    clearInterval(poll);
                    status.innerText = `R = ${d.resistance.toFixed(2)} Ohms (Saved)`;
                    status.style.color = 'var(--success-color)';
                } else if (d.state === 'ERROR') {
                    clearInterval(poll);
                    status.innerText = "Error: Low Current / Disconnected";
                    status.style.color = 'var(--danger-color)';
                } else if (d.state === 'IDLE') {
                    clearInterval(poll);
                    status.innerText = "Timed out or Aborted";
                }
            });
        }, 500);
    })
    .catch(() => {
        status.innerText = "Request Failed";
        status.style.color = 'var(--danger-color)';
    });
}
