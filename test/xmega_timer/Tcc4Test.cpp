// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later
//
// What is TCC4's own: its four channels on PC0 to PC3, and its interrupt
// vector. Only in the variant tcc4, which switches it on; in the others, using
// it does not compile (test/compile_fail/tcc4_switched_off.cpp).

#include "xmega_timer/Tcc4.h"

#if XMEGATIMER_USE_TCC4

#include <avr/interrupt.h>
#include <avr/io.h>
#include <gtest/gtest.h>

#include "test/fixture.h"
#include "test/printers.h"  // IWYU pragma: keep
#include "xmega_timer/Error.h"
#include "xmega_timer/Tcc5.h"
#include "xmega_timer/Timer.h"

namespace xmega_timer {
namespace {

using Tcc4Test = TimerFixture;

TEST_F(Tcc4Test, EnablePwmOnChannelADrivesPc0) {
  ASSERT_EQ(tcc4.Begin(1000u), Error::kOk);

  EXPECT_EQ(tcc4.EnablePwm<Channel::kA>(128u), Error::kOk);

  EXPECT_EQ(PORTC.DIRSET, PIN0_bm);
  EXPECT_EQ(Bits::ReadBitGroup(TCC4.CTRLE, TC4_CCAMODE_gm),
            TC45_CCAMODE_COMP_gc);
  EXPECT_EQ(TCC4.CCABUF, 2000u);
  EXPECT_EQ(TCC5.CTRLE, 0u);
}

TEST_F(Tcc4Test, EnablePwmOnChannelBDrivesPc1) {
  ASSERT_EQ(tcc4.Begin(1000u), Error::kOk);

  EXPECT_EQ(tcc4.EnablePwm<Channel::kB>(256u), Error::kOk);

  EXPECT_EQ(PORTC.DIRSET, PIN1_bm);
  EXPECT_EQ(Bits::ReadBitGroup(TCC4.CTRLE, TC4_CCBMODE_gm),
            TC45_CCBMODE_COMP_gc);
  EXPECT_EQ(TCC4.CCBBUF, 4000u);
}

TEST_F(Tcc4Test, EnablePwmOnChannelCDrivesPc2) {
  ASSERT_EQ(tcc4.Begin(1000u), Error::kOk);

  EXPECT_EQ(tcc4.EnablePwm<Channel::kC>(512u), Error::kOk);

  EXPECT_EQ(PORTC.DIRSET, PIN2_bm);
  EXPECT_EQ(Bits::ReadBitGroup(TCC4.CTRLE, TC4_CCCMODE_gm),
            TC45_CCCMODE_COMP_gc);
  EXPECT_EQ(TCC4.CCCBUF, 8000u);
}

TEST_F(Tcc4Test, EnablePwmOnChannelDDrivesPc3) {
  ASSERT_EQ(tcc4.Begin(1000u), Error::kOk);

  EXPECT_EQ(tcc4.EnablePwm<Channel::kD>(Tcc4::kPwmDutyMax), Error::kOk);

  EXPECT_EQ(PORTC.DIRSET, PIN3_bm);
  EXPECT_EQ(Bits::ReadBitGroup(TCC4.CTRLE, TC4_CCDMODE_gm),
            TC45_CCDMODE_COMP_gc);
  EXPECT_EQ(TCC4.CCDBUF, 15984u);
}

TEST_F(Tcc4Test, SetPeriodKeepsTheDutyOfAllFourChannels) {
  ASSERT_EQ(tcc4.Begin(1000u), Error::kOk);
  ASSERT_EQ(tcc4.EnablePwm<Channel::kC>(512u), Error::kOk);
  ASSERT_EQ(tcc4.EnablePwm<Channel::kD>(256u), Error::kOk);

  EXPECT_EQ(tcc4.SetPeriod(2000u), Error::kOk);
  EXPECT_EQ(tcc4.Start(), Error::kOk);

  EXPECT_EQ(TCC4.CCCBUF, 16000u);
  EXPECT_EQ(TCC4.CCDBUF, 8000u);
  EXPECT_EQ(TCC4.CCC, 16000u);
  EXPECT_EQ(TCC4.CCD, 8000u);
}

TEST_F(Tcc4Test, ItsVectorCallsItsOwnCallback) {
  ASSERT_EQ(tcc4.Begin(1000u, OnTick), Error::kOk);
  ASSERT_EQ(tcc5.Begin(1000u, OnOtherTick), Error::kOk);

  TCC4_OVF_vect();

  EXPECT_EQ(TCC4.INTFLAGS, TC4_OVFIF_bm);
  EXPECT_EQ(ticks, 1);
  EXPECT_EQ(other_ticks, 0);
}

}  // namespace
}  // namespace xmega_timer

#endif  // XMEGATIMER_USE_TCC4
