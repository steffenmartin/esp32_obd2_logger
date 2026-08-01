#include "ui_state.h"

#include "ble_gateway.h"

namespace {
UiState state = UiState::Disconnected;
String deviceAddress;
String deviceName;
String dropReason;

const char *stateName(UiState s) {
  switch (s) {
    case UiState::Disconnected: return "disconnected";
    case UiState::Scanning: return "scanning";
    case UiState::Connecting: return "connecting";
    case UiState::Connected: return "connected";
    case UiState::Dropped: return "dropped";
  }
  return "disconnected";  // unreachable - keeps -Wreturn-type happy
}

// Single choke point for every state change. Not just a convenience -
// this guarantees the UART trace can never drift out of sync with the
// real state, the way it would if each call site printed its own
// message next to a separate assignment.
void transitionTo(UiState newState) {
  Serial.printf("[UiState] %s -> %s\n", stateName(state), stateName(newState));
  state = newState;
}

String jsonEscape(const String &value) {
  String escaped;
  for (size_t i = 0; i < value.length(); ++i) {
    char c = value[i];
    if (c == '"' || c == '\\') escaped += '\\';
    escaped += c;
  }
  return escaped;
}
}  // namespace

bool uiStateTryStartConnect(const String &address, const String &name) {
  // Disconnected -> Connecting covers both a fresh connect and the
  // reconnect shortcut (design doc S2); Dropped -> Connecting covers
  // Retry. Scanning is deliberately excluded even though the state
  // diagram draws Scanning -> Connecting as a source edge - that edge
  // means "tapping a device row while a real Scanning state is active,"
  // which doesn't exist yet (see the header comment). Nothing currently
  // drives state into Scanning, so this omission is inert for now rather
  // than a bug waiting to happen.
  if (state != UiState::Disconnected && state != UiState::Dropped) return false;
  deviceAddress = address;
  deviceName = name;
  dropReason = "";
  Serial.printf("[UiState] connect requested: %s (%s)\n", name.c_str(), address.c_str());
  transitionTo(UiState::Connecting);
  return true;
}

bool uiStateTryRetry() {
  if (state != UiState::Dropped) return false;
  return uiStateTryStartConnect(deviceAddress, deviceName);
}

void uiStateConnectSucceeded() {
  if (state != UiState::Connecting) return;
  transitionTo(UiState::Connected);
}

void uiStateConnectFailed() {
  if (state != UiState::Connecting) return;
  // Back to Disconnected rather than Dropped - Dropped is specifically
  // for losing a link that was actually established (design doc S5); a
  // failed attempt never had one to lose.
  transitionTo(UiState::Disconnected);
}

void uiStateDisconnected() {
  dropReason = "";
  transitionTo(UiState::Disconnected);
}

void uiStateTick() {
  // Only Connected can silently become un-connected out from under us -
  // Connecting's outcome is always reported explicitly via
  // uiStateConnectSucceeded()/Failed(), so it doesn't need polling here.
  if (state != UiState::Connected) return;
  if (bleGatewayIsConnected()) return;
  // Real reason classification (design doc S5's taxonomy) needs a BLE
  // disconnect-reason callback ble_gateway doesn't expose yet - this is
  // the single generic placeholder that unblocks Dropped existing at
  // all, not a stand-in for that follow-up.
  dropReason = "connection lost";
  Serial.println("[UiState] drop reason: connection lost");
  transitionTo(UiState::Dropped);
}

UiState uiStateCurrent() { return state; }
String uiStateDeviceAddress() { return deviceAddress; }
String uiStateDeviceName() { return deviceName; }
String uiStateDropReason() { return dropReason; }

String uiStateStatusJson() {
  String json = "{";
  json += "\"state\":\"" + String(stateName(state)) + "\",";
  json += "\"mode\":\"manual\",";  // no autologging trigger yet - S6 follow-up
  json += "\"context\":" + String(state == UiState::Connected ? "\"terminal\"" : "null") + ",";
  if (deviceAddress.isEmpty()) {
    json += "\"device\":null,";
  } else {
    json += "\"device\":{\"addr\":\"" + jsonEscape(deviceAddress) + "\",\"name\":\"" + jsonEscape(deviceName) + "\"},";
  }
  json += "\"reason\":" + String(dropReason.isEmpty() ? "null" : "\"" + jsonEscape(dropReason) + "\"");
  json += "}";
  return json;
}
