#include <Arduino.h>
#include <WiFi.h>
#include "secrets.h"

#include "ble_gateway.h"
#include "obd_poller.h"
#include "obd_survey.h"  // add to the existing #include block at the top
#include "ui_state.h"
#include "web_server.h"
#include "wifi_manager.h"

bool webServerStarted = false;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 4000) delay(10);

  wifiManagerBegin(WIFI_SSID, WIFI_PASSWORD);
  bleGatewayBegin();
}

void loop() {
  wifiManagerTick();

  // webServerBegin() needs an assigned IP to be meaningful, so it
  // starts on the first loop() iteration where WiFi is actually
  // Connected rather than unconditionally in setup() - matching the
  // sandbox repo's own pattern (see wifi_manager.h). Everything else
  // (BLE gateway, OBD poller/survey, UiState) does NOT wait on this:
  // none of it depends on WiFi being up, and gating it too would just
  // reintroduce the same "nothing runs until WiFi associates" problem
  // this change is meant to remove, one layer up.
  if (!webServerStarted && wifiManagerState() == WifiState::Connected) {
    webServerBegin();
    webServerStarted = true;
    Serial.print("[WiFi] Web server started. IP: http://");
    Serial.println(WiFi.localIP());
  }

  if (webServerStarted) webServerTick();
  bleGatewayTick();
  obdPollerTick();
  obdSurveyTick();
  uiStateTick();
}
