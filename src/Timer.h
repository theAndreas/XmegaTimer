// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The 16-bit Timer/Counters TC4 and TC5 of the ATxmega E5 as a periodic
 *        interrupt and a PWM source.
 *
 * One class for TCC4 and TCC5, picked by a traits struct. Everything that
 * differs between the two kinds, the register struct, the number of compare
 * channels and the pins those channels drive, lives in the traits, so the
 * class never has to ask at run time which kind it is. A channel the timer
 * does not have is a compile error.
 *
 * The timer counts dual-slope and interrupts at BOTTOM, so a period spans
 * 2 * PER timer cycles and the PWM is phase-correct.
 */

#ifndef XMEGATIMER_SRC_TIMER_H_
#define XMEGATIMER_SRC_TIMER_H_

#include <avr/io.h>

#include "StandardTypes.h"

/*!
 * \brief Whether TCC4 gets an instance and an overflow ISR; off by default.
 *
 * The xmegaForArduino core drives tone() and analogWrite() with TCC4, and
 * tone() defines TCC4_OVF_vect itself; also its channels A and B sit on
 * PC0/PC1, the TWI pins. TCD5 cannot be selected at all, see TimerC5.
 */
#ifndef TIMER_USE_TCC4
#define TIMER_USE_TCC4 STD_OFF
#endif
/*! \brief Whether TCC5 gets an instance and an overflow ISR; on by default. */
#ifndef TIMER_USE_TCC5
#define TIMER_USE_TCC5 STD_ON
#endif

/*!
 * \brief The traits of TCC4: four compare channels, OC4A..OC4D on PC0..PC3.
 *
 * PORTC.REMAP could move them to PC4..PC7, which this class does not do.
 */
struct TimerC4 {
  /*! \brief The register struct of the timer. */
  using RegisterType = TC4_t;
  /*! \brief The registers of the timer. */
  static RegisterType& getTimer() { return TCC4; }
  /*! \brief The port of the compare channels' pins. */
  static PORT_t& getPort() { return PORTC; }
  /*! \brief The number of compare channels. */
  static constexpr uint8_t NumberOfChannels{4u};
  /*! \brief The pin of channel A in getPort(); the others follow it. */
  static constexpr uint8_t FirstPin{0u};
  /*! \brief Whether the configuration switches the timer on. */
  static constexpr bool Enabled{TIMER_USE_TCC4 == STD_ON};
};

/*!
 * \brief The traits of TCC5: two compare channels, OC5A/OC5B on PC4/PC5, the
 *        SPI SS and SCK pins.
 *
 * There are no traits for TCD5 on purpose: the xmegaForArduino core counts
 * millis() on it and defines TCD5_OVF_vect itself, so a second ISR would not
 * link, and reprogramming the timer would stop the system clock.
 */
struct TimerC5 {
  /*! \brief The register struct of the timer. */
  using RegisterType = TC5_t;
  /*! \brief The registers of the timer. */
  static RegisterType& getTimer() { return TCC5; }
  /*! \brief The port of the compare channels' pins. */
  static PORT_t& getPort() { return PORTC; }
  /*! \brief The number of compare channels. */
  static constexpr uint8_t NumberOfChannels{2u};
  /*! \brief The pin of channel A in getPort(); the others follow it. */
  static constexpr uint8_t FirstPin{4u};
  /*! \brief Whether the configuration switches the timer on. */
  static constexpr bool Enabled{TIMER_USE_TCC5 == STD_ON};
};

/*!
 * \brief A TC4 or TC5 as a periodic interrupt, a PWM on each of its compare
 *        channels and a time readout.
 *
 * There is one per timer, getInstance(); init() first, then start(). A
 * function that can refuse returns E_NOT_OK, and leaves the timer as it was,
 * unless it says otherwise.
 *
 * \tparam TimerTraits TimerC4 or TimerC5
 */
template <typename TimerTraits>
class Timer {
 public:
  /*! \brief The function the overflow interrupt calls. */
  typedef void (*TimerIsrCallbackF_void)(void);

  /*! \brief Where the timer is in its life. */
  typedef enum {
    STATE_NONE,    /*!< Not initialized: init() comes first. */
    STATE_INIT,    /*!< Within init(). */
    STATE_READY,   /*!< Initialized, not started. */
    STATE_RUNNING, /*!< Counting. */
    STATE_STOPPED  /*!< Stopped by stop(). */
  } StateType;

  /*! \brief The values of the clock select bits CLKSEL of CTRLA. */
  typedef enum {
    TIMER_REG_CS_NO_CLOCK = TC45_CLKSEL_OFF_gc, /*!< The counter stands. */
    TIMER_REG_CS_NO_PRESCALER =
        TC45_CLKSEL_DIV1_gc,                       /*!< The peripheral clock. */
    TIMER_REG_CS_PRESCALE_2 = TC45_CLKSEL_DIV2_gc, /*!< The clock / 2. */
    TIMER_REG_CS_PRESCALE_4 = TC45_CLKSEL_DIV4_gc, /*!< The clock / 4. */
    TIMER_REG_CS_PRESCALE_8 = TC45_CLKSEL_DIV8_gc, /*!< The clock / 8. */
    TIMER_REG_CS_PRESCALE_64 = TC45_CLKSEL_DIV64_gc,   /*!< The clock / 64. */
    TIMER_REG_CS_PRESCALE_256 = TC45_CLKSEL_DIV256_gc, /*!< The clock / 256. */
    TIMER_REG_CS_PRESCALE_1024 =
        TC45_CLKSEL_DIV1024_gc /*!< The clock / 1024. */
  } TimerClockSelectType;

  /*! \brief The compare channels; each drives one PWM pin. */
  typedef enum {
    TIMER_CHANNEL_A, /*!< Channel A, on TimerTraits::FirstPin. */
    TIMER_CHANNEL_B, /*!< Channel B, on the pin after A's. */
    TIMER_CHANNEL_C, /*!< Channel C, TC4 only. */
    TIMER_CHANNEL_D  /*!< Channel D, TC4 only. */
  } TimerChannelType;

  /*! \brief The direction the counter counts in. */
  typedef enum {
    TIMER_COUNT_DIRECTION_UP,  /*!< From BOTTOM to TOP. */
    TIMER_COUNT_DIRECTION_DOWN /*!< From TOP to BOTTOM. */
  } TimerCountDirection;

  /*! \brief The largest duty, 100 %: 10 bits, as TimerOne offers. */
  static constexpr uint16_t PwmDutyMax{1023u};

 private:
  // TC4 and TC5 share the bit layout of every register used here; the class
  // relies on that rather than carrying a second set of masks in the traits.
  static_assert(TC4_CLKSEL_gm == TC5_CLKSEL_gm, "TC4/TC5 clock select differ");
  static_assert(TC4_WGMODE_gm == TC5_WGMODE_gm, "TC4/TC5 waveform mode differ");
  static_assert(TC4_DIR_bm == TC5_DIR_bm, "TC4/TC5 direction bit differs");
  static_assert(TC4_OVFINTLVL_gm == TC5_OVFINTLVL_gm,
                "TC4/TC5 overflow interrupt level differs");
  static_assert(TC4_OVFIF_bm == TC5_OVFIF_bm, "TC4/TC5 overflow flag differs");
  static_assert(TC4_CCAMODE_gm == TC5_CCAMODE_gm,
                "TC4/TC5 channel mode differs");

  // Without this a timer switched off in the configuration above still
  // compiles here and only fails at the link, as an undefined reference to
  // every member function used.
  static_assert(TimerTraits::Enabled,
                "this timer is switched off - see "
                "TIMER_USE_TCC4/TIMER_USE_TCC5");

  // setPeriod() and read() convert with whole timer cycles per half and per
  // whole microsecond.
  static_assert(F_CPU % 2000000uL == 0u, "F_CPU has to be a multiple of 2 MHz");

  static constexpr uint8_t ChannelModeWidth{2u};
  static constexpr uint8_t PwmDutyBits{10u};

  static Timer Instance;

  StateType State;
  TimerClockSelectType ClockSelectBitGroup;
  TimerIsrCallbackF_void TimerOverflowCallback;
  // What PER and the compare channels are meant to hold. Kept here rather than
  // read back from the registers, because a value on its way through a buffer
  // register is not in them until BOTTOM, and because a duty is a fraction of
  // the period that has to be applied again when it changes.
  uint16_t Top;
  uint16_t PwmDuty[TimerTraits::NumberOfChannels];

  constexpr Timer()
      : State(STATE_NONE),
        ClockSelectBitGroup(TIMER_REG_CS_NO_PRESCALER),
        TimerOverflowCallback(nullptr),
        Top(0xFFFFu),
        PwmDuty{} {}

  static typename TimerTraits::RegisterType& getTimer() {
    return TimerTraits::getTimer();
  }

  static constexpr uint8_t toPinMask(TimerChannelType Channel) {
    return static_cast<uint8_t>(1u << (TimerTraits::FirstPin + Channel));
  }
  static constexpr uint8_t toChannelModeMask(TimerChannelType Channel) {
    return static_cast<uint8_t>(TC4_CCAMODE_gm << (Channel * ChannelModeWidth));
  }
  static constexpr uint8_t toChannelModeCompare(TimerChannelType Channel) {
    return static_cast<uint8_t>(TC45_CCAMODE_COMP_gc
                                << (Channel * ChannelModeWidth));
  }
  static constexpr uint16_t toCompareValue(uint16_t sTop, uint16_t Duty) {
    return static_cast<uint16_t>((static_cast<uint32_t>(sTop) * Duty) >>
                                 PwmDutyBits);
  }
  // Shifted before it is divided: the other way round the truncation is scaled
  // up by the prescaler, and at 1024 read() moved in steps of a millisecond
  // where a tick is 32 us. 2 * 0xFFFF << 10 is still well inside 32 bit.
  static constexpr uint32_t toMicroseconds(uint32_t TimerCycles,
                                           TimerClockSelectType ClockSelect) {
    return (TimerCycles << toPrescalerShift(ClockSelect)) / (F_CPU / 1000000uL);
  }
  // One expression, as a constexpr function of C++11 has a single return
  // statement.
  static constexpr uint8_t toPrescalerShift(TimerClockSelectType ClockSelect) {
    return static_cast<uint8_t>(
        (ClockSelect == TIMER_REG_CS_PRESCALE_2)      ? 1u
        : (ClockSelect == TIMER_REG_CS_PRESCALE_4)    ? 2u
        : (ClockSelect == TIMER_REG_CS_PRESCALE_8)    ? 3u
        : (ClockSelect == TIMER_REG_CS_PRESCALE_64)   ? 6u
        : (ClockSelect == TIMER_REG_CS_PRESCALE_256)  ? 8u
        : (ClockSelect == TIMER_REG_CS_PRESCALE_1024) ? 10u
                                                      : 0u);
  }

  TimerCountDirection getCountingDirection();
  void setClockSelect(TimerClockSelectType);
  void setCompareBuffer(TimerChannelType);
  void loadPeriodAndCompare();
  StdReturnType enablePwmChannel(TimerChannelType, uint16_t);
  StdReturnType disablePwmChannel(TimerChannelType);
  StdReturnType setPwmDutyChannel(TimerChannelType, uint16_t);

 public:
  /*! \brief The one Timer of this TC4 or TC5. */
  static Timer& getInstance() { return Instance; }

  /*!
   * \brief Sets the timer up, dual-slope with the overflow interrupt at BOTTOM,
   *        with a period and a callback; the timer does not count before
   *        start().
   *
   * The xmegaForArduino core leaves TC4/TC5 running for analogWrite(): init()
   * stops the timer and resets it first.
   *
   * \param Microseconds the period of the interrupt and of the PWM, see
   *                     setPeriod()
   * \param sTimerOverflowCallback what the overflow interrupt calls, or nullptr
   *                               for none
   * \return E_NOT_OK if the timer was initialized already, or the period is out
   *         of range; in the latter case the timer is initialized still, with a
   *         TOP of 0xFFFF and no prescaler.
   */
  StdReturnType init(uint32_t Microseconds = 1000u,
                     TimerIsrCallbackF_void sTimerOverflowCallback = nullptr);

  /*!
   * \brief Sets the period of the interrupt and of the PWM, and chooses the
   *        smallest prescaler that reaches it.
   *
   * The period goes through PERBUF and is taken over at the next BOTTOM; the
   * compare values go with it, computed again from each channel's duty, so a
   * running PWM keeps its ratio across the change. A new prescaler of a running
   * timer takes effect at once.
   *
   * \param Microseconds the period, more than 0 and less than 2 * 65536 * 1024
   *                     cycles of F_CPU, 4194304 at 32 MHz
   * \return E_NOT_OK if the period is out of range; nothing changes then.
   */
  StdReturnType setPeriod(uint32_t Microseconds);

  /*!
   * \brief Starts the timer from 0, with the period and the compare values
   *        loaded at once rather than at the first BOTTOM.
   * \return E_NOT_OK if the timer was not initialized.
   */
  StdReturnType start();

  /*!
   * \brief Stops the clock of the timer; the counter keeps its value.
   *
   * Does nothing before init(): the timer may still be the core's then,
   * running analogWrite().
   */
  void stop();

  /*!
   * \brief Lets a stopped timer go on where it stopped.
   * \return E_NOT_OK if the timer was not stopped.
   */
  StdReturnType resume();

  /*!
   * \brief Sets the function the overflow interrupt calls, and enables the
   *        interrupt at low level, so that the core's millis() at high level
   *        keeps interrupting the callback.
   *
   * A flag of an overflow from while the interrupt was off is cleared, so the
   * callback is not called at once.
   *
   * \param sTimerOverflowCallback what the interrupt calls
   * \return E_NOT_OK if it is nullptr.
   */
  StdReturnType attachInterrupt(TimerIsrCallbackF_void sTimerOverflowCallback);

  /*! \brief Disables the overflow interrupt and forgets the callback. */
  void detachInterrupt();

  /*!
   * \brief The time since the last overflow, to one timer tick.
   * \param[out] Microseconds the time, in microseconds; unchanged if refused
   * \return E_NOT_OK if the timer is neither running nor stopped.
   */
  StdReturnType read(uint32_t& Microseconds);

  /*!
   * \brief What the overflow ISR of this timer calls: clears the flag and calls
   *        the callback.
   *
   * Unlike TC0/TC1 of the other XMEGAs, TC4/TC5 do not clear their interrupt
   * flag when the vector executes; without the clear the ISR is entered again
   * at once.
   */
  void handleOverflowInterrupt();

  /*!
   * \brief Drives the pin of a compare channel with a PWM of a duty.
   *
   * The pin becomes an output. The channel is a template argument, so that
   * channel C or D on a TC5 fails to compile instead of writing into reserved
   * registers at run time.
   *
   * \tparam Channel the compare channel
   * \param Duty 0 .. PwmDutyMax, a fraction of the period
   * \return E_NOT_OK if the duty is above PwmDutyMax; nothing changes then.
   */
  template <TimerChannelType Channel>
  StdReturnType enablePwm(uint16_t Duty) {
    static_assert(Channel < TimerTraits::NumberOfChannels,
                  "this timer has no such compare channel");
    return enablePwmChannel(Channel, Duty);
  }

  /*!
   * \brief Takes a compare channel's pin from the PWM; the pin stays an output
   *        and shows its PORT.OUT value.
   * \tparam Channel the compare channel
   * \return E_OK.
   */
  template <TimerChannelType Channel>
  StdReturnType disablePwm() {
    static_assert(Channel < TimerTraits::NumberOfChannels,
                  "this timer has no such compare channel");
    return disablePwmChannel(Channel);
  }

  /*!
   * \brief Sets the duty of a compare channel, taken over at the next BOTTOM,
   *        so a running PWM never sees half a period of each.
   * \tparam Channel the compare channel
   * \param Duty 0 .. PwmDutyMax, a fraction of the period
   * \return E_NOT_OK if the duty is above PwmDutyMax; nothing changes then.
   */
  template <TimerChannelType Channel>
  StdReturnType setPwmDuty(uint16_t Duty) {
    static_assert(Channel < TimerTraits::NumberOfChannels,
                  "this timer has no such compare channel");
    return setPwmDutyChannel(Channel, Duty);
  }
};

template <typename TimerTraits>
Timer<TimerTraits> Timer<TimerTraits>::Instance;

#endif  // XMEGATIMER_SRC_TIMER_H_
