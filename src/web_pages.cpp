#include "web_pages.h"

String webPagesDashboard() {
  return R"HTML(<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>ESP32 OBD-II Logger</title>
  <style>
    body { font: 16px system-ui; margin: 2rem; max-width: 900px; }
    table { border-collapse: collapse; width: 100%; cursor: pointer; }
    td, th { padding: .6rem; border-bottom: 1px solid #ddd; text-align: left; }
    th { background: #f4f4f4; }
    code { background: #eee; padding: .15rem; }
    .actions { margin: 1rem 0; }
    .lelink { background: #e8f5e9; }
  </style>
  <script>
    let devices = [];
    let sortCol = 'name';
    let sortAsc = true;

    function sortDevices(col) {
      if (sortCol === col) sortAsc = !sortAsc;
      else { sortCol = col; sortAsc = true; }
      render();
    }

    function render() {
      let tbodyLe = document.getElementById('device-list-le');
      let tbodyOther = document.getElementById('device-list-other');
      tbodyLe.innerHTML = '';
      tbodyOther.innerHTML = '';

      let sorted = [...devices].sort((a, b) => {
        let valA = a[sortCol].toString().toLowerCase();
        let valB = b[sortCol].toString().toLowerCase();
        let cmp = valA < valB ? -1 : valA > valB ? 1 : 0;
        return sortAsc ? cmp : -cmp;
      });

      sorted.forEach(d => {
        let row = document.createElement('tr');
        row.innerHTML = `<td>${d.name}</td><td><code>${d.address}</code></td><td>${d.rssi}</td>
                         <td>${d.isLeLink ? `<a href='/terminal?addr=${d.address}&type=${d.type}&name=${encodeURIComponent(d.name)}'>Connect</a>` : ''}</td>`;
        if (d.isLeLink) tbodyLe.appendChild(row);
        else tbodyOther.appendChild(row);
      });
    }

    async function refresh() {
      try {
        let response = await fetch('/devices-json');
        devices = await response.json();
        render();
      } catch (e) { console.error(e); }
    }
    setInterval(refresh, 5000);
  </script>
</head>
<body>
  <h1>ESP32 OBD-II Logger</h1>
  <p>Select a compatible BLE dongle. Once connected, the logger records raw OBD exchanges automatically.</p>
  <p class="actions">
    <a href="/raw-log.csv">Download raw log (CSV)</a> · 
    <a href="/diagnostics">Diagnostics</a>
  </p>
  <h3>LELink Candidates</h3>
  <table>
    <thead>
      <tr>
        <th onclick="sortDevices('name')">Name</th>
        <th onclick="sortDevices('address')">Address</th>
        <th onclick="sortDevices('rssi')">RSSI</th>
        <th>Action</th>
      </tr>
    </thead>
    <tbody id="device-list-le"></tbody>
  </table>
  <h3>Other Devices</h3>
  <table>
    <thead>
      <tr>
        <th onclick="sortDevices('name')">Name</th>
        <th onclick="sortDevices('address')">Address</th>
        <th onclick="sortDevices('rssi')">RSSI</th>
        <th>Action</th>
      </tr>
    </thead>
    <tbody id="device-list-other"><tr><td colspan="4">Loading…</td></tr></tbody>
  </table>
  <script>refresh();</script>
</body>
</html>)HTML";
}

String webPagesTerminal(const String &address) {
  return String(R"HTML(<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>OBD Terminal</title>
  <style>
    body { font: 16px system-ui; margin: 2rem; max-width: 900px; }
    pre { background: #111; color: #0f0; padding: 1rem; min-height: 20rem; white-space: pre-wrap; }
    input { width: 70%; padding: .5rem; }
  </style>
  <script>
    async function refresh() {
      let logElement = document.getElementById('log');
      let response = await fetch('/diagnostics');
      logElement.textContent = await response.text();
      logElement.scrollTop = logElement.scrollHeight;
    }

    async function send() {
      let commandInput = document.getElementById('command');
      if (!commandInput.value) return;
      await fetch('/send?cmd=' + encodeURIComponent(commandInput.value));
      commandInput.value = '';
      setTimeout(refresh, 500);
    }

    window.onload = async () => {
      document.getElementById('log').textContent = "Connecting...";
      await fetch('/connect');
      refresh();
    };
  </script>
</head>
<body>
  <h1>OBD Terminal: )HTML") + address + R"HTML(</h1>
  <p>Interactive Mode: Commands are sent manually.</p>
  
  <pre id="log">Initializing...</pre>
  
  <input id="command" 
         placeholder="ATZ or 010C" 
         onkeydown="if(event.key==='Enter') send()">
  <button onclick="send()">Send</button>
  <button onclick="refresh()">Refresh</button>
  
  <p>
    <a href="javascript:void(0)" onclick="disconnect()">Disconnect & Back to discovery</a>
  </p>
  <script>
    async function disconnect() {
      await fetch('/disconnect');
      window.location.href = '/';
    }
  </script>
</body>
</html>)HTML";
}

String webPagesServerStatus() {
  return R"HTML(<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>Server Status</title>
  <style>
    body { font: 16px system-ui; margin: 2rem; }
    pre { background: #eee; padding: 1rem; }
  </style>
  <script>
    function formatNumber(num) {
      return num.toLocaleString();
    }

    function formatTime(ms) {
      let seconds = Math.floor(ms / 1000);
      let minutes = Math.floor(seconds / 60);
      let hours = Math.floor(minutes / 60);
      let days = Math.floor(hours / 24);
      return `${days}d ${hours % 24}h ${minutes % 60}m ${seconds % 60}s ${ms % 1000}ms`;
    }

    async function updateStatus() {
      try {
        let response = await fetch('/server/status-json');
        let data = await response.json();
        document.getElementById('status').textContent = 
          `Free heap: ${formatNumber(data.freeHeap)} bytes\n` +
          `Uptime: ${formatTime(data.millis)}`;
      } catch (e) { console.error(e); }
    }
    setInterval(updateStatus, 1000);
    updateStatus();
  </script>
</head>
<body>
  <h1>Server Status</h1>
  <pre id="status">Loading...</pre>
</body>
</html>)HTML";
}