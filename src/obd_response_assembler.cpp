#include "obd_response_assembler.h"

#include "app_config.h"
#include "obd_log.h"

namespace {
String pendingCommand;
String pendingResponse;
bool pending = false;
unsigned long pendingSinceMs = 0;
}

bool obdResponseAssemblerBegin(const String &command) {
  if (pending) return false;
  pendingCommand = command;
  pendingResponse = "";
  pending = true;
  pendingSinceMs = millis();
  return true;
}

void obdResponseAssemblerAppend(const uint8_t *data, size_t length) {
  if (!pending) return;
  for (size_t i = 0; i < length && pendingResponse.length() < OBD_RESPONSE_SIZE - 1; ++i) {
    pendingResponse += static_cast<char>(data[i]);
  }
  if (memchr(data, '>', length) != nullptr || pendingResponse.indexOf("ERROR") != -1) {
    obdLogAppend(pendingCommand, pendingResponse, true);
    pending = false;
  }
}

void obdResponseAssemblerTimeout() {
  if (!pending) return;
  obdLogAppend(pendingCommand, pendingResponse, false);
  pending = false;
}

bool obdResponseAssemblerPending() { return pending; }

bool obdResponseAssemblerTimedOut(unsigned long timeoutMs) {
  return pending && millis() - pendingSinceMs >= timeoutMs;
}
