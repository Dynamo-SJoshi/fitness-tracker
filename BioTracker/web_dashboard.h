#pragma once

#include <Arduino.h>

// Self-contained, zero-dependency, dark-mode responsive dashboard
// Designed for 100% off-grid SoftAP deployment (no CDN, no external assets)
const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<title>BioTracker Ankle Hub</title>
<style>
  :root {
    --bg-base: #0a0d14;
    --card-bg: rgba(21, 28, 44, 0.75);
    --card-border: rgba(255, 255, 255, 0.08);
    --text-primary: #f1f5f9;
    --text-secondary: #94a3b8;
    --accent-cyan: #06b6d4;
    --accent-emerald: #10b981;
    --accent-rose: #f43f5e;
    --accent-amber: #f59e0b;
    --accent-purple: #a855f7;
    --accent-blue: #3b82f6;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; }
  body {
    background-color: var(--bg-base);
    color: var(--text-primary);
    padding: 14px;
    padding-bottom: 40px;
    min-height: 100vh;
  }
  .header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 12px 6px 18px;
    border-bottom: 1px solid var(--card-border);
    margin-bottom: 14px;
  }
  .title-area h1 { font-size: 1.25rem; font-weight: 700; letter-spacing: -0.5px; display: flex; align-items: center; gap: 8px; }
  .title-area p { font-size: 0.75rem; color: var(--text-secondary); margin-top: 2px; }
  .status-badge {
    background: rgba(16, 185, 129, 0.15);
    color: var(--accent-emerald);
    border: 1px solid rgba(16, 185, 129, 0.3);
    padding: 4px 10px;
    border-radius: 9999px;
    font-size: 0.72rem;
    font-weight: 600;
    display: flex;
    align-items: center;
    gap: 6px;
  }
  .status-dot { width: 7px; height: 7px; border-radius: 50%; background: var(--accent-emerald); box-shadow: 0 0 8px var(--accent-emerald); animation: pulseDot 2s infinite; }
  @keyframes pulseDot { 0%, 100% { opacity: 1; transform: scale(1); } 50% { opacity: 0.4; transform: scale(0.85); } }

  .grid {
    display: grid;
    grid-template-columns: repeat(2, 1fr);
    gap: 12px;
    margin-bottom: 14px;
  }
  .grid-full { grid-column: span 2; }
  
  .card {
    background: var(--card-bg);
    border: 1px solid var(--card-border);
    border-radius: 14px;
    padding: 14px;
    backdrop-filter: blur(12px);
    position: relative;
    overflow: hidden;
  }
  .card::before {
    content: "";
    position: absolute;
    top: 0; left: 0; right: 0; height: 3px;
    background: var(--card-accent, transparent);
  }
  .card-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 8px;
  }
  .card-label {
    font-size: 0.72rem;
    font-weight: 600;
    text-transform: uppercase;
    letter-spacing: 0.8px;
    color: var(--text-secondary);
  }
  .card-icon { font-size: 1rem; opacity: 0.9; }
  .card-value {
    font-size: 1.85rem;
    font-weight: 800;
    line-height: 1.1;
    letter-spacing: -0.5px;
    margin-bottom: 4px;
  }
  .card-unit { font-size: 0.85rem; font-weight: 500; color: var(--text-secondary); margin-left: 2px; }
  .card-subtext {
    font-size: 0.74rem;
    color: var(--text-secondary);
    display: flex;
    justify-content: space-between;
  }
  .card-bpm { --card-accent: var(--accent-rose); }
  .card-steps { --card-accent: var(--accent-cyan); }
  .card-calories { --card-accent: var(--accent-amber); }
  .card-altitude { --card-accent: var(--accent-purple); }
  .card-sweat { --card-accent: var(--accent-emerald); }

  .bpm-pulse { display: inline-block; animation: heartBeat 1.2s infinite ease-in-out; }
  @keyframes heartBeat { 0%, 100% { transform: scale(1); } 15% { transform: scale(1.22); } 30% { transform: scale(1.05); } 45% { transform: scale(1.18); } }

  /* Canvas chart */
  canvas {
    width: 100%;
    height: 70px;
    border-radius: 8px;
    background: rgba(0, 0, 0, 0.25);
    margin-top: 8px;
    display: block;
  }

  /* Form & Settings */
  .section-title {
    font-size: 0.82rem;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.9px;
    color: var(--text-secondary);
    margin: 18px 4px 8px;
    display: flex;
    align-items: center;
    gap: 6px;
  }
  .form-group {
    display: flex;
    gap: 10px;
    margin-bottom: 10px;
  }
  .input-wrapper { flex: 1; }
  .input-wrapper label {
    display: block;
    font-size: 0.72rem;
    color: var(--text-secondary);
    margin-bottom: 4px;
    font-weight: 600;
  }
  input[type="number"] {
    width: 100%;
    background: rgba(10, 13, 20, 0.8);
    border: 1px solid var(--card-border);
    border-radius: 8px;
    padding: 10px 12px;
    color: var(--text-primary);
    font-size: 0.95rem;
    font-weight: 600;
    outline: none;
  }
  input[type="number"]:focus {
    border-color: var(--accent-cyan);
    box-shadow: 0 0 0 2px rgba(6, 182, 212, 0.2);
  }

  /* Buttons */
  .btn-row { display: flex; gap: 8px; margin-top: 6px; }
  button {
    flex: 1;
    background: #1e293b;
    border: 1px solid var(--card-border);
    color: var(--text-primary);
    font-size: 0.82rem;
    font-weight: 600;
    padding: 11px 12px;
    border-radius: 8px;
    cursor: pointer;
    transition: all 0.2s;
    display: flex;
    justify-content: center;
    align-items: center;
    gap: 6px;
  }
  button:active { transform: scale(0.98); }
  .btn-primary { background: linear-gradient(135deg, #0284c7, #06b6d4); border: none; }
  .btn-secondary { background: rgba(255, 255, 255, 0.05); }
  .btn-danger { background: rgba(244, 63, 94, 0.15); color: #fb7185; border-color: rgba(244, 63, 94, 0.3); }

  .meta-footer {
    text-align: center;
    margin-top: 24px;
    font-size: 0.7rem;
    color: #64748b;
    line-height: 1.5;
  }
  .tag { display: inline-block; padding: 2px 6px; border-radius: 4px; background: rgba(255, 255, 255, 0.06); font-family: monospace; }
  
  .badge-sweat {
    display: inline-block;
    padding: 2px 8px;
    border-radius: 6px;
    font-size: 0.7rem;
    font-weight: 700;
    text-transform: uppercase;
    background: rgba(16, 185, 129, 0.2);
    color: var(--accent-emerald);
  }
</style>
</head>
<body>

  <!-- Top Bar -->
  <header class="header">
    <div class="title-area">
      <h1>🦶 BioTracker <span>Node</span></h1>
      <p>Off-Grid Lower-Limb Biometrics &bull; ESP32</p>
    </div>
    <div class="status-badge" id="conn-badge">
      <div class="status-dot"></div>
      <span id="conn-text">SoftAP Live</span>
    </div>
  </header>

  <!-- Metric Grid -->
  <div class="grid">
    <!-- Heart Rate PPG -->
    <div class="card card-bpm">
      <div class="card-header">
        <span class="card-label">Heart Rate</span>
        <span class="card-icon bpm-pulse">❤️</span>
      </div>
      <div class="card-value"><span id="val-bpm">--</span><span class="card-unit">BPM</span></div>
      <div class="card-subtext">
        <span>Y1708 Inner Ankle</span>
        <span id="val-raw-ppg">ADC: --</span>
      </div>
    </div>

    <!-- Pedometer Steps -->
    <div class="card card-steps">
      <div class="card-header">
        <span class="card-label">Footfalls</span>
        <span class="card-icon">👟</span>
      </div>
      <div class="card-value"><span id="val-steps">0</span><span class="card-unit">steps</span></div>
      <div class="card-subtext">
        <span>Cadence: <b id="val-cadence">0</b> spm</span>
        <span>MPU6050</span>
      </div>
    </div>

    <!-- Dynamic Caloric Burn -->
    <div class="card card-calories">
      <div class="card-header">
        <span class="card-label">Active Burn</span>
        <span class="card-icon">🔥</span>
      </div>
      <div class="card-value"><span id="val-cals">0.0</span><span class="card-unit">kcal</span></div>
      <div class="card-subtext">
        <span>Flat: <span id="val-flat-cals">0.0</span></span>
        <span>Climb: <span id="val-vert-cals">0.0</span></span>
      </div>
    </div>

    <!-- BMP280 Elevation Gain -->
    <div class="card card-altitude">
      <div class="card-header">
        <span class="card-label">Elevation Gain</span>
        <span class="card-icon">⛰️</span>
      </div>
      <div class="card-value"><span id="val-elev">0.0</span><span class="card-unit">m</span></div>
      <div class="card-subtext">
        <span>Alt: <span id="val-alt">--</span>m</span>
        <span>BMP280</span>
      </div>
    </div>

    <!-- Perspiration / Sweat Microclimate Card (Full Width) -->
    <div class="card card-sweat grid-full">
      <div class="card-header">
        <span class="card-label">Hydration & Microclimate</span>
        <span class="badge-sweat" id="val-sweat-badge">Basal / Dry</span>
      </div>
      <div style="display: flex; justify-content: space-between; align-items: baseline; margin: 4px 0 8px;">
        <div class="card-value" style="font-size: 1.5rem;">
          <span id="val-deltarh">+0.0</span><span class="card-unit">% &Delta;RH</span>
        </div>
        <div style="font-size: 0.85rem; color: var(--text-secondary); text-align: right;">
          Sweat Rate: <b id="val-sweatrate" style="color: var(--accent-emerald);">0.00</b> %/min<br>
          Skin Temp: <b id="val-temp" style="color: var(--text-primary);">--</b>&deg;C &bull; RH: <span id="val-rh">--</span>%
        </div>
      </div>
      <canvas id="motionChart" height="60"></canvas>
    </div>
  </div>

  <!-- Profile Settings & Flash Memory Interaction -->
  <div class="section-title">⚙️ Athlete Profile (ESP32 NVS Flash)</div>
  <div class="card">
    <div class="form-group">
      <div class="input-wrapper">
        <label for="input-weight">Weight (kg)</label>
        <input type="number" id="input-weight" step="0.5" min="30" max="250" placeholder="70.0">
      </div>
      <div class="input-wrapper">
        <label for="input-height">Height (cm)</label>
        <input type="number" id="input-height" step="1" min="100" max="230" placeholder="175">
      </div>
    </div>
    <div class="btn-row">
      <button class="btn-primary" onclick="saveProfile()">💾 Push to NVS Memory</button>
    </div>
  </div>

  <!-- Session Controls -->
  <div class="section-title">⚡ Session Controls</div>
  <div class="card">
    <div class="btn-row">
      <button class="btn-secondary" onclick="calibrateBaseline()">💧 Zero Humidity</button>
      <button class="btn-secondary" onclick="zeroAltitude()">⛰️ Zero Elevation</button>
      <button class="btn-danger" onclick="resetSession()">🔄 Reset Session</button>
    </div>
  </div>

  <!-- System Information & Academic Attribution -->
  <div class="meta-footer">
    Split-Node Harness: Pod A (Laces) &harr; 5-Wire &harr; Pod B (Cuff)<br>
    Off-Grid AP: <span class="tag">ESP32_BioTracker</span> &bull; IP: <span class="tag">192.168.4.1</span><br>
    Uptime: <span id="val-uptime">0s</span> &bull; Debounce: <span class="tag">350ms</span> &bull; Draft Filter: <span class="tag">0.5m</span>
  </div>

<script>
  // Dynamic Canvas Real-Time Chart (Zero External Dependency)
  const canvas = document.getElementById('motionChart');
  const ctx = canvas.getContext('2d');
  const chartHistory = [];
  const MAX_HISTORY = 40;

  function resizeCanvas() {
    canvas.width = canvas.parentElement.clientWidth - 28;
    canvas.height = 60;
  }
  window.addEventListener('resize', resizeCanvas);
  resizeCanvas();

  function drawChart(val) {
    chartHistory.push(val);
    if (chartHistory.length > MAX_HISTORY) chartHistory.shift();

    ctx.clearRect(0, 0, canvas.width, canvas.height);
    if (chartHistory.length < 2) return;

    const max = Math.max(...chartHistory, 20.0);
    const min = Math.min(...chartHistory, 0.0);
    const range = (max - min) || 1.0;

    // Gridline
    ctx.strokeStyle = "rgba(255,255,255,0.06)";
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(0, canvas.height / 2);
    ctx.lineTo(canvas.width, canvas.height / 2);
    ctx.stroke();

    // Line
    ctx.beginPath();
    ctx.strokeStyle = "#10b981";
    ctx.lineWidth = 2;
    for (let i = 0; i < chartHistory.length; i++) {
      const x = (i / (MAX_HISTORY - 1)) * canvas.width;
      const y = canvas.height - ((chartHistory[i] - min) / range) * (canvas.height - 12) - 6;
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.stroke();

    // Glow under fill
    ctx.lineTo(canvas.width, canvas.height);
    ctx.lineTo(0, canvas.height);
    ctx.fillStyle = "rgba(16, 185, 129, 0.12)";
    ctx.fill();
  }

  // Real-Time AJAX Polling (1000ms loop)
  let profileLoaded = false;
  async function fetchBiometrics() {
    try {
      const res = await fetch('/api/data', { cache: 'no-store' });
      if (!res.ok) throw new Error("HTTP error " + res.status);
      const data = await res.json();

      // Update UI elements
      document.getElementById('val-bpm').innerText = data.bpm > 0 ? data.bpm : '--';
      document.getElementById('val-raw-ppg').innerText = 'ADC: ' + data.raw_ppg;
      document.getElementById('val-steps').innerText = data.steps.toLocaleString();
      document.getElementById('val-cadence').innerText = Math.round(data.cadence);
      document.getElementById('val-cals').innerText = data.calories.toFixed(1);
      document.getElementById('val-flat-cals').innerText = data.flat_cal.toFixed(1);
      document.getElementById('val-vert-cals').innerText = data.vert_cal.toFixed(1);
      document.getElementById('val-elev').innerText = data.elevation_gain.toFixed(1);
      document.getElementById('val-alt').innerText = data.altitude.toFixed(0);

      document.getElementById('val-deltarh').innerText = (data.delta_rh >= 0 ? '+' : '') + data.delta_rh.toFixed(1);
      document.getElementById('val-sweatrate').innerText = data.sweat_rate.toFixed(2);
      document.getElementById('val-temp').innerText = data.temp.toFixed(1);
      document.getElementById('val-rh').innerText = data.humidity.toFixed(0);
      document.getElementById('val-sweat-badge').innerText = data.sweat_status;

      // Update badge style
      const badge = document.getElementById('val-sweat-badge');
      if (data.delta_rh > 25) {
        badge.style.background = 'rgba(244, 63, 94, 0.25)';
        badge.style.color = '#fb7185';
      } else if (data.delta_rh > 12) {
        badge.style.background = 'rgba(245, 158, 11, 0.25)';
        badge.style.color = '#fbbf24';
      } else {
        badge.style.background = 'rgba(16, 185, 129, 0.2)';
        badge.style.color = '#34d399';
      }

      drawChart(data.delta_rh);

      // Form fill once on load
      if (!profileLoaded && data.weight && data.height) {
        document.getElementById('input-weight').value = data.weight;
        document.getElementById('input-height').value = data.height;
        profileLoaded = true;
      }

      // Format uptime
      const sec = data.uptime % 60;
      const min = Math.floor(data.uptime / 60) % 60;
      const hr = Math.floor(data.uptime / 3600);
      document.getElementById('val-uptime').innerText = (hr > 0 ? hr + 'h ' : '') + min + 'm ' + sec + 's';

      document.getElementById('conn-text').innerText = 'SoftAP Live';
      document.getElementById('conn-badge').style.borderColor = 'rgba(16, 185, 129, 0.3)';
    } catch (err) {
      document.getElementById('conn-text').innerText = 'Reconnecting...';
      document.getElementById('conn-badge').style.borderColor = 'rgba(244, 63, 94, 0.5)';
    }
  }

  setInterval(fetchBiometrics, 1000);
  fetchBiometrics();

  // Two-Way Profile Updates to NVS
  async function saveProfile() {
    const weight = parseFloat(document.getElementById('input-weight').value);
    const height = parseFloat(document.getElementById('input-height').value);
    if (!weight || !height) {
      alert("Please enter valid positive numbers for weight and height.");
      return;
    }
    try {
      const res = await fetch(`/api/profile?weight=${encodeURIComponent(weight)}&height=${encodeURIComponent(height)}`, { method: 'POST' });
      if (res.ok) {
        alert("✅ Athlete profile successfully committed to ESP32 Flash memory!");
        fetchBiometrics();
      } else {
        alert("❌ Failed to save profile.");
      }
    } catch (e) {
      alert("Error sending profile: " + e.message);
    }
  }

  async function resetSession() {
    if (confirm("Reset current step count, elevation, and calorie counters to zero?")) {
      await fetch('/api/reset', { method: 'POST' });
      fetchBiometrics();
    }
  }

  async function calibrateBaseline() {
    if (confirm("Calibrate current skin microclimate humidity as the reference baseline?")) {
      await fetch('/api/calibrate-humidity', { method: 'POST' });
      fetchBiometrics();
    }
  }

  async function zeroAltitude() {
    if (confirm("Zero altitude baseline to current barometric reading?")) {
      await fetch('/api/zero-altitude', { method: 'POST' });
      fetchBiometrics();
    }
  }
</script>
</body>
</html>
)rawliteral";
