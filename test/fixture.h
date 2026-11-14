// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later
//
// What every test of a timer starts from: a process of its own, as each timer
// is a global object that Begin() takes once, the registers of the mock as
// after a reset, and two callbacks that count their calls.

#ifndef XMEGATIMER_TEST_FIXTURE_H_
#define XMEGATIMER_TEST_FIXTURE_H_

#include <avr/io.h>
#include <gtest/gtest.h>
#include <stdint.h>

#include "xmega_timer/Bit.h"

namespace xmega_timer {

using Bits = Bit<uint8_t>;

// What the callbacks counted, which a test reads; set back by the fixture.
inline int ticks{0};
inline int other_ticks{0};
// How many tests this process has begun: one, or the timers are not new.
inline int tests_begun{0};

inline void OnTick() { ++ticks; }
inline void OnOtherTick() { ++other_ticks; }

class TimerFixture : public ::testing::Test {
 protected:
  void SetUp() override {
    ++tests_begun;
    ASSERT_EQ(tests_begun, 1)
        << "A timer is a global object that Begin() takes once: run each test "
           "in a process of its own, as ctest does.";
    MockReset();
    ticks = 0;
    other_ticks = 0;
  }
};

// The prescaler a timer counts at, from CTRLA.
inline uint8_t ClockSelect(MockTimer const& timer) {
  return Bits::ReadBitGroup(timer.CTRLA, TC4_CLKSEL_gm);
}

}  // namespace xmega_timer

#endif  // XMEGATIMER_TEST_FIXTURE_H_
