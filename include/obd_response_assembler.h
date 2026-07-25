#pragma once

#include <Arduino.h>

bool obdResponseAssemblerBegin(const String &command);
void obdResponseAssemblerAppend(const uint8_t *data, size_t length);
void obdResponseAssemblerTimeout();
bool obdResponseAssemblerPending();
bool obdResponseAssemblerTimedOut(unsigned long timeoutMs);
