// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

#include "xmega_timer/Timer.h"

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stdint.h>
#include <util/atomic.h>

#include "xmega_timer/Bit.h"
#include "xmega_timer/Error.h"
#include "xmega_timer/Result.h"
#include "xmega_timer/Status.h"
#include "xmega_timer/Tcc4.h"
#include "xmega_timer/Tcc5.h"
#include "xmega_timer/Utility/array.h"

namespace xmega_timer {
namespace {

using internal::ClockSelect;

// Timer cycles per half a microsecond: the counter runs back after TOP, and
// the interrupt is at BOTTOM, so a period is twice the cycles of TOP.
constexpr uint32_t kCyclesPerHalfMicrosecond{
  internal::kCyclesPerMicrosecond / 2u};

// The steps of a duty, from 0 to kPwmDutyMax: 10 bits.
constexpr uint32_t kDutySteps{1024u};

// A prescaler: its clock select, and what it divides the clock by as a shift.
struct Prescaler {
  uint8_t clock_select;
  uint8_t shift;
};

// From the smallest to the largest. In flash (AVR-1): a table that is not would
// be copied into RAM at start. The divisors are powers of 2, so that no
// division of 32 bits, a call into libgcc of 68 bytes, divides by them.
constexpr internal::array<Prescaler, 7u> kPrescalers PROGMEM{{
    {static_cast<uint8_t>(ClockSelect::kDivide1), 0u},
    {static_cast<uint8_t>(ClockSelect::kDivide2), 1u},
    {static_cast<uint8_t>(ClockSelect::kDivide4), 2u},
    {static_cast<uint8_t>(ClockSelect::kDivide8), 3u},
    {static_cast<uint8_t>(ClockSelect::kDivide64), 6u},
    {static_cast<uint8_t>(ClockSelect::kDivide256), 8u},
    {static_cast<uint8_t>(ClockSelect::kDivide1024), 10u},
}};

// The 16 bits of the counter, and those above them, of a count of cycles
// divided by a prescaler.
constexpr uint32_t kCounterBits{0x0000FFFFu};
constexpr uint32_t kBeyondCounterBits{0xFFFF0000u};

uint8_t ShiftOf(uint8_t index) {
  return pgm_read_byte(&kPrescalers[index].shift);
}

ClockSelect ClockSelectOf(uint8_t index) {
  return static_cast<ClockSelect>(
      pgm_read_byte(&kPrescalers[index].clock_select));
}

// The place of the smallest prescaler that brings `cycles` below the counter's
// 16 bits. The largest is not asked: Timer::kPeriodMax is what it brings below
// them.
uint8_t SmallestPrescaler(uint32_t cycles) {
  uint8_t const largest{static_cast<uint8_t>(kPrescalers.size() - 1u)};
  for (uint8_t i{0u}; i < largest; ++i) {
    uint32_t const beyond{
        Bit<uint32_t>::ReadBitGroup(cycles, kBeyondCounterBits, ShiftOf(i))};
    if (beyond == 0u) {
      return i;
    }
  }
  return largest;
}

}  // namespace

template <typename Traits>
Status Timer<Traits>::Begin(uint32_t microseconds, Callback callback) {
  if (state_ != State::kNotBegun) {
    return Error::kBegunAlready;
  }
  // Begun from here on, so that SetPeriod() below takes the period; before
  // Start() it restarts nothing.
  state_ = State::kBegun;
  // The core leaves TC4 and TC5 in byte mode with a running clock for
  // analogWrite(), so stop it and start from the reset values rather than
  // only changing the waveform mode.
  SetClockSelect(ClockSelect::kOff);
  Traits::Registers().CTRLGSET = TC45_CMD_RESET_gc;
  // Dual-slope, the overflow interrupt at BOTTOM.
  Bits::WriteBitGroup(Traits::Registers().CTRLB, TC4_WGMODE_gm,
                      TC45_WGMODE_DSBOTTOM_gc);
  Status const period{SetPeriod(microseconds)};
  if (callback != nullptr) {
    EnableOverflowInterrupt(callback);
  }
  return period;
}

template <typename Traits>
Status Timer<Traits>::SetPeriod(uint32_t microseconds) {
  // Before Begin() the timer may still be the core's, running analogWrite().
  if (state_ == State::kNotBegun) {
    return Error::kNotBegun;
  }
  bool const in_range{(microseconds > 0u) && (microseconds <= kPeriodMax)};
  if (!in_range) {
    return Error::kPeriodOutOfRange;
  }
  uint32_t const cycles{kCyclesPerHalfMicrosecond * microseconds};
  PrescalerIndex const prescaler{SmallestPrescaler(cycles)};
  bool const new_prescaler{prescaler != prescaler_};
  prescaler_ = prescaler;

  // A 16-bit write goes through the shared TEMP register, so an interrupt
  // touching another 16-bit register of this timer must not get in between.
  // Through PERBUF, not PER: in dual-slope mode a TOP written below the counter
  // in the middle of a period is not met until the counter has gone round
  // 0xFFFF, one period of up to several seconds where a shorter one was asked
  // for. The buffer is taken over at BOTTOM. The compare values go with it,
  // computed again from their duty, so that the PWM keeps its ratio across the
  // change and takes the new period at the same BOTTOM.
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    top_ = static_cast<uint16_t>(
        Bit<uint32_t>::ReadBitGroup(cycles, kCounterBits, ShiftOf(prescaler)));
    Traits::Registers().PERBUF = top_;
    WriteCompareValues(ChannelIndex<0u>{}, Target::kBuffer);
  }
  if (new_prescaler) {
    TakeOverPrescaler();
  }
  return Error::kOk;
}

template <typename Traits>
Status Timer<Traits>::Start() {
  if (state_ == State::kNotBegun) {
    return Error::kNotBegun;
  }
  RestartCounting(ClockSelectOf(prescaler_));
  state_ = State::kRunning;
  return Error::kOk;
}

template <typename Traits>
void Timer<Traits>::Stop() {
  // Before Begin() the timer may still be the core's, running analogWrite().
  if (state_ == State::kNotBegun) {
    return;
  }
  SetClockSelect(ClockSelect::kOff);
  if (state_ == State::kRunning) {
    state_ = State::kStopped;
  }
}

template <typename Traits>
Status Timer<Traits>::Resume() {
  if (state_ == State::kNotBegun) {
    return Error::kNotBegun;
  }
  if (state_ != State::kStopped) {
    return Error::kNotStopped;
  }
  SetClockSelect(ClockSelectOf(prescaler_));
  state_ = State::kRunning;
  return Error::kOk;
}

template <typename Traits>
Status Timer<Traits>::AttachInterrupt(Callback callback) {
  if (state_ == State::kNotBegun) {
    return Error::kNotBegun;
  }
  if (callback == nullptr) {
    return Error::kNoCallback;
  }
  EnableOverflowInterrupt(callback);
  return Error::kOk;
}

template <typename Traits>
void Timer<Traits>::DetachInterrupt() {
  if (state_ == State::kNotBegun) {
    return;
  }
  Bits::WriteBitGroup(Traits::Registers().INTCTRLA, TC4_OVFINTLVL_gm,
                      TC45_OVFINTLVL_OFF_gc);
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { callback_ = nullptr; }
}

template <typename Traits>
Result<uint32_t> Timer<Traits>::Read() const {
  if (state_ == State::kNotBegun) {
    return Error::kNotBegun;
  }
  if (state_ == State::kBegun) {
    return Error::kNotStarted;
  }
  uint32_t count{0u};
  uint32_t top{0u};
  bool down{false};
  // Counter and direction have to come from the same instant, or a turn at
  // TOP in between reads as a whole period too much.
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    count = Traits::Registers().CNT;
    top = Traits::Registers().PER;
    down = IsCountingDown();
  }
  // Counting down, the way back from TOP adds to the way up.
  uint32_t const cycles{down ? ((top - count) + top) : count};
  return ToMicroseconds(cycles);
}

template <typename Traits>
void Timer<Traits>::HandleOverflow() {
  Traits::Registers().INTFLAGS = TC4_OVFIF_bm;
  if (callback_ != nullptr) {
    callback_();
  }
}

template <typename Traits>
uint16_t Timer<Traits>::CompareValue(uint16_t duty) const {
  uint32_t const share{static_cast<uint32_t>(top_) * duty};
  return static_cast<uint16_t>(share / kDutySteps);
}

// Multiplied before it is divided: the other way round the truncation is
// scaled up by the prescaler, and at 1024 Read() moved in steps of a
// millisecond where a tick is 32 us. 2 * 0xFFFF * 1024 is still well inside
// 32 bits.
template <typename Traits>
uint32_t Timer<Traits>::ToMicroseconds(uint32_t cycles) const {
  // The divisor is a power of 2: the mask of the bit of its shift.
  uint16_t const divisor{Bit<uint16_t>::ToMask(ShiftOf(prescaler_))};
  return (cycles * divisor) / internal::kCyclesPerMicrosecond;
}

// CTRLA has no buffer: a new prescaler written at once would count the rest of
// the period at the new rate to the old TOP, up to 1024 times as long or as
// short. So the counter starts again from 0 instead, with the new period at
// once; a stopped timer stays stopped, and Resume() begins that period. Before
// Start() the clock is off, and Start() loads everything.
template <typename Traits>
void Timer<Traits>::TakeOverPrescaler() {
  if (state_ == State::kRunning) {
    RestartCounting(ClockSelectOf(prescaler_));
  } else if (state_ == State::kStopped) {
    RestartCounting(ClockSelect::kOff);
  } else {
    // Begun but not started: the clock is off, and Start() loads everything.
  }
}

// Ends with the clock it is given, so that a stopped timer is set back to 0
// without counting a single cycle.
template <typename Traits>
void Timer<Traits>::RestartCounting(ClockSelect clock_select) {
  SetClockSelect(ClockSelect::kOff);
  LoadPeriodAndCompare();
  // RESTART clears the counter and the direction, so that the first period is
  // a whole one.
  Traits::Registers().CTRLGSET = TC45_CMD_RESTART_gc;
  SetClockSelect(clock_select);
}

// Straight into PER and CCx, past the buffers, which is only safe while the
// timer has no clock. Without it the first period after Start() would run on
// whatever the registers held: the buffers are taken over at BOTTOM, and the
// first BOTTOM after RESTART is a whole period away, with CCx at the 0 that
// the RESET of Begin() left there. Not a forced UPDATE before the RESTART
// instead: both are written to CTRLGSET, and an UPDATE still pending there
// or'ed with RESTART reads as RESET.
template <typename Traits>
void Timer<Traits>::LoadPeriodAndCompare() {
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    Traits::Registers().PER = top_;
    WriteCompareValues(ChannelIndex<0u>{}, Target::kNow);
  }
}

// The callback is not nullptr: AttachInterrupt() refuses it, and Begin() does
// not call this then.
template <typename Traits>
void Timer<Traits>::EnableOverflowInterrupt(Callback callback) {
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { callback_ = callback; }
  // The flag is set at every BOTTOM whether the interrupt is on or not, so
  // one left over from while it was off would call the new callback at once.
  // Only then: with the interrupt already on, a set flag is a real overflow
  // about to be served.
  bool const off{Bits::ReadBitGroup(Traits::Registers().INTCTRLA,
                                    TC4_OVFINTLVL_gm) == TC45_OVFINTLVL_OFF_gc};
  if (off) {
    Traits::Registers().INTFLAGS = TC4_OVFIF_bm;
  }
  // Low level, so that the core's millis() on TCD5, at high level, keeps
  // interrupting the callback.
  Bits::WriteBitGroup(Traits::Registers().INTCTRLA, TC4_OVFINTLVL_gm,
                      TC45_OVFINTLVL_LO_gc);
  Bits::SetBits(PMIC.CTRL, PMIC_LOLVLEN_bm);
}

// Into the buffer register, so that the new compare value takes effect at the
// next BOTTOM and a running PWM never sees half a period of each.
template <typename Traits>
void Timer<Traits>::StoreDuty(uint8_t index, uint16_t duty,
                              register16_t& buffer) {
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    duties_[index] = duty;
    buffer = CompareValue(duty);
  }
}

template <typename Traits>
void Timer<Traits>::SetClockSelect(ClockSelect clock_select) {
  Bits::WriteBitGroup(Traits::Registers().CTRLA, TC4_CLKSEL_gm,
                      static_cast<uint8_t>(clock_select));
}

template <typename Traits>
bool Timer<Traits>::IsCountingDown() {
  return Bits::ReadBitGroup(Traits::Registers().CTRLGSET, TC4_DIR_bm) != 0u;
}

// Called with interrupts off: a 16-bit write goes through the TEMP register.
template <typename Traits>
template <uint8_t Index>
void Timer<Traits>::WriteCompareValues(ChannelIndex<Index> /*index*/,
                                       Target target) {
  using ChannelRegisters = CompareChannel<static_cast<Channel>(Index)>;
  uint16_t const value{CompareValue(duties_[Index])};
  if (target == Target::kBuffer) {
    ChannelRegisters::Buffer() = value;
  } else {
    ChannelRegisters::Compare() = value;
  }
  WriteCompareValues(ChannelIndex<static_cast<uint8_t>(Index + 1u)>{}, target);
}

#if XMEGATIMER_USE_TCC4
template class Timer<Tcc4Traits>;
#endif

#if XMEGATIMER_USE_TCC5
template class Timer<Tcc5Traits>;
#endif

}  // namespace xmega_timer
