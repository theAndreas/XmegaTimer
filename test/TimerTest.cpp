// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Timer against the mock of the registers of TCC4, TCC5, PORTC and the PMIC,
// at the 32 MHz of the ATxmega32E5: what it does today. Timer is a singleton
// that init() takes once, so each test needs a process of its own, which ctest
// gives it.

#include "Timer.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <gtest/gtest.h>
#include <stdint.h>

#include <vector>

#include "StandardTypes.h"

namespace {

using Tc5 = Timer<TimerC5>;

int tests_begun = 0;
int ticks = 0;
int other_ticks = 0;

void OnTick() { ++ticks; }
void OnOtherTick() { ++other_ticks; }

uint8_t ClockSelect(MockTimer const& timer) {
  return static_cast<uint8_t>(timer.CTRLA & TC4_CLKSEL_gm);
}

// The mode of a compare channel in CTRLE, 0 for A to 3 for D.
uint8_t ChannelMode(MockTimer const& timer, unsigned channel) {
  return static_cast<uint8_t>(
      (static_cast<unsigned>(timer.CTRLE) >> (2U * channel)) & TC4_CCAMODE_gm);
}

class TimerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ++tests_begun;
    ASSERT_EQ(tests_begun, 1)
        << "Timer is a singleton that init() takes once: run each test in a "
           "process of its own, as ctest does.";
    MockReset();
    ticks = 0;
    other_ticks = 0;
  }

  static Tc5& Tcc5() { return Tc5::getInstance(); }
};

TEST_F(TimerTest, InitStopsAndResetsTheTimerAndSetsDualSlopeAtBottom) {
  // The core leaves the timer running for analogWrite().
  TCC5.CTRLA = TC45_CLKSEL_DIV64_gc;

  EXPECT_EQ(Tcc5().init(1000U), E_OK);

  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_OFF_gc);
  EXPECT_EQ(TCC5.CTRLGSET.Commands(), std::vector<uint8_t>{TC45_CMD_RESET_gc});
  EXPECT_EQ(TCC5.CTRLB & TC4_WGMODE_gm, TC45_WGMODE_DSBOTTOM_gc);
  EXPECT_EQ(TCC5.PERBUF, 16000U);
}

TEST_F(TimerTest, InitASecondTimeIsRefusedAndChangesNothing) {
  ASSERT_EQ(Tcc5().init(1000U), E_OK);

  EXPECT_EQ(Tcc5().init(2000U, OnTick), E_NOT_OK);

  EXPECT_EQ(TCC5.PERBUF, 16000U);
  EXPECT_EQ(TCC5.INTCTRLA & TC4_OVFINTLVL_gm, TC45_OVFINTLVL_OFF_gc);
}

TEST_F(TimerTest, InitWithoutCallbackLeavesTheInterruptOff) {
  ASSERT_EQ(Tcc5().init(1000U), E_OK);

  EXPECT_EQ(TCC5.INTCTRLA & TC4_OVFINTLVL_gm, TC45_OVFINTLVL_OFF_gc);
  EXPECT_EQ(PMIC.CTRL, 0U);
}

TEST_F(TimerTest, InitWithCallbackEnablesTheInterruptAtLowLevel) {
  ASSERT_EQ(Tcc5().init(1000U, OnTick), E_OK);

  EXPECT_EQ(TCC5.INTCTRLA & TC4_OVFINTLVL_gm, TC45_OVFINTLVL_LO_gc);
  EXPECT_EQ(PMIC.CTRL & PMIC_LOLVLEN_bm, PMIC_LOLVLEN_bm);
}

TEST_F(TimerTest, InitWithAPeriodOutOfRangeIsRefusedButInitializes) {
  EXPECT_EQ(Tcc5().init(0U), E_NOT_OK);

  // A TOP of 0xFFFF and no prescaler.
  EXPECT_EQ(Tcc5().start(), E_OK);
  EXPECT_EQ(TCC5.PER, 0xFFFFU);
  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_DIV1_gc);
}

TEST_F(TimerTest, SetPeriodChoosesTheSmallestPrescaler) {
  struct PeriodCase {
    uint32_t microseconds;
    uint8_t clock_select;
    uint16_t top;
  };
  // 16 timer cycles per microsecond, as the counter goes up and down again.
  std::vector<PeriodCase> const cases{
      {1U, TC45_CLKSEL_DIV1_gc, 16U},
      {1000U, TC45_CLKSEL_DIV1_gc, 16000U},
      {4095U, TC45_CLKSEL_DIV1_gc, 65520U},
      {4096U, TC45_CLKSEL_DIV2_gc, 32768U},
      {10000U, TC45_CLKSEL_DIV4_gc, 40000U},
      {30000U, TC45_CLKSEL_DIV8_gc, 60000U},
      {100000U, TC45_CLKSEL_DIV64_gc, 25000U},
      {1000000U, TC45_CLKSEL_DIV256_gc, 62500U},
      {4000000U, TC45_CLKSEL_DIV1024_gc, 62500U},
      {4194303U, TC45_CLKSEL_DIV1024_gc, 65535U},
  };
  ASSERT_EQ(Tcc5().init(1000U), E_OK);

  for (PeriodCase const& c : cases) {
    SCOPED_TRACE(c.microseconds);
    EXPECT_EQ(Tcc5().setPeriod(c.microseconds), E_OK);
    EXPECT_EQ(TCC5.PERBUF, c.top);
    EXPECT_EQ(Tcc5().start(), E_OK);
    EXPECT_EQ(ClockSelect(TCC5), c.clock_select);
    EXPECT_EQ(TCC5.PER, c.top);
  }
}

TEST_F(TimerTest, SetPeriodOutOfRangeIsRefusedAndChangesNothing) {
  ASSERT_EQ(Tcc5().init(1000U), E_OK);

  EXPECT_EQ(Tcc5().setPeriod(0U), E_NOT_OK);
  EXPECT_EQ(Tcc5().setPeriod(4194304U), E_NOT_OK);

  EXPECT_EQ(TCC5.PERBUF, 16000U);
  ASSERT_EQ(Tcc5().start(), E_OK);
  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_DIV1_gc);
}

TEST_F(TimerTest, SetPeriodOfAReadyTimerLeavesItsClockOff) {
  ASSERT_EQ(Tcc5().init(1000U), E_OK);

  EXPECT_EQ(Tcc5().setPeriod(10000U), E_OK);

  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_OFF_gc);
}

TEST_F(TimerTest, SetPeriodOfARunningTimerGoesThroughTheBuffer) {
  ASSERT_EQ(Tcc5().init(1000U), E_OK);
  ASSERT_EQ(Tcc5().start(), E_OK);

  EXPECT_EQ(Tcc5().setPeriod(10000U), E_OK);

  // The prescaler at once, the period at the next BOTTOM.
  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_DIV4_gc);
  EXPECT_EQ(TCC5.PERBUF, 40000U);
  EXPECT_EQ(TCC5.PER, 16000U);
}

TEST_F(TimerTest, StartBeforeInitIsRefused) {
  EXPECT_EQ(Tcc5().start(), E_NOT_OK);

  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_OFF_gc);
  EXPECT_TRUE(TCC5.CTRLGSET.Commands().empty());
}

TEST_F(TimerTest, StartLoadsThePeriodAndRestartsFromZeroCountingUp) {
  ASSERT_EQ(Tcc5().init(1000U), E_OK);
  TCC5.CTRLGSET.SetCountingDown(true);

  EXPECT_EQ(Tcc5().start(), E_OK);

  EXPECT_EQ(TCC5.PER, 16000U);
  EXPECT_EQ(TCC5.CTRLGSET.Commands(),
            (std::vector<uint8_t>{TC45_CMD_RESET_gc, TC45_CMD_RESTART_gc}));
  EXPECT_EQ(TCC5.CTRLGSET & TC4_DIR_bm, 0U);
  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_DIV1_gc);
}

TEST_F(TimerTest, StopTakesTheClock) {
  ASSERT_EQ(Tcc5().init(1000U), E_OK);
  ASSERT_EQ(Tcc5().start(), E_OK);

  Tcc5().stop();

  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_OFF_gc);
}

TEST_F(TimerTest, StopBeforeInitLeavesTheTimerToTheCore) {
  TCC5.CTRLA = TC45_CLKSEL_DIV64_gc;

  Tcc5().stop();

  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_DIV64_gc);
}

TEST_F(TimerTest, ResumeGoesOnWithThePrescalerWithoutRestart) {
  ASSERT_EQ(Tcc5().init(10000U), E_OK);
  ASSERT_EQ(Tcc5().start(), E_OK);
  Tcc5().stop();

  EXPECT_EQ(Tcc5().resume(), E_OK);

  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_DIV4_gc);
  EXPECT_EQ(TCC5.CTRLGSET.Commands(),
            (std::vector<uint8_t>{TC45_CMD_RESET_gc, TC45_CMD_RESTART_gc}));
}

TEST_F(TimerTest, ResumeOfATimerNotStoppedIsRefused) {
  EXPECT_EQ(Tcc5().resume(), E_NOT_OK);
  ASSERT_EQ(Tcc5().init(1000U), E_OK);
  EXPECT_EQ(Tcc5().resume(), E_NOT_OK);
  ASSERT_EQ(Tcc5().start(), E_OK);
  EXPECT_EQ(Tcc5().resume(), E_NOT_OK);
}

TEST_F(TimerTest, AttachInterruptOfNullptrIsRefused) {
  ASSERT_EQ(Tcc5().init(1000U), E_OK);

  EXPECT_EQ(Tcc5().attachInterrupt(nullptr), E_NOT_OK);

  EXPECT_EQ(TCC5.INTCTRLA & TC4_OVFINTLVL_gm, TC45_OVFINTLVL_OFF_gc);
}

TEST_F(TimerTest, AttachInterruptClearsAFlagFromWhileItWasOff) {
  ASSERT_EQ(Tcc5().init(1000U), E_OK);
  TCC5.INTFLAGS = 0U;

  EXPECT_EQ(Tcc5().attachInterrupt(OnTick), E_OK);

  EXPECT_EQ(TCC5.INTFLAGS, TC4_OVFIF_bm);
  EXPECT_EQ(TCC5.INTCTRLA & TC4_OVFINTLVL_gm, TC45_OVFINTLVL_LO_gc);
}

TEST_F(TimerTest, AttachInterruptWhileOnLeavesAPendingOverflow) {
  ASSERT_EQ(Tcc5().init(1000U, OnTick), E_OK);
  TCC5.INTFLAGS = 0U;

  EXPECT_EQ(Tcc5().attachInterrupt(OnOtherTick), E_OK);

  EXPECT_EQ(TCC5.INTFLAGS, 0U);
  TCC5_OVF_vect();
  EXPECT_EQ(ticks, 0);
  EXPECT_EQ(other_ticks, 1);
}

TEST_F(TimerTest, OverflowClearsTheFlagAndCallsTheCallback) {
  ASSERT_EQ(Tcc5().init(1000U, OnTick), E_OK);
  ASSERT_EQ(Tcc5().start(), E_OK);
  TCC5.INTFLAGS = 0U;

  TCC5_OVF_vect();

  EXPECT_EQ(TCC5.INTFLAGS, TC4_OVFIF_bm);
  EXPECT_EQ(ticks, 1);
}

TEST_F(TimerTest, OverflowWithoutCallbackClearsTheFlag) {
  ASSERT_EQ(Tcc5().init(1000U), E_OK);

  TCC5_OVF_vect();

  EXPECT_EQ(TCC5.INTFLAGS, TC4_OVFIF_bm);
}

TEST_F(TimerTest, DetachInterruptTurnsItOffAndForgetsTheCallback) {
  ASSERT_EQ(Tcc5().init(1000U, OnTick), E_OK);

  Tcc5().detachInterrupt();

  EXPECT_EQ(TCC5.INTCTRLA & TC4_OVFINTLVL_gm, TC45_OVFINTLVL_OFF_gc);
  TCC5_OVF_vect();
  EXPECT_EQ(ticks, 0);
}

TEST_F(TimerTest, ReadBeforeStartIsRefusedAndLeavesTheValue) {
  uint32_t microseconds{7U};
  EXPECT_EQ(Tcc5().read(microseconds), E_NOT_OK);
  ASSERT_EQ(Tcc5().init(1000U), E_OK);
  EXPECT_EQ(Tcc5().read(microseconds), E_NOT_OK);

  EXPECT_EQ(microseconds, 7U);
}

TEST_F(TimerTest, ReadCountingUpIsTheCounter) {
  ASSERT_EQ(Tcc5().init(1000U), E_OK);
  ASSERT_EQ(Tcc5().start(), E_OK);
  TCC5.CNT = 1000U;

  uint32_t microseconds{0U};
  EXPECT_EQ(Tcc5().read(microseconds), E_OK);

  // 1000 cycles of 32 MHz.
  EXPECT_EQ(microseconds, 31U);
}

TEST_F(TimerTest, ReadCountingDownAddsTheWayBackFromTop) {
  ASSERT_EQ(Tcc5().init(1000U), E_OK);
  ASSERT_EQ(Tcc5().start(), E_OK);
  TCC5.CNT = 1000U;
  TCC5.CTRLGSET.SetCountingDown(true);

  uint32_t microseconds{0U};
  EXPECT_EQ(Tcc5().read(microseconds), E_OK);

  // 16000 up and 15000 down again.
  EXPECT_EQ(microseconds, 968U);
}

TEST_F(TimerTest, ReadAtTheLargestPrescalerCountsEachTick) {
  ASSERT_EQ(Tcc5().init(4000000U), E_OK);
  ASSERT_EQ(Tcc5().start(), E_OK);

  uint32_t microseconds{0U};
  TCC5.CNT = 1U;
  EXPECT_EQ(Tcc5().read(microseconds), E_OK);
  EXPECT_EQ(microseconds, 32U);
  TCC5.CNT = 3U;
  EXPECT_EQ(Tcc5().read(microseconds), E_OK);
  EXPECT_EQ(microseconds, 96U);
}

TEST_F(TimerTest, ReadOfAStoppedTimer) {
  ASSERT_EQ(Tcc5().init(1000U), E_OK);
  ASSERT_EQ(Tcc5().start(), E_OK);
  Tcc5().stop();
  TCC5.CNT = 3200U;

  uint32_t microseconds{0U};
  EXPECT_EQ(Tcc5().read(microseconds), E_OK);

  EXPECT_EQ(microseconds, 100U);
}

TEST_F(TimerTest, EnablePwmOnChannelADrivesPc4) {
  ASSERT_EQ(Tcc5().init(1000U), E_OK);

  EXPECT_EQ(Tcc5().enablePwm<Tc5::TIMER_CHANNEL_A>(512U), E_OK);

  EXPECT_EQ(PORTC.DIRSET, 1U << 4U);
  EXPECT_EQ(ChannelMode(TCC5, 0), TC45_CCAMODE_COMP_gc);
  EXPECT_EQ(ChannelMode(TCC5, 1), TC45_CCAMODE_DISABLE_gc);
  EXPECT_EQ(TCC5.CCABUF, 8000U);
}

TEST_F(TimerTest, EnablePwmOnChannelBDrivesPc5) {
  ASSERT_EQ(Tcc5().init(1000U), E_OK);

  EXPECT_EQ(Tcc5().enablePwm<Tc5::TIMER_CHANNEL_B>(256U), E_OK);

  EXPECT_EQ(PORTC.DIRSET, 1U << 5U);
  EXPECT_EQ(ChannelMode(TCC5, 0), TC45_CCAMODE_DISABLE_gc);
  EXPECT_EQ(ChannelMode(TCC5, 1), TC45_CCAMODE_COMP_gc);
  EXPECT_EQ(TCC5.CCBBUF, 4000U);
  EXPECT_EQ(TCC5.CCABUF, 0U);
}

TEST_F(TimerTest, EnablePwmAboveTheMaximumIsRefusedAndChangesNothing) {
  ASSERT_EQ(Tcc5().init(1000U), E_OK);

  EXPECT_EQ(Tcc5().enablePwm<Tc5::TIMER_CHANNEL_A>(Tc5::PwmDutyMax + 1U),
            E_NOT_OK);

  EXPECT_EQ(PORTC.DIRSET, 0U);
  EXPECT_EQ(ChannelMode(TCC5, 0), TC45_CCAMODE_DISABLE_gc);
  EXPECT_EQ(TCC5.CCABUF, 0U);
}

TEST_F(TimerTest, PwmDutyFromZeroToTheMaximum) {
  ASSERT_EQ(Tcc5().init(1000U), E_OK);

  EXPECT_EQ(Tcc5().enablePwm<Tc5::TIMER_CHANNEL_A>(0U), E_OK);
  EXPECT_EQ(TCC5.CCABUF, 0U);
  EXPECT_EQ(Tcc5().setPwmDuty<Tc5::TIMER_CHANNEL_A>(Tc5::PwmDutyMax), E_OK);
  EXPECT_EQ(TCC5.CCABUF, 15984U);
}

TEST_F(TimerTest, SetPwmDutyGoesThroughTheBuffer) {
  ASSERT_EQ(Tcc5().init(1000U), E_OK);
  ASSERT_EQ(Tcc5().enablePwm<Tc5::TIMER_CHANNEL_A>(512U), E_OK);

  EXPECT_EQ(Tcc5().setPwmDuty<Tc5::TIMER_CHANNEL_A>(256U), E_OK);

  EXPECT_EQ(TCC5.CCABUF, 4000U);
  EXPECT_EQ(TCC5.CCA, 0U);
}

TEST_F(TimerTest, SetPwmDutyAboveTheMaximumIsRefusedAndChangesNothing) {
  ASSERT_EQ(Tcc5().init(1000U), E_OK);
  ASSERT_EQ(Tcc5().enablePwm<Tc5::TIMER_CHANNEL_B>(512U), E_OK);

  EXPECT_EQ(Tcc5().setPwmDuty<Tc5::TIMER_CHANNEL_B>(Tc5::PwmDutyMax + 1U),
            E_NOT_OK);

  EXPECT_EQ(TCC5.CCBBUF, 8000U);
}

TEST_F(TimerTest, DisablePwmLeavesThePinAnOutput) {
  ASSERT_EQ(Tcc5().init(1000U), E_OK);
  ASSERT_EQ(Tcc5().enablePwm<Tc5::TIMER_CHANNEL_A>(512U), E_OK);
  PORTC.DIRSET = 0U;

  EXPECT_EQ(Tcc5().disablePwm<Tc5::TIMER_CHANNEL_A>(), E_OK);

  EXPECT_EQ(ChannelMode(TCC5, 0), TC45_CCAMODE_DISABLE_gc);
  EXPECT_EQ(PORTC.DIRSET, 0U);
}

TEST_F(TimerTest, SetPeriodKeepsTheDutyOfEachChannel) {
  ASSERT_EQ(Tcc5().init(1000U), E_OK);
  ASSERT_EQ(Tcc5().enablePwm<Tc5::TIMER_CHANNEL_A>(512U), E_OK);
  ASSERT_EQ(Tcc5().enablePwm<Tc5::TIMER_CHANNEL_B>(256U), E_OK);

  EXPECT_EQ(Tcc5().setPeriod(2000U), E_OK);

  EXPECT_EQ(TCC5.PERBUF, 32000U);
  EXPECT_EQ(TCC5.CCABUF, 16000U);
  EXPECT_EQ(TCC5.CCBBUF, 8000U);
}

TEST_F(TimerTest, StartLoadsTheCompareValuesAtOnce) {
  ASSERT_EQ(Tcc5().init(1000U), E_OK);
  ASSERT_EQ(Tcc5().enablePwm<Tc5::TIMER_CHANNEL_A>(512U), E_OK);
  ASSERT_EQ(Tcc5().enablePwm<Tc5::TIMER_CHANNEL_B>(256U), E_OK);

  EXPECT_EQ(Tcc5().start(), E_OK);

  EXPECT_EQ(TCC5.CCA, 8000U);
  EXPECT_EQ(TCC5.CCB, 4000U);
}

#if (TIMER_USE_TCC4 == STD_ON)

using Tc4 = Timer<TimerC4>;

TEST_F(TimerTest, Tcc4DrivesItsFourChannelsOnPc0ToPc3) {
  Tc4& tcc4{Tc4::getInstance()};
  ASSERT_EQ(tcc4.init(1000U), E_OK);

  EXPECT_EQ(tcc4.enablePwm<Tc4::TIMER_CHANNEL_A>(128U), E_OK);
  EXPECT_EQ(PORTC.DIRSET, 1U << 0U);
  EXPECT_EQ(tcc4.enablePwm<Tc4::TIMER_CHANNEL_B>(256U), E_OK);
  EXPECT_EQ(PORTC.DIRSET, 1U << 1U);
  EXPECT_EQ(tcc4.enablePwm<Tc4::TIMER_CHANNEL_C>(512U), E_OK);
  EXPECT_EQ(PORTC.DIRSET, 1U << 2U);
  EXPECT_EQ(tcc4.enablePwm<Tc4::TIMER_CHANNEL_D>(Tc4::PwmDutyMax), E_OK);
  EXPECT_EQ(PORTC.DIRSET, 1U << 3U);

  for (unsigned channel = 0U; channel < 4U; ++channel) {
    SCOPED_TRACE(channel);
    EXPECT_EQ(ChannelMode(TCC4, channel), TC45_CCAMODE_COMP_gc);
  }
  EXPECT_EQ(TCC4.CCABUF, 2000U);
  EXPECT_EQ(TCC4.CCBBUF, 4000U);
  EXPECT_EQ(TCC4.CCCBUF, 8000U);
  EXPECT_EQ(TCC4.CCDBUF, 15984U);
  EXPECT_EQ(TCC5.CTRLE, 0U);
}

TEST_F(TimerTest, Tcc4SetPeriodKeepsTheDutyOfAllFourChannels) {
  Tc4& tcc4{Tc4::getInstance()};
  ASSERT_EQ(tcc4.init(1000U), E_OK);
  ASSERT_EQ(tcc4.enablePwm<Tc4::TIMER_CHANNEL_C>(512U), E_OK);
  ASSERT_EQ(tcc4.enablePwm<Tc4::TIMER_CHANNEL_D>(256U), E_OK);

  EXPECT_EQ(tcc4.setPeriod(2000U), E_OK);
  EXPECT_EQ(tcc4.start(), E_OK);

  EXPECT_EQ(TCC4.CCCBUF, 16000U);
  EXPECT_EQ(TCC4.CCDBUF, 8000U);
  EXPECT_EQ(TCC4.CCC, 16000U);
  EXPECT_EQ(TCC4.CCD, 8000U);
}

TEST_F(TimerTest, Tcc4OverflowCallsItsOwnCallback) {
  ASSERT_EQ(Tc4::getInstance().init(1000U, OnTick), E_OK);
  ASSERT_EQ(Tcc5().init(1000U, OnOtherTick), E_OK);

  TCC4_OVF_vect();

  EXPECT_EQ(TCC4.INTFLAGS, TC4_OVFIF_bm);
  EXPECT_EQ(ticks, 1);
  EXPECT_EQ(other_ticks, 0);
}

#endif

}  // namespace
