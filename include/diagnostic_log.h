#pragma once

#include <Arduino.h>

void diagnosticLogAppend(const String &line);
String diagnosticLogGet();
