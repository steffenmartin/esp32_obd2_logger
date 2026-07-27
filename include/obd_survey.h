#pragma once

// obd_survey.h
//
// Autonomous ECU/PID discovery ("survey mode") for the OBD-II logger.
//
// Purpose
// -------
// Before the logger can be configured to continuously poll a specific set
// of PIDs for a given vehicle, we need to find out two things empirically:
//   1. Which ECUs (diagnostic request headers) actually respond at all.
//   2. Within each responding ECU, which PIDs/local IDs return data.
//
// This module drives that discovery process autonomously: once started, it
// issues a scripted sequence of probe commands (one at a time, waiting for
// each response or timeout before sending the next) and lets the existing
// obd_response_assembler / obd_log pipeline record the raw exchanges. It
// does NOT interpret any response bytes - it only looks at whether a
// response arrived at all, which is enough to know "supported" vs
// "not supported" without violating the project's "raw values stay raw"
// boundary (see ARCHITECTURE.md, "Boundaries to preserve").
//
// Ownership of the command channel
// ---------------------------------
// Only one OBD command can be in flight at a time (enforced by
// obd_response_assembler). obd_mode.h is the traffic cop that decides which
// subsystem (Terminal / Poller / Survey) is currently allowed to send
// commands. obd_survey only ever sends a command when obdModeCurrent() ==
// ObdMode::Survey, and callers (the web server routes) are responsible for
// requesting that mode before starting a survey and releasing it back to
// Idle when the survey stops.
//
// Usage
// -----
//   if (obdModeRequest(ObdMode::Survey)) {
//     obdSurveyStart();
//   }
//   // ... call obdSurveyTick() once per loop() iteration, same as every
//   // other *Tick() function in this project ...
//   obdSurveyStatus();   // for displaying progress in the web UI
//   obdSurveyStop();     // to abort early; also release the mode:
//   obdModeRequest(ObdMode::Idle);

#include <Arduino.h>

// Resets all internal sweep state and begins a new survey run, starting
// from the ECU-discovery phase. Does NOT check or acquire ObdMode::Survey
// itself - the caller must have already done that via obdModeRequest(),
// so that mode acquisition and survey state reset happen atomically from
// the caller's point of view (avoids a window where mode is Survey but
// the survey's internal state hasn't been reset yet, or vice versa).
void obdSurveyStart();

// Halts the survey immediately, leaving whatever ECUs/PIDs were already
// discovered untouched. Does not clear obd_mode - callers should follow
// this with obdModeRequest(ObdMode::Idle) to release the command channel
// for Terminal/Poller use again.
void obdSurveyStop();

// Advance the survey state machine by (at most) one step. Safe to call
// every loop() iteration unconditionally - it's a no-op whenever the
// survey isn't the current owner of the command channel, isn't running,
// or is still waiting on a pending command/timeout.
void obdSurveyTick();

// True from obdSurveyStart() until the survey reaches SurveyPhase::Done
// (or obdSurveyStop() is called). Useful for the web UI to decide whether
// to show "Start Survey" or "Stop Survey".
bool obdSurveyRunning();

// One-line, human-readable snapshot of current progress (e.g. "Mode 21
// sweep: ECU 2/3, local ID 0x4A"), intended for polling from a web page
// while a survey is in progress. Deliberately not machine-structured -
// if the web UI later needs progress as JSON/percentages, add a separate
// accessor rather than parsing this string.
String obdSurveyStatus();