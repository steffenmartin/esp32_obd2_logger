#pragma once

#include <stddef.h>

constexpr unsigned long BLE_SCAN_INTERVAL_MS = 6000;
constexpr unsigned long OBD_POLL_INTERVAL_MS = 1000;
constexpr unsigned long OBD_RESPONSE_TIMEOUT_MS = 3000;
constexpr size_t OBD_LOG_CAPACITY = 32;
constexpr size_t OBD_COMMAND_SIZE = 32;
constexpr size_t OBD_RESPONSE_SIZE = 256;
constexpr size_t DIAGNOSTIC_LOG_SIZE = 4096;

constexpr const char *LELINK_SERVICE_UUID = "0000ffe0-0000-1000-8000-00805f9b34fb";
constexpr const char *LELINK_CHARACTERISTIC_UUID = "0000ffe1-0000-1000-8000-00805f9b34fb";
