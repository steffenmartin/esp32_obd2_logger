#pragma once

#include <Arduino.h>

void bleGatewayBegin();
void bleGatewayTick();
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
