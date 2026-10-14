# XmegaTimer

Periodic interrupt and PWM on the 16-bit Timer/Counters TC4 and TC5 of the
**ATxmega E5** — the XMEGA counterpart of the TimerOne library, for use next to the
xmegaForArduino core.

```cpp
#include "Timer.h"

static void onTick() { /* every 1000 us */ }

auto& timer = Timer<TimerC5>::getInstance();
timer.init(1000u, onTick);
timer.enablePwm<Timer<TimerC5>::TIMER_CHANNEL_A>(512u);   // 50 %, duty 0 .. 1023
timer.start();
```

## Which timer

| Timer   | Channels | PWM pins  | Available | Why |
|---------|----------|-----------|-----------|-----|
| `TCC4`  | A–D      | PC0–PC3   | off by default (`TIMER_USE_TCC4`) | the core's `tone()` defines `TCC4_OVF_vect` and `analogWrite()` uses the timer; PC0/PC1 are also SDA/SCL |
| `TCC5`  | A, B     | PC4, PC5  | on (`TIMER_USE_TCC5`) | unused by the core |
| `TCD5`  | A, B     | PD4, PD5  | not offered | the core counts `millis()` on it and owns `TCD5_OVF_vect` |

TC4 and TC5 differ in their register struct (`TC4_t` / `TC5_t`), their number of compare
channels and their pins; the bit layout of every register used here is the same. The
differences live in a traits struct (`TimerC4`, `TimerC5`), so asking for a channel a timer
does not have — `enablePwm<TIMER_CHANNEL_C>` on a TC5 — is a compile error.

## Behaviour worth knowing

- The timer counts **dual-slope** and interrupts at BOTTOM: one period is `2 * PER` cycles,
  and PWM is phase-correct.
- The overflow interrupt runs at **low** level, so `millis()` (high level) can interrupt the
  callback. `attachInterrupt()` enables the low level in the PMIC.
- TC4/TC5 do **not** clear their interrupt flag when the vector runs; the ISR does it.
- Duty values are written to the `CCxBUF` registers and take effect at the next BOTTOM.
- `read()` returns the time since the last overflow in microseconds.

## Building

The three files are the whole library. Built and linked with avr-gcc 14.3 for
`atxmega32e5`:

```bash
avr-g++ -mmcu=atxmega32e5 -std=gnu++17 -Os -DF_CPU=32000000L -fno-threadsafe-statics \
        -Wall -Wextra -I. -c Timer.cpp
```

Not yet run on hardware.
