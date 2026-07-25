#include "diagnostic_log.h"

#include "app_config.h"

namespace {
String logBuffer = "Logger initialized.\n";
}

void diagnosticLogAppend(const String &line) {
  logBuffer += line;
  if (!line.endsWith("\n")) logBuffer += "\n";
  if (logBuffer.length() > DIAGNOSTIC_LOG_SIZE) {
    logBuffer.remove(0, logBuffer.length() - DIAGNOSTIC_LOG_SIZE);
  }
}

String diagnosticLogGet() { return logBuffer; }
