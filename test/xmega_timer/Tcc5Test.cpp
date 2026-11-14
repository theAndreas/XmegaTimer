// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later
//
// What is TCC5's own: its channels on PC4 and PC5, and its interrupt vector.

#include "xmega_timer/Tcc5.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <gtest/gtest.h>

#include "test/fixture.h"
#include "test/printers.h"  // IWYU pragma: keep
#include "xmega_timer/Error.h"
#include "xmega_timer/Timer.h"

namespace xmega_timer {
namespace {

using Tcc5Test = TimerFixture;

TEST_F(Tcc5Test, EnablePwmOnChannelADrivesPc4) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);

  EXPECT_EQ(tcc5.EnablePwm<Channel::kA>(512u), Error::kOk);

  EXPECT_EQ(PORTC.DIRSET, PIN4_bm);
  EXPECT_EQ(Bits::ReadBitGroup(TCC5.CTRLE, TC5_CCAMODE_gm),
            TC45_CCAMODE_COMP_gc);
  EXPECT_EQ(Bits::ReadBitGroup(TCC5.CTRLE, TC5_CCBMODE_gm),
            TC45_CCBMODE_DISABLE_gc);
  EXPECT_EQ(TCC5.CCABUF, 8000u);
}

TEST_F(Tcc5Test, EnablePwmOnChannelBDrivesPc5) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);

  EXPECT_EQ(tcc5.EnablePwm<Channel::kB>(256u), Error::kOk);

  EXPECT_EQ(PORTC.DIRSET, PIN5_bm);
  EXPECT_EQ(Bits::ReadBitGroup(TCC5.CTRLE, TC5_CCAMODE_gm),
            TC45_CCAMODE_DISABLE_gc);
  EXPECT_EQ(Bits::ReadBitGroup(TCC5.CTRLE, TC5_CCBMODE_gm),
            TC45_CCBMODE_COMP_gc);
  EXPECT_EQ(TCC5.CCBBUF, 4000u);
  EXPECT_EQ(TCC5.CCABUF, 0u);
}

TEST_F(Tcc5Test, ItsVectorCallsItsCallback) {
  ASSERT_EQ(tcc5.Begin(1000u, OnTick), Error::kOk);
  TCC5.INTFLAGS = 0u;

  TCC5_OVF_vect();

  EXPECT_EQ(TCC5.INTFLAGS, TC4_OVFIF_bm);
  EXPECT_EQ(ticks, 1);
}

}  // namespace
}  // namespace xmega_timer
