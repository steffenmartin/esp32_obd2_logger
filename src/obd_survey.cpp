#include "obd_survey.h"

#include "app_config.h"
#include "ble_gateway.h"
#include "obd_log.h"
#include "obd_mode.h"
#include "obd_response_assembler.h"

namespace {

// ---------------------------------------------------------------------
// Survey phases
// ---------------------------------------------------------------------
// The survey progresses through these phases strictly in order, never
// backward. Each phase has its own position counters below (ecuIndex,
// walkEcuPos/walkPidBase, sweepEcuPos/sweepLocalId) so that resuming a
// phase after a single obdSurveyTick() call just means "look at the
// counters and send the next command for wherever we left off."
//
//   Idle        - no survey has been started yet, or one was stopped.
//   EcuSweep    - probe headers 0x7E0-0x7E7 one at a time to find out
//                 which ECUs are present on the bus at all.
//   Mode01Walk  - for each ECU found above, walk the standard OBD Mode 01
//                 "supported PIDs" bitmask chain (00, 20, 40, ... E0).
//                 Cheap and standards-compliant; doesn't tell us about
//                 manufacturer-specific PIDs like the Sonata's Mode 21
//                 BMS data, which is what the next phase is for.
//   Mode21Sweep - for each ECU found above, brute-force every possible
//                 single-byte "local ID" (0x00-0xFF) under Mode 21, since
//                 there's no standard discovery mechanism for
//                 manufacturer-specific services the way there is for
//                 Mode 01.
//   Done        - all phases finished; obdSurveyTick() becomes a no-op
//                 until obdSurveyStart() is called again.
//
// Deliberately NOT included yet: a Mode 22 (2-byte DID) sweep. At 65,536
// possible values per ECU, that phase needs an append-to-storage sink
// (SD/NDJSON, per ARCHITECTURE.md's Extension Points) rather than the
// RAM-only obd_log ring this module currently writes into. Add it once
// that storage layer exists rather than trying to make it fit in RAM.
enum class SurveyPhase { Idle, EcuSweep, Mode01Walk, Mode21Sweep, Done };

SurveyPhase phase = SurveyPhase::Idle;

// Mirrors "has a survey been started and not yet finished/stopped" -
// kept as a separate flag (rather than just checking phase != Idle/Done)
// so obdSurveyRunning() reads clearly at call sites without needing to
// know about the phase enum's internal ordering.
bool running = false;

// --- "waiting on the dongle" bookkeeping -------------------------------
//
// obdSurveyTick() sends at most one command per call, then must wait for
// that command to complete (or time out) before sending the next. These
// two flags track that wait:
//
//   awaitingResponse - true from the moment we call bleGatewaySendCommand()
//                       until we've processed the corresponding completed
//                       exchange. Cleared at the top of the next
//                       obdSurveyTick() call once obd_response_assembler
//                       confirms the command finished.
//
//   awaitingIsProbe  - distinguishes what kind of command we just sent:
//                       true  = an actual discovery probe (e.g. "3E00",
//                               "0100", "2107") whose outcome should
//                               advance the phase's sweep position.
//                       false = a housekeeping "ATSH<header>" command,
//                               which only exists to point the dongle at
//                               the right ECU and should NOT be treated
//                               as a discovery result once it completes.
//                       See ensureHeader()/sendProbe() below for where
//                       each is set.
bool awaitingResponse = false;
bool awaitingIsProbe = false;

// The header (e.g. 0x7E4) most recently sent via "ATSH<hex>". Used to
// avoid re-sending an identical ATSH before every single probe against
// the same ECU - once set, subsequent probes against that same header
// skip straight to the actual command. Reset to -1 (sentinel for
// "unknown") at the start of every survey run.
int appliedHeader = -1;

// --- EcuSweep phase position --------------------------------------------
// ecuIndex counts 0..7, mapping to headers 0x7E0..0x7E7 (the standard
// ISO 15765-4 physical-addressing range for up to 8 ECUs on one bus).
uint8_t ecuIndex = 0;

// Headers that answered during EcuSweep, in the order they were found.
// Fixed-size array sized to the maximum this phase could ever discover
// (8, since ecuIndex only covers 0x7E0-0x7E7) - no dynamic allocation
// needed.
uint8_t discoveredHeaders[8];
uint8_t discoveredCount = 0;

// --- Mode01Walk phase position ------------------------------------------
// walkEcuPos indexes into discoveredHeaders[]; walkPidBase is the current
// "supported PIDs" bitmask query (0x00, 0x20, 0x40, ... 0xE0 - each Mode
// 01 PID in this specific sequence returns a 4-byte bitmask covering the
// next 32 PIDs, per SAE J1979).
uint8_t walkEcuPos = 0;
uint8_t walkPidBase = 0;

// --- Mode21Sweep phase position -----------------------------------------
// sweepEcuPos indexes into discoveredHeaders[]; sweepLocalId is the
// current Mode 21 local ID being probed (0x00-0xFF). Declared as uint16_t
// rather than uint8_t specifically so the loop-termination check
// (sweepLocalId > 0xFF) can actually be true - an 8-bit counter would
// wrap back to 0 at 0x100 and this phase would never terminate.
uint8_t sweepEcuPos = 0;
uint16_t sweepLocalId = 0;

// Sends "ATSH<hex header>" if - and only if - the dongle isn't already
// pointed at that header (per our appliedHeader cache). Returns true if
// it sent the ATSH command (meaning the caller must stop and wait for it
// to complete before doing anything else this tick), or false if the
// header was already applied and the caller should go ahead and send its
// actual probe command this same tick.
//
// Caveat: this trusts that the ATSH succeeded rather than parsing its
// response for "OK" - it just optimistically updates appliedHeader and
// moves on. If a particular ATSH is ever rejected by the dongle, every
// subsequent probe in that block will silently run against the wrong
// header. The raw log will still show the ATSH exchange verbatim for
// manual review, but nothing here currently detects/retries that failure
// automatically. Worth hardening if this turns out to happen in practice.
bool ensureHeader(uint8_t header) {
  if (appliedHeader == header) return false;
  char buf[8];
  snprintf(buf, sizeof(buf), "ATSH%02X", header);
  bleGatewaySendCommand(String(buf));
  appliedHeader = header;
  awaitingResponse = true;
  awaitingIsProbe = false;  // this is header housekeeping, not a discovery probe
  return true;
}

// Sends an actual discovery probe (as opposed to an ATSH housekeeping
// command) and marks that we're now waiting on a result that SHOULD
// advance the current phase's sweep position once it completes.
void sendProbe(const String &command) {
  bleGatewaySendCommand(command);
  awaitingResponse = true;
  awaitingIsProbe = true;
}

// Looks at the most recently completed raw exchange - written into the
// shared obd_log ring by obd_response_assembler - and decides whether the
// ECU we just probed actually answered.
//
// This is deliberately shallow: it only checks "did we get a non-empty
// response that isn't the literal ELM327 'NO DATA' string", not whether
// the response was a positive vs. negative (0x7F ...) UDS response. That
// distinction matters for the offline analysis this project intentionally
// defers to external tooling (see ARCHITECTURE.md) - here we only need
// "is something out there to investigate further", which both positive
// and explicit-negative responses satisfy (a negative response still
// means the ECU is alive and addressable, just that this particular ID
// isn't supported... though for the purposes of this survey we currently
// treat it the same as "supported" and let the raw log sort out which is
// which offline. If false positives from negative responses turn out to
// be a nuisance, this is the function to make smarter first.)
bool lastProbeGotResponse() {
  if (obdLogCount() == 0) return false;
  String response = obdLogAt(obdLogCount() - 1).response;
  return response.length() > 0 && response.indexOf("NO DATA") == -1;
}

// Called once per tick, right after we've confirmed the previous command
// finished (obd_response_assembler is no longer pending) - but only takes
// action if that completed command was an actual probe (awaitingIsProbe),
// not just an ATSH header-set. This is where each phase's sweep position
// actually advances to the next thing to try.
void handleCompletedStep() {
  if (!awaitingIsProbe) return;  // just an ATSH ack - nothing to advance

  if (phase == SurveyPhase::EcuSweep) {
    if (lastProbeGotResponse() && discoveredCount < 8) {
      discoveredHeaders[discoveredCount++] = 0x7E0 + ecuIndex;
    }
    ++ecuIndex;
  } else if (phase == SurveyPhase::Mode01Walk) {
    walkPidBase += 0x20;
    if (walkPidBase > 0xE0) {
      // Finished this ECU's whole bitmask chain - move to the next ECU,
      // starting its chain back at PID 0x00.
      walkPidBase = 0;
      ++walkEcuPos;
    }
  } else if (phase == SurveyPhase::Mode21Sweep) {
    ++sweepLocalId;
    if (sweepLocalId > 0xFF) {
      // Finished sweeping all 256 local IDs for this ECU - move to the
      // next ECU, starting its sweep back at local ID 0x00.
      sweepLocalId = 0;
      ++sweepEcuPos;
    }
  }
}

// --- Phase step functions ------------------------------------------------
// Each of these is called once per tick while its phase is active. Every
// one follows the same shape:
//   1. Check whether this phase has run out of work; if so, transition to
//      the next phase (resetting that phase's position counters) and
//      return without sending anything this tick.
//   2. Otherwise, make sure the dongle is pointed at the right header via
//      ensureHeader() - if that itself needed to send an ATSH, stop here
//      and let the next tick send the actual probe.
//   3. Send this step's actual probe command.

void stepEcuSweep() {
  if (ecuIndex >= 8) {
    // Exhausted the whole 0x7E0-0x7E7 range. Hand off to Mode01Walk,
    // starting at the first discovered ECU (if any - if none were found,
    // Mode01Walk's own bounds check will immediately fall through to
    // Mode21Sweep, which will do the same and land on Done).
    phase = SurveyPhase::Mode01Walk;
    walkEcuPos = 0;
    walkPidBase = 0;
    return;
  }
  uint8_t header = 0x7E0 + ecuIndex;
  if (ensureHeader(header)) return;  // ATSH sent - wait for it first

  // Tester Present (UDS service 0x3E), deliberately NOT Mode 01 PID 00.
  // A manufacturer-specific ECU that only implements Mode 21 - like the
  // Sonata's BMS at 0x7E4 - won't respond to a Mode 01 query at all,
  // which would wrongly mark it "not present" and exclude it from every
  // later phase. Tester Present is answered by any UDS-capable ECU
  // regardless of which higher-level services it happens to support, so
  // it's a more reliable presence probe for this specific use case.
  sendProbe("3E00");
}

void stepMode01Walk() {
  if (walkEcuPos >= discoveredCount) {
    // Walked every discovered ECU's Mode 01 bitmask chain. Hand off to
    // the (slower) Mode 21 brute-force sweep, starting over at the first
    // discovered ECU.
    phase = SurveyPhase::Mode21Sweep;
    sweepEcuPos = 0;
    sweepLocalId = 0;
    return;
  }
  uint8_t header = discoveredHeaders[walkEcuPos];
  if (ensureHeader(header)) return;
  char buf[8];
  snprintf(buf, sizeof(buf), "01%02X", walkPidBase);
  sendProbe(String(buf));
}

void stepMode21Sweep() {
  if (sweepEcuPos >= discoveredCount) {
    // Swept every local ID for every discovered ECU - the whole survey
    // (as currently scoped, i.e. not counting a future Mode 22 phase) is
    // complete.
    phase = SurveyPhase::Done;
    running = false;
    return;
  }
  uint8_t header = discoveredHeaders[sweepEcuPos];
  if (ensureHeader(header)) return;
  char buf[8];
  snprintf(buf, sizeof(buf), "21%02X", sweepLocalId);
  sendProbe(String(buf));
}

}  // namespace

// ---------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------

void obdSurveyStart() {
  // Reset every phase's position counters back to the start, regardless
  // of which phase a previous run might have ended in - a fresh
  // obdSurveyStart() always means "begin again from ECU discovery",
  // there's currently no support for resuming a partial run.
  ecuIndex = 0;
  discoveredCount = 0;
  walkEcuPos = 0;
  walkPidBase = 0;
  sweepEcuPos = 0;
  sweepLocalId = 0;
  appliedHeader = -1;
  awaitingResponse = false;
  awaitingIsProbe = false;
  phase = SurveyPhase::EcuSweep;
  running = true;
}

void obdSurveyStop() {
  // Leaves discoveredHeaders/discoveredCount and whatever's already in
  // obd_log untouched - stopping early doesn't discard progress, it just
  // stops making more of it. Does NOT release ObdMode::Survey; the
  // caller (a web server route) is expected to do that via
  // obdModeRequest(ObdMode::Idle) immediately after calling this.
  phase = SurveyPhase::Idle;
  running = false;
}

bool obdSurveyRunning() { return running; }

void obdSurveyTick() {
  // Only act if we currently own the command channel. If some other mode
  // (Terminal/Poller) is active, or nobody's requested Survey mode yet,
  // do nothing - this makes obdSurveyTick() safe to call unconditionally
  // from loop() every iteration, same as the project's other *Tick()
  // functions.
  if (obdModeCurrent() != ObdMode::Survey) return;
  if (phase == SurveyPhase::Idle || phase == SurveyPhase::Done) return;

  // Survey probes use a shorter timeout than continuous polling
  // (OBD_SURVEY_RESPONSE_TIMEOUT_MS vs. OBD_RESPONSE_TIMEOUT_MS) because
  // an unsupported ID is expected to be common during a sweep, and we'd
  // rather move on quickly than wait the full 3-second continuous-poll
  // timeout on every single one of them.
  if (obdResponseAssemblerTimedOut(OBD_SURVEY_RESPONSE_TIMEOUT_MS)) {
    obdResponseAssemblerTimeout();
  }
  // Still waiting on a response (and it hasn't timed out per the check
  // above) - nothing to do this tick.
  if (obdResponseAssemblerPending()) return;

  // The previous command (if any) has now either completed or just timed
  // out. If it was an actual probe, let the current phase's step logic
  // record the outcome and advance its position before we send the next
  // command.
  if (awaitingResponse) {
    awaitingResponse = false;
    handleCompletedStep();
  }

  switch (phase) {
    case SurveyPhase::EcuSweep:
      stepEcuSweep();
      break;
    case SurveyPhase::Mode01Walk:
      stepMode01Walk();
      break;
    case SurveyPhase::Mode21Sweep:
      stepMode21Sweep();
      break;
    default:
      break;  // Idle/Done - nothing to do (guarded above, but exhaustive)
  }
}

String obdSurveyStatus() {
  // Human-readable only, by design - see the header comment on this
  // function in obd_survey.h for why this isn't structured/JSON.
  switch (phase) {
    case SurveyPhase::Idle:
      return "Idle";
    case SurveyPhase::EcuSweep:
      return "ECU sweep: header 0x" + String(0x7E0 + ecuIndex, HEX) +
             " (" + String(ecuIndex) + "/8)";
    case SurveyPhase::Mode01Walk:
      return "Mode 01 walk: ECU " + String(walkEcuPos + 1) + "/" +
             String(discoveredCount) + ", PID block 0x" +
             String(walkPidBase, HEX);
    case SurveyPhase::Mode21Sweep:
      return "Mode 21 sweep: ECU " + String(sweepEcuPos + 1) + "/" +
             String(discoveredCount) + ", local ID 0x" +
             String(sweepLocalId, HEX);
    case SurveyPhase::Done:
      return "Done - " + String(discoveredCount) + " ECU(s) found";
  }
  return "";  // unreachable - keeps -Wreturn-type happy
}