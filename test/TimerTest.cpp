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
#include "xmega_timer/Bit.h"

namespace {

using Tc5 = Timer<TimerC5>;
using Bits = xmega_timer::Bit<uint8_t>;

int tests_begun{0};
int ticks{0};
int other_ticks{0};

void OnTick() { ++ticks; }
void OnOtherTick() { ++other_ticks; }

uint8_t ClockSelect(MockTimer const& timer) {
  return Bits::ReadBitGroup(timer.CTRLA, TC4_CLKSEL_gm);
}

// The mode of a compare channel in CTRLE, 0 for A to 3 for D.
uint8_t ChannelMode(MockTimer const& timer, unsigned channel) {
  uint8_t const position{static_cast<uint8_t>(2u * channel)};
  return Bits::ReadBitGroup(timer.CTRLE, TC4_CCAMODE_gm, position);
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

  EXPECT_EQ(Tcc5().init(1000u), E_OK);

  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_OFF_gc);
  EXPECT_EQ(TCC5.CTRLGSET.Commands(), std::vector<uint8_t>{TC45_CMD_RESET_gc});
  EXPECT_EQ(Bits::ReadBitGroup(TCC5.CTRLB, TC4_WGMODE_gm),
            TC45_WGMODE_DSBOTTOM_gc);
  EXPECT_EQ(TCC5.PERBUF, 16000u);
}

TEST_F(TimerTest, InitASecondTimeIsRefusedAndChangesNothing) {
  ASSERT_EQ(Tcc5().init(1000u), E_OK);

  EXPECT_EQ(Tcc5().init(2000u, OnTick), E_NOT_OK);

  EXPECT_EQ(TCC5.PERBUF, 16000u);
  EXPECT_EQ(Bits::ReadBitGroup(TCC5.INTCTRLA, TC4_OVFINTLVL_gm),
            TC45_OVFINTLVL_OFF_gc);
}

TEST_F(TimerTest, InitWithoutCallbackLeavesTheInterruptOff) {
  ASSERT_EQ(Tcc5().init(1000u), E_OK);

  EXPECT_EQ(Bits::ReadBitGroup(TCC5.INTCTRLA, TC4_OVFINTLVL_gm),
            TC45_OVFINTLVL_OFF_gc);
  EXPECT_EQ(PMIC.CTRL, 0u);
}

TEST_F(TimerTest, InitWithCallbackEnablesTheInterruptAtLowLevel) {
  ASSERT_EQ(Tcc5().init(1000u, OnTick), E_OK);

  EXPECT_EQ(Bits::ReadBitGroup(TCC5.INTCTRLA, TC4_OVFINTLVL_gm),
            TC45_OVFINTLVL_LO_gc);
  EXPECT_EQ(Bits::ReadBitGroup(PMIC.CTRL, PMIC_LOLVLEN_bm), PMIC_LOLVLEN_bm);
}

TEST_F(TimerTest, InitWithAPeriodOutOfRangeIsRefusedButInitializes) {
  EXPECT_EQ(Tcc5().init(0u), E_NOT_OK);

  // A TOP of 0xFFFF and no prescaler.
  EXPECT_EQ(Tcc5().start(), E_OK);
  EXPECT_EQ(TCC5.PER, 0xFFFFu);
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
      {1u, TC45_CLKSEL_DIV1_gc, 16u},
      {1000u, TC45_CLKSEL_DIV1_gc, 16000u},
      {4095u, TC45_CLKSEL_DIV1_gc, 65520u},
      {4096u, TC45_CLKSEL_DIV2_gc, 32768u},
      {10000u, TC45_CLKSEL_DIV4_gc, 40000u},
      {30000u, TC45_CLKSEL_DIV8_gc, 60000u},
      {100000u, TC45_CLKSEL_DIV64_gc, 25000u},
      {1000000u, TC45_CLKSEL_DIV256_gc, 62500u},
      {4000000u, TC45_CLKSEL_DIV1024_gc, 62500u},
      {4194303u, TC45_CLKSEL_DIV1024_gc, 65535u},
  };
  ASSERT_EQ(Tcc5().init(1000u), E_OK);

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
  ASSERT_EQ(Tcc5().init(1000u), E_OK);

  EXPECT_EQ(Tcc5().setPeriod(0u), E_NOT_OK);
  EXPECT_EQ(Tcc5().setPeriod(4194304u), E_NOT_OK);

  EXPECT_EQ(TCC5.PERBUF, 16000u);
  ASSERT_EQ(Tcc5().start(), E_OK);
  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_DIV1_gc);
}

TEST_F(TimerTest, SetPeriodOfAReadyTimerLeavesItsClockOff) {
  ASSERT_EQ(Tcc5().init(1000u), E_OK);

  EXPECT_EQ(Tcc5().setPeriod(10000u), E_OK);

  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_OFF_gc);
}

TEST_F(TimerTest,
       SetPeriodOfARunningTimerKeepingItsPrescalerGoesThroughTheBuffer) {
  ASSERT_EQ(Tcc5().init(1000u), E_OK);
  ASSERT_EQ(Tcc5().start(), E_OK);

  EXPECT_EQ(Tcc5().setPeriod(2000u), E_OK);

  EXPECT_EQ(TCC5.PERBUF, 32000u);
  EXPECT_EQ(TCC5.PER, 16000u);
  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_DIV1_gc);
  EXPECT_EQ(TCC5.CTRLGSET.Commands(),
            (std::vector<uint8_t>{TC45_CMD_RESET_gc, TC45_CMD_RESTART_gc}));
}

TEST_F(TimerTest, SetPeriodOfARunningTimerWithANewPrescalerRestartsItAtOnce) {
  ASSERT_EQ(Tcc5().init(1000u), E_OK);
  ASSERT_EQ(Tcc5().start(), E_OK);

  EXPECT_EQ(Tcc5().setPeriod(10000u), E_OK);

  EXPECT_EQ(TCC5.PER, 40000u);
  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_DIV4_gc);
  EXPECT_EQ(TCC5.CTRLGSET.Commands(),
            (std::vector<uint8_t>{TC45_CMD_RESET_gc, TC45_CMD_RESTART_gc,
                                  TC45_CMD_RESTART_gc}));
}

TEST_F(TimerTest, SetPeriodOfARunningTimerWithANewPrescalerLoadsTheDutyAtOnce) {
  ASSERT_EQ(Tcc5().init(1000u), E_OK);
  ASSERT_EQ(Tcc5().enablePwm<Tc5::TIMER_CHANNEL_A>(512u), E_OK);
  ASSERT_EQ(Tcc5().start(), E_OK);

  EXPECT_EQ(Tcc5().setPeriod(10000u), E_OK);

  EXPECT_EQ(TCC5.CCA, 20000u);
}

TEST_F(TimerTest,
       SetPeriodOfAStoppedTimerKeepingItsPrescalerGoesThroughTheBuffer) {
  ASSERT_EQ(Tcc5().init(1000u), E_OK);
  ASSERT_EQ(Tcc5().start(), E_OK);
  Tcc5().stop();

  EXPECT_EQ(Tcc5().setPeriod(2000u), E_OK);

  EXPECT_EQ(TCC5.PERBUF, 32000u);
  EXPECT_EQ(TCC5.PER, 16000u);
  EXPECT_EQ(TCC5.CTRLGSET.Commands(),
            (std::vector<uint8_t>{TC45_CMD_RESET_gc, TC45_CMD_RESTART_gc}));
}

TEST_F(TimerTest, SetPeriodOfAStoppedTimerWithANewPrescalerSetsItBackStopped) {
  ASSERT_EQ(Tcc5().init(1000u), E_OK);
  ASSERT_EQ(Tcc5().start(), E_OK);
  Tcc5().stop();

  EXPECT_EQ(Tcc5().setPeriod(10000u), E_OK);

  EXPECT_EQ(TCC5.PER, 40000u);
  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_OFF_gc);
  EXPECT_EQ(TCC5.CTRLGSET.Commands(),
            (std::vector<uint8_t>{TC45_CMD_RESET_gc, TC45_CMD_RESTART_gc,
                                  TC45_CMD_RESTART_gc}));
}

TEST_F(TimerTest, SetPeriodOfAStoppedTimerWithANewPrescalerLoadsTheDutyAtOnce) {
  ASSERT_EQ(Tcc5().init(1000u), E_OK);
  ASSERT_EQ(Tcc5().enablePwm<Tc5::TIMER_CHANNEL_A>(512u), E_OK);
  ASSERT_EQ(Tcc5().start(), E_OK);
  Tcc5().stop();

  EXPECT_EQ(Tcc5().setPeriod(10000u), E_OK);

  EXPECT_EQ(TCC5.CCA, 20000u);
}

TEST_F(TimerTest, ResumeAfterANewPrescalerCountsTheNewPeriod) {
  ASSERT_EQ(Tcc5().init(1000u), E_OK);
  ASSERT_EQ(Tcc5().start(), E_OK);
  Tcc5().stop();
  ASSERT_EQ(Tcc5().setPeriod(10000u), E_OK);

  EXPECT_EQ(Tcc5().resume(), E_OK);

  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_DIV4_gc);
  EXPECT_EQ(TCC5.PER, 40000u);
}

TEST_F(TimerTest, StartBeforeInitIsRefused) {
  EXPECT_EQ(Tcc5().start(), E_NOT_OK);

  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_OFF_gc);
  EXPECT_TRUE(TCC5.CTRLGSET.Commands().empty());
}

TEST_F(TimerTest, StartLoadsThePeriodAndRestartsFromZeroCountingUp) {
  ASSERT_EQ(Tcc5().init(1000u), E_OK);
  TCC5.CTRLGSET.SetCountingDown(true);

  EXPECT_EQ(Tcc5().start(), E_OK);

  EXPECT_EQ(TCC5.PER, 16000u);
  EXPECT_EQ(TCC5.CTRLGSET.Commands(),
            (std::vector<uint8_t>{TC45_CMD_RESET_gc, TC45_CMD_RESTART_gc}));
  EXPECT_EQ(Bits::ReadBitGroup(TCC5.CTRLGSET, TC4_DIR_bm), 0u);
  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_DIV1_gc);
}

TEST_F(TimerTest, StopTakesTheClock) {
  ASSERT_EQ(Tcc5().init(1000u), E_OK);
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
  ASSERT_EQ(Tcc5().init(10000u), E_OK);
  ASSERT_EQ(Tcc5().start(), E_OK);
  Tcc5().stop();

  EXPECT_EQ(Tcc5().resume(), E_OK);

  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_DIV4_gc);
  EXPECT_EQ(TCC5.CTRLGSET.Commands(),
            (std::vector<uint8_t>{TC45_CMD_RESET_gc, TC45_CMD_RESTART_gc}));
}

TEST_F(TimerTest, ResumeOfATimerNotStoppedIsRefused) {
  EXPECT_EQ(Tcc5().resume(), E_NOT_OK);
  ASSERT_EQ(Tcc5().init(1000u), E_OK);
  EXPECT_EQ(Tcc5().resume(), E_NOT_OK);
  ASSERT_EQ(Tcc5().start(), E_OK);
  EXPECT_EQ(Tcc5().resume(), E_NOT_OK);
}

TEST_F(TimerTest, AttachInterruptOfNullptrIsRefused) {
  ASSERT_EQ(Tcc5().init(1000u), E_OK);

  EXPECT_EQ(Tcc5().attachInterrupt(nullptr), E_NOT_OK);

  EXPECT_EQ(Bits::ReadBitGroup(TCC5.INTCTRLA, TC4_OVFINTLVL_gm),
            TC45_OVFINTLVL_OFF_gc);
}

TEST_F(TimerTest, AttachInterruptClearsAFlagFromWhileItWasOff) {
  ASSERT_EQ(Tcc5().init(1000u), E_OK);
  TCC5.INTFLAGS = 0u;

  EXPECT_EQ(Tcc5().attachInterrupt(OnTick), E_OK);

  EXPECT_EQ(TCC5.INTFLAGS, TC4_OVFIF_bm);
  EXPECT_EQ(Bits::ReadBitGroup(TCC5.INTCTRLA, TC4_OVFINTLVL_gm),
            TC45_OVFINTLVL_LO_gc);
}

TEST_F(TimerTest, AttachInterruptWhileOnLeavesAPendingOverflow) {
  ASSERT_EQ(Tcc5().init(1000u, OnTick), E_OK);
  TCC5.INTFLAGS = 0u;

  EXPECT_EQ(Tcc5().attachInterrupt(OnOtherTick), E_OK);

  EXPECT_EQ(TCC5.INTFLAGS, 0u);
  TCC5_OVF_vect();
  EXPECT_EQ(ticks, 0);
  EXPECT_EQ(other_ticks, 1);
}

TEST_F(TimerTest, OverflowClearsTheFlagAndCallsTheCallback) {
  ASSERT_EQ(Tcc5().init(1000u, OnTick), E_OK);
  ASSERT_EQ(Tcc5().start(), E_OK);
  TCC5.INTFLAGS = 0u;

  TCC5_OVF_vect();

  EXPECT_EQ(TCC5.INTFLAGS, TC4_OVFIF_bm);
  EXPECT_EQ(ticks, 1);
}

TEST_F(TimerTest, OverflowWithoutCallbackClearsTheFlag) {
  ASSERT_EQ(Tcc5().init(1000u), E_OK);

  TCC5_OVF_vect();

  EXPECT_EQ(TCC5.INTFLAGS, TC4_OVFIF_bm);
}

TEST_F(TimerTest, DetachInterruptTurnsItOffAndForgetsTheCallback) {
  ASSERT_EQ(Tcc5().init(1000u, OnTick), E_OK);

  Tcc5().detachInterrupt();

  EXPECT_EQ(Bits::ReadBitGroup(TCC5.INTCTRLA, TC4_OVFINTLVL_gm),
            TC45_OVFINTLVL_OFF_gc);
  TCC5_OVF_vect();
  EXPECT_EQ(ticks, 0);
}

TEST_F(TimerTest, ReadBeforeStartIsRefusedAndLeavesTheValue) {
  uint32_t microseconds{7u};
  EXPECT_EQ(Tcc5().read(microseconds), E_NOT_OK);
  ASSERT_EQ(Tcc5().init(1000u), E_OK);
  EXPECT_EQ(Tcc5().read(microseconds), E_NOT_OK);

  EXPECT_EQ(microseconds, 7u);
}

TEST_F(TimerTest, ReadCountingUpIsTheCounter) {
  ASSERT_EQ(Tcc5().init(1000u), E_OK);
  ASSERT_EQ(Tcc5().start(), E_OK);
  TCC5.CNT = 1000u;

  uint32_t microseconds{0u};
  EXPECT_EQ(Tcc5().read(microseconds), E_OK);

  // 1000 cycles of 32 MHz.
  EXPECT_EQ(microseconds, 31u);
}

TEST_F(TimerTest, ReadCountingDownAddsTheWayBackFromTop) {
  ASSERT_EQ(Tcc5().init(1000u), E_OK);
  ASSERT_EQ(Tcc5().start(), E_OK);
  TCC5.CNT = 1000u;
  TCC5.CTRLGSET.SetCountingDown(true);

  uint32_t microseconds{0u};
  EXPECT_EQ(Tcc5().read(microseconds), E_OK);

  // 16000 up and 15000 down again.
  EXPECT_EQ(microseconds, 968u);
}

TEST_F(TimerTest, ReadAtTheLargestPrescalerCountsEachTick) {
  ASSERT_EQ(Tcc5().init(4000000u), E_OK);
  ASSERT_EQ(Tcc5().start(), E_OK);

  uint32_t microseconds{0u};
  TCC5.CNT = 1u;
  EXPECT_EQ(Tcc5().read(microseconds), E_OK);
  EXPECT_EQ(microseconds, 32u);
  TCC5.CNT = 3u;
  EXPECT_EQ(Tcc5().read(microseconds), E_OK);
  EXPECT_EQ(microseconds, 96u);
}

TEST_F(TimerTest, ReadOfAStoppedTimer) {
  ASSERT_EQ(Tcc5().init(1000u), E_OK);
  ASSERT_EQ(Tcc5().start(), E_OK);
  Tcc5().stop();
  TCC5.CNT = 3200u;

  uint32_t microseconds{0u};
  EXPECT_EQ(Tcc5().read(microseconds), E_OK);

  EXPECT_EQ(microseconds, 100u);
}

TEST_F(TimerTest, EnablePwmOnChannelADrivesPc4) {
  ASSERT_EQ(Tcc5().init(1000u), E_OK);

  EXPECT_EQ(Tcc5().enablePwm<Tc5::TIMER_CHANNEL_A>(512u), E_OK);

  EXPECT_EQ(PORTC.DIRSET, Bits::ToMask(4));
  EXPECT_EQ(ChannelMode(TCC5, 0), TC45_CCAMODE_COMP_gc);
  EXPECT_EQ(ChannelMode(TCC5, 1), TC45_CCAMODE_DISABLE_gc);
  EXPECT_EQ(TCC5.CCABUF, 8000u);
}

TEST_F(TimerTest, EnablePwmOnChannelBDrivesPc5) {
  ASSERT_EQ(Tcc5().init(1000u), E_OK);

  EXPECT_EQ(Tcc5().enablePwm<Tc5::TIMER_CHANNEL_B>(256u), E_OK);

  EXPECT_EQ(PORTC.DIRSET, Bits::ToMask(5));
  EXPECT_EQ(ChannelMode(TCC5, 0), TC45_CCAMODE_DISABLE_gc);
  EXPECT_EQ(ChannelMode(TCC5, 1), TC45_CCAMODE_COMP_gc);
  EXPECT_EQ(TCC5.CCBBUF, 4000u);
  EXPECT_EQ(TCC5.CCABUF, 0u);
}

TEST_F(TimerTest, EnablePwmAboveTheMaximumIsRefusedAndChangesNothing) {
  ASSERT_EQ(Tcc5().init(1000u), E_OK);

  EXPECT_EQ(Tcc5().enablePwm<Tc5::TIMER_CHANNEL_A>(Tc5::PwmDutyMax + 1u),
            E_NOT_OK);

  EXPECT_EQ(PORTC.DIRSET, 0u);
  EXPECT_EQ(ChannelMode(TCC5, 0), TC45_CCAMODE_DISABLE_gc);
  EXPECT_EQ(TCC5.CCABUF, 0u);
}

TEST_F(TimerTest, PwmDutyFromZeroToTheMaximum) {
  ASSERT_EQ(Tcc5().init(1000u), E_OK);

  EXPECT_EQ(Tcc5().enablePwm<Tc5::TIMER_CHANNEL_A>(0u), E_OK);
  EXPECT_EQ(TCC5.CCABUF, 0u);
  EXPECT_EQ(Tcc5().setPwmDuty<Tc5::TIMER_CHANNEL_A>(Tc5::PwmDutyMax), E_OK);
  EXPECT_EQ(TCC5.CCABUF, 15984u);
}

TEST_F(TimerTest, SetPwmDutyGoesThroughTheBuffer) {
  ASSERT_EQ(Tcc5().init(1000u), E_OK);
  ASSERT_EQ(Tcc5().enablePwm<Tc5::TIMER_CHANNEL_A>(512u), E_OK);

  EXPECT_EQ(Tcc5().setPwmDuty<Tc5::TIMER_CHANNEL_A>(256u), E_OK);

  EXPECT_EQ(TCC5.CCABUF, 4000u);
  EXPECT_EQ(TCC5.CCA, 0u);
}

TEST_F(TimerTest, SetPwmDutyAboveTheMaximumIsRefusedAndChangesNothing) {
  ASSERT_EQ(Tcc5().init(1000u), E_OK);
  ASSERT_EQ(Tcc5().enablePwm<Tc5::TIMER_CHANNEL_B>(512u), E_OK);

  EXPECT_EQ(Tcc5().setPwmDuty<Tc5::TIMER_CHANNEL_B>(Tc5::PwmDutyMax + 1u),
            E_NOT_OK);

  EXPECT_EQ(TCC5.CCBBUF, 8000u);
}

TEST_F(TimerTest, DisablePwmLeavesThePinAnOutput) {
  ASSERT_EQ(Tcc5().init(1000u), E_OK);
  ASSERT_EQ(Tcc5().enablePwm<Tc5::TIMER_CHANNEL_A>(512u), E_OK);
  PORTC.DIRSET = 0u;

  EXPECT_EQ(Tcc5().disablePwm<Tc5::TIMER_CHANNEL_A>(), E_OK);

  EXPECT_EQ(ChannelMode(TCC5, 0), TC45_CCAMODE_DISABLE_gc);
  EXPECT_EQ(PORTC.DIRSET, 0u);
}

TEST_F(TimerTest, SetPeriodKeepsTheDutyOfEachChannel) {
  ASSERT_EQ(Tcc5().init(1000u), E_OK);
  ASSERT_EQ(Tcc5().enablePwm<Tc5::TIMER_CHANNEL_A>(512u), E_OK);
  ASSERT_EQ(Tcc5().enablePwm<Tc5::TIMER_CHANNEL_B>(256u), E_OK);

  EXPECT_EQ(Tcc5().setPeriod(2000u), E_OK);

  EXPECT_EQ(TCC5.PERBUF, 32000u);
  EXPECT_EQ(TCC5.CCABUF, 16000u);
  EXPECT_EQ(TCC5.CCBBUF, 8000u);
}

TEST_F(TimerTest, StartLoadsTheCompareValuesAtOnce) {
  ASSERT_EQ(Tcc5().init(1000u), E_OK);
  ASSERT_EQ(Tcc5().enablePwm<Tc5::TIMER_CHANNEL_A>(512u), E_OK);
  ASSERT_EQ(Tcc5().enablePwm<Tc5::TIMER_CHANNEL_B>(256u), E_OK);

  EXPECT_EQ(Tcc5().start(), E_OK);

  EXPECT_EQ(TCC5.CCA, 8000u);
  EXPECT_EQ(TCC5.CCB, 4000u);
}

#if (TIMER_USE_TCC4 == STD_ON)

using Tc4 = Timer<TimerC4>;

TEST_F(TimerTest, Tcc4DrivesItsFourChannelsOnPc0ToPc3) {
  Tc4& tcc4{Tc4::getInstance()};
  ASSERT_EQ(tcc4.init(1000u), E_OK);

  EXPECT_EQ(tcc4.enablePwm<Tc4::TIMER_CHANNEL_A>(128u), E_OK);
  EXPECT_EQ(PORTC.DIRSET, Bits::ToMask(0));
  EXPECT_EQ(tcc4.enablePwm<Tc4::TIMER_CHANNEL_B>(256u), E_OK);
  EXPECT_EQ(PORTC.DIRSET, Bits::ToMask(1));
  EXPECT_EQ(tcc4.enablePwm<Tc4::TIMER_CHANNEL_C>(512u), E_OK);
  EXPECT_EQ(PORTC.DIRSET, Bits::ToMask(2));
  EXPECT_EQ(tcc4.enablePwm<Tc4::TIMER_CHANNEL_D>(Tc4::PwmDutyMax), E_OK);
  EXPECT_EQ(PORTC.DIRSET, Bits::ToMask(3));

  for (unsigned channel{0u}; channel < 4u; ++channel) {
    SCOPED_TRACE(channel);
    EXPECT_EQ(ChannelMode(TCC4, channel), TC45_CCAMODE_COMP_gc);
  }
  EXPECT_EQ(TCC4.CCABUF, 2000u);
  EXPECT_EQ(TCC4.CCBBUF, 4000u);
  EXPECT_EQ(TCC4.CCCBUF, 8000u);
  EXPECT_EQ(TCC4.CCDBUF, 15984u);
  EXPECT_EQ(TCC5.CTRLE, 0u);
}

TEST_F(TimerTest, Tcc4SetPeriodKeepsTheDutyOfAllFourChannels) {
  Tc4& tcc4{Tc4::getInstance()};
  ASSERT_EQ(tcc4.init(1000u), E_OK);
  ASSERT_EQ(tcc4.enablePwm<Tc4::TIMER_CHANNEL_C>(512u), E_OK);
  ASSERT_EQ(tcc4.enablePwm<Tc4::TIMER_CHANNEL_D>(256u), E_OK);

  EXPECT_EQ(tcc4.setPeriod(2000u), E_OK);
  EXPECT_EQ(tcc4.start(), E_OK);

  EXPECT_EQ(TCC4.CCCBUF, 16000u);
  EXPECT_EQ(TCC4.CCDBUF, 8000u);
  EXPECT_EQ(TCC4.CCC, 16000u);
  EXPECT_EQ(TCC4.CCD, 8000u);
}

TEST_F(TimerTest, Tcc4OverflowCallsItsOwnCallback) {
  ASSERT_EQ(Tc4::getInstance().init(1000u, OnTick), E_OK);
  ASSERT_EQ(Tcc5().init(1000u, OnOtherTick), E_OK);

  TCC4_OVF_vect();

  EXPECT_EQ(TCC4.INTFLAGS, TC4_OVFIF_bm);
  EXPECT_EQ(ticks, 1);
  EXPECT_EQ(other_ticks, 0);
}

#endif

}  // namespace
