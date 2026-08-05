#include "ui_state.h"

#include "ble_gateway.h"

namespace {
UiState state = UiState::Disconnected;
String deviceAddress;
String deviceName;
String dropReason;

// Remembers whether the currently in-flight Connecting attempt started
// from Dropped (a Retry) rather than Disconnected (a fresh connect).
// uiStateConnectFailed() uses this to decide where to land on failure -
// see that function for why this matters.
bool connectingFromDropped = false;

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

bool uiStateTryStartScan() {
  if (state != UiState::Disconnected) return false;
  transitionTo(UiState::Scanning);
  return true;
}

void uiStateCancelScan() {
  if (state != UiState::Scanning) return;
  transitionTo(UiState::Disconnected);
}

bool uiStateTryStartConnect(const String &address, const String &name) {
  // Disconnected -> Connecting covers both a fresh connect and the
  // reconnect shortcut (design doc S2); Dropped -> Connecting covers
  // Retry; Scanning -> Connecting covers tapping a device row while a
  // scan is still in progress (the state diagram draws this edge
  // explicitly - scanning doesn't need to finish or be cancelled first).
  if (state != UiState::Disconnected && state != UiState::Dropped && state != UiState::Scanning) return false;
  connectingFromDropped = (state == UiState::Dropped);
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
  if (connectingFromDropped) {
    // A Retry failed - go back to Dropped rather than Disconnected, so
    // the device info and Retry/Abort UI survive and another attempt
    // stays possible without navigating away. Before this check
    // existed, every failed Retry fell through to Disconnected, which
    // the dashboard/terminal pages render no banner for at all -
    // Retry/Abort would vanish with no way back except leaving the
    // page entirely.
    dropReason = "reconnect attempt failed";
    transitionTo(UiState::Dropped);
    return;
  }
  // Back to Disconnected rather than Dropped - Dropped is specifically
  // for losing a link that was actually established (design doc S5); a
  // failed first attempt never had one to lose.
  transitionTo(UiState::Disconnected);
}

void uiStateDisconnected() {
  dropReason = "";
  transitionTo(UiState::Disconnected);
}

void uiStateTick() {
  // Scanning ends on its own once the ~3s radio scan completes (see
  // bleGatewayStartScan()) - this is the counterpart to Cancel
  // (uiStateCancelScan()) for that same transition, detected the same
  // way Connected->Dropped is below: polling the ble_gateway accessor
  // each tick rather than ble_gateway calling into UiState directly,
  // keeping ble_gateway itself UI-agnostic.
  if (state == UiState::Scanning) {
    if (!bleGatewayIsScanning()) transitionTo(UiState::Disconnected);
    return;
  }

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
