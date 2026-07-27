#include "web_pages.h"

String webPagesBase(const String &title, const String &content) {
  return R"HTML(<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>)HTML" + title + R"HTML( - OBDII Logger</title>
  <script defer src="https://cdn.jsdelivr.net/npm/alpinejs@3.x.x/dist/cdn.min.js"></script>
  <style>
    body { font: 16px system-ui; margin: 2rem; max-width: 900px; }
    .status-bar { padding: 1rem; margin-bottom: 1rem; border-radius: 8px; font-weight: bold; }
    .connected { background: #d4edda; color: #155724; }
    .disconnected { background: #f8d7da; color: #721c24; }
    .connecting { background: #fff3cd; color: #856404; }
    .content { padding: 1rem; border: 1px solid #ddd; }
  </style>
  <script>
    document.addEventListener("alpine:init", () => {
      Alpine.store("conn", {
        status: "DISCONNECTED",
        target: "",
        devices: [],
        selectedAddr: "",
        async init() {
          this.refresh();
          setInterval(() => { if (this.status !== "CONNECTED") this.refresh(); }, 5000);
        },
        async refresh() {
          try {
            const res = await fetch("/api/state");
            const data = await res.json();
            this.status = data.connection;
            this.target = data.target;
            if (data.devices) {
              this.devices = data.devices.filter(d => d.isLeLink);
            }
          } catch(e) { console.error("API error", e); }
        },
        async connect() {
          if (!this.selectedAddr) return;
          this.status = "CONNECTING";
          await fetch("/connect?addr=" + this.selectedAddr);
          // Wait briefly, then refresh
          setTimeout(() => this.refresh(), 1000);
        }
      });
    });
  </script>
</head>
<body x-data>
  <nav style="padding: 1rem; border-bottom: 1px solid #ccc; margin-bottom: 1rem;">
    <a href="/">Home</a> | 
    <span x-show="$store.conn.status === 'CONNECTED'">
        <a href="/terminal">Terminal</a> | 
        <a href="/survey">Survey</a>
    </span>
  </nav>

  <div class="status-bar" :class="{ 'connected': $store.conn.status === 'CONNECTED', 'disconnected': $store.conn.status === 'DISCONNECTED', 'connecting': $store.conn.status === 'CONNECTING' }">
    System: <span x-text="$store.conn.status"></span>
    <template x-if="$store.conn.status === 'CONNECTED'">
      <span>(Target: <span x-text="$store.conn.target"></span>)</span>
    </template>
    <button x-show="$store.conn.status === 'CONNECTED'" @click="fetch('/disconnect').then(() => location.reload())">Disconnect</button>
  </div>
  
  <div class="content">
  )HTML" + content + R"HTML(
  </div>
</body>
</html>)HTML";
}

String webPagesDashboard() {
  String inner = R"HTML(
    <h1>Home</h1>
    <div x-show="$store.conn.status === 'DISCONNECTED'">
      <p>Select a compatible BLE dongle.</p>
      <select x-model="$store.conn.selectedAddr">
        <option value="">-- Choose a device --</option>
        <template x-for="dev in $store.conn.devices" :key="dev.address">
          <option :value="dev.address" x-text="dev.name + ' (' + dev.address + ')'"></option>
        </template>
      </select>
      <button @click="$store.conn.connect()" :disabled="!$store.conn.selectedAddr">Connect</button>
      <button @click="$store.conn.refresh()">Refresh List</button>
    </div>
    <div x-show="$store.conn.status === 'CONNECTING'">
      <p>Attempting to connect to dongle... please wait.</p>
    </div>
    <div x-show="$store.conn.status === 'CONNECTED'">
      <p>System Ready. Use the navigation menu above to access tools.</p>
    </div>
  )HTML";
  return webPagesBase("Home", inner);
}

String webPagesTerminal(const String &address) {
  String inner = R"HTML(
    <h1>OBD Terminal</h1>
    <div x-show="$store.conn.status === 'DISCONNECTED'">
        <p>Not connected. <a href="/">Return to Home</a></p>
    </div>
    <div x-show="$store.conn.status === 'CONNECTING'">
        <p>Connecting... please wait.</p>
    </div>
    <div x-show="$store.conn.status === 'CONNECTED'">
        <p>Interactive Mode: Commands are sent manually.</p>
        <pre id="log">Active connection to <span x-text="$store.conn.target"></span></pre>
    </div>
  )HTML";
  return webPagesBase("Terminal", inner);
}

String webPagesServerStatus() {
  String inner = R"HTML(
    <h1>Server Status</h1>
    <pre id="status">Loading...</pre>
  )HTML";
  return webPagesBase("Server Status", inner);
}
