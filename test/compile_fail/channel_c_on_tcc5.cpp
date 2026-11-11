// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later
//
// TCC5 has the channels A and B only: a PWM on channel C does not compile.
// Expected: this timer has no such compare channel

// The clock a board's build defines, which this call of the compiler does not.
#define F_CPU 32000000uL

#include "Timer.h"

StdReturnType EnablePwmOnChannelC() {
  return Timer<TimerC5>::getInstance()
      .enablePwm<Timer<TimerC5>::TIMER_CHANNEL_C>(512u);
}
