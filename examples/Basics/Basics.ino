// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The example of the README: TCC5 interrupts every 1000 us and drives a PWM of 50 % on its
// channel A, pin PC4.

#include <XmegaTimer.h>

namespace {

volatile uint32_t ticks = 0u;

void OnTick() { ticks = ticks + 1u; }

}  // namespace

void setup() {
  Timer<TimerC5>& timer = Timer<TimerC5>::getInstance();
  timer.init(1000u, OnTick);
  timer.enablePwm<Timer<TimerC5>::TIMER_CHANNEL_A>(512u);  // 50 %, duty 0 .. 1023
  timer.start();
}

void loop() {}
