#pragma once

#include <Arduino.h>

void bleGatewayBegin();
void bleGatewayTick();
void bleGatewaySetTargetAddress(const String &address);
String bleGatewayTargetAddress();
String bleGatewayDevicesHtml();
bool bleGatewayEnsureConnected();
bool bleGatewayIsConnected();
bool bleGatewaySendCommand(const String &command);
