#include "obd_log.h"

#include "app_config.h"

namespace {
ObdLogRecord records[OBD_LOG_CAPACITY];
size_t firstRecord = 0;
size_t recordCount = 0;
unsigned long nextSequence = 0;
}

void obdLogAppend(const String &command, const String &response, bool complete) {
  size_t index = (firstRecord + recordCount) % OBD_LOG_CAPACITY;
  if (recordCount == OBD_LOG_CAPACITY) {
    firstRecord = (firstRecord + 1) % OBD_LOG_CAPACITY;
  } else {
    ++recordCount;
  }
  records[index] = {millis(), nextSequence++, complete, command, response};
}

size_t obdLogCount() { return recordCount; }

ObdLogRecord obdLogAt(size_t index) {
  return records[(firstRecord + index) % OBD_LOG_CAPACITY];
}
