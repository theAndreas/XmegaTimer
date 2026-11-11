# XmegaTimer

Periodic interrupt and PWM on the 16-bit Timer/Counters TC4 and TC5 of the
**ATxmega E5** — the XMEGA counterpart of the TimerOne library, for use next to the
xmegaForArduino core.

```cpp
#include <XmegaTimer.h>

static void onTick() { /* every 1000 us */ }

auto& timer = Timer<TimerC5>::getInstance();
timer.init(1000u, onTick);
timer.enablePwm<Timer<TimerC5>::TIMER_CHANNEL_A>(512u);   // 50 %, duty 0 .. 1023
timer.start();
```

This is [examples/Basics](examples/Basics/Basics.ino).

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
- A duty is a fraction of the period and stays one: `setPeriod()` goes through `PERBUF` and
  recomputes every channel's `CCxBUF` with it, so the new period and the PWM that fits it start
  at the same BOTTOM. Writing `PER` outright could leave the counter above the new TOP, and it
  would then run round `0xFFFF` once before the shorter period began. A new prescaler, though,
  takes effect at once, so the rest of the current period runs at the new rate
  ([OIL 3](https://github.com/theAndreas/XmegaTimer/blob/main/OIL.md)).
- `start()` loads `PER` and the compare values directly while the timer has no clock, so the first
  period is already the configured one rather than waiting a period for the buffers.
- `read()` returns the time since the last overflow in microseconds, to one timer tick.
- `F_CPU` has to be a multiple of 2 MHz. Selecting a timer whose `TIMER_USE_*` is off is a compile
  error; both switches can be set with `-D` instead of editing `Timer.h`.

## Building

XmegaTimer is an Arduino library for the ATxmega32E5 of
[XMegaForArduino](https://github.com/XMegaForArduino), whose core takes avr-gcc from the Arduino
AVR core. With arduino-cli:

```bash
arduino-cli core install XMegaForArduino:avr@0.9.2 \
  --additional-urls https://raw.githubusercontent.com/XMegaForArduino/IDE/master/package_XMegaForArduino_index.json
arduino-cli core install arduino:avr@1.8.8
arduino-cli compile --fqbn XMegaForArduino:avr:xmega32e5 --library . examples/Basics
```

Not yet run on hardware.

## Development

XmegaTimer is built and checked as the projects made from
[embedded-cpp-template](https://github.com/theAndreas/embedded-cpp-template) are: `make` lists
every build and check, `make check` is what CI runs. [OIL.md](https://github.com/theAndreas/XmegaTimer/blob/main/OIL.md) is what is planned, and
[AGENTS.md](https://github.com/theAndreas/XmegaTimer/blob/main/AGENTS.md) what a contributor has to know.

## Licence

GPL-3.0-or-later, see [LICENSE](LICENSE).
