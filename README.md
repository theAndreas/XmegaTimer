# XmegaTimer

Periodic interrupt and PWM on the 16-bit Timer/Counters TC4 and TC5 of the
**ATxmega E5** — the XMEGA counterpart of the TimerOne library, for use next to the
xmegaForArduino core.

```cpp
#include <XmegaTimer.h>

void OnTick() { /* every 1000 us */ }

void setup() {
  xmega_timer::Discard(xmega_timer::tcc5.Begin(1000u, OnTick));
  // 50 %, of a duty of 0 to 1023.
  xmega_timer::Discard(xmega_timer::tcc5.EnablePwm<xmega_timer::Channel::kA>(512u));
  xmega_timer::Discard(xmega_timer::tcc5.Start());
}
```

This is [examples/Basics](examples/Basics/Basics.ino), which has nothing to do on a refusal, as
its period and duty are in range.

## Which timer

| Timer   | Channels | PWM pins  | Available | Why |
|---------|----------|-----------|-----------|-----|
| `TCC4`  | A–D      | PC0–PC3   | off by default (`XMEGATIMER_USE_TCC4`) | the core's `tone()` defines `TCC4_OVF_vect` and `analogWrite()` uses the timer; PC0/PC1 are also SDA/SCL |
| `TCC5`  | A, B     | PC4, PC5  | on (`XMEGATIMER_USE_TCC5`) | unused by the core |
| `TCD5`  | A, B     | PD4, PD5  | not offered | the core counts `millis()` on it and owns `TCD5_OVF_vect` |

TC4 and TC5 differ in their register struct (`TC4_t` / `TC5_t`), their number of compare
channels and their pins; the bit layout of every register used here is the same. The
differences live in a traits struct (`Tcc4Traits`, `Tcc5Traits`), so asking for a channel a timer
does not have — `EnablePwm<Channel::kC>()` on `tcc5` — is a compile error.

## Behaviour worth knowing

- The timer counts **dual-slope** and interrupts at BOTTOM: one period is `2 * PER` cycles,
  and PWM is phase-correct.
- The overflow interrupt runs at **low** level, so `millis()` (high level) can interrupt the
  callback. `AttachInterrupt()` enables the low level in the PMIC.
- TC4/TC5 do **not** clear their interrupt flag when the vector runs; the ISR does it.
- Duty values are written to the `CCxBUF` registers and take effect at the next BOTTOM.
- A duty is a fraction of the period and stays one: `SetPeriod()` goes through `PERBUF` and
  recomputes every channel's `CCxBUF` with it, so the new period and the PWM that fits it start
  at the same BOTTOM. Writing `PER` outright could leave the counter above the new TOP, and it
  would then run round `0xFFFF` once before the shorter period began.
- A period that needs another prescaler cannot wait for BOTTOM, as the prescaler has no buffer:
  `SetPeriod()` restarts a running timer from 0 with the new period and duty instead. The interval
  in which it changes lasts what was counted of it plus the new period, and the PWM begins its
  new period at that moment. A stopped timer is set back to 0 the same way and stays stopped;
  `Resume()` begins the new period.
- `Start()` loads `PER` and the compare values directly while the timer has no clock, so the first
  period is already the configured one rather than waiting a period for the buffers.
- `Read()` returns the time since the last overflow in microseconds, to one timer tick.
- `F_CPU` has to be a multiple of 2 MHz. Using a timer whose `XMEGATIMER_USE_*` is 0 is a compile
  error; both switches are set with `-D`.

## When a call refuses

A call that can refuse returns a `Status`, or a `Result` where it has a value, as `Read()`, and
leaves the timer as it was. `IsOk()` says whether it did what it was asked, a comparison with an
`Error` why not, and `ErrorText()` says it as a text for `Serial`:

```cpp
xmega_timer::Status const status{xmega_timer::tcc5.SetPeriod(5000000u)};
if (status == xmega_timer::Error::kPeriodOutOfRange) {
  Serial.println(status.ErrorText());  // "period out of range"
}
```

The compiler warns where a sketch drops one; `xmega_timer::Discard()` drops it on purpose.

## From 0.1.0

0.2.0 changes every name, on purpose: a sketch of 0.1.0 does not compile until it is changed,
rather than behaving otherwise without notice. There are no aliases of the old names; what the
timer does is the same.

| 0.1.0 | 0.2.0 |
|---|---|
| `Timer<TimerC5>::getInstance()`, `Timer<TimerC4>::getInstance()` | `xmega_timer::tcc5`, `xmega_timer::tcc4` |
| `init(us, f)` | `Begin(us, f)` |
| `setPeriod(us)`, `start()`, `stop()`, `resume()` | `SetPeriod(us)`, `Start()`, `Stop()`, `Resume()` |
| `attachInterrupt(f)`, `detachInterrupt()` | `AttachInterrupt(f)`, `DetachInterrupt()` |
| `read(us)`, the time into `us` | `Read()`, a `Result<uint32_t>` of the time |
| `enablePwm<Timer<TimerC5>::TIMER_CHANNEL_A>(duty)` | `EnablePwm<xmega_timer::Channel::kA>(duty)` |
| `setPwmDuty<...>(duty)`, `disablePwm<...>()` | `SetPwmDuty<...>(duty)`, `DisablePwm<...>()`, which returns nothing |
| `PwmDutyMax` | `kPwmDutyMax` |
| `E_OK`, `E_NOT_OK` of `StdReturnType` | `Status`, with an `Error` that says why |
| `TIMER_USE_TCC4`, `TIMER_USE_TCC5` as `STD_ON`, `STD_OFF` | `XMEGATIMER_USE_TCC4`, `XMEGATIMER_USE_TCC5` as 1, 0 |

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
