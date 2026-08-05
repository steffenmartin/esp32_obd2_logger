#include "ble_gateway.h"

#include <NimBLEDevice.h>

#include "app_config.h"
#include "diagnostic_log.h"
#include "obd_response_assembler.h"

namespace {
NimBLEScan *scan = nullptr;
NimBLEClient *client = nullptr;
NimBLERemoteCharacteristic *remoteCharacteristic = nullptr;
String devicesHtml = "<tr><td colspan='4' style='text-align:center;'>No scan yet - click Scan to search for nearby BLE devices.</td></tr>";
String devicesJson = "[]";
String targetAddress;
String targetName;
uint8_t targetAddressType = BLE_ADDR_PUBLIC;
bool scanning = false;
bool connected = false;

// NimBLEClientCallbacks::onConnParamsUpdateRequest() defaults to
// silently accepting any post-connection parameter update the
// peripheral proposes - see app_config.h's comment above
// BLE_CONN_SUPERVISION_TIMEOUT for why that's a problem. This override
// only rejects updates that would *loosen* (increase) the supervision
// timeout beyond what we originally requested; anything at or tighter
// than that is still accepted, since there's no reason to fight a
// peripheral proposing something equally fast or faster.
//
// A single static instance, not one per connection - NimBLEClient's
// setClientCallbacks() takes a pointer and doesn't need a fresh object
// per connect attempt, and this class carries no per-connection state
// of its own.
class ClientCallbacks : public NimBLEClientCallbacks {
  bool onConnParamsUpdateRequest(NimBLEClient *, const ble_gap_upd_params *params) override {
    if (params->supervision_timeout > BLE_CONN_SUPERVISION_TIMEOUT) {
      diagnosticLogAppend("[BLE] Rejected peer's connection parameter update (would have loosened supervision timeout to " +
                           String(params->supervision_timeout * 10) + "ms).");
      return false;
    }
    return true;
  }

  // Logs the moment NimBLE's host stack itself notices the link is
  // gone - confirmed via testing to fire promptly on an actual
  // supervision timeout (verified against real hardware power-loss,
  // not just stopping the host-side emulator process, which leaves its
  // ESP32 radio still holding the link up at the controller level with
  // nothing to time out against). Event-driven rather than only ever
  // polling bleGatewayIsConnected() from uiStateTick() - a useful
  // permanent addition to the diagnostic log independent of that.
  //
  // Signature confirmed against this project's actual resolved
  // NimBLE-Arduino version (.pio/libdeps/.../NimBLEClient.h:134) - no
  // reason code on the client-side callback, unlike
  // NimBLEServerCallbacks::onDisconnect()'s two overloads (a different
  // class, don't confuse the two).
  void onDisconnect(NimBLEClient *) override {
    diagnosticLogAppend("[BLE] Link lost (onDisconnect).");
  }
};

ClientCallbacks clientCallbacks;

void notifyCallback(NimBLERemoteCharacteristic *, uint8_t *data, size_t length, bool) {
  String fragment;
  for (size_t i = 0; i < length; ++i) fragment += static_cast<char>(data[i]);
  diagnosticLogAppend("[Dongle] " + fragment);
  obdResponseAssemblerAppend(data, length);
}

void scanCompleteCallback(NimBLEScanResults foundDevices) {
  String html;
  String json = "[";
  if (foundDevices.getCount() == 0) {
    html = "<tr><td colspan='4' style='text-align:center;'>No devices found.</td></tr>";
  }
  for (int i = 0; i < foundDevices.getCount(); ++i) {
    NimBLEAdvertisedDevice device = foundDevices.getDevice(i);
    String name = device.haveName() ? device.getName().c_str() : "Unknown Device";
    String address = device.getAddress().toString().c_str();
    bool isLeLink = name.indexOf("OBDBLE") != -1 || name.indexOf("LELink") != -1 ||
      (device.haveServiceUUID() && device.isAdvertisingService(NimBLEUUID(LELINK_SERVICE_UUID)));
    uint8_t addressType = device.getAddressType();
    
    html += "<tr" + String(isLeLink ? " style='background:#d4edda;font-weight:bold;'" : "") + ">";
    html += "<td>" + name + String(isLeLink ? " (LELink candidate)" : "") + "</td>";
    html += "<td><code>" + address + "</code> <small>(" + String(addressType == BLE_ADDR_PUBLIC ? "public" : "random") + ")</small></td><td>" + String(device.getRSSI()) + " dBm</td>";
    html += isLeLink ? "<td><a href='/terminal?addr=" + address + "&type=" + String(addressType) + "'>Open terminal</a></td></tr>" : "<td>Standard BLE</td></tr>";
    
    json += (i > 0 ? "," : "") + String("{\"name\":\"") + name + "\",\"address\":\"" + address + "\",\"rssi\":" + device.getRSSI() + ",\"isLeLink\":" + (isLeLink ? "true" : "false") + ",\"type\":" + addressType + "}";
  }
  json += "]";
  devicesHtml = html;
  devicesJson = json;
  scan->clearResults();
  scanning = false;
  // Fires here regardless of whether the scan ran its full ~3s or was
  // cut short by bleGatewayCancelScan()'s scan->stop() - see that
  // function's comment for why. If a Cancel log line is immediately
  // followed by this one, that's expected on a version where stop()
  // itself triggers this same callback, not a double-fire bug.
  diagnosticLogAppend("[BLE] Scan complete. " + String(foundDevices.getCount()) + " device(s) found.");
}
}

void bleGatewayBegin() {
  NimBLEDevice::init("");
  scan = NimBLEDevice::getScan();
  scan->setActiveScan(true);
  scan->setInterval(100);
  scan->setWindow(99);
}

void bleGatewayTick() {
  // Used to run a periodic auto-scan here (every BLE_SCAN_INTERVAL_MS).
  // Per docs/design/webui-state-design.md S1 ("a fresh BLE scan is
  // always user-triggered - no auto-scan on boot or page load"),
  // scanning is now explicitly started via bleGatewayStartScan()
  // (called from web_server's /scan route) rather than driven from
  // here. Left as an empty stub rather than removed entirely, matching
  // every other module's *Tick() function being unconditionally called
  // from loop() regardless of whether it currently has anything to do
  // (see obd_poller.cpp/obd_survey.cpp for the same pattern).
}

void bleGatewayStartScan() {
  if (bleGatewayIsConnected() || scanning) return;
  scanning = true;
  diagnosticLogAppend("[BLE] Scan started.");
  scan->start(3, scanCompleteCallback, false);
}

void bleGatewayCancelScan() {
  if (!scanning) return;
  diagnosticLogAppend("[BLE] Scan cancelled.");
  scan->stop();
  // Explicitly cleared here rather than assuming scan->stop() itself
  // triggers scanCompleteCallback - whether it does depends on this
  // project's exact resolved NimBLE-Arduino version, and this project
  // has already been burned twice by assuming NimBLE API behavior
  // instead of checking it (see app_config.h's BLE_CONNECT_TIMEOUT_S
  // and this file's onDisconnect() comments). If stop() does also
  // invoke the callback, this is harmless - it would just set
  // scanning = false again and repopulate the device tables with
  // whatever partial results came in before the cancel.
  scanning = false;
}

bool bleGatewayIsScanning() { return scanning; }

void bleGatewaySetTargetAddress(const String &address, uint8_t addressType, const String &name) {
  targetAddress = address;
  targetName = name;
  targetAddressType = addressType;
}
void bleGatewayUnsetTargetAddress() {
  targetAddress.clear();
  targetName.clear();
  targetAddressType = BLE_ADDR_PUBLIC;
}
String bleGatewayTargetAddress() { return targetAddress; }
String bleGatewayTargetName() { return targetName; }
uint8_t bleGatewayTargetAddressType() { return targetAddressType; }
String bleGatewayDevicesHtml() { return devicesHtml; }
String bleGatewayDevicesJson() { return devicesJson; }
bool bleGatewayIsConnected() { return connected && client != nullptr && client->isConnected(); }

bool bleGatewayEnsureConnected() {
  if (bleGatewayIsConnected()) return true;
  if (targetAddress.isEmpty()) { diagnosticLogAppend("[BLE] No target address selected."); return false; }
  if (scan->isScanning()) scan->stop();
  if (client == nullptr) {
    client = NimBLEDevice::createClient();
    // false = this is a static instance (see clientCallbacks above),
    // not something NimBLE should take ownership of and delete.
    client->setClientCallbacks(&clientCallbacks, false);
    client->setConnectTimeout(BLE_CONNECT_TIMEOUT_S);
  }
  String typeLabel = targetAddressType == BLE_ADDR_PUBLIC ? "PUBLIC" : "RANDOM";
  diagnosticLogAppend("[BLE] Connecting to " + targetAddress + " (" + typeLabel + ")...");
  client->setConnectionParams(BLE_CONN_MIN_INTERVAL, BLE_CONN_MAX_INTERVAL, BLE_CONN_LATENCY, BLE_CONN_SUPERVISION_TIMEOUT);
  if (!client->connect(NimBLEAddress(targetAddress.c_str(), targetAddressType))) {
    diagnosticLogAppend("[BLE] Connection failed (address type: " + typeLabel + ").");
    return false;
  }
  NimBLERemoteService *service = client->getService(NimBLEUUID(LELINK_SERVICE_UUID));
  if (service == nullptr) { diagnosticLogAppend("[BLE] FFE0 service not found."); client->disconnect(); return false; }
  remoteCharacteristic = service->getCharacteristic(NimBLEUUID(LELINK_CHARACTERISTIC_UUID));
  if (remoteCharacteristic == nullptr || !remoteCharacteristic->canNotify() || !remoteCharacteristic->subscribe(true, notifyCallback)) {
    diagnosticLogAppend("[BLE] FFE1 notification setup failed.");
    client->disconnect();
    remoteCharacteristic = nullptr;
    return false;
  }
  connected = true;
  diagnosticLogAppend("[BLE] OBD terminal ready.");
  return true;
}

bool bleGatewaySendCommand(const String &command) {
  if (!bleGatewayEnsureConnected()) return false;
  if (obdResponseAssemblerPending()) {
    diagnosticLogAppend("[User] Warning: Previous command still pending, timing it out.");
    obdResponseAssemblerTimeout();
  }
  if (!obdResponseAssemblerBegin(command)) return false;
  String payload = command + "\r";
  if (!remoteCharacteristic->writeValue(reinterpret_cast<const uint8_t *>(payload.c_str()), payload.length(), true)) {
    obdResponseAssemblerTimeout();
    diagnosticLogAppend("[BLE] Command write failed.");
    return false;
  }
  diagnosticLogAppend("[User] " + command);
  return true;
}

void bleGatewayDisconnect() {
  if (client != nullptr) {
    client->disconnect();
    client = nullptr;
    remoteCharacteristic = nullptr;
    connected = false;
  }
}
