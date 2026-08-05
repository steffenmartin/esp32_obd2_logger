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
    .status-banner { padding: .75rem 1rem; margin: 1rem 0; border-radius: 4px; display: none; }
    .status-dropped { background: #fdecea; border: 1px solid #f5c2c0; }
    .status-connecting { background: #fff8e1; border: 1px solid #ffe082; }
    .status-connected { background: #e8f5e9; border: 1px solid #a5d6a7; }
    .status-scanning { background: #e3f2fd; border: 1px solid #90caf9; }
    #scan-button:disabled { opacity: .5; cursor: default; }
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

      if (devices.length === 0) {
        // Covers both "never scanned yet" (boot-time state) and "scan
        // completed but found nothing" - the /devices-json payload
        // alone can't distinguish the two, and a single message that
        // reads fine either way is simpler than trying to.
        let emptyRow = '<tr><td colspan="4">No devices found. Click "Scan for devices" to search.</td></tr>';
        tbodyLe.innerHTML = emptyRow;
        tbodyOther.innerHTML = emptyRow;
        return;
      }

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

    // Local mirror of "was the last known state Scanning" - used only
    // to detect the *moment* a scan ends (naturally completing or being
    // cancelled), so results can be pulled exactly once right then,
    // rather than continuously polling /devices-json in the background
    // the way this page used to (see design doc S1: scans are static
    // per-request snapshots, not a live background feed).
    let wasScanning = false;

    function renderStatus(status) {
      let banner = document.getElementById('status-banner');
      let scanButton = document.getElementById('scan-button');
      let name = status.device ? status.device.name : 'device';

      scanButton.disabled = (status.state !== 'disconnected');

      if (status.state === 'scanning') {
        wasScanning = true;
        banner.className = 'status-banner status-scanning';
        banner.style.display = 'block';
        banner.innerHTML = 'Scanning for nearby BLE devices… ' +
          '<button onclick="cancelScan()">Cancel</button>';
        return;
      }
      if (wasScanning) {
        // Scan just ended (completed on its own or was cancelled) -
        // pull whatever results it found. A one-shot fetch here, not a
        // recurring poll - see the comment on wasScanning above.
        wasScanning = false;
        refresh();
      }

      if (status.state === 'dropped') {
        banner.className = 'status-banner status-dropped';
        banner.style.display = 'block';
        banner.innerHTML = `<strong>Connection dropped:</strong> ${name} (${status.reason || 'unknown reason'}). ` +
          `<button onclick="retryConnect()">Retry</button> ` +
          `<button onclick="abortConnect()">Abort</button>`;
      } else if (status.state === 'connecting') {
        banner.className = 'status-banner status-connecting';
        banner.style.display = 'block';
        banner.textContent = `Connecting to ${name}…`;
      } else if (status.state === 'connected') {
        banner.className = 'status-banner status-connected';
        banner.style.display = 'block';
        banner.innerHTML = `Connected to ${name}. <a href="/terminal">Open terminal</a>`;
      } else {
        banner.style.display = 'none';
      }
    }

    // Starts a scan (design doc S1 - always user-triggered, never
    // automatic). The actual "in progress" feedback comes from the next
    // /api/status poll picking up state=scanning, same pattern as
    // retryConnect() below - no need for an eager local banner update
    // here since /scan itself doesn't block the way /connect does.
    async function startScan() {
      await fetch('/scan');
      refreshStatus();
    }

    async function cancelScan() {
      await fetch('/scan/cancel');
      refreshStatus();
    }

    // Reuses the same /connect route and guard (guardedConnect() in
    // web_server.cpp) as a fresh connection - retrying from Dropped
    // works because bleGatewayTargetAddress()/Name() are still holding
    // the last-connected device (only an explicit /disconnect clears
    // them), so this "just" re-attempts the same target.
    // Updates the banner immediately, client-side, before the fetch
    // even starts - /connect blocks the whole server for up to
    // BLE_CONNECT_TIMEOUT_S seconds (see app_config.h), during which
    // the browser's own /api/status polling can't get a response
    // either (single-threaded WebServer - see ble_gateway.cpp), so
    // without this the banner would just sit frozen on stale content
    // for the entire attempt with no sign anything is happening. This
    // also matters on a *second* attempt after a failed one - without
    // an eager update here, clicking Retry again while already showing
    // "reconnect attempt failed" gave no visible sign the click
    // registered at all.
    async function retryConnect() {
      let banner = document.getElementById('status-banner');
      banner.className = 'status-banner status-connecting';
      banner.style.display = 'block';
      banner.textContent = 'Reconnecting…';
      let response = await fetch('/connect');
      if (response.ok) window.location.href = '/terminal';
      else refreshStatus();
    }

    async function abortConnect() {
      await fetch('/disconnect');
      refreshStatus();
      refresh();
    }

    async function refreshStatus() {
      try {
        let response = await fetch('/api/status');
        renderStatus(await response.json());
      } catch (e) { console.error(e); }
    }
    setInterval(refreshStatus, 2000);
  </script>
</head>
<body>
  <h1>ESP32 OBD-II Logger</h1>
  <div id="status-banner"></div>
  <p>Select a compatible BLE dongle. Once connected, the logger records raw OBD exchanges automatically.</p>
  <p class="actions">
    <button id="scan-button" onclick="startScan()">Scan for devices</button> ·
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
    <tbody id="device-list-le"><tr><td colspan="4">No scan yet - click "Scan for devices" above.</td></tr></tbody>
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
    <tbody id="device-list-other"><tr><td colspan="4">No scan yet - click "Scan for devices" above.</td></tr></tbody>
  </table>
  <script>refresh(); refreshStatus();</script>
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
    .status-banner { padding: .75rem 1rem; margin: 1rem 0; border-radius: 4px; display: none; }
    .status-dropped { background: #fdecea; border: 1px solid #f5c2c0; }
    .status-connecting { background: #fff8e1; border: 1px solid #ffe082; }
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

    // Terminal previously had no way to notice a dropped link short of
    // staring at a silently stale page - see docs/design/webui-state-
    // design.md S5 for the full "Dropped card" this is a reduced version
    // of (device + reason + Retry/Abort only, no session-duration or
    // data-safety readout yet).
    function renderStatus(status) {
      let banner = document.getElementById('status-banner');
      if (status.state === 'dropped') {
        let name = status.device ? status.device.name : 'device';
        banner.className = 'status-banner status-dropped';
        banner.style.display = 'block';
        banner.innerHTML = `<strong>Connection dropped:</strong> ${name} (${status.reason || 'unknown reason'}). ` +
          `<button onclick="retryConnect()">Retry</button> ` +
          `<button onclick="disconnect()">Back to discovery</button>`;
      } else {
        banner.style.display = 'none';
      }
    }

    // Same guarded /connect route as a fresh connection - see the
    // comment on the equivalent function on the dashboard page for why
    // this correctly re-targets the same device, and for why the
    // eager banner update below (before the fetch even starts) matters
    // given /connect's blocking duration.
    async function retryConnect() {
      let banner = document.getElementById('status-banner');
      banner.className = 'status-banner status-connecting';
      banner.style.display = 'block';
      banner.textContent = 'Reconnecting…';
      let response = await fetch('/connect');
      if (response.ok) {
        banner.style.display = 'none';
        refresh();
      } else {
        refreshStatus();
      }
    }

    async function refreshStatus() {
      try {
        let response = await fetch('/api/status');
        renderStatus(await response.json());
      } catch (e) { console.error(e); }
    }
    setInterval(refreshStatus, 2000);
    refreshStatus();
  </script>
</head>
<body>
  <h1>OBD Terminal: )HTML") + address + R"HTML(</h1>
  <div id="status-banner"></div>
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