#pragma once

#include <Arduino.h>

void bleGatewayBegin();
void bleGatewayTick();
// Explicitly starts a single ~3s active scan. Per docs/design/webui-
// state-design.md S1 ("a fresh BLE scan is always user-triggered - no
// auto-scan on boot or page load"), this replaces what bleGatewayTick()
// used to do on its own every BLE_SCAN_INTERVAL_MS. No-op (returns
// immediately, doesn't restart) if a scan is already running or a
// device is already connected - callers that need to know whether a
// request actually took effect should check bleGatewayIsScanning()
// afterward, though in practice UiState's own Disconnected-only guard
// on uiStateTryStartScan() already prevents most redundant calls before
// they'd ever reach here.
void bleGatewayStartScan();
// Stops an in-progress scan early (the Web UI's Cancel action).
// Harmless no-op if no scan is running.
void bleGatewayCancelScan();
bool bleGatewayIsScanning();
void bleGatewaySetTargetAddress(const String &address, uint8_t addressType, const String &name = "");
void bleGatewayUnsetTargetAddress();
String bleGatewayTargetAddress();
String bleGatewayTargetName();
uint8_t bleGatewayTargetAddressType();
String bleGatewayDevicesHtml();
String bleGatewayDevicesJson();
bool bleGatewayEnsureConnected();
bool bleGatewayIsConnected();
bool bleGatewaySendCommand(const String &command);
void bleGatewayDisconnect();
