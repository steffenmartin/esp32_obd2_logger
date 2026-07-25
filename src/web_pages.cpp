#include "web_pages.h"

String webPagesDashboard() {
  return R"HTML(<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>ESP32 OBD-II Logger</title>
  <style>
    body { font: 16px system-ui; margin: 2rem; max-width: 900px; }
    table { border-collapse: collapse; width: 100%; }
    td, th { padding: .6rem; border-bottom: 1px solid #ddd; }
    code { background: #eee; padding: .15rem; }
    .actions { margin: 1rem 0; }
  </style>
  <script>
    async function refresh() {
      let list = document.getElementById('device-list');
      list.innerHTML = await (await fetch('/devices')).text();
    }
    setInterval(refresh, 2000);
  </script>
</head>
<body>
  <h1>ESP32 OBD-II Logger</h1>
  <p>Select a compatible BLE dongle. Once connected, the logger records raw OBD exchanges automatically.</p>
  <p class="actions">
    <a href="/raw-log.csv">Download raw log (CSV)</a> · 
    <a href="/diagnostics">Diagnostics</a>
  </p>
  <table>
    <thead>
      <tr><th>Name</th><th>Address</th><th>RSSI</th><th></th></tr>
    </thead>
    <tbody id="device-list">Loading…</tbody>
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
    <a href="/">Back to discovery</a>
  </p>
</body>
</html>)HTML";
}
