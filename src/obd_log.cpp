#include "obd_log.h"

#include "app_config.h"
#include "mutex_guard.h"

namespace {
ObdLogRecord records[OBD_LOG_CAPACITY];
size_t firstRecord = 0;
size_t recordCount = 0;
unsigned long nextSequence = 0;

// Guards the ring buffer's contents and bookkeeping. obdLogAppend() is
// reached from two different tasks depending on the caller:
// obd_response_assembler's obdResponseAssemblerAppend() calls it from
// NimBLE's host-stack task (a real response arrived), while its
// obdResponseAssemblerTimeout() calls it from whichever loop()-task
// module is currently polling (a command timed out). obdLogAt()/
// obdLogCount() are read-side callers from loop()-task code only
// (web_server's /raw-log.csv export, obd_survey's
// lastProbeGotResponse()), but still need the same guard - a read
// racing an in-progress append could otherwise observe firstRecord and
// recordCount mid-update, or a String mid-copy while the other task is
// still writing/reallocating it, which is undefined behavior rather
// than just a stale value. See mutex_guard.h for why this is a real
// mutex rather than a portMUX_TYPE critical section.
//
// Note this does NOT make a whole export loop (obdLogCount() then N
// calls to obdLogAt()) atomic as a unit - each individual call is
// internally consistent, but the ring buffer can still be appended to
// (and old entries evicted) by the other task between those calls,
// which could make handleRawLog()'s CSV skip or repeat a record at the
// boundary during a live poll. That's a pre-existing, currently
// unaddressed limitation, not something this change introduces - fixing
// it would mean adding a single obdLogSnapshot() that copies the whole
// buffer under one lock hold, which is more invasive than this fix
// needs to be unless CSV exports turn out to need exact consistency in
// practice.
SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
}

void obdLogAppend(const String &command, const String &response, bool complete) {
  MutexGuard guard(mutex);
  size_t index = (firstRecord + recordCount) % OBD_LOG_CAPACITY;
  if (recordCount == OBD_LOG_CAPACITY) {
    firstRecord = (firstRecord + 1) % OBD_LOG_CAPACITY;
  } else {
    ++recordCount;
  }
  records[index] = {millis(), nextSequence++, complete, command, response};
}

size_t obdLogCount() {
  MutexGuard guard(mutex);
  return recordCount;
}

ObdLogRecord obdLogAt(size_t index) {
  MutexGuard guard(mutex);
  return records[(firstRecord + index) % OBD_LOG_CAPACITY];
}
