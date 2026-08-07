#include "web_pages.h"

String webPagesDashboard() {
  return R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>OBD2 Logger</title>
  <script src="/static/alpine.min.js" defer></script>
  <style>
    :root {
      --bg: #0a0f0c;
      --panel: #0f1611;
      --panel-raised: #141d16;
      --phosphor: #4dff9e;
      --phosphor-dim: #2a6b48;
      --phosphor-faint: #163828;
      --amber: #ffb000;
      --red: #ff5d5d;
      --mono: ui-monospace, "SF Mono", "Cascadia Code", Consolas, "Roboto Mono", monospace;
      --sans: -apple-system, "Segoe UI", Roboto, sans-serif;
    }
    * { box-sizing: border-box; }
    html, body {
      margin: 0;
      min-height: 100%;
      background: var(--bg);
      background-image: radial-gradient(ellipse 900px 500px at 50% -10%, rgba(77,255,158,0.08), transparent 60%);
      color: var(--phosphor);
      font-family: var(--sans);
      -webkit-font-smoothing: antialiased;
    }
    a { color: var(--phosphor); }
    button {
      font-family: var(--mono);
      text-transform: uppercase;
      letter-spacing: .06em;
      font-weight: 600;
      font-size: .8rem;
      background: transparent;
      color: var(--phosphor);
      border: 1px solid var(--phosphor-dim);
      border-radius: 3px;
      padding: .6rem 1rem;
      cursor: pointer;
      transition: border-color .15s, background .15s;
    }
    button:hover:not(:disabled) { border-color: var(--phosphor); background: rgba(77,255,158,0.08); }
    button:disabled { opacity: .35; cursor: default; }
    button.primary { border-color: var(--phosphor); }
    button.danger { border-color: var(--red); color: var(--red); }
    button.danger:hover:not(:disabled) { background: rgba(255,93,93,0.08); }

    .crt {
      position: relative;
      max-width: 720px;
      margin: 0 auto;
      min-height: 100vh;
      padding: 1.5rem 1.25rem 3rem;
      overflow: hidden;
    }
    @media (min-width: 640px) {
      .crt {
        margin: 2.5rem auto;
        min-height: 0;
        border: 1px solid var(--phosphor-faint);
        border-radius: 10px;
        box-shadow: 0 0 0 1px rgba(0,0,0,0.4), 0 0 50px rgba(77,255,158,0.07), 0 0 140px rgba(77,255,158,0.03);
        background: var(--panel);
      }
    }
    .crt::before {
      content: "";
      position: absolute;
      inset: 0;
      pointer-events: none;
      background-image: repeating-linear-gradient(
        to bottom,
        rgba(77,255,158,0.035) 0px,
        rgba(77,255,158,0.035) 1px,
        transparent 1px,
        transparent 2px
      );
      mix-blend-mode: screen;
    }

    header {
      display: flex;
      align-items: baseline;
      justify-content: space-between;
      margin-bottom: 1.5rem;
      padding-bottom: 1rem;
      border-bottom: 1px solid var(--phosphor-faint);
    }
    header h1 {
      font-family: var(--mono);
      font-size: 1rem;
      font-weight: 700;
      letter-spacing: .08em;
      margin: 0;
      display: flex;
      align-items: baseline;
      gap: .5rem;
    }
    .cursor { animation: blink 1.1s steps(1) infinite; }
    @keyframes blink { 50% { opacity: 0; } }
    @media (prefers-reduced-motion: reduce) {
      .cursor { animation: none; }
    }
    .wifi-indicator {
      font-family: var(--mono);
      font-size: .7rem;
      letter-spacing: .04em;
      display: flex;
      align-items: center;
      gap: .4rem;
      color: var(--phosphor-dim);
    }
    .wifi-dot {
      width: 6px; height: 6px; border-radius: 50%;
      background: var(--phosphor-dim);
      box-shadow: 0 0 6px currentColor;
    }
    .wifi-indicator.strong { color: var(--phosphor); } .wifi-indicator.strong .wifi-dot { background: var(--phosphor); }
    .wifi-indicator.weak { color: var(--amber); } .wifi-indicator.weak .wifi-dot { background: var(--amber); }
    .wifi-indicator.down { color: var(--red); } .wifi-indicator.down .wifi-dot { background: var(--red); }

    .eyebrow {
      font-family: var(--mono);
      font-size: .7rem;
      font-weight: 700;
      letter-spacing: .12em;
      text-transform: uppercase;
      color: var(--phosphor-dim);
      margin: 1.75rem 0 .6rem;
    }
    .eyebrow.collapsible {
      cursor: pointer;
      user-select: none;
      display: flex;
      align-items: center;
      gap: .4rem;
    }
    .eyebrow.collapsible:hover { color: var(--phosphor); }
    .eyebrow .chevron { font-size: .65rem; display: inline-block; width: .8em; }

    .lead {
      font-size: .9rem;
      line-height: 1.5;
      color: #9fd8b8;
      margin: 0 0 1.25rem;
    }

    .banner {
      font-family: var(--mono);
      font-size: .82rem;
      line-height: 1.6;
      border: 1px solid var(--phosphor-dim);
      background: var(--panel-raised);
      border-radius: 4px;
      padding: .85rem 1rem;
      margin-bottom: 1.25rem;
      display: flex;
      flex-wrap: wrap;
      align-items: center;
      gap: .6rem .9rem;
    }
    .banner .label {
      font-weight: 700;
      letter-spacing: .06em;
      text-transform: uppercase;
      padding: .1rem .4rem;
      border-radius: 2px;
      font-size: .7rem;
    }
    .banner.connecting { border-color: var(--amber); }
    .banner.connecting .label { background: rgba(255,176,0,0.15); color: var(--amber); }
    .banner.dropped { border-color: var(--red); }
    .banner.dropped .label { background: rgba(255,93,93,0.15); color: var(--red); }
    .banner.connected .label { background: rgba(77,255,158,0.15); color: var(--phosphor); }
    .banner .msg { flex: 1 1 auto; min-width: 0; }
    .banner .actions { display: flex; gap: .5rem; flex-wrap: wrap; }
    .banner button { font-size: .7rem; padding: .4rem .7rem; }

    .scan-row {
      display: flex;
      align-items: center;
      gap: 1rem;
      margin-bottom: .25rem;
      min-height: 2.4rem;
    }
    .scan-inline {
      display: flex;
      align-items: center;
      gap: .75rem;
      font-family: var(--mono);
      font-size: .8rem;
      color: var(--amber);
    }
    .scan-inline button {
      font-size: .7rem;
      padding: .4rem .7rem;
      border-color: var(--amber);
      color: var(--amber);
    }
    .scan-inline button:hover:not(:disabled) { background: rgba(255,176,0,0.08); }

    .device-list { display: flex; flex-direction: column; gap: .5rem; }
    .device-row {
      display: flex;
      align-items: center;
      gap: .75rem;
      padding: .7rem .8rem;
      background: var(--panel-raised);
      border: 1px solid var(--phosphor-faint);
      border-radius: 4px;
    }
    .device-row.candidate { border-color: var(--phosphor-dim); }
    .device-info { flex: 1 1 auto; min-width: 0; }
    .device-name { font-size: .9rem; font-weight: 600; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
    .device-meta {
      font-family: var(--mono);
      font-size: .72rem;
      color: var(--phosphor-dim);
      display: flex;
      gap: .6rem;
      margin-top: .15rem;
      flex-wrap: wrap;
    }
    .device-meta .rssi.strong { color: var(--phosphor); }
    .device-meta .rssi.medium { color: var(--amber); }
    .device-meta .rssi.weak { color: var(--red); }
    .device-row button { flex-shrink: 0; }

    .empty-state {
      font-family: var(--mono);
      font-size: .8rem;
      color: var(--phosphor-dim);
      border: 1px dashed var(--phosphor-faint);
      border-radius: 4px;
      padding: 1.25rem;
      text-align: center;
    }

    footer {
      margin-top: 2.5rem;
      padding-top: 1rem;
      border-top: 1px solid var(--phosphor-faint);
      font-family: var(--mono);
      font-size: .72rem;
      color: var(--phosphor-dim);
      display: flex;
      gap: 1rem;
      flex-wrap: wrap;
    }
    footer a { color: var(--phosphor-dim); text-decoration: none; }
    footer a:hover { color: var(--phosphor); text-decoration: underline; }
  </style>
</head>
<body>
  <div class="crt" x-data="dashboard()" x-init="init()">
    <header>
      <h1>&gt;_ OBD2 LOGGER<span class="cursor">_</span></h1>
      <div class="wifi-indicator" :class="wifiClass()">
        <span class="wifi-dot"></span>
        <span x-text="wifiLabel()"></span>
      </div>
    </header>

    <template x-if="status.state !== 'disconnected' && status.state !== 'scanning'">
      <div class="banner" :class="status.state">
        <span class="label" x-text="status.state"></span>
        <span class="msg" x-text="bannerMessage()"></span>
        <span class="actions">
          <template x-if="status.state === 'dropped'">
            <button @click="retryConnect()">Retry</button>
          </template>
          <template x-if="status.state === 'dropped'">
            <button class="danger" @click="abortConnect()">Abort</button>
          </template>
          <template x-if="status.state === 'connected'">
            <button class="primary" @click="window.location.href='/terminal'">Open terminal</button>
          </template>
        </span>
      </div>
    </template>

    <p class="lead">Select a compatible BLE dongle. Once connected, the logger records raw OBD exchanges automatically.</p>

    <div class="scan-row">
      <template x-if="status.state !== 'scanning'">
        <button class="primary" @click="startScan()" :disabled="status.state !== 'disconnected'">Scan for devices</button>
      </template>
      <template x-if="status.state === 'scanning'">
        <span class="scan-inline">
          <span>Scanning for nearby BLE devices…</span>
          <button @click="cancelScan()">Cancel</button>
        </span>
      </template>
    </div>

    <h2 class="eyebrow">LELink candidates</h2>
    <div class="device-list" x-show="candidates().length > 0">
      <template x-for="d in candidates()" :key="d.address">
        <div class="device-row candidate">
          <div class="device-info">
            <div class="device-name" x-text="d.name"></div>
            <div class="device-meta">
              <span x-text="d.address"></span>
              <span class="rssi" :class="rssiClass(d.rssi)" x-text="d.rssi + ' dBm'"></span>
            </div>
          </div>
          <button @click="connectTo(d)">Connect</button>
        </div>
      </template>
    </div>
    <div class="empty-state" x-show="candidates().length === 0">
      No devices found. Press "Scan for devices" to search.
    </div>

    <h2 class="eyebrow collapsible" @click="othersExpanded = !othersExpanded">
      <span class="chevron" x-text="othersExpanded ? '▾' : '▸'"></span>
      Other devices (<span x-text="others().length"></span>)
    </h2>
    <div class="device-list" x-show="othersExpanded && others().length > 0">
      <template x-for="d in others()" :key="d.address">
        <div class="device-row">
          <div class="device-info">
            <div class="device-name" x-text="d.name"></div>
            <div class="device-meta">
              <span x-text="d.address"></span>
              <span class="rssi" :class="rssiClass(d.rssi)" x-text="d.rssi + ' dBm'"></span>
            </div>
          </div>
        </div>
      </template>
    </div>
    <div class="empty-state" x-show="othersExpanded && others().length === 0">
      No devices found. Press "Scan for devices" to search.
    </div>

    <footer>
      <a href="/raw-log.csv">raw-log.csv</a>
      <a href="/diagnostics">diagnostics</a>
      <a href="/server/status">server status</a>
    </footer>
  </div>

  <script>
    function dashboard() {
      return {
        status: { state: 'disconnected', device: null, reason: null },
        devices: [],
        wifiRssi: null,
        // Collapsed by default - a full scan can turn up 30+ unrelated
        // BLE devices, which pushed the footer links (diagnostics, raw
        // log) well below the fold. Candidates stay always-expanded
        // since that's the section someone actually came here to act on.
        othersExpanded: false,

        init() {
          this.refreshStatus();
          this.refreshDevices();
          setInterval(() => this.refreshStatus(), 2000);
          setInterval(() => this.refreshWifi(), 5000);
          this.refreshWifi();
        },

        async refreshStatus() {
          try {
            let wasScanning = this.status.state === 'scanning';
            let response = await fetch('/api/status');
            this.status = await response.json();
            // A scan just ended (completed on its own or was cancelled) -
            // pull whatever results it found. A one-shot fetch here, not
            // a recurring poll - devices are a static snapshot per scan,
            // not a live background feed (design doc S1).
            if (wasScanning && this.status.state !== 'scanning') this.refreshDevices();
          } catch (e) { console.error(e); }
        },

        async refreshDevices() {
          try {
            let response = await fetch('/devices-json');
            this.devices = await response.json();
          } catch (e) { console.error(e); }
        },

        async refreshWifi() {
          try {
            let response = await fetch('/server/status-json');
            let data = await response.json();
            this.wifiRssi = ('wifiRssi' in data) ? data.wifiRssi : null;
          } catch (e) { console.error(e); }
        },

        async startScan() {
          await fetch('/scan');
          this.refreshStatus();
        },

        async cancelScan() {
          await fetch('/scan/cancel');
          this.refreshStatus();
        },

        async connectTo(d) {
          window.location.href = '/terminal?addr=' + d.address + '&type=' + d.type + '&name=' + encodeURIComponent(d.name);
        },

        async retryConnect() {
          let response = await fetch('/connect');
          if (response.ok) window.location.href = '/terminal';
          else this.refreshStatus();
        },

        async abortConnect() {
          await fetch('/disconnect');
          this.refreshStatus();
        },

        candidates() {
          return [...this.devices].filter(d => d.isLeLink).sort((a, b) => b.rssi - a.rssi);
        },
        others() {
          return [...this.devices].filter(d => !d.isLeLink).sort((a, b) => b.rssi - a.rssi);
        },

        rssiClass(rssi) {
          if (rssi >= -70) return 'strong';
          if (rssi >= -90) return 'medium';
          return 'weak';
        },

        bannerMessage() {
          let name = this.status.device ? this.status.device.name : 'device';
          switch (this.status.state) {
            case 'connecting': return 'Connecting to ' + name + '…';
            case 'dropped': return 'Connection dropped: ' + name + ' (' + (this.status.reason || 'unknown reason') + ')';
            case 'connected': return 'Connected to ' + name + '.';
            default: return '';
          }
        },

        wifiClass() {
          if (this.wifiRssi === null) return 'down';
          if (this.wifiRssi >= -60) return 'strong';
          if (this.wifiRssi >= -75) return 'weak';
          return 'down';
        },
        wifiLabel() {
          return this.wifiRssi === null ? 'wifi --' : 'wifi ' + this.wifiRssi + 'dBm';
        }
      };
    }
  </script>
</body>
</html>
)HTML";
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