#include "ble_gateway.h"

#include <NimBLEDevice.h>

#include "app_config.h"
#include "diagnostic_log.h"
#include "obd_response_assembler.h"

namespace {
NimBLEScan *scan = nullptr;
NimBLEClient *client = nullptr;
NimBLERemoteCharacteristic *remoteCharacteristic = nullptr;
String devicesHtml = "<tr><td colspan='4' style='text-align:center;'>Scan sequence pending...</td></tr>";
String devicesJson = "[]";
String targetAddress;
String targetName;
uint8_t targetAddressType = BLE_ADDR_PUBLIC;
unsigned long lastScanMs = 0;
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
  if (bleGatewayIsConnected() || scanning || millis() - lastScanMs < BLE_SCAN_INTERVAL_MS) return;
  lastScanMs = millis();
  scanning = true;
  scan->start(3, scanCompleteCallback, false);
}

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
