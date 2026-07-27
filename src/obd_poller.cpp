#include "obd_poller.h"

#include "app_config.h"
#include "ble_gateway.h"
#include "obd_mode.h"
#include "obd_response_assembler.h"

void obdPollerTick() {
  // Only the continuous poller is allowed to send while ObdMode::Poller
  // is active - if Terminal or Survey currently owns the command channel,
  // stay out of the way entirely rather than trying to interleave.
  if (obdModeCurrent() != ObdMode::Poller) return;

  static unsigned long lastPollMs = 0;
  if (obdResponseAssemblerTimedOut(OBD_RESPONSE_TIMEOUT_MS)) obdResponseAssemblerTimeout();
  if (!bleGatewayIsConnected() || obdResponseAssemblerPending()) return;
  if (millis() - lastPollMs < OBD_POLL_INTERVAL_MS) return;
  lastPollMs = millis();
  bleGatewaySendCommand("010C");
}