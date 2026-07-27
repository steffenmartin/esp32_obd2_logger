#pragma once

// obd_mode.h
//
// Coordinates which subsystem currently owns the single in-flight OBD
// command slot (see obd_response_assembler.h - only one command can be
// outstanding at a time).
//
// Before this module existed, mutual exclusion between Terminal and
// Poller was handled by literally commenting obdPollerTick() out of
// main.cpp's loop(). That doesn't scale once a third mode (Survey) needs
// the same exclusive access, so this makes "who's allowed to send right
// now" an explicit, runtime-switchable state instead of a build-time
// choice.
//
// This is a GATE, not a scheduler: it doesn't queue mode-switch requests
// or arbitrate between competing ones. If two callers both try to switch
// modes in the same loop() iteration, whichever calls obdModeRequest()
// first wins for that iteration; there's no fairness or priority logic.
// In practice this hasn't been a problem because mode switches are
// user-initiated (opening the terminal, clicking Start Survey) rather
// than something multiple subsystems attempt concurrently on their own.

enum class ObdMode { Idle, Terminal, Poller, Survey };

// Attempts to switch the active mode. Fails (returns false, mode
// unchanged) if a command is currently in flight - this prevents, e.g.,
// starting a survey mid-way through a Terminal command and leaving that
// command's response orphaned with nothing watching for it. Callers that
// get false back should simply try again on a later tick/request; there's
// no need for exponential backoff or similar given how infrequently mode
// switches happen relative to the tick rate.
bool obdModeRequest(ObdMode mode);

// Current mode. *Tick() functions in obd_poller.cpp / obd_survey.cpp each
// check this before doing anything, so they only act while they're
// actually the acquired mode.
ObdMode obdModeCurrent();