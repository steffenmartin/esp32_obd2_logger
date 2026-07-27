#include "obd_mode.h"

#include "obd_response_assembler.h"

namespace {
// Starts at Idle: nothing is allowed to send commands until something
// (a web route, currently) explicitly requests a mode.
ObdMode currentMode = ObdMode::Idle;
}

bool obdModeRequest(ObdMode mode) {
  // Refuse to switch out from under an in-flight command - if we allowed
  // this, the command's eventual response (or timeout) would arrive after
  // the mode has already changed, and whichever *Tick() owns the new mode
  // would misinterpret it as its own result.
  if (obdResponseAssemblerPending()) return false;
  currentMode = mode;
  return true;
}

ObdMode obdModeCurrent() { return currentMode; }