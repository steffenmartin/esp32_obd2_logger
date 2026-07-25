#include "web_server.h"

#include <WebServer.h>

#include "ble_gateway.h"
#include "diagnostic_log.h"
#include "obd_log.h"
#include "web_pages.h"

namespace {
WebServer server(80);

String csvEscape(const String &value) {
  String escaped = "\"";
  for (size_t i = 0; i < value.length(); ++i) escaped += value[i] == '\"' ? "\"\"" : String(value[i]);
  return escaped + "\"";
}

void handleDisconnect() {
  bleGatewayDisconnect();
  server.send(200, "text/plain", "Disconnected");
}

void handleRoot() { 
  if (bleGatewayIsConnected()) {
    server.sendHeader("Location", "/terminal", true);
    server.send(302, "text/plain", "");
  } else {
    server.send(200, "text/html; charset=utf-8", webPagesDashboard());
  }
}
void handleDevices() {
  if (bleGatewayIsConnected()) {
    server.sendHeader("Location", "/terminal", true);
    server.send(302, "text/plain", "");
  } else {
    server.send(200, "text/html; charset=utf-8", bleGatewayDevicesHtml());
  }
}
void handleTerminal() {
  if (server.hasArg("addr")) {
    uint8_t addressType = server.hasArg("type") ? static_cast<uint8_t>(server.arg("type").toInt()) : 0; // 0 = BLE_ADDR_PUBLIC
    bleGatewaySetTargetAddress(server.arg("addr"), addressType);
    bleGatewayEnsureConnected(); // Attempt to connect upon navigating to terminal with addr
  }
  if (!bleGatewayIsConnected()) {
    server.sendHeader("Location", "/devices", true);
    server.send(302, "text/plain", "");
    return;
  }
  server.send(200, "text/html; charset=utf-8", webPagesTerminal(bleGatewayTargetAddress()));
}
void handleDiagnostics() { server.send(200, "text/plain; charset=utf-8", diagnosticLogGet()); }
void handleConnect() {
  bool connected = bleGatewayEnsureConnected();
  server.send(connected ? 200 : 409, "text/plain", connected ? "Connected" : "Connection failed");
}

void handleSend() {
  if (!server.hasArg("cmd")) { server.send(400, "text/plain", "Missing command parameter"); return; }
  String command = server.arg("cmd");
  if (command.isEmpty()) { server.send(400, "text/plain", "Empty command"); return; }
  bool sent = bleGatewaySendCommand(command);
  server.send(sent ? 200 : 409, "text/plain", sent ? "Acknowledged" : "Command already in progress or connection failed");
}
void handleRawLog() {
  server.sendHeader("Content-Disposition", "attachment; filename=obd-raw-log.csv");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/csv; charset=utf-8", "");
  server.sendContent("timestamp_ms,sequence,command,response,complete\r\n");
  for (size_t i = 0; i < obdLogCount(); ++i) {
    ObdLogRecord record = obdLogAt(i);
    server.sendContent(String(record.timestampMs) + "," + String(record.sequence) + "," + csvEscape(record.command) + "," + csvEscape(record.response) + "," + (record.complete ? "true" : "false") + "\r\n");
  }
}
}

void webServerBegin() {
  server.on("/", handleRoot);
  server.on("/devices", handleDevices);
  server.on("/terminal", handleTerminal);
  server.on("/diagnostics", handleDiagnostics);
  server.on("/connect", handleConnect);
  server.on("/disconnect", handleDisconnect);
  server.on("/send", handleSend);
  server.on("/raw-log.csv", handleRawLog);
  server.begin();
}

void webServerTick() { server.handleClient(); }
