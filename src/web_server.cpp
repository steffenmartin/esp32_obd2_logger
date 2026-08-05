#include "web_server.h"

#include <WebServer.h>

#include "ble_gateway.h"
#include "diagnostic_log.h"
#include "obd_log.h"
#include "obd_mode.h"
#include "obd_survey.h"
#include "ui_state.h"
#include "web_pages.h"

namespace {
WebServer server(80);

String csvEscape(const String &value) {
  String escaped = "\"";
  for (size_t i = 0; i < value.length(); ++i) escaped += value[i] == '\"' ? "\"\"" : String(value[i]);
  return escaped + "\"";
}

// Shared by handleConnect and handleTerminal's addr-initiated connect.
// Applies the UiState guard (design doc S3) before touching the BLE
// stack at all, then reconciles the outcome back into UiState. If the
// guard rejects the attempt - some other connect is already in flight,
// or we're in a state where connecting isn't legal - this doesn't touch
// bleGateway at all; the caller falls back to whatever "not connected"
// behavior it already had (redirect to /devices, 409, etc).
bool guardedConnect() {
  if (bleGatewayIsConnected()) return true;  // idempotent - e.g. terminal page reload
  if (!uiStateTryStartConnect(bleGatewayTargetAddress(), bleGatewayTargetName())) return false;
  bool connected = bleGatewayEnsureConnected();
  if (connected) uiStateConnectSucceeded(); else uiStateConnectFailed();
  return connected;
}

void handleDisconnect() {
  bleGatewayDisconnect();
  bleGatewayUnsetTargetAddress();
  uiStateDisconnected();
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
    server.send(200, "application/json", bleGatewayDevicesJson());
  }
}
void handleTerminal() {
  if (server.hasArg("addr")) {
    uint8_t addressType = server.hasArg("type") ? static_cast<uint8_t>(server.arg("type").toInt()) : 0; // 0 = BLE_ADDR_PUBLIC
    bleGatewaySetTargetAddress(server.arg("addr"), addressType, server.arg("name"));
    guardedConnect();  // Attempt to connect upon navigating to terminal with addr
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
  bool connected = guardedConnect();
  server.send(connected ? 200 : 409, "text/plain", connected ? "Connected" : "Connection failed or already in progress");
}

// Starts a user-triggered scan (design doc S1). 409 if the guard
// rejects it - already scanning, already connected/connecting, etc -
// same "try again in a moment" convention as handleSurveyStart().
void handleScanStart() {
  bool started = uiStateTryStartScan();
  if (started) bleGatewayStartScan();
  server.send(started ? 200 : 409, "text/plain", started ? "Scan started" : "Cannot start scan in current state");
}

// Cancels an in-progress scan early. Always 200 - cancelling something
// that isn't running is treated as already-achieved, not an error, same
// as handleSurveyStop() and handleDisconnect() below.
void handleScanCancel() {
  bleGatewayCancelScan();
  uiStateCancelScan();
  server.send(200, "text/plain", "Scan cancelled");
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

// Attempts to acquire the command channel for Survey mode and, if
// successful, starts a fresh survey run. Returns 409 (Conflict) rather
// than starting anything if a command from another mode is still in
// flight - the browser-side UI should treat this as "try again in a
// moment" rather than a hard failure.
void handleSurveyStart() {
  bool started = obdModeRequest(ObdMode::Survey);
  if (started) obdSurveyStart();
  server.send(started ? 200 : 409, "text/plain",
              started ? "Survey started" : "Command in flight, try again");
}

// Stops the survey and immediately releases the command channel back to
// Idle, so Terminal/Poller can be used again right away without the user
// needing a separate "release mode" step.
void handleSurveyStop() {
  obdSurveyStop();
  obdModeRequest(ObdMode::Idle);
  server.send(200, "text/plain", "Survey stopped");
}

// Plain-text progress snapshot, intended to be polled periodically (e.g.
// every second or two) by a web page while a survey is running.
void handleSurveyStatus() {
  server.send(200, "text/plain", obdSurveyStatus());
}

void handleApiStatus() { server.send(200, "application/json", uiStateStatusJson()); }

void handleServerStatusJson() {
  String json = "{";
  json += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
  json += "\"millis\":" + String(millis());
  json += "}";
  server.send(200, "application/json", json);
}
}

void handleServerStatus() {
  server.send(200, "text/html; charset=utf-8", webPagesServerStatus());
}

void webServerBegin() {
  server.on("/", handleRoot);
  server.on("/devices", handleDevices);
  server.on("/devices-json", handleDevices);
  server.on("/terminal", handleTerminal);
  server.on("/diagnostics", handleDiagnostics);
  server.on("/connect", handleConnect);
  server.on("/disconnect", handleDisconnect);
  server.on("/scan", handleScanStart);
  server.on("/scan/cancel", handleScanCancel);
  server.on("/send", handleSend);
  server.on("/raw-log.csv", handleRawLog);
  server.on("/survey/start", handleSurveyStart);
  server.on("/survey/stop", handleSurveyStop);
  server.on("/survey/status", handleSurveyStatus);
  server.on("/server/status", handleServerStatus);
  server.on("/server/status-json", handleServerStatusJson);
  server.on("/api/status", handleApiStatus);
  server.begin();
}

void webServerTick() { server.handleClient(); }
