#pragma once

#include <Arduino.h>

// wifi_manager.h
//
// Non-blocking WiFi connection management, adapted from discovery work in
// the companion esp32_c3_wifi sandbox repo (steffenmartin/esp32_c3_wifi).
// That repo's own findings, carried over here:
//
//   - This board (esp32-c3-devkitm-1) overheats and drops connections at
//     the default WiFi TX power; a reduced power level resolved both -
//     see wifiManagerBegin()'s call to esp_wifi_set_max_tx_power() for
//     the exact value and a units correction (the sandbox repo's own
//     README/comments mislabel this - see that call site).
//   - ESP-IDF's built-in auto-reconnect (WiFi.setAutoReconnect()) fights
//     a custom state machine if left enabled - explicitly disabled here.
//   - An event-driven state machine (WiFi.onEvent()) avoids ever
//     blocking loop() waiting on association, unlike the boot sequence
//     this replaces: main.cpp previously did WiFi.begin() followed by
//     an unconditional `while (WiFi.status() != WL_CONNECTED) delay();`
//     loop, meaning nothing else in this project - BLE included - could
//     run at all until WiFi associated, with no timeout or fallback.
//   - Forcing a reconnect below a weak-signal RSSI threshold catches a
//     degrading connection proactively rather than waiting for a hard
//     drop.
//
// Deliberately NOT carried over from that repo:
//   - Its Chart.js RSSI dashboard and runtime TX-power-tuning HTTP
//     endpoint. That was diagnostic tooling used to reach the findings
//     above, not something this project's Web UI needs permanently -
//     on-device visualization is exactly what ARCHITECTURE.md's
//     "interpretation happens offline" boundary already argues against.
//   - Its Task Watchdog Timer (autonomous reboot on a stalled loop()) -
//     a real behavioral change (a reboot drops any in-flight BLE
//     session and the RAM log ring buffer) intentionally held back as
//     its own deliberate follow-up rather than bundled into this first
//     pass.

enum class WifiState { Disconnected, Connecting, Connected };

// Stores ssid/password and performs one-time setup: WiFi mode, TX
// power, auto-reconnect, and the event handler registration. ssid/
// password are passed in rather than this file including secrets.h
// directly, matching this project's existing convention of only
// main.cpp touching that file.
void wifiManagerBegin(const char *ssid, const char *password);

// Advances the connection state machine by (at most) one thing per
// call - safe to call every loop() iteration unconditionally, same
// convention as this project's other *Tick() functions. Never blocks:
// WiFi.begin() is fire-and-forget here, with the actual outcome
// (success or failure) arriving asynchronously via the WiFi.onEvent()
// callback registered in wifiManagerBegin(), not via a return value or
// a wait inside this function.
void wifiManagerTick();

WifiState wifiManagerState();

// Current RSSI in dBm. Only meaningful while Connected; returns 0
// otherwise - not a realistic real-world reading, so it's
// unambiguously "no current reading" rather than colliding with a
// plausible-but-coincidental signal strength.
int32_t wifiManagerRssi();
