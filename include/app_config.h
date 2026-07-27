#pragma once

#include <stddef.h>

constexpr unsigned long BLE_SCAN_INTERVAL_MS = 6000;
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