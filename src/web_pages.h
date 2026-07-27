#ifndef WEB_PAGES_H
#define WEB_PAGES_H
#include <Arduino.h>

String webPagesBase(const String &title, const String &content);
String webPagesDashboard();
String webPagesTerminal(const String &address);
String webPagesServerStatus();

#endif
