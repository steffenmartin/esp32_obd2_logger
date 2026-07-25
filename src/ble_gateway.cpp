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
String targetAddress;
unsigned long lastScanMs = 0;
bool scanning = false;
bool connected = false;

void notifyCallback(NimBLERemoteCharacteristic *, uint8_t *data, size_t length, bool) {
  String fragment;
  for (size_t i = 0; i < length; ++i) fragment += static_cast<char>(data[i]);
  diagnosticLogAppend("[Dongle] " + fragment);
  obdResponseAssemblerAppend(data, length);
}

void scanCompleteCallback(NimBLEScanResults foundDevices) {
  String html;
  if (foundDevices.getCount() == 0) {
    html = "<tr><td colspan='4' style='text-align:center;'>No devices found.</td></tr>";
  }
  for (int i = 0; i < foundDevices.getCount(); ++i) {
    NimBLEAdvertisedDevice device = foundDevices.getDevice(i);
    String name = device.haveName() ? device.getName().c_str() : "Unknown Device";
    String address = device.getAddress().toString().c_str();
    bool isLeLink = name.indexOf("OBDBLE") != -1 || name.indexOf("LELink") != -1 ||
      (device.haveServiceUUID() && device.isAdvertisingService(NimBLEUUID(LELINK_SERVICE_UUID)));
    html += "<tr" + String(isLeLink ? " style='background:#d4edda;font-weight:bold;'" : "") + ">";
    html += "<td>" + name + String(isLeLink ? " (LELink candidate)" : "") + "</td>";
    html += "<td><code>" + address + "</code></td><td>" + String(device.getRSSI()) + " dBm</td>";
    html += isLeLink ? "<td><a href='/terminal?addr=" + address + "'>Open terminal</a></td></tr>" : "<td>Standard BLE</td></tr>";
  }
  devicesHtml = html;
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

void bleGatewaySetTargetAddress(const String &address) { targetAddress = address; }
String bleGatewayTargetAddress() { return targetAddress; }
String bleGatewayDevicesHtml() { return devicesHtml; }
bool bleGatewayIsConnected() { return connected && client != nullptr && client->isConnected(); }

bool bleGatewayEnsureConnected() {
  if (bleGatewayIsConnected()) return true;
  if (targetAddress.isEmpty()) { diagnosticLogAppend("[BLE] No target address selected."); return false; }
  if (scan->isScanning()) scan->stop();
  if (client == nullptr) client = NimBLEDevice::createClient();
  diagnosticLogAppend("[BLE] Connecting to " + targetAddress + "...");
  client->setConnectionParams(24, 40, 0, 50); if (!client->connect(NimBLEAddress(targetAddress.c_str()), BLE_ADDR_PUBLIC) && !client->connect(NimBLEAddress(targetAddress.c_str()), BLE_ADDR_RANDOM)) { diagnosticLogAppend("[BLE] Connection failed."); return false; }
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
  if (obdResponseAssemblerPending()) return false;
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
