// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The member functions of Timer, instantiated once per timer enabled in
 *        Timer.h, next to the overflow ISR of that timer.
 */

#include "Timer.h"

#include <avr/interrupt.h>
#include <util/atomic.h>

// All timers of the XMEGA are 16 bit.
#define TIMER_NUMBER_OF_BITS 16u
#define TIMER_RESOLUTION (1uL << TIMER_NUMBER_OF_BITS)
#define TIMER_MAX_PRESCALER 1024u
// The longest period a 16-bit TOP reaches at the largest prescaler, counted up
// and down again.
#define TIMER_MAX_PERIOD \
  ((TIMER_RESOLUTION / (F_CPU / 1000000uL)) * TIMER_MAX_PRESCALER * 2u)

template <typename TimerTraits>
StdReturnType Timer<TimerTraits>::init(
    uint32_t Microseconds, TimerIsrCallbackF_void sTimerOverflowCallback) {
  StdReturnType returnValue{E_NOT_OK};

  if (STATE_NONE == State) {
    returnValue = E_OK;
    State = STATE_INIT;

    // The xmegaForArduino core leaves TC4/TC5 in byte mode with a running clock
    // for analogWrite(), so stop it and start from the reset values rather
    // than only changing the waveform mode.
    setClockSelect(TIMER_REG_CS_NO_CLOCK);
    getTimer().CTRLGSET = TC45_CMD_RESET_gc;

    // Mode 7: dual-slope PWM, overflow interrupt at BOTTOM.
    writeBitGroup(getTimer().CTRLB, TC4_WGMODE_gm, TC45_WGMODE_DSBOTTOM_gc);

    if (setPeriod(Microseconds) == E_NOT_OK) {
      returnValue = E_NOT_OK;
    }
    if (sTimerOverflowCallback != nullptr) {
      if (attachInterrupt(sTimerOverflowCallback) == E_NOT_OK) {
        returnValue = E_NOT_OK;
      }
    }

    State = STATE_READY;
  }
  return returnValue;
}

template <typename TimerTraits>
StdReturnType Timer<TimerTraits>::setPeriod(uint32_t Microseconds) {
  StdReturnType returnValue{E_NOT_OK};

  // An out of range period is refused and changes nothing.
  if ((Microseconds > 0u) && (Microseconds < TIMER_MAX_PERIOD)) {
    returnValue = E_OK;
    TimerClockSelectType const previousClockSelect{ClockSelectBitGroup};
    // The timer cycles of the period: the counter runs backwards after TOP,
    // and the interrupt is at BOTTOM, so half the microseconds.
    uint32_t timerCycles = (F_CPU / 2000000uL) * Microseconds;
    // The smallest prescaler that brings them below the resolution.
    if (timerCycles < TIMER_RESOLUTION) {
      ClockSelectBitGroup = TIMER_REG_CS_NO_PRESCALER;
    } else if ((timerCycles >>= 1) < TIMER_RESOLUTION) {
      ClockSelectBitGroup = TIMER_REG_CS_PRESCALE_2;
    } else if ((timerCycles >>= 1) < TIMER_RESOLUTION) {
      ClockSelectBitGroup = TIMER_REG_CS_PRESCALE_4;
    } else if ((timerCycles >>= 1) < TIMER_RESOLUTION) {
      ClockSelectBitGroup = TIMER_REG_CS_PRESCALE_8;
    } else if ((timerCycles >>= 3) < TIMER_RESOLUTION) {
      ClockSelectBitGroup = TIMER_REG_CS_PRESCALE_64;
    } else if ((timerCycles >>= 2) < TIMER_RESOLUTION) {
      ClockSelectBitGroup = TIMER_REG_CS_PRESCALE_256;
    } else {
      // TIMER_MAX_PERIOD above is what keeps this one inside 16 bit.
      timerCycles >>= 2;
      ClockSelectBitGroup = TIMER_REG_CS_PRESCALE_1024;
    }

    // A 16-bit write goes through the shared TEMP register, so an ISR touching
    // another 16-bit register of this timer must not get in between.
    // Through PERBUF, not PER: in dual-slope mode a TOP written below the
    // counter mid-period is not met until the counter has gone round 0xFFFF,
    // one period of up to several seconds where a shorter one was asked for.
    // The buffer is taken over at BOTTOM. The compare values go with it,
    // recomputed from their duty, so the PWM keeps its ratio across the change
    // and takes the new period at the same BOTTOM.
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      Top = static_cast<uint16_t>(timerCycles);
      getTimer().PERBUF = Top;
      for (uint8_t Channel = 0u; Channel < TimerTraits::NumberOfChannels;
           Channel++) {
        setCompareBuffer(static_cast<TimerChannelType>(Channel));
      }
    }

    if (ClockSelectBitGroup != previousClockSelect) {
      takeOverClockSelect();
    }
  }
  return returnValue;
}

// CTRLA has no buffer: a new prescaler written at once would count the rest of
// the period at the new rate to the old TOP, up to 1024 times as long or as
// short. So the counter starts again from 0 instead, with the new period at
// once; a stopped timer stays stopped, and resume() begins that period. Before
// start() the clock is off and start() loads everything.
template <typename TimerTraits>
void Timer<TimerTraits>::takeOverClockSelect() {
  if (STATE_RUNNING == State) {
    restartCounting(ClockSelectBitGroup);
  } else if (STATE_STOPPED == State) {
    restartCounting(TIMER_REG_CS_NO_CLOCK);
  }
}

template <typename TimerTraits>
StdReturnType Timer<TimerTraits>::start() {
  StdReturnType returnValue{E_NOT_OK};

  if (STATE_READY == State || STATE_RUNNING == State ||
      STATE_STOPPED == State) {
    returnValue = E_OK;
    restartCounting(ClockSelectBitGroup);
    State = STATE_RUNNING;
  }
  return returnValue;
}

// Ends with the clock it is given, so a stopped timer is set back to 0 without
// counting a single cycle.
template <typename TimerTraits>
void Timer<TimerTraits>::restartCounting(TimerClockSelectType ClockSelect) {
  setClockSelect(TIMER_REG_CS_NO_CLOCK);
  loadPeriodAndCompare();
  // RESTART clears the counter and the direction, so the first period is a
  // whole one.
  getTimer().CTRLGSET = TC45_CMD_RESTART_gc;
  setClockSelect(ClockSelect);
}

template <typename TimerTraits>
void Timer<TimerTraits>::stop() {
  // Before init() the timer may still be the core's, running analogWrite().
  if (STATE_NONE != State) {
    setClockSelect(TIMER_REG_CS_NO_CLOCK);
    if (STATE_RUNNING == State) {
      State = STATE_STOPPED;
    }
  }
}

template <typename TimerTraits>
StdReturnType Timer<TimerTraits>::resume() {
  StdReturnType returnValue{E_NOT_OK};

  if (STATE_STOPPED == State) {
    returnValue = E_OK;
    setClockSelect(ClockSelectBitGroup);
    State = STATE_RUNNING;
  }
  return returnValue;
}

template <typename TimerTraits>
StdReturnType Timer<TimerTraits>::attachInterrupt(
    TimerIsrCallbackF_void sTimerOverflowCallback) {
  StdReturnType returnValue{E_NOT_OK};

  if (sTimerOverflowCallback != nullptr) {
    returnValue = E_OK;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      TimerOverflowCallback = sTimerOverflowCallback;
    }
    // The flag is set at every BOTTOM whether the interrupt is on or not, so
    // one left over from while it was off would call the new callback at once.
    // Only then: with the interrupt already on, a set flag is a real overflow
    // about to be served.
    if (readBitGroup(getTimer().INTCTRLA, TC4_OVFINTLVL_gm) ==
        TC45_OVFINTLVL_OFF_gc) {
      getTimer().INTFLAGS = TC4_OVFIF_bm;
    }
    // Low level, so the core's millis() on TCD5 (high level) keeps
    // interrupting the callback.
    writeBitGroup(getTimer().INTCTRLA, TC4_OVFINTLVL_gm, TC45_OVFINTLVL_LO_gc);
    PMIC.CTRL |= PMIC_LOLVLEN_bm;
  }
  return returnValue;
}

template <typename TimerTraits>
void Timer<TimerTraits>::detachInterrupt() {
  writeBitGroup(getTimer().INTCTRLA, TC4_OVFINTLVL_gm, TC45_OVFINTLVL_OFF_gc);
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { TimerOverflowCallback = nullptr; }
}

template <typename TimerTraits>
StdReturnType Timer<TimerTraits>::read(uint32_t& Microseconds) {
  StdReturnType returnValue{E_NOT_OK};

  if (STATE_RUNNING == State || STATE_STOPPED == State) {
    uint32_t counterValue{0u};
    uint32_t topValue{0u};
    TimerCountDirection direction{TIMER_COUNT_DIRECTION_UP};
    returnValue = E_OK;

    // Counter and direction have to come from the same instant, or a turn at
    // TOP in between reads as a whole period too much.
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      counterValue = getTimer().CNT;
      topValue = getTimer().PER;
      direction = getCountingDirection();
    }
    // Counting down, the way back from TOP adds to the way up.
    if (direction == TIMER_COUNT_DIRECTION_DOWN) {
      counterValue = (topValue - counterValue) + topValue;
    }

    Microseconds = toMicroseconds(counterValue, ClockSelectBitGroup);
  }
  return returnValue;
}

template <typename TimerTraits>
void Timer<TimerTraits>::handleOverflowInterrupt() {
  getTimer().INTFLAGS = TC4_OVFIF_bm;
  if (TimerOverflowCallback != nullptr) {
    TimerOverflowCallback();
  }
}

template <typename TimerTraits>
typename Timer<TimerTraits>::TimerCountDirection
Timer<TimerTraits>::getCountingDirection() {
  if (getTimer().CTRLGSET & TC4_DIR_bm) {
    return TIMER_COUNT_DIRECTION_DOWN;
  } else {
    return TIMER_COUNT_DIRECTION_UP;
  }
}

template <typename TimerTraits>
void Timer<TimerTraits>::setClockSelect(TimerClockSelectType ClockSelect) {
  writeBitGroup(getTimer().CTRLA, TC4_CLKSEL_gm, ClockSelect);
}

// Called with interrupts off: the write goes through the TEMP register.
template <typename TimerTraits>
void Timer<TimerTraits>::setCompareBuffer(TimerChannelType Channel) {
  // CCABUF..CCDBUF are consecutive in both TC4_t and TC5_t; the channel was
  // range checked at compile time.
  (&getTimer().CCABUF)[Channel] = toCompareValue(Top, PwmDuty[Channel]);
}

// Straight into PER and CCx, past the buffers, which is only safe while the
// timer has no clock. Without it the first period after start() would run on
// whatever the registers held: the buffers are taken over at BOTTOM, and the
// first BOTTOM after RESTART is a whole period away, with CCx at the 0 that
// init()'s RESET left there. Not a forced UPDATE before the RESTART instead:
// both are written to CTRLGSET, and an UPDATE still pending there ORed with
// RESTART reads as RESET.
template <typename TimerTraits>
void Timer<TimerTraits>::loadPeriodAndCompare() {
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    getTimer().PER = Top;
    for (uint8_t Channel = 0u; Channel < TimerTraits::NumberOfChannels;
         Channel++) {
      // CCA..CCD are consecutive in both TC4_t and TC5_t, like their buffers.
      (&getTimer().CCA)[Channel] = toCompareValue(Top, PwmDuty[Channel]);
    }
  }
}

template <typename TimerTraits>
StdReturnType Timer<TimerTraits>::enablePwmChannel(TimerChannelType Channel,
                                                   uint16_t Duty) {
  StdReturnType returnValue = setPwmDutyChannel(Channel, Duty);

  if (E_OK == returnValue) {
    TimerTraits::getPort().DIRSET = toPinMask(Channel);
    writeBitGroup(getTimer().CTRLE, toChannelModeMask(Channel),
                  toChannelModeCompare(Channel));
  }
  return returnValue;
}

template <typename TimerTraits>
StdReturnType Timer<TimerTraits>::disablePwmChannel(TimerChannelType Channel) {
  writeBitGroup(getTimer().CTRLE, toChannelModeMask(Channel),
                TC45_CCAMODE_DISABLE_gc);
  // The pin stays an output; it now shows its PORT.OUT value instead of the
  // waveform.
  return E_OK;
}

// Written to the buffer register, so the new compare value takes effect at the
// next BOTTOM and a running PWM never sees half a period of each.
template <typename TimerTraits>
StdReturnType Timer<TimerTraits>::setPwmDutyChannel(TimerChannelType Channel,
                                                    uint16_t Duty) {
  StdReturnType returnValue{E_NOT_OK};

  if (Duty <= PwmDutyMax) {
    returnValue = E_OK;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      PwmDuty[Channel] = Duty;
      setCompareBuffer(Channel);
    }
  }
  return returnValue;
}

#if (TIMER_USE_TCC4 == STD_ON)
template class Timer<TimerC4>;
ISR(TCC4_OVF_vect) { Timer<TimerC4>::getInstance().handleOverflowInterrupt(); }
#endif

#if (TIMER_USE_TCC5 == STD_ON)
template class Timer<TimerC5>;
ISR(TCC5_OVF_vect) { Timer<TimerC5>::getInstance().handleOverflowInterrupt(); }
#endif
