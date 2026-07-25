#include "obd_poller.h"

#include "app_config.h"
#include "ble_gateway.h"
#include "obd_response_assembler.h"

void obdPollerTick() {
  static unsigned long lastPollMs = 0;
  if (obdResponseAssemblerTimedOut(OBD_RESPONSE_TIMEOUT_MS)) obdResponseAssemblerTimeout();
  if (!bleGatewayIsConnected() || obdResponseAssemblerPending()) return;
  if (millis() - lastPollMs < OBD_POLL_INTERVAL_MS) return;
  lastPollMs = millis();
  bleGatewaySendCommand("010C");
}
