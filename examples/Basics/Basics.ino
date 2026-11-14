// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The example of the README: TCC5 interrupts every 1000 us and drives a PWM of
// 50 % on its channel A, pin PC4.

#include <XmegaTimer.h>
#include <stdint.h>

namespace {

uint32_t volatile ticks{0u};

void OnTick() { ticks = ticks + 1u; }

}  // namespace

void setup() {
  // Nothing to do on a refusal here: the period and the duty are in range, and
  // the timer is begun once.
  xmega_timer::Discard(xmega_timer::tcc5.Begin(1000u, OnTick));
  // 50 %, of a duty of 0 to 1023.
  xmega_timer::Discard(
      xmega_timer::tcc5.EnablePwm<xmega_timer::Channel::kA>(512u));
  xmega_timer::Discard(xmega_timer::tcc5.Start());
}

void loop() {}
