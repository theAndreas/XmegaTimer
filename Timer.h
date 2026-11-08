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
/**     \file       Timer.h
 *      \brief      16-bit Timer/Counter TC4/TC5 of the XMEGA E5 as periodic interrupt and PWM source
 *
 *      \details    One class for TCC4 and TCC5, picked by a traits struct. Everything that differs
 *                  between the two kinds - the register struct, the number of compare channels and
 *                  the pins those channels drive - lives in the traits, so the class never has to ask
 *                  at run time which kind it is. A channel the timer does not have is a compile error.
 *
 *                  The timer counts dual-slope and interrupts at BOTTOM, so a period spans 2 * PER
 *                  timer cycles and PWM is phase-correct.
 *
******************************************************************************************************************************************************/
#ifndef _TIMER_H_
#define _TIMER_H_

/******************************************************************************************************************************************************
 * I N C L U D E S
******************************************************************************************************************************************************/
#include "StandardTypes.h"
#include <avr/io.h>


/******************************************************************************************************************************************************
 *  G L O B A L   C O N S T A N T   M A C R O S
******************************************************************************************************************************************************/
/* Timer configuration parameter: which timers get an instance and an overflow ISR.
 * TCC4 is off by default, because the xmegaForArduino core drives tone() and analogWrite() with it
 * and tone() defines TCC4_OVF_vect itself; also its channels A and B sit on PC0/PC1, the TWI pins.
 * TCD5 cannot be selected at all, see the note above TimerC5. */
#ifndef TIMER_USE_TCC4
    #define TIMER_USE_TCC4                      STD_OFF
#endif
#ifndef TIMER_USE_TCC5
    #define TIMER_USE_TCC5                      STD_ON
#endif


/******************************************************************************************************************************************************
 *  T I M E R   T R A I T S
******************************************************************************************************************************************************/
/* TC4: four compare channels, OC4A..OC4D on PC0..PC3 (PC4..PC7 if PORTC.REMAP moves them, which
 * this class does not do) */
struct TimerC4
{
    using RegisterType = TC4_t;
    static RegisterType& getTimer() { return TCC4; }
    static PORT_t& getPort() { return PORTC; }
    static constexpr uint8_t NumberOfChannels{4u};
    static constexpr uint8_t FirstPin{0u};
    static constexpr bool Enabled{TIMER_USE_TCC4 == STD_ON};
};

/* TC5: two compare channels, OC5A/OC5B on PC4/PC5 - the SPI SS and SCK pins.
 *
 * There are no traits for TCD5 on purpose: the xmegaForArduino core counts millis() on it and
 * defines TCD5_OVF_vect itself, so a second ISR would not link, and reprogramming the timer would
 * stop the system clock. */
struct TimerC5
{
    using RegisterType = TC5_t;
    static RegisterType& getTimer() { return TCC5; }
    static PORT_t& getPort() { return PORTC; }
    static constexpr uint8_t NumberOfChannels{2u};
    static constexpr uint8_t FirstPin{4u};
    static constexpr bool Enabled{TIMER_USE_TCC5 == STD_ON};
};


/******************************************************************************************************************************************************
 *  C L A S S   T I M E R
******************************************************************************************************************************************************/
template <typename TimerTraits>
class Timer
{
/******************************************************************************************************************************************************
 *  P U B L I C   D A T A   T Y P E S   A N D   S T R U C T U R E S
******************************************************************************************************************************************************/
  public:
    /* Timer ISR callback function */
    typedef void (*TimerIsrCallbackF_void)(void);

    /* Type which describes the internal state of the TIMER */
    typedef enum {
        STATE_NONE,
        STATE_INIT,
        STATE_READY,
        STATE_RUNNING,
        STATE_STOPPED
    } StateType;

    /* Type which includes the values of the Clock Select Bit Group */
    typedef enum {
        TIMER_REG_CS_NO_CLOCK = TC45_CLKSEL_OFF_gc,
        TIMER_REG_CS_NO_PRESCALER = TC45_CLKSEL_DIV1_gc,
        TIMER_REG_CS_PRESCALE_2 = TC45_CLKSEL_DIV2_gc,
        TIMER_REG_CS_PRESCALE_4 = TC45_CLKSEL_DIV4_gc,
        TIMER_REG_CS_PRESCALE_8 = TC45_CLKSEL_DIV8_gc,
        TIMER_REG_CS_PRESCALE_64 = TC45_CLKSEL_DIV64_gc,
        TIMER_REG_CS_PRESCALE_256 = TC45_CLKSEL_DIV256_gc,
        TIMER_REG_CS_PRESCALE_1024 = TC45_CLKSEL_DIV1024_gc
    } TimerClockSelectType;

    /* Type which defines the compare channels, each drives one PWM pin */
    typedef enum {
        TIMER_CHANNEL_A,
        TIMER_CHANNEL_B,
        TIMER_CHANNEL_C,
        TIMER_CHANNEL_D
    } TimerChannelType;

    typedef enum {
        TIMER_COUNT_DIRECTION_UP,
        TIMER_COUNT_DIRECTION_DOWN
    } TimerCountDirection;

    /* PWM duty resolution, the same 10 bit TimerOne offers */
    static constexpr uint16_t PwmDutyMax{1023u};

/******************************************************************************************************************************************************
 *  P R I V A T E   D A T A   A N D   F U N C T I O N S
******************************************************************************************************************************************************/
  private:
    /* TC4 and TC5 share the bit layout of every register used here; the class relies on that
     * rather than carrying a second set of masks in the traits */
    static_assert(TC4_CLKSEL_gm == TC5_CLKSEL_gm, "TC4/TC5 clock select differ");
    static_assert(TC4_WGMODE_gm == TC5_WGMODE_gm, "TC4/TC5 waveform mode differ");
    static_assert(TC4_DIR_bm == TC5_DIR_bm, "TC4/TC5 direction bit differs");
    static_assert(TC4_OVFINTLVL_gm == TC5_OVFINTLVL_gm, "TC4/TC5 overflow interrupt level differs");
    static_assert(TC4_OVFIF_bm == TC5_OVFIF_bm, "TC4/TC5 overflow flag differs");
    static_assert(TC4_CCAMODE_gm == TC5_CCAMODE_gm, "TC4/TC5 channel mode differs");

    /* Without this a timer switched off in the configuration above still compiles here and only
     * fails at the link, as an undefined reference to every member function used */
    static_assert(TimerTraits::Enabled, "this timer is switched off - see TIMER_USE_TCC4/TIMER_USE_TCC5");

    /* setPeriod() and read() convert with whole timer cycles per half and per whole microsecond */
    static_assert(F_CPU % 2000000uL == 0u, "F_CPU has to be a multiple of 2 MHz");

    static constexpr uint8_t ChannelModeWidth{2u};
    static constexpr uint8_t PwmDutyBits{10u};

    static Timer Instance;

    StateType State;
    TimerClockSelectType ClockSelectBitGroup;
    TimerIsrCallbackF_void TimerOverflowCallback;
    /* What PER and the compare channels are meant to hold. Kept here rather than read back from the
     * registers, because a value on its way through a buffer register is not in them until BOTTOM,
     * and because a duty is a fraction of the period that has to be applied again when it changes. */
    uint16_t Top;
    uint16_t PwmDuty[TimerTraits::NumberOfChannels];

    constexpr Timer() : State(STATE_NONE), ClockSelectBitGroup(TIMER_REG_CS_NO_PRESCALER), TimerOverflowCallback(nullptr), Top(0xFFFFu), PwmDuty{} { }

    // functions
    static typename TimerTraits::RegisterType& getTimer() { return TimerTraits::getTimer(); }

    static constexpr uint8_t toPinMask(TimerChannelType Channel) {
        return static_cast<uint8_t>(1u << (TimerTraits::FirstPin + Channel));
    }
    static constexpr uint8_t toChannelModeMask(TimerChannelType Channel) {
        return static_cast<uint8_t>(TC4_CCAMODE_gm << (Channel * ChannelModeWidth));
    }
    static constexpr uint8_t toChannelModeCompare(TimerChannelType Channel) {
        return static_cast<uint8_t>(TC45_CCAMODE_COMP_gc << (Channel * ChannelModeWidth));
    }
    static constexpr uint16_t toCompareValue(uint16_t sTop, uint16_t Duty) {
        return static_cast<uint16_t>((static_cast<uint32_t>(sTop) * Duty) >> PwmDutyBits);
    }
    /* Shifted before it is divided: the other way round the truncation is scaled up by the
     * prescaler, and at 1024 read() moved in steps of a millisecond where a tick is 32 us.
     * 2 * 0xFFFF << 10 is still well inside 32 bit. */
    static constexpr uint32_t toMicroseconds(uint32_t TimerCycles, TimerClockSelectType ClockSelect) {
        return (TimerCycles << toPrescalerShift(ClockSelect)) / (F_CPU / 1000000uL);
    }
    static constexpr uint8_t toPrescalerShift(TimerClockSelectType ClockSelect) {
        switch(ClockSelect) {
            case TIMER_REG_CS_PRESCALE_2:       return 1u;
            case TIMER_REG_CS_PRESCALE_4:       return 2u;
            case TIMER_REG_CS_PRESCALE_8:       return 3u;
            case TIMER_REG_CS_PRESCALE_64:      return 6u;
            case TIMER_REG_CS_PRESCALE_256:     return 8u;
            case TIMER_REG_CS_PRESCALE_1024:    return 10u;
            default:                            return 0u;
        }
    }

    TimerCountDirection getCountingDirection();
    void setClockSelect(TimerClockSelectType);
    void setCompareBuffer(TimerChannelType);
    void loadPeriodAndCompare();
    StdReturnType enablePwmChannel(TimerChannelType, uint16_t);
    StdReturnType disablePwmChannel(TimerChannelType);
    StdReturnType setPwmDutyChannel(TimerChannelType, uint16_t);

/******************************************************************************************************************************************************
 *  P U B L I C   F U N C T I O N S
******************************************************************************************************************************************************/
  public:
    // methods
    static Timer& getInstance() { return Instance; }
    StdReturnType init(uint32_t = 1000u, TimerIsrCallbackF_void = nullptr);
    StdReturnType setPeriod(uint32_t);
    StdReturnType start();
    void stop();
    StdReturnType resume();
    StdReturnType attachInterrupt(TimerIsrCallbackF_void);
    void detachInterrupt();
    StdReturnType read(uint32_t&);
    void handleOverflowInterrupt();

    /* The channel is a template argument so that channel C or D on a TC5 fails to compile
     * instead of writing into reserved registers at run time */
    template <TimerChannelType Channel> StdReturnType enablePwm(uint16_t Duty) {
        static_assert(Channel < TimerTraits::NumberOfChannels, "this timer has no such compare channel");
        return enablePwmChannel(Channel, Duty);
    }
    template <TimerChannelType Channel> StdReturnType disablePwm() {
        static_assert(Channel < TimerTraits::NumberOfChannels, "this timer has no such compare channel");
        return disablePwmChannel(Channel);
    }
    template <TimerChannelType Channel> StdReturnType setPwmDuty(uint16_t Duty) {
        static_assert(Channel < TimerTraits::NumberOfChannels, "this timer has no such compare channel");
        return setPwmDutyChannel(Channel, Duty);
    }
};

template <typename TimerTraits> Timer<TimerTraits> Timer<TimerTraits>::Instance;

#endif

/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
******************************************************************************************************************************************************/
