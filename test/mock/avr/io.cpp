// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

#include "avr/io.h"

// NOLINTBEGIN(readability-identifier-naming,cppcoreguidelines-avoid-non-const-global-variables)
TC4_t TCC4{};
TC5_t TCC5{};
PORT_t PORTC{};
PMIC_t PMIC{};
// NOLINTEND(readability-identifier-naming,cppcoreguidelines-avoid-non-const-global-variables)

MockCommandRegister& MockCommandRegister::operator=(uint8_t value) {
  uint8_t const command{static_cast<uint8_t>(value & TC4_CMD_gm)};
  if (command != TC45_CMD_NONE_gc) {
    commands_.push_back(command);
  }
  if (command == TC45_CMD_RESTART_gc || command == TC45_CMD_RESET_gc) {
    counting_down_ = false;
  }
  if ((value & TC4_DIR_bm) != 0u) {
    counting_down_ = true;
  }
  return *this;
}

MockCommandRegister::operator uint8_t() const {
  return counting_down_ ? TC4_DIR_bm : uint8_t{0u};
}

void MockCommandRegister::Clear() {
  counting_down_ = false;
  commands_.clear();
}

namespace {

void ResetTimer(MockTimer& timer) {
  timer.CTRLA = 0u;
  timer.CTRLB = 0u;
  timer.CTRLC = 0u;
  timer.CTRLD = 0u;
  timer.CTRLE = 0u;
  timer.CTRLF = 0u;
  timer.INTCTRLA = 0u;
  timer.INTCTRLB = 0u;
  timer.CTRLGCLR = 0u;
  timer.CTRLGSET.Clear();
  timer.CTRLHCLR = 0u;
  timer.CTRLHSET = 0u;
  timer.INTFLAGS = 0u;
  timer.CNT = 0u;
  timer.PER = 0xFFFFu;
  timer.CCA = 0u;
  timer.CCB = 0u;
  timer.CCC = 0u;
  timer.CCD = 0u;
  timer.PERBUF = 0xFFFFu;
  timer.CCABUF = 0u;
  timer.CCBBUF = 0u;
  timer.CCCBUF = 0u;
  timer.CCDBUF = 0u;
}

}  // namespace

void MockReset() {
  ResetTimer(TCC4);
  ResetTimer(TCC5);
  PORTC.DIRSET = 0u;
  PMIC.CTRL = 0u;
}
