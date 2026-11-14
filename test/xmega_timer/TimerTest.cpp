// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Timer, the class template both timers are, through tcc5, against the mock of
// the registers at the 32 MHz of the ATxmega32E5. What is TCC5's own, its pins
// and its interrupt vector, is in Tcc5Test.cpp.

#include "xmega_timer/Timer.h"

#include <avr/io.h>
#include <gtest/gtest.h>
#include <stdint.h>

#include <ostream>
#include <string>
#include <vector>

#include "test/fixture.h"
#include "test/printers.h"  // IWYU pragma: keep
#include "xmega_timer/Error.h"
#include "xmega_timer/Result.h"
#include "xmega_timer/Tcc5.h"

namespace xmega_timer {
namespace {

using TimerTest = TimerFixture;

TEST_F(TimerTest, BeginStopsAndResetsTheTimerAndSetsDualSlopeAtBottom) {
  // The core leaves the timer running for analogWrite().
  TCC5.CTRLA = TC45_CLKSEL_DIV64_gc;

  EXPECT_EQ(tcc5.Begin(1000u), Error::kOk);

  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_OFF_gc);
  EXPECT_EQ(TCC5.CTRLGSET.Commands(), std::vector<uint8_t>{TC45_CMD_RESET_gc});
  EXPECT_EQ(Bits::ReadBitGroup(TCC5.CTRLB, TC4_WGMODE_gm),
            TC45_WGMODE_DSBOTTOM_gc);
  EXPECT_EQ(TCC5.PERBUF, 16000u);
}

TEST_F(TimerTest, BeginASecondTimeIsRefusedAndChangesNothing) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);

  EXPECT_EQ(tcc5.Begin(2000u, OnTick), Error::kBegunAlready);

  EXPECT_EQ(TCC5.PERBUF, 16000u);
  EXPECT_EQ(Bits::ReadBitGroup(TCC5.INTCTRLA, TC4_OVFINTLVL_gm),
            TC45_OVFINTLVL_OFF_gc);
}

TEST_F(TimerTest, BeginWithoutCallbackLeavesTheInterruptOff) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);

  EXPECT_EQ(Bits::ReadBitGroup(TCC5.INTCTRLA, TC4_OVFINTLVL_gm),
            TC45_OVFINTLVL_OFF_gc);
  EXPECT_EQ(PMIC.CTRL, 0u);
}

TEST_F(TimerTest, BeginWithCallbackEnablesTheInterruptAtLowLevel) {
  ASSERT_EQ(tcc5.Begin(1000u, OnTick), Error::kOk);

  EXPECT_EQ(Bits::ReadBitGroup(TCC5.INTCTRLA, TC4_OVFINTLVL_gm),
            TC45_OVFINTLVL_LO_gc);
  EXPECT_EQ(Bits::ReadBitGroup(PMIC.CTRL, PMIC_LOLVLEN_bm), PMIC_LOLVLEN_bm);
}

TEST_F(TimerTest, BeginWithAPeriodOutOfRangeIsRefusedButBegins) {
  EXPECT_EQ(tcc5.Begin(0u), Error::kPeriodOutOfRange);

  // A TOP of 0xFFFF and no prescaler.
  EXPECT_EQ(tcc5.Start(), Error::kOk);
  EXPECT_EQ(TCC5.PER, 0xFFFFu);
  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_DIV1_gc);
}

struct PeriodCase {
  char const* name;
  uint32_t microseconds;
  uint8_t clock_select;
  uint16_t top;
};

// By its name, as ctest names the test after it.
// NOLINTNEXTLINE(readability-identifier-naming)
void PrintTo(PeriodCase const& test_case, std::ostream* out) {
  *out << test_case.name;
}

class TimerPeriodTest : public TimerFixture,
                        public ::testing::WithParamInterface<PeriodCase> {};

TEST_P(TimerPeriodTest, SetPeriodChoosesTheSmallestPrescaler) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);

  EXPECT_EQ(tcc5.SetPeriod(GetParam().microseconds), Error::kOk);

  EXPECT_EQ(TCC5.PERBUF, GetParam().top);
  ASSERT_EQ(tcc5.Start(), Error::kOk);
  EXPECT_EQ(ClockSelect(TCC5), GetParam().clock_select);
  EXPECT_EQ(TCC5.PER, GetParam().top);
}

// 16 timer cycles per microsecond, as the counter goes up and down again.
INSTANTIATE_TEST_SUITE_P(
    Periods, TimerPeriodTest,
    ::testing::Values(
        PeriodCase{"Shortest", 1u, TC45_CLKSEL_DIV1_gc, 16u},
        PeriodCase{"Divide1", 1000u, TC45_CLKSEL_DIV1_gc, 16000u},
        PeriodCase{"LongestAt1", 4095u, TC45_CLKSEL_DIV1_gc, 65520u},
        PeriodCase{"ShortestAt2", 4096u, TC45_CLKSEL_DIV2_gc, 32768u},
        PeriodCase{"Divide4", 10000u, TC45_CLKSEL_DIV4_gc, 40000u},
        PeriodCase{"Divide8", 30000u, TC45_CLKSEL_DIV8_gc, 60000u},
        PeriodCase{"Divide64", 100000u, TC45_CLKSEL_DIV64_gc, 25000u},
        PeriodCase{"Divide256", 1000000u, TC45_CLKSEL_DIV256_gc, 62500u},
        PeriodCase{"Divide1024", 4000000u, TC45_CLKSEL_DIV1024_gc, 62500u},
        PeriodCase{"Longest", 4194303u, TC45_CLKSEL_DIV1024_gc, 65535u}),
    [](::testing::TestParamInfo<PeriodCase> const& case_info) {
      return std::string{case_info.param.name};
    });

TEST_F(TimerTest, SetPeriodBeforeBeginIsRefusedAndLeavesTheTimerToTheCore) {
  // The core leaves the timer running for analogWrite().
  TCC5.CTRLA = TC45_CLKSEL_DIV64_gc;

  EXPECT_EQ(tcc5.SetPeriod(2000u), Error::kNotBegun);

  // Its value after reset.
  EXPECT_EQ(TCC5.PERBUF, 0xFFFFu);
  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_DIV64_gc);
  EXPECT_TRUE(TCC5.CTRLGSET.Commands().empty());
}

TEST_F(TimerTest, SetPeriodOfZeroIsRefusedAndChangesNothing) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);

  EXPECT_EQ(tcc5.SetPeriod(0u), Error::kPeriodOutOfRange);

  EXPECT_EQ(TCC5.PERBUF, 16000u);
  ASSERT_EQ(tcc5.Start(), Error::kOk);
  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_DIV1_gc);
}

TEST_F(TimerTest, SetPeriodOneBeyondTheLongestIsRefusedAndChangesNothing) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);

  EXPECT_EQ(tcc5.SetPeriod(Tcc5::kPeriodMax + 1u), Error::kPeriodOutOfRange);

  EXPECT_EQ(TCC5.PERBUF, 16000u);
  ASSERT_EQ(tcc5.Start(), Error::kOk);
  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_DIV1_gc);
}

TEST_F(TimerTest, SetPeriodOfABegunTimerLeavesItsClockOff) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);

  EXPECT_EQ(tcc5.SetPeriod(10000u), Error::kOk);

  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_OFF_gc);
}

TEST_F(TimerTest,
       SetPeriodOfARunningTimerKeepingItsPrescalerGoesThroughTheBuffer) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);
  ASSERT_EQ(tcc5.Start(), Error::kOk);

  EXPECT_EQ(tcc5.SetPeriod(2000u), Error::kOk);

  EXPECT_EQ(TCC5.PERBUF, 32000u);
  EXPECT_EQ(TCC5.PER, 16000u);
  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_DIV1_gc);
  EXPECT_EQ(TCC5.CTRLGSET.Commands(),
            (std::vector<uint8_t>{TC45_CMD_RESET_gc, TC45_CMD_RESTART_gc}));
}

TEST_F(TimerTest, SetPeriodOfARunningTimerWithANewPrescalerRestartsItAtOnce) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);
  ASSERT_EQ(tcc5.Start(), Error::kOk);

  EXPECT_EQ(tcc5.SetPeriod(10000u), Error::kOk);

  EXPECT_EQ(TCC5.PER, 40000u);
  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_DIV4_gc);
  EXPECT_EQ(TCC5.CTRLGSET.Commands(),
            (std::vector<uint8_t>{TC45_CMD_RESET_gc, TC45_CMD_RESTART_gc,
                                  TC45_CMD_RESTART_gc}));
}

TEST_F(TimerTest, SetPeriodOfARunningTimerWithANewPrescalerLoadsTheDutyAtOnce) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);
  ASSERT_EQ(tcc5.EnablePwm<Channel::kA>(512u), Error::kOk);
  ASSERT_EQ(tcc5.Start(), Error::kOk);

  EXPECT_EQ(tcc5.SetPeriod(10000u), Error::kOk);

  EXPECT_EQ(TCC5.CCA, 20000u);
}

TEST_F(TimerTest,
       SetPeriodOfAStoppedTimerKeepingItsPrescalerGoesThroughTheBuffer) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);
  ASSERT_EQ(tcc5.Start(), Error::kOk);
  tcc5.Stop();

  EXPECT_EQ(tcc5.SetPeriod(2000u), Error::kOk);

  EXPECT_EQ(TCC5.PERBUF, 32000u);
  EXPECT_EQ(TCC5.PER, 16000u);
  EXPECT_EQ(TCC5.CTRLGSET.Commands(),
            (std::vector<uint8_t>{TC45_CMD_RESET_gc, TC45_CMD_RESTART_gc}));
}

TEST_F(TimerTest, SetPeriodOfAStoppedTimerWithANewPrescalerSetsItBackStopped) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);
  ASSERT_EQ(tcc5.Start(), Error::kOk);
  tcc5.Stop();

  EXPECT_EQ(tcc5.SetPeriod(10000u), Error::kOk);

  EXPECT_EQ(TCC5.PER, 40000u);
  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_OFF_gc);
  EXPECT_EQ(TCC5.CTRLGSET.Commands(),
            (std::vector<uint8_t>{TC45_CMD_RESET_gc, TC45_CMD_RESTART_gc,
                                  TC45_CMD_RESTART_gc}));
}

TEST_F(TimerTest, SetPeriodOfAStoppedTimerWithANewPrescalerLoadsTheDutyAtOnce) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);
  ASSERT_EQ(tcc5.EnablePwm<Channel::kA>(512u), Error::kOk);
  ASSERT_EQ(tcc5.Start(), Error::kOk);
  tcc5.Stop();

  EXPECT_EQ(tcc5.SetPeriod(10000u), Error::kOk);

  EXPECT_EQ(TCC5.CCA, 20000u);
}

TEST_F(TimerTest, SetPeriodKeepsTheDutyOfEachChannel) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);
  ASSERT_EQ(tcc5.EnablePwm<Channel::kA>(512u), Error::kOk);
  ASSERT_EQ(tcc5.EnablePwm<Channel::kB>(256u), Error::kOk);

  EXPECT_EQ(tcc5.SetPeriod(2000u), Error::kOk);

  EXPECT_EQ(TCC5.PERBUF, 32000u);
  EXPECT_EQ(TCC5.CCABUF, 16000u);
  EXPECT_EQ(TCC5.CCBBUF, 8000u);
}

TEST_F(TimerTest, StartBeforeBeginIsRefused) {
  EXPECT_EQ(tcc5.Start(), Error::kNotBegun);

  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_OFF_gc);
  EXPECT_TRUE(TCC5.CTRLGSET.Commands().empty());
}

TEST_F(TimerTest, StartLoadsThePeriodAndRestartsFromZeroCountingUp) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);
  TCC5.CTRLGSET.SetCountingDown(true);

  EXPECT_EQ(tcc5.Start(), Error::kOk);

  EXPECT_EQ(TCC5.PER, 16000u);
  EXPECT_EQ(TCC5.CTRLGSET.Commands(),
            (std::vector<uint8_t>{TC45_CMD_RESET_gc, TC45_CMD_RESTART_gc}));
  EXPECT_EQ(Bits::ReadBitGroup(TCC5.CTRLGSET, TC4_DIR_bm), 0u);
  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_DIV1_gc);
}

TEST_F(TimerTest, StartLoadsTheCompareValuesAtOnce) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);
  ASSERT_EQ(tcc5.EnablePwm<Channel::kA>(512u), Error::kOk);
  ASSERT_EQ(tcc5.EnablePwm<Channel::kB>(256u), Error::kOk);

  EXPECT_EQ(tcc5.Start(), Error::kOk);

  EXPECT_EQ(TCC5.CCA, 8000u);
  EXPECT_EQ(TCC5.CCB, 4000u);
}

TEST_F(TimerTest, StartOfAStoppedTimerRestartsItFromZero) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);
  ASSERT_EQ(tcc5.Start(), Error::kOk);
  tcc5.Stop();

  EXPECT_EQ(tcc5.Start(), Error::kOk);

  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_DIV1_gc);
  EXPECT_EQ(TCC5.CTRLGSET.Commands(),
            (std::vector<uint8_t>{TC45_CMD_RESET_gc, TC45_CMD_RESTART_gc,
                                  TC45_CMD_RESTART_gc}));
  // Running again, not stopped.
  EXPECT_EQ(tcc5.Resume(), Error::kNotStopped);
}

TEST_F(TimerTest, StopTakesTheClock) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);
  ASSERT_EQ(tcc5.Start(), Error::kOk);

  tcc5.Stop();

  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_OFF_gc);
}

TEST_F(TimerTest, StopOfATimerNotStartedLeavesNothingToResume) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);

  tcc5.Stop();

  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_OFF_gc);
  EXPECT_EQ(tcc5.Resume(), Error::kNotStopped);
}

TEST_F(TimerTest, StopBeforeBeginLeavesTheTimerToTheCore) {
  TCC5.CTRLA = TC45_CLKSEL_DIV64_gc;

  tcc5.Stop();

  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_DIV64_gc);
}

TEST_F(TimerTest, ResumeGoesOnWithThePrescalerWithoutRestart) {
  ASSERT_EQ(tcc5.Begin(10000u), Error::kOk);
  ASSERT_EQ(tcc5.Start(), Error::kOk);
  tcc5.Stop();

  EXPECT_EQ(tcc5.Resume(), Error::kOk);

  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_DIV4_gc);
  EXPECT_EQ(TCC5.CTRLGSET.Commands(),
            (std::vector<uint8_t>{TC45_CMD_RESET_gc, TC45_CMD_RESTART_gc}));
}

TEST_F(TimerTest, ResumeAfterANewPrescalerCountsTheNewPeriod) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);
  ASSERT_EQ(tcc5.Start(), Error::kOk);
  tcc5.Stop();
  ASSERT_EQ(tcc5.SetPeriod(10000u), Error::kOk);

  EXPECT_EQ(tcc5.Resume(), Error::kOk);

  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_DIV4_gc);
  EXPECT_EQ(TCC5.PER, 40000u);
}

TEST_F(TimerTest, ResumeBeforeBeginIsRefused) {
  EXPECT_EQ(tcc5.Resume(), Error::kNotBegun);

  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_OFF_gc);
}

TEST_F(TimerTest, ResumeOfATimerNotStartedIsRefused) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);

  EXPECT_EQ(tcc5.Resume(), Error::kNotStopped);

  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_OFF_gc);
}

TEST_F(TimerTest, ResumeOfARunningTimerIsRefused) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);
  ASSERT_EQ(tcc5.Start(), Error::kOk);

  EXPECT_EQ(tcc5.Resume(), Error::kNotStopped);

  EXPECT_EQ(ClockSelect(TCC5), TC45_CLKSEL_DIV1_gc);
}

TEST_F(TimerTest, AttachInterruptBeforeBeginIsRefused) {
  EXPECT_EQ(tcc5.AttachInterrupt(OnTick), Error::kNotBegun);

  EXPECT_EQ(Bits::ReadBitGroup(TCC5.INTCTRLA, TC4_OVFINTLVL_gm),
            TC45_OVFINTLVL_OFF_gc);
  EXPECT_EQ(PMIC.CTRL, 0u);
  tcc5.HandleOverflow();
  EXPECT_EQ(ticks, 0);
}

TEST_F(TimerTest, AttachInterruptOfNullptrIsRefused) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);

  EXPECT_EQ(tcc5.AttachInterrupt(nullptr), Error::kNoCallback);

  EXPECT_EQ(Bits::ReadBitGroup(TCC5.INTCTRLA, TC4_OVFINTLVL_gm),
            TC45_OVFINTLVL_OFF_gc);
}

TEST_F(TimerTest, AttachInterruptClearsAFlagFromWhileItWasOff) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);
  TCC5.INTFLAGS = 0u;

  EXPECT_EQ(tcc5.AttachInterrupt(OnTick), Error::kOk);

  EXPECT_EQ(TCC5.INTFLAGS, TC4_OVFIF_bm);
  EXPECT_EQ(Bits::ReadBitGroup(TCC5.INTCTRLA, TC4_OVFINTLVL_gm),
            TC45_OVFINTLVL_LO_gc);
}

TEST_F(TimerTest, AttachInterruptWhileOnLeavesAPendingOverflow) {
  ASSERT_EQ(tcc5.Begin(1000u, OnTick), Error::kOk);
  TCC5.INTFLAGS = 0u;

  EXPECT_EQ(tcc5.AttachInterrupt(OnOtherTick), Error::kOk);

  EXPECT_EQ(TCC5.INTFLAGS, 0u);
  tcc5.HandleOverflow();
  EXPECT_EQ(ticks, 0);
  EXPECT_EQ(other_ticks, 1);
}

TEST_F(TimerTest, DetachInterruptBeforeBeginLeavesTheTimerToTheCore) {
  TCC5.INTCTRLA = TC45_OVFINTLVL_LO_gc;

  tcc5.DetachInterrupt();

  EXPECT_EQ(Bits::ReadBitGroup(TCC5.INTCTRLA, TC4_OVFINTLVL_gm),
            TC45_OVFINTLVL_LO_gc);
}

TEST_F(TimerTest, DetachInterruptTurnsItOffAndForgetsTheCallback) {
  ASSERT_EQ(tcc5.Begin(1000u, OnTick), Error::kOk);

  tcc5.DetachInterrupt();

  EXPECT_EQ(Bits::ReadBitGroup(TCC5.INTCTRLA, TC4_OVFINTLVL_gm),
            TC45_OVFINTLVL_OFF_gc);
  tcc5.HandleOverflow();
  EXPECT_EQ(ticks, 0);
}

TEST_F(TimerTest, HandleOverflowClearsTheFlagAndCallsTheCallback) {
  ASSERT_EQ(tcc5.Begin(1000u, OnTick), Error::kOk);
  ASSERT_EQ(tcc5.Start(), Error::kOk);
  TCC5.INTFLAGS = 0u;

  tcc5.HandleOverflow();

  EXPECT_EQ(TCC5.INTFLAGS, TC4_OVFIF_bm);
  EXPECT_EQ(ticks, 1);
}

TEST_F(TimerTest, HandleOverflowWithoutCallbackClearsTheFlag) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);

  tcc5.HandleOverflow();

  EXPECT_EQ(TCC5.INTFLAGS, TC4_OVFIF_bm);
}

TEST_F(TimerTest, ReadBeforeBeginIsRefused) {
  Result<uint32_t> const elapsed{tcc5.Read()};

  EXPECT_EQ(elapsed.status(), Error::kNotBegun);
  EXPECT_EQ(elapsed.value(), 0u);
}

TEST_F(TimerTest, ReadOfATimerNotStartedIsRefused) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);

  Result<uint32_t> const elapsed{tcc5.Read()};

  EXPECT_EQ(elapsed.status(), Error::kNotStarted);
  EXPECT_EQ(elapsed.value(), 0u);
}

TEST_F(TimerTest, ReadCountingUpIsTheCounter) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);
  ASSERT_EQ(tcc5.Start(), Error::kOk);
  TCC5.CNT = 1000u;

  Result<uint32_t> const elapsed{tcc5.Read()};

  ASSERT_EQ(elapsed.status(), Error::kOk);
  // 1000 cycles of 32 MHz.
  EXPECT_EQ(elapsed.value(), 31u);
}

TEST_F(TimerTest, ReadCountingDownAddsTheWayBackFromTop) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);
  ASSERT_EQ(tcc5.Start(), Error::kOk);
  TCC5.CNT = 1000u;
  TCC5.CTRLGSET.SetCountingDown(true);

  Result<uint32_t> const elapsed{tcc5.Read()};

  ASSERT_EQ(elapsed.status(), Error::kOk);
  // 16000 up and 15000 down again.
  EXPECT_EQ(elapsed.value(), 968u);
}

TEST_F(TimerTest, ReadAtTheLargestPrescalerCountsEachTick) {
  ASSERT_EQ(tcc5.Begin(4000000u), Error::kOk);
  ASSERT_EQ(tcc5.Start(), Error::kOk);
  TCC5.CNT = 3u;

  Result<uint32_t> const elapsed{tcc5.Read()};

  ASSERT_EQ(elapsed.status(), Error::kOk);
  // 32 us a tick, not rounded to a millisecond.
  EXPECT_EQ(elapsed.value(), 96u);
}

TEST_F(TimerTest, ReadOfAStoppedTimer) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);
  ASSERT_EQ(tcc5.Start(), Error::kOk);
  tcc5.Stop();
  TCC5.CNT = 3200u;

  Result<uint32_t> const elapsed{tcc5.Read()};

  ASSERT_EQ(elapsed.status(), Error::kOk);
  EXPECT_EQ(elapsed.value(), 100u);
}

struct ReadCase {
  char const* name;
  uint32_t period;
  uint8_t clock_select;
  uint32_t microseconds;
};

// By its name, as ctest names the test after it.
// NOLINTNEXTLINE(readability-identifier-naming)
void PrintTo(ReadCase const& test_case, std::ostream* out) {
  *out << test_case.name;
}

class TimerReadTest : public TimerFixture,
                      public ::testing::WithParamInterface<ReadCase> {};

TEST_P(TimerReadTest, ReadScalesTheCounterByThePrescaler) {
  ASSERT_EQ(tcc5.Begin(GetParam().period), Error::kOk);
  ASSERT_EQ(tcc5.Start(), Error::kOk);
  ASSERT_EQ(ClockSelect(TCC5), GetParam().clock_select);
  TCC5.CNT = 3200u;

  Result<uint32_t> const elapsed{tcc5.Read()};

  ASSERT_EQ(elapsed.status(), Error::kOk);
  EXPECT_EQ(elapsed.value(), GetParam().microseconds);
}

// 3200 ticks, 100 us at 32 MHz times the prescaler.
INSTANTIATE_TEST_SUITE_P(
    Prescalers, TimerReadTest,
    ::testing::Values(
        ReadCase{"Divide2", 4096u, TC45_CLKSEL_DIV2_gc, 200u},
        ReadCase{"Divide4", 10000u, TC45_CLKSEL_DIV4_gc, 400u},
        ReadCase{"Divide8", 30000u, TC45_CLKSEL_DIV8_gc, 800u},
        ReadCase{"Divide64", 100000u, TC45_CLKSEL_DIV64_gc, 6400u},
        ReadCase{"Divide256", 1000000u, TC45_CLKSEL_DIV256_gc, 25600u}),
    [](::testing::TestParamInfo<ReadCase> const& case_info) {
      return std::string{case_info.param.name};
    });

TEST_F(TimerTest, EnablePwmBeforeBeginIsRefusedAndChangesNothing) {
  EXPECT_EQ(tcc5.EnablePwm<Channel::kA>(512u), Error::kNotBegun);

  EXPECT_EQ(PORTC.DIRSET, 0u);
  EXPECT_EQ(Bits::ReadBitGroup(TCC5.CTRLE, TC5_CCAMODE_gm),
            TC45_CCAMODE_DISABLE_gc);
  EXPECT_EQ(TCC5.CCABUF, 0u);
}

TEST_F(TimerTest, EnablePwmAboveTheLargestDutyIsRefusedAndChangesNothing) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);

  EXPECT_EQ(tcc5.EnablePwm<Channel::kA>(Tcc5::kPwmDutyMax + 1u),
            Error::kDutyOutOfRange);

  EXPECT_EQ(PORTC.DIRSET, 0u);
  EXPECT_EQ(Bits::ReadBitGroup(TCC5.CTRLE, TC5_CCAMODE_gm),
            TC45_CCAMODE_DISABLE_gc);
  EXPECT_EQ(TCC5.CCABUF, 0u);
}

TEST_F(TimerTest, EnablePwmOfTheLargestDutyIsTheWholePeriodBarOneStep) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);

  EXPECT_EQ(tcc5.EnablePwm<Channel::kA>(Tcc5::kPwmDutyMax), Error::kOk);

  EXPECT_EQ(TCC5.CCABUF, 15984u);
}

TEST_F(TimerTest, EnablePwmOfZeroIsNoPulse) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);

  EXPECT_EQ(tcc5.EnablePwm<Channel::kA>(0u), Error::kOk);

  EXPECT_EQ(TCC5.CCABUF, 0u);
}

TEST_F(TimerTest, SetPwmDutyGoesThroughTheBuffer) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);
  ASSERT_EQ(tcc5.EnablePwm<Channel::kA>(512u), Error::kOk);

  EXPECT_EQ(tcc5.SetPwmDuty<Channel::kA>(256u), Error::kOk);

  EXPECT_EQ(TCC5.CCABUF, 4000u);
  EXPECT_EQ(TCC5.CCA, 0u);
}

TEST_F(TimerTest, SetPwmDutyBeforeBeginIsRefusedAndChangesNothing) {
  EXPECT_EQ(tcc5.SetPwmDuty<Channel::kA>(512u), Error::kNotBegun);

  EXPECT_EQ(TCC5.CCABUF, 0u);
}

TEST_F(TimerTest, SetPwmDutyAboveTheLargestIsRefusedAndChangesNothing) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);
  ASSERT_EQ(tcc5.EnablePwm<Channel::kB>(512u), Error::kOk);

  EXPECT_EQ(tcc5.SetPwmDuty<Channel::kB>(Tcc5::kPwmDutyMax + 1u),
            Error::kDutyOutOfRange);

  EXPECT_EQ(TCC5.CCBBUF, 8000u);
}

TEST_F(TimerTest, DisablePwmBeforeBeginLeavesTheTimerToTheCore) {
  // The core drives the channel for analogWrite().
  TCC5.CTRLE = TC45_CCAMODE_COMP_gc;

  tcc5.DisablePwm<Channel::kA>();

  EXPECT_EQ(Bits::ReadBitGroup(TCC5.CTRLE, TC5_CCAMODE_gm),
            TC45_CCAMODE_COMP_gc);
}

TEST_F(TimerTest, DisablePwmLeavesThePinAnOutput) {
  ASSERT_EQ(tcc5.Begin(1000u), Error::kOk);
  ASSERT_EQ(tcc5.EnablePwm<Channel::kA>(512u), Error::kOk);
  PORTC.DIRSET = 0u;

  tcc5.DisablePwm<Channel::kA>();

  EXPECT_EQ(Bits::ReadBitGroup(TCC5.CTRLE, TC5_CCAMODE_gm),
            TC45_CCAMODE_DISABLE_gc);
  EXPECT_EQ(PORTC.DIRSET, 0u);
}

}  // namespace
}  // namespace xmega_timer
