#include <Arduino.h>
#include <WiFi.h>
#include "secrets.h"

#include "ble_gateway.h"
#include "obd_poller.h"
#include "obd_survey.h"  // add to the existing #include block at the top
#include "ui_state.h"
#include "web_server.h"

// Preserve the project's existing Wi-Fi configuration. This should eventually
// move to an ignored local configuration file or provisioning flow.
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 4000) delay(10);

  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nServer IP: http://");
  Serial.println(WiFi.localIP());

  bleGatewayBegin();
  webServerBegin();
}

void loop() {
  webServerTick();
  bleGatewayTick();
  obdPollerTick();
  obdSurveyTick();
  uiStateTick();
}
