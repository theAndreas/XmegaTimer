// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Timer: a TC4 or TC5 of the ATxmega E5 as a periodic interrupt, a PWM
 *        on each of its compare channels and a time readout.
 */

#ifndef XMEGATIMER_SRC_XMEGA_TIMER_TIMER_H_
#define XMEGATIMER_SRC_XMEGA_TIMER_TIMER_H_

#include <avr/io.h>
#include <stdint.h>

#include "xmega_timer/Bit.h"
#include "xmega_timer/Result.h"
#include "xmega_timer/Status.h"
#include "xmega_timer/Utility/array.h"
#include "xmega_timer/attributes.h"

namespace xmega_timer {

/*! \brief The function the overflow interrupt calls. */
using Callback = void (*)();

/*! \brief The compare channels of a timer; each drives one pin with a PWM. */
enum class Channel : uint8_t {
  kA, /*!< Channel A. */
  kB, /*!< Channel B. */
  kC, /*!< Channel C, TC4 only. */
  kD  /*!< Channel D, TC4 only. */
};

namespace internal {

// All timers of the XMEGA count in 16 bits.
constexpr uint32_t kCounterValues{65536u};
constexpr uint16_t kLargestTop{0xFFFFu};
constexpr uint32_t kMicrosecondsPerSecond{1000000u};
// Whole cycles of the timer's clock per microsecond; Timer asserts that there
// are, and half as many per half a microsecond.
constexpr uint32_t kCyclesPerMicrosecond{F_CPU / kMicrosecondsPerSecond};
// The largest prescaler.
constexpr uint32_t kLargestDivisor{1024u};
// The microseconds of the largest TOP at the largest prescaler, counted up and
// down again.
constexpr uint32_t kLongestPeriod{(kCounterValues / kCyclesPerMicrosecond) *
                                  kLargestDivisor * 2u};

// The values of the clock select bits CLKSEL of CTRLA.
enum class ClockSelect : uint8_t {
  kOff = TC45_CLKSEL_OFF_gc,
  kDivide1 = TC45_CLKSEL_DIV1_gc,
  kDivide2 = TC45_CLKSEL_DIV2_gc,
  kDivide4 = TC45_CLKSEL_DIV4_gc,
  kDivide8 = TC45_CLKSEL_DIV8_gc,
  kDivide64 = TC45_CLKSEL_DIV64_gc,
  kDivide256 = TC45_CLKSEL_DIV256_gc,
  kDivide1024 = TC45_CLKSEL_DIV1024_gc
};

}  // namespace internal

/*!
 * \brief A TC4 or TC5 as a periodic interrupt, a PWM on each of its compare
 *        channels and a time readout.
 *
 * There is one of each timer, tcc4 and tcc5, the objects their interrupts
 * serve; a Timer of a sketch's own would get no interrupt, and none is copied.
 * Begin() first, then Start(). The
 * timer counts dual-slope and interrupts at BOTTOM, so that a period spans
 * twice the timer cycles of TOP and the PWM is phase-correct.
 *
 * \code{.cpp}
 * void setup() {
 *   xmega_timer::Status status{xmega_timer::tcc5.Begin(1000u, OnTick)};
 *   if (status.IsOk()) {
 *     status = xmega_timer::tcc5.EnablePwm<xmega_timer::Channel::kA>(512u);
 *   }
 *   if (status.IsOk()) {
 *     status = xmega_timer::tcc5.Start();
 *   }
 * }
 * \endcode
 *
 * A call that can refuse returns a Status, or a Result, and leaves the timer as
 * it was, unless its block says otherwise.
 *
 * About 800 bytes of flash with one channel of PWM, as Basics measures it on
 * the ATxmega32E5, and 10 bytes of RAM for tcc5, 14 for tcc4.
 *
 * \tparam Traits What differs between the timers: Tcc4Traits or Tcc5Traits,
 *                their registers, their compare channels and the pins those
 *                drive. A channel the timer does not have fails to compile.
 */
template <typename Traits>
class Timer {
 public:
  /*! \brief The largest duty, 100 %: 10 bits, as TimerOne offers. */
  static constexpr uint16_t kPwmDutyMax{1023u};

  /*!
   * \brief The longest period, in microseconds: 2 * 65536 cycles at the largest
   *        prescaler, less one; 4194303 at 32 MHz.
   */
  static constexpr uint32_t kPeriodMax{internal::kLongestPeriod - 1u};

  /*! \brief The period Begin() takes where none is given: 1 ms. */
  static constexpr uint32_t kDefaultPeriod{1000u};

  /*!
   * \brief Makes a timer that is not begun.
   *
   * When compiling, so that tcc4 and tcc5 need no code at start (CodingStyle
   * M-5).
   */
  constexpr Timer() = default;

  /*! \brief Deleted: the interrupt serves tcc4 and tcc5, not a copy. */
  Timer(Timer const&) = delete;

  /*! \brief Deleted, as the copy. */
  Timer(Timer&&) = delete;

  /*! \brief Deleted, as the copy. */
  Timer& operator=(Timer const&) = delete;

  /*! \brief Deleted, as the copy. */
  Timer& operator=(Timer&&) = delete;

  /*! \brief Trivial, so that tcc4 and tcc5 need no code at the end. */
  ~Timer() = default;

  /*!
   * \brief Sets the timer up, dual-slope with the overflow interrupt at BOTTOM,
   *        with a period and a callback; the timer does not count before
   *        Start().
   *
   * The core leaves TC4 and TC5 running for analogWrite(): Begin() stops the
   * timer and resets it first.
   *
   * \param[in] microseconds The period of the interrupt and of the PWM, see
   *                         SetPeriod().
   * \param[in] callback What the overflow interrupt calls, or nullptr for none.
   * \return Error::kBegunAlready if Begin() was called already; nothing changes
   *         then. Error::kPeriodOutOfRange if the period is out of range; the
   *         timer is begun still, with a TOP of 0xFFFF and no prescaler.
   */
  XMEGATIMER_NODISCARD Status Begin(uint32_t microseconds = kDefaultPeriod,
                                    Callback callback = nullptr);

  /*!
   * \brief Sets the period of the interrupt and of the PWM, and chooses the
   *        smallest prescaler that reaches it.
   *
   * The period goes through PERBUF and is taken over at the next BOTTOM; the
   * compare values go with it, computed again from each channel's duty, so that
   * a running PWM keeps its ratio across the change.
   *
   * A new prescaler cannot wait for BOTTOM: a running timer starts again from 0
   * with the new period and compare values, as Start() does. The interval in
   * which it changes then lasts what was counted of it plus the new period, and
   * the PWM begins its new period at that moment. A stopped timer is set back
   * to 0 the same way and stays stopped; Resume() begins the new period.
   *
   * \param[in] microseconds The period, 1 to kPeriodMax.
   * \return Error::kNotBegun before Begin(), as the timer may still be the
   *         core's then; Error::kPeriodOutOfRange if the period is out of
   *         range. Nothing changes then.
   */
  XMEGATIMER_NODISCARD Status SetPeriod(uint32_t microseconds);

  /*!
   * \brief Starts the timer from 0, with the period and the compare values
   *        loaded at once rather than at the first BOTTOM.
   *
   * \return Error::kNotBegun before Begin().
   */
  XMEGATIMER_NODISCARD Status Start();

  /*!
   * \brief Stops the clock of the timer; the counter keeps its value.
   *
   * Does nothing before Begin(): the timer may still be the core's then,
   * running analogWrite().
   */
  void Stop();

  /*!
   * \brief Lets a stopped timer go on where it stopped.
   *
   * Where SetPeriod() changed the prescaler meanwhile, the timer was set back
   * to 0, and it begins the new period instead.
   *
   * \return Error::kNotBegun before Begin(), Error::kNotStopped if the timer
   *         was not stopped by Stop() since it started.
   */
  XMEGATIMER_NODISCARD Status Resume();

  /*!
   * \brief Sets the function the overflow interrupt calls, and enables the
   *        interrupt at low level, so that the core's millis() at high level
   *        keeps interrupting the callback.
   *
   * A flag of an overflow from while the interrupt was off is cleared, so that
   * the callback is not called at once.
   *
   * \param[in] callback What the interrupt calls.
   * \return Error::kNotBegun before Begin(), Error::kNoCallback if it is
   *         nullptr; nothing changes then.
   */
  XMEGATIMER_NODISCARD Status AttachInterrupt(Callback callback);

  /*!
   * \brief Disables the overflow interrupt and forgets the callback.
   *
   * Does nothing before Begin(): the timer may still be the core's then.
   */
  void DetachInterrupt();

  /*!
   * \brief Returns the time since the last overflow, to one timer tick.
   *
   * \return The time in microseconds; Error::kNotBegun before Begin(),
   *         Error::kNotStarted before Start().
   */
  XMEGATIMER_NODISCARD Result<uint32_t> Read() const;

  /*!
   * \brief Drives the pin of a compare channel with a PWM of `duty`.
   *
   * The pin becomes an output. The channel is a template argument, so that
   * channel C or D on a TC5 fails to compile instead of writing into reserved
   * registers.
   *
   * \tparam Which The compare channel.
   * \param[in] duty 0 to kPwmDutyMax, the share of the period.
   * \return Error::kNotBegun before Begin(), Error::kDutyOutOfRange if the duty
   *         is above kPwmDutyMax; nothing changes then.
   */
  template <Channel Which>
  XMEGATIMER_NODISCARD Status EnablePwm(uint16_t duty) {
    static_assert(HasChannel(Which), "this timer has no such compare channel");
    Status const status{SetPwmDuty<Which>(duty)};
    if (status.IsOk()) {
      Traits::Port().DIRSET = CompareChannel<Which>::kPinMask;
      Bits::WriteBitGroup(Traits::Registers().CTRLE,
                          CompareChannel<Which>::kModeMask,
                          CompareChannel<Which>::kModeCompare);
    }
    return status;
  }

  /*!
   * \brief Sets the duty of a compare channel, taken over at the next BOTTOM,
   *        so that a running PWM never sees half a period of each.
   *
   * \tparam Which The compare channel.
   * \param[in] duty 0 to kPwmDutyMax, the share of the period.
   * \return Error::kNotBegun before Begin(), Error::kDutyOutOfRange if the duty
   *         is above kPwmDutyMax; nothing changes then.
   */
  template <Channel Which>
  XMEGATIMER_NODISCARD Status SetPwmDuty(uint16_t duty) {
    static_assert(HasChannel(Which), "this timer has no such compare channel");
    if (state_ == State::kNotBegun) {
      return Error::kNotBegun;
    }
    if (duty > kPwmDutyMax) {
      return Error::kDutyOutOfRange;
    }
    StoreDuty(IndexOf(Which), duty, CompareChannel<Which>::Buffer());
    return Error::kOk;
  }

  /*!
   * \brief Takes a compare channel's pin from the PWM; the pin stays an output
   *        and shows its value of PORT.OUT.
   *
   * Does nothing before Begin(): the timer may still be the core's then.
   *
   * \tparam Which The compare channel.
   */
  template <Channel Which>
  void DisablePwm() {
    static_assert(HasChannel(Which), "this timer has no such compare channel");
    if (state_ == State::kNotBegun) {
      return;
    }
    Bits::WriteBitGroup(Traits::Registers().CTRLE,
                        CompareChannel<Which>::kModeMask,
                        CompareChannel<Which>::kModeDisable);
  }

  /*!
   * \brief Clears the overflow flag and calls the callback: what the timer's
   *        interrupt routine calls, not a sketch.
   *
   * Unlike TC0 and TC1 of the other XMEGAs, TC4 and TC5 do not clear their
   * interrupt flag when the vector executes; without the clear the routine is
   * entered again at once.
   */
  void HandleOverflow();

 private:
  using Bits = Bit<uint8_t>;
  using ClockSelect = internal::ClockSelect;

  // Where the timer is in its life.
  enum class State : uint8_t { kNotBegun, kBegun, kRunning, kStopped };

  // Which registers WriteCompareValues() writes: the buffers, taken over at
  // BOTTOM, or the compare registers themselves, while the clock is off.
  enum class Target : uint8_t { kBuffer, kNow };

  template <Channel Which>
  using CompareChannel = typename Traits::template CompareChannel<Which>;

  // The prescaler the timer counts at, by its place in the table of Timer.cpp.
  using PrescalerIndex = uint8_t;

  // A channel by its place, to go through all of them when compiling: each
  // has registers of its own, which no index reaches at run time.
  template <uint8_t Index>
  struct ChannelIndex {};

  // TC4 and TC5 share the bit layout of every register used here; the class
  // relies on that rather than carrying a second set of masks in the traits.
  static_assert(TC4_CLKSEL_gm == TC5_CLKSEL_gm, "TC4/TC5 clock select differ");
  static_assert(TC4_WGMODE_gm == TC5_WGMODE_gm, "TC4/TC5 waveform mode differ");
  static_assert(TC4_DIR_bm == TC5_DIR_bm, "TC4/TC5 direction bit differs");
  static_assert(TC4_OVFINTLVL_gm == TC5_OVFINTLVL_gm,
                "TC4/TC5 overflow interrupt level differs");
  static_assert(TC4_OVFIF_bm == TC5_OVFIF_bm, "TC4/TC5 overflow flag differs");

  // Without this a timer switched off still compiles here and only fails at
  // the link, as an undefined reference to every member function used.
  static_assert(Traits::kEnabled,
                "this timer is switched off - see XMEGATIMER_USE_TCC4 and "
                "XMEGATIMER_USE_TCC5");

  // SetPeriod() and Read() convert with whole timer cycles per half and per
  // whole microsecond.
  static_assert((F_CPU % (2u * internal::kMicrosecondsPerSecond)) == 0u,
                "F_CPU has to be a multiple of 2 MHz");

  static constexpr uint8_t IndexOf(Channel which) {
    return static_cast<uint8_t>(which);
  }

  // GCOVR_EXCL_START: asked only by static_assert, when compiling, so that no
  // test executes it
  static constexpr bool HasChannel(Channel which) {
    return IndexOf(which) < Traits::kChannelCount;
  }
  // GCOVR_EXCL_STOP

  uint16_t CompareValue(uint16_t duty) const;
  uint32_t ToMicroseconds(uint32_t cycles) const;
  void TakeOverPrescaler();
  void RestartCounting(ClockSelect clock_select);
  void LoadPeriodAndCompare();
  void EnableOverflowInterrupt(Callback callback);
  void StoreDuty(uint8_t index, uint16_t duty, register16_t& buffer);
  static void SetClockSelect(ClockSelect clock_select);
  static bool IsCountingDown();

  template <uint8_t Index>
  void WriteCompareValues(ChannelIndex<Index> /*index*/, Target target);
  void WriteCompareValues(ChannelIndex<Traits::kChannelCount> /*end*/,
                          Target /*target*/) {}

  State state_{State::kNotBegun};
  PrescalerIndex prescaler_{0u};
  // Read by the interrupt, written in an ATOMIC_BLOCK, whose memory barrier
  // keeps the write where it stands; the interrupt reads it anew each time it
  // runs. Not volatile (CodingStyle H-6): a volatile member would make Timer
  // no literal type, and tcc4 and tcc5 would need code at start (M-5).
  Callback callback_{nullptr};
  // What PER and the compare channels are meant to hold. Kept here rather than
  // read back from the registers, because a value on its way through a buffer
  // register is not in them until BOTTOM, and because a duty is a share of the
  // period that has to be applied again when it changes.
  uint16_t top_{internal::kLargestTop};
  internal::array<uint16_t, Traits::kChannelCount> duties_{};
};

}  // namespace xmega_timer

#endif  // XMEGATIMER_SRC_XMEGA_TIMER_TIMER_H_
