#pragma once

#include <stddef.h>
#include <stdint.h>

constexpr unsigned long OBD_POLL_INTERVAL_MS = 1000;
constexpr unsigned long OBD_RESPONSE_TIMEOUT_MS = 3000;

// Bumped up from the original 32. Continuous polling only ever needs a
// handful of recent exchanges live in RAM at once, but a single survey
// pass generates far more records - see obd_survey.cpp's phase comments
// for the full breakdown. 400 is a rough starting point sized for a
// modest number of discovered ECUs; it has NOT been measured against
// real free-heap numbers on hardware yet. Before relying on this for a
// real survey run, watch ESP.getFreeHeap() before/after a full pass and
// adjust up or down accordingly. This will NOT be enough for a future
// Mode 22 sweep (up to 65,536 records/ECU) - that phase needs to write
// to SD/flash as it goes rather than buffering in this RAM ring at all.
constexpr size_t OBD_LOG_CAPACITY = 400;

constexpr size_t OBD_COMMAND_SIZE = 32;
constexpr size_t OBD_RESPONSE_SIZE = 256;
constexpr size_t DIAGNOSTIC_LOG_SIZE = 4096;

constexpr const char *LELINK_SERVICE_UUID = "0000ffe0-0000-1000-8000-00805f9b34fb";
constexpr const char *LELINK_CHARACTERISTIC_UUID = "0000ffe1-0000-1000-8000-00805f9b34fb";

// BLE connection parameters requested when connecting to the dongle -
// see ble_gateway.cpp's bleGatewayEnsureConnected(). Units per the
// Bluetooth spec / NimBLE-Arduino's API: interval fields are in 1.25ms
// units, the timeout field is in 10ms units.
constexpr uint16_t BLE_CONN_MIN_INTERVAL = 24;          // 24 * 1.25ms = 30ms
constexpr uint16_t BLE_CONN_MAX_INTERVAL = 40;          // 40 * 1.25ms = 50ms
constexpr uint16_t BLE_CONN_LATENCY = 0;
constexpr uint16_t BLE_CONN_SUPERVISION_TIMEOUT = 50;   // 50 * 10ms = 500ms

// The parameters above are only a *request* sent during the initial
// connect - nothing stops the peripheral from subsequently issuing its
// own Connection Parameter Update Request once connected, and
// NimBLEClientCallbacks::onConnParamsUpdateRequest() defaults to
// silently ACCEPTING any such request. Without an explicit override
// (see ble_gateway.cpp's ClientCallbacks), a peripheral proposing its
// own - likely much longer - preferred supervision timeout would
// quietly replace the tight one requested above, defeating it
// entirely. This was the confirmed root cause of drop detection taking
// far longer than BLE_CONN_SUPERVISION_TIMEOUT alone would suggest.

// How long (seconds) a single BLE connect attempt (NimBLEClient::
// connect()) is allowed to run before giving up. NimBLE-Arduino's own
// default is 30 seconds - fine when the target device is usually
// there, but a failed attempt (e.g. hitting Retry while the dongle is
// still powered off) blocks the *entire* single-threaded WebServer for
// that whole duration, since server.handleClient() is what's actually
// stuck waiting on it. This project's cooperative-superloop
// architecture (see ARCHITECTURE.md) deliberately doesn't want an
// async/task-based restructure here, so shortening the blocking window
// is the right-sized fix rather than eliminating the block outright -
// it does NOT make /connect non-blocking, it just makes a failed
// attempt fail faster. 5s is a starting point, not empirically tuned:
// long enough that a dongle taking a moment to start advertising after
// power-on still connects normally, short enough that a genuinely-
// absent device fails fast enough for a usable Retry loop from the Web
// UI.
//
// Type/units confirmed against this project's actual resolved
// NimBLE-Arduino version - see the @param doc-comment on
// NimBLEClient::setConnectTimeout() in
// .pio/libdeps/esp32-c3-devkitm-1/NimBLE-Arduino/src/NimBLEClient.cpp:
// seconds, not milliseconds, and uint8_t, not uint32_t (max 255s,
// nowhere near a real constraint here).
constexpr uint8_t BLE_CONNECT_TIMEOUT_S = 5;

// Response timeout used specifically by obd_survey.cpp, distinct from
// OBD_RESPONSE_TIMEOUT_MS (used by the continuous poller and terminal).
// A survey expects to hit a lot of unsupported IDs, each of which should
// fail fast rather than eat the full 3-second continuous-poll timeout -
// at 256 IDs per ECU for the Mode 21 sweep alone, the difference between
// this and OBD_RESPONSE_TIMEOUT_MS is the difference between a survey
// taking ~2 minutes vs. ~13 minutes per ECU in the worst case (all IDs
// unsupported and silently timing out rather than returning fast
// "NO DATA").
constexpr unsigned long OBD_SURVEY_RESPONSE_TIMEOUT_MS = 500;