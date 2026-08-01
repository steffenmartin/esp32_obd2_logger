#include "obd_response_assembler.h"

#include "app_config.h"
#include "mutex_guard.h"
#include "obd_log.h"

namespace {
String pendingCommand;
String pendingResponse;
bool pending = false;
unsigned long pendingSinceMs = 0;

// Guards all four fields above. obdResponseAssemblerAppend() is called
// from notifyCallback() in ble_gateway.cpp, which runs on NimBLE's own
// host-stack task - concurrently with every other function below, which
// are all called from loop()-task code (obd_poller, obd_survey,
// ble_gateway's bleGatewaySendCommand(), web_server's /send route).
// Before this, pending/pendingCommand/pendingResponse/pendingSinceMs
// were read and written from both tasks with no synchronization at all;
// see mutex_guard.h for why this is a real mutex rather than a
// portMUX_TYPE critical section.
//
// Safe to create as a global initializer here: on ESP32 Arduino, the
// FreeRTOS scheduler is already running by the time C++ global
// constructors execute (ESP-IDF's startup calls __libc_init_array(),
// which runs them, from inside a task after vTaskStartScheduler() - not
// before it), so xSemaphoreCreateMutex() always has a live scheduler to
// allocate against.
SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
}

bool obdResponseAssemblerBegin(const String &command) {
  MutexGuard guard(mutex);
  if (pending) return false;
  pendingCommand = command;
  pendingResponse = "";
  pending = true;
  pendingSinceMs = millis();
  return true;
}

void obdResponseAssemblerAppend(const uint8_t *data, size_t length) {
  MutexGuard guard(mutex);
  if (!pending) return;
  for (size_t i = 0; i < length && pendingResponse.length() < OBD_RESPONSE_SIZE - 1; ++i) {
    pendingResponse += static_cast<char>(data[i]);
  }
  if (memchr(data, '>', length) != nullptr || pendingResponse.indexOf("ERROR") != -1) {
    // obdLogAppend() takes obd_log's own separate mutex while this
    // function still holds ours - always in this order (assembler then
    // log, never the reverse anywhere in the codebase), so this can't
    // deadlock against obd_log's lock.
    obdLogAppend(pendingCommand, pendingResponse, true);
    pending = false;
  }
}

void obdResponseAssemblerTimeout() {
  MutexGuard guard(mutex);
  if (!pending) return;
  obdLogAppend(pendingCommand, pendingResponse, false);
  pending = false;
}

bool obdResponseAssemblerPending() {
  MutexGuard guard(mutex);
  return pending;
}

bool obdResponseAssemblerTimedOut(unsigned long timeoutMs) {
  MutexGuard guard(mutex);
  return pending && millis() - pendingSinceMs >= timeoutMs;
}
