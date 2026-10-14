/******************************************************************************************************************************************************
 *  COPYRIGHT
 *  ---------------------------------------------------------------------------------------------------------------------------------------------------
 *  \verbatim
 *  Copyright (c) Andreas Burnickl                                                                                                 All rights reserved.
 *
 *  \endverbatim
 *  ---------------------------------------------------------------------------------------------------------------------------------------------------
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------------------------------------*/
/**     \file       Timer.cpp
 *      \brief      16-bit Timer/Counter TC4/TC5 of the XMEGA E5 as periodic interrupt and PWM source
 *
 *      \details    The member functions are instantiated here, once per timer enabled in Timer.h,
 *                  next to the overflow ISR of that timer.
 *
******************************************************************************************************************************************************/
#define _TIMER_SOURCE_

/******************************************************************************************************************************************************
 * I N C L U D E S
******************************************************************************************************************************************************/
#include "Timer.h"
#include <avr/interrupt.h>
#include <util/atomic.h>

/******************************************************************************************************************************************************
 *  L O C A L   C O N S T A N T   M A C R O S
******************************************************************************************************************************************************/
/* All Timers of XMEGA are 16 bit */
#define TIMER_NUMBER_OF_BITS                        16u
#define TIMER_RESOLUTION                            (1uL << TIMER_NUMBER_OF_BITS)
#define TIMER_MAX_PRESCALER                         1024u
#define TIMER_PWM_DUTY_BITS                         10u

/******************************************************************************************************************************************************
 *  L O C A L   F U N C T I O N   M A C R O S
******************************************************************************************************************************************************/


/******************************************************************************************************************************************************
 *  L O C A L   D A T A   T Y P E S   A N D   S T R U C T U R E S
******************************************************************************************************************************************************/



/******************************************************************************************************************************************************
 * P U B L I C   F U N C T I O N S
******************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  init()
******************************************************************************************************************************************************/
template <typename TimerTraits>
StdReturnType Timer<TimerTraits>::init(uint32_t Microseconds, TimerIsrCallbackF_void sTimerOverflowCallback)
{
    StdReturnType returnValue{E_NOT_OK};

    if(STATE_NONE == State) {
        returnValue = E_OK;
        State = STATE_INIT;

        /* the xmegaForArduino core leaves TC4/TC5 in byte mode with a running clock for analogWrite(),
           so stop it and start from the reset values rather than only changing the waveform mode */
        setClockSelect(TIMER_REG_CS_NO_CLOCK);
        getTimer().CTRLGSET = TC45_CMD_RESET_gc;

        /* set mode 7: Dual-slope PWM, overflow interrupt at BOTTOM */
        writeBitGroup(getTimer().CTRLB, TC4_WGMODE_gm, TC45_WGMODE_DSBOTTOM_gc);

        if(setPeriod(Microseconds) == E_NOT_OK) returnValue = E_NOT_OK;
        if(sTimerOverflowCallback != nullptr) if(attachInterrupt(sTimerOverflowCallback) == E_NOT_OK) returnValue = E_NOT_OK;

        State = STATE_READY;
    }
    return returnValue;
} /* init */


/******************************************************************************************************************************************************
  setPeriod()
******************************************************************************************************************************************************/
/*! \brief          set period of the timer overflow interrupt
 *  \details        this functions sets the period of the timer overflow interrupt therefore
 *                  prescaler and timer top value will be calculated
 *  \param[in]      Microseconds                period of the timer overflow interrupt
 *  \return         E_OK
 *                  E_NOT_OK
 *****************************************************************************************************************************************************/
template <typename TimerTraits>
StdReturnType Timer<TimerTraits>::setPeriod(uint32_t Microseconds)
{
    StdReturnType returnValue{E_NOT_OK};

    /* was request out of bounds? */
    if(Microseconds < ((TIMER_RESOLUTION / (F_CPU / 1000000uL)) * TIMER_MAX_PRESCALER * 2u)) {
        returnValue = E_OK;
        /* calculate timer cycles to reach timer period, counter runs backwards after TOP, interrupt is at BOTTOM so divide microseconds by 2 */
        uint32_t timerCycles = (F_CPU / 2000000uL) * Microseconds;
        /* calculate timer prescaler */
        if(timerCycles < TIMER_RESOLUTION)              ClockSelectBitGroup = TIMER_REG_CS_NO_PRESCALER;
        else if((timerCycles >>= 1) < TIMER_RESOLUTION) ClockSelectBitGroup = TIMER_REG_CS_PRESCALE_2;
        else if((timerCycles >>= 1) < TIMER_RESOLUTION) ClockSelectBitGroup = TIMER_REG_CS_PRESCALE_4;
        else if((timerCycles >>= 1) < TIMER_RESOLUTION) ClockSelectBitGroup = TIMER_REG_CS_PRESCALE_8;
        else if((timerCycles >>= 3) < TIMER_RESOLUTION) ClockSelectBitGroup = TIMER_REG_CS_PRESCALE_64;
        else if((timerCycles >>= 2) < TIMER_RESOLUTION) ClockSelectBitGroup = TIMER_REG_CS_PRESCALE_256;
        else if((timerCycles >>= 2) < TIMER_RESOLUTION) ClockSelectBitGroup = TIMER_REG_CS_PRESCALE_1024;
        else {
            /* request was out of bounds, set to maximum */
            timerCycles = TIMER_RESOLUTION - 1u;
            ClockSelectBitGroup = TIMER_REG_CS_PRESCALE_1024;
            returnValue = E_NOT_OK;
        }
        /* Set TOP value of timer; a 16-bit write goes through the shared TEMP register, so an ISR
           touching another 16-bit register of this timer must not get in between */
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { getTimer().PER = timerCycles; }

        if(STATE_RUNNING == State) { setClockSelect(ClockSelectBitGroup); }
    }
    return returnValue;
} /* setPeriod */


/******************************************************************************************************************************************************
  start()
******************************************************************************************************************************************************/
/*! \brief          start the timer from zero
 *  \return         E_OK
 *                  E_NOT_OK        timer was not initialized
 *****************************************************************************************************************************************************/
template <typename TimerTraits>
StdReturnType Timer<TimerTraits>::start()
{
    StdReturnType returnValue{E_NOT_OK};

    if(STATE_READY == State || STATE_RUNNING == State || STATE_STOPPED == State) {
        returnValue = E_OK;
        /* RESTART clears the counter and the direction, so the first period is a whole one */
        getTimer().CTRLGSET = TC45_CMD_RESTART_gc;
        setClockSelect(ClockSelectBitGroup);
        State = STATE_RUNNING;
    }
    return returnValue;
} /* start */


/******************************************************************************************************************************************************
  stop()
******************************************************************************************************************************************************/
template <typename TimerTraits>
void Timer<TimerTraits>::stop()
{
    setClockSelect(TIMER_REG_CS_NO_CLOCK);
    if(STATE_RUNNING == State) { State = STATE_STOPPED; }
} /* stop */


/******************************************************************************************************************************************************
  resume()
******************************************************************************************************************************************************/
/*! \brief          continue a stopped timer where it stopped
 *  \return         E_OK
 *                  E_NOT_OK        timer was not stopped
 *****************************************************************************************************************************************************/
template <typename TimerTraits>
StdReturnType Timer<TimerTraits>::resume()
{
    StdReturnType returnValue{E_NOT_OK};

    if(STATE_STOPPED == State) {
        returnValue = E_OK;
        setClockSelect(ClockSelectBitGroup);
        State = STATE_RUNNING;
    }
    return returnValue;
} /* resume */


/******************************************************************************************************************************************************
  attachInterrupt()
******************************************************************************************************************************************************/
template <typename TimerTraits>
StdReturnType Timer<TimerTraits>::attachInterrupt(TimerIsrCallbackF_void sTimerOverflowCallback)
{
    StdReturnType returnValue{E_NOT_OK};

    if(sTimerOverflowCallback != nullptr) {
        returnValue = E_OK;
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { TimerOverflowCallback = sTimerOverflowCallback; }
        /* low level, so the core's millis() on TCD5 (high level) keeps interrupting the callback */
        writeBitGroup(getTimer().INTCTRLA, TC4_OVFINTLVL_gm, TC45_OVFINTLVL_LO_gc);
        PMIC.CTRL |= PMIC_LOLVLEN_bm;
    }
    return returnValue;
} /* attachInterrupt */


/******************************************************************************************************************************************************
  detachInterrupt()
******************************************************************************************************************************************************/
template <typename TimerTraits>
void Timer<TimerTraits>::detachInterrupt()
{
    writeBitGroup(getTimer().INTCTRLA, TC4_OVFINTLVL_gm, TC45_OVFINTLVL_OFF_gc);
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { TimerOverflowCallback = nullptr; }
} /* detachInterrupt */


/******************************************************************************************************************************************************
  read()
******************************************************************************************************************************************************/
/*! \brief          read current timer value in microseconds
 *  \details        this function returns the time since the last overflow interrupt in microseconds
 *
 *  \param[out]     Microseconds        current timer value
 *  \return         E_OK
 *                  E_NOT_OK
 *  \pre            Timer has to be in RUNNING or STOPPED STATE
 *****************************************************************************************************************************************************/
template <typename TimerTraits>
StdReturnType Timer<TimerTraits>::read(uint32_t& Microseconds)
{
    StdReturnType returnValue{E_NOT_OK};

    if(STATE_RUNNING == State || STATE_STOPPED == State) {
        uint32_t counterValue{0u};
        uint32_t topValue{0u};
        TimerCountDirection direction{TIMER_COUNT_DIRECTION_UP};
        returnValue = E_OK;

        /* counter and direction have to come from the same instant, or a turn at TOP in between
           reads as a whole period too much */
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            counterValue = getTimer().CNT;
            topValue = getTimer().PER;
            direction = getCountingDirection();
        }
        /* if counter counting down, add top value to current value */
        if(direction == TIMER_COUNT_DIRECTION_DOWN) { counterValue = (topValue - counterValue) + topValue; }

        /* transform counter value to microseconds in an efficient way */
        Microseconds = ((counterValue * 1000uL) / (F_CPU / 1000uL)) << toPrescalerShift(ClockSelectBitGroup);
    }
    return returnValue;
} /* read */


/******************************************************************************************************************************************************
  handleOverflowInterrupt()
******************************************************************************************************************************************************/
/*! \brief          called from the overflow ISR of this timer
 *  \details        Unlike TC0/TC1 of the other XMEGAs, TC4/TC5 do not clear their interrupt flag when
 *                  the vector executes; without the explicit clear the ISR is entered again at once.
 *****************************************************************************************************************************************************/
template <typename TimerTraits>
void Timer<TimerTraits>::handleOverflowInterrupt()
{
    getTimer().INTFLAGS = TC4_OVFIF_bm;
    if(TimerOverflowCallback != nullptr) { TimerOverflowCallback(); }
} /* handleOverflowInterrupt */


/******************************************************************************************************************************************************
 * P R I V A T E   F U N C T I O N S
******************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  getCountingDirection
******************************************************************************************************************************************************/
template <typename TimerTraits>
typename Timer<TimerTraits>::TimerCountDirection Timer<TimerTraits>::getCountingDirection()
{
    if(getTimer().CTRLGSET & TC4_DIR_bm) { return TIMER_COUNT_DIRECTION_DOWN; }
    else { return TIMER_COUNT_DIRECTION_UP; }
} /* getCountingDirection */


/******************************************************************************************************************************************************
  setClockSelect
******************************************************************************************************************************************************/
template <typename TimerTraits>
void Timer<TimerTraits>::setClockSelect(TimerClockSelectType ClockSelect)
{
    writeBitGroup(getTimer().CTRLA, TC4_CLKSEL_gm, ClockSelect);
} /* setClockSelect */


/******************************************************************************************************************************************************
  enablePwmChannel
******************************************************************************************************************************************************/
template <typename TimerTraits>
StdReturnType Timer<TimerTraits>::enablePwmChannel(TimerChannelType Channel, uint16_t Duty)
{
    StdReturnType returnValue = setPwmDutyChannel(Channel, Duty);

    if(E_OK == returnValue) {
        TimerTraits::getPort().DIRSET = toPinMask(Channel);
        writeBitGroup(getTimer().CTRLE, toChannelModeMask(Channel), toChannelModeCompare(Channel));
    }
    return returnValue;
} /* enablePwmChannel */


/******************************************************************************************************************************************************
  disablePwmChannel
******************************************************************************************************************************************************/
template <typename TimerTraits>
StdReturnType Timer<TimerTraits>::disablePwmChannel(TimerChannelType Channel)
{
    writeBitGroup(getTimer().CTRLE, toChannelModeMask(Channel), TC45_CCAMODE_DISABLE_gc);
    /* the pin stays an output, it now shows its PORT.OUT value instead of the waveform */
    return E_OK;
} /* disablePwmChannel */


/******************************************************************************************************************************************************
  setPwmDutyChannel
******************************************************************************************************************************************************/
/*! \brief          set duty of one compare channel, 0 .. PwmDutyMax
 *  \details        Written to the buffer register, so the new compare value takes effect at the next
 *                  BOTTOM and a running PWM never sees half a period of each.
 *****************************************************************************************************************************************************/
template <typename TimerTraits>
StdReturnType Timer<TimerTraits>::setPwmDutyChannel(TimerChannelType Channel, uint16_t Duty)
{
    StdReturnType returnValue{E_NOT_OK};

    if(Duty <= PwmDutyMax) {
        returnValue = E_OK;
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            const uint32_t compareValue = (static_cast<uint32_t>(getTimer().PER) * Duty) >> TIMER_PWM_DUTY_BITS;
            /* CCABUF..CCDBUF are consecutive in both TC4_t and TC5_t; the channel was range checked at compile time */
            (&getTimer().CCABUF)[Channel] = static_cast<uint16_t>(compareValue);
        }
    }
    return returnValue;
} /* setPwmDutyChannel */


/******************************************************************************************************************************************************
 * I N S T A N C E S   A N D   I N T E R R U P T S
******************************************************************************************************************************************************/
#if (TIMER_USE_TCC4 == STD_ON)
template class Timer<TimerC4>;
ISR(TCC4_OVF_vect) { Timer<TimerC4>::getInstance().handleOverflowInterrupt(); }
#endif

#if (TIMER_USE_TCC5 == STD_ON)
template class Timer<TimerC5>;
ISR(TCC5_OVF_vect) { Timer<TimerC5>::getInstance().handleOverflowInterrupt(); }
#endif


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
******************************************************************************************************************************************************/
