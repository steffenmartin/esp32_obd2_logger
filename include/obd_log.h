#pragma once

#include <Arduino.h>

struct ObdLogRecord {
  unsigned long timestampMs;
  unsigned long sequence;
  bool complete;
  String command;
  String response;
};

void obdLogAppend(const String &command, const String &response, bool complete);
size_t obdLogCount();
ObdLogRecord obdLogAt(size_t index);
