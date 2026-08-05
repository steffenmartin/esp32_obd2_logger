#pragma once

#include <Arduino.h>

// ui_state.h
//
// Web UI state machine described in docs/design/webui-state-design.md
// (S2-3). Single source of truth for what the browser should show:
// connection status, which device is involved, and (for Dropped) why
// the link went down.
//
// Deliberately NOT included yet, per the design doc's own scope notes -
// each is its own follow-up slice:
//   - ConnectedContext registry (S4) - every Connected session currently
//     reports context "terminal", the only context that exists today.
//   - mode (manual/auto) and autologging (S6) - always "manual"; there's
//     no autologging trigger yet to ever produce "auto".
//   - Persisted Configuration (S7).
//   - Reason taxonomy (S5) - Dropped currently records one generic
//     string, not a classified code; real classification needs BLE
//     disconnect-reason plumbing ble_gateway doesn't expose yet.

enum class UiState { Disconnected, Scanning, Connecting, Connected, Dropped };

// Starts a scan (Disconnected -> Scanning only - see uiStateTryStartConnect()
// for why Dropped isn't a valid source here despite being one for a
// connect attempt: Dropped implies a specific known device to retry,
// scanning is specifically for finding a *different* one).
bool uiStateTryStartScan();

// User-initiated Cancel (Scanning -> Disconnected). Distinct from the
// scan naturally finishing on its own, which uiStateTick() detects and
// handles the same way - see that function.
void uiStateCancelScan();

// Mirrors tryStartConnect() from S3: single compare-and-swap-style guard
// so a second /connect call (e.g. a stale second browser tab) is
// rejected rather than racing the first one. Legal from Disconnected
// (fresh connect), Dropped (Retry), or Scanning (tapping a device row
// while a scan is still in progress, per the state diagram's
// Scanning -> Connecting edge).
bool uiStateTryStartConnect(const String &address, const String &name);

// Retry from Dropped (S5's "Dropped card" primary action). Same guard,
// reuses the already-recorded device rather than requiring the caller
// to pass it again.
bool uiStateTryRetry();

// Called by web_server once the outcome of a Connecting attempt is
// known (bleGatewayEnsureConnected()'s return value).
void uiStateConnectSucceeded();
void uiStateConnectFailed();

// Deliberate, user-initiated disconnect (the /disconnect route) - goes
// straight to Disconnected, distinct from an async drop.
void uiStateDisconnected();

// Advances the state machine by (at most) one thing per call: detects an
// async link loss while Connected and moves to Dropped. Safe to call
// every loop() iteration unconditionally, same convention as this
// project's other *Tick() functions.
void uiStateTick();

UiState uiStateCurrent();
String uiStateDeviceAddress();
String uiStateDeviceName();
String uiStateDropReason();

// JSON per docs/design/webui-state-design.md S2 ("/api/status shape").
String uiStateStatusJson();
