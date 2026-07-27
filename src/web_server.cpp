#include "web_server.h"
#include <WebServer.h>
#include "ble_gateway.h"
#include "diagnostic_log.h"
#include "obd_log.h"
#include "obd_mode.h"
#include "obd_survey.h"
#include "web_pages.h"

namespace {
WebServer server(80);

String csvEscape(const String &value) {
  String escaped = "\"";
  for (size_t i = 0; i < value.length(); ++i) {
    if (value[i] == '\"') {
      escaped += "\"\"";
    } else {
      escaped += value[i];
    }
  }
  escaped += "\"";
  return escaped;
}

void handleDisconnect() {
  bleGatewayDisconnect();
  bleGatewayUnsetTargetAddress();
  server.send(200, "text/plain", "Disconnected");
}

void handleRoot() { server.send(200, "text/html; charset=utf-8", webPagesDashboard()); }
void handleDevices() { server.send(200, "application/json", bleGatewayDevicesJson()); }
void handleTerminal() { server.send(200, "text/html; charset=utf-8", webPagesTerminal(bleGatewayTargetAddress())); }
void handleDiagnostics() { server.send(200, "text/plain; charset=utf-8", diagnosticLogGet()); }

void handleConnect() {
  if (server.hasArg("addr")) {
    bleGatewaySetTargetAddress(server.arg("addr"), 0);
  }
  bleGatewayEnsureConnected();
  server.send(202, "text/plain", "Connection requested");
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
    String line = String(record.timestampMs) + "," + String(record.sequence) + "," + csvEscape(record.command) + "," + csvEscape(record.response) + "," + (record.complete ? "true" : "false") + "\r\n";
    server.sendContent(line);
  }
}

void handleSurveyStart() {
  bool started = obdModeRequest(ObdMode::Survey);
  if (started) obdSurveyStart();
  server.send(started ? 200 : 409, "text/plain", started ? "Survey started" : "Command in flight, try again");
}

void handleSurveyStop() {
  obdSurveyStop();
  obdModeRequest(ObdMode::Idle);
  server.send(200, "text/plain", "Survey stopped");
}

void handleSurveyStatus() { server.send(200, "text/plain", obdSurveyStatus()); }

void handleServerStatusJson() {
  String json = "{\"freeHeap\":" + String(ESP.getFreeHeap()) + ",\"millis\":" + String(millis()) + "}";
  server.send(200, "application/json", json);
}

void handleApiState() {
  String devicesJson = bleGatewayDevicesJson();
  String json = "{\"connection\":\"" + String(bleGatewayIsConnected() ? "CONNECTED" : "DISCONNECTED") + 
                "\",\"target\":\"" + bleGatewayTargetAddress() + 
                "\",\"devices\":" + (devicesJson.length() > 0 ? devicesJson : "[]") + "}";
  server.send(200, "application/json", json);
}
}

void handleServerStatus() { server.send(200, "text/html; charset=utf-8", webPagesServerStatus()); }

void webServerBegin() {
  server.on("/", handleRoot);
  server.on("/devices", handleDevices);
  server.on("/devices-json", handleDevices);
  server.on("/terminal", handleTerminal);
  server.on("/diagnostics", handleDiagnostics);
  server.on("/connect", handleConnect);
  server.on("/disconnect", handleDisconnect);
  server.on("/send", handleSend);
  server.on("/raw-log.csv", handleRawLog);
  server.on("/survey/start", handleSurveyStart);
  server.on("/survey/stop", handleSurveyStop);
  server.on("/survey/status", handleSurveyStatus);
  server.on("/server/status", handleServerStatus);
  server.on("/server/status-json", handleServerStatusJson);
  server.on("/api/state", handleApiState);
  server.begin();
}

void webServerTick() { server.handleClient(); }
