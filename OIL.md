# Open Issue List

What is planned for XmegaTimer, in the order it can be done. A pull request that works on an item
sets its status and links itself in the same PR; an item is `done` once that PR is merged. New
items go at the end with the next free number, so a number never changes meaning.

Status: `open`, `in progress`, `done`, `dropped` (with the reason in the item).

## Items

| # | Item | Depends on | Status | PR |
|---|------|------------|--------|----|
| 1 | On the template embedded-cpp-template | – | in progress | [#2](https://github.com/theAndreas/XmegaTimer/pull/2) |
| 2 | What the template is to take over from 1 | 1 | open | |
| 3 | A new prescaler waits for BOTTOM as the period does | 1 | open | |
| 4 | Version 2.0.0: a namespace, constants for macros, the template's checks | 3 | open | |
| 5 | Run on an ATxmega32E5 | 1 | open | |

### 1 On the template embedded-cpp-template

The checks, CI and the dev container from the copier template
[embedded-cpp-template](https://github.com/theAndreas/embedded-cpp-template), which the other
libraries share, so that a change to one reaches the others with `copier update`; the template's
roadmap names it R-2. The library layout 1.5, `src/` and `examples/`; GPL-3.0-or-later as the
licence, which XmegaTimer had none of; the default branch `main`.

The board is the ATxmega32E5 of XMegaForArduino 0.9.2, `XMegaForArduino:avr:xmega32e5`, from
its index on GitHub. Its platform brings no compiler: `prepare()` in `tools/project.sh`
installs `arduino:avr@1.8.8` beside it for its avr-gcc 7.3. simavr does not know the ATxmega E5,
so there is no SelfCheck.

The code keeps its API and its behaviour: clang-format once, the documentation of the API, and
what cpplint finds; `toPrescalerShift()` as one expression, as avr-gcc builds gnu++11. The groups
of clang-tidy that the code fails wait for 4, in `src/.clang-tidy` and `examples/.clang-tidy`.
Tests on the host against a mock of the registers of TC4 and TC5 hold what works today.

*Done when* `make check` passes, and flash and RAM of Basics on the ATxmega32E5 are those of
before, or differ by what a commit names.

### 2 What the template is to take over from 1

What bringing in XmegaTimer found in the template, for embedded-cpp-template:

- `tools/compile_examples.sh` let only the warnings of `cores/arduino/` pass; XMegaForArduino's
  core warns in `cores/xmega/`. XmegaTimer's copy lets any core's pass, `/cores/[^/]+/`.
- The presets `sanitize` and `coverage` have no `ci-` twin, so a warning there does not fail
  `make check`: `-Wconversion` reported `toMicroseconds()` under the sanitizers only, and the
  check passed.
- A third-party core whose platform takes its compiler from another core's tools needs both;
  `prepare()` installs the second. `ARDUINO_CORE` could take a list instead.

### 3 A new prescaler waits for BOTTOM as the period does

`setPeriod()` on a running timer writes the period and the compare values to their buffers,
which the timer takes over at the next BOTTOM, but the new prescaler to CTRLA at once. Where the
prescaler changes, the rest of the current period counts at the new rate to the old TOP, so it
lasts up to 1024 times as long, or as short, as it would have, and the PWM with it. The README says that the
new period and its PWM start at the same BOTTOM, which holds only while the prescaler stays.

Shown by a test first, then the prescaler set from the overflow interrupt at that BOTTOM, or the
timer restarted where the prescaler changes; which, to decide with the fix.

### 4 Version 2.0.0: a namespace, constants for macros, the template's checks

As AtmegaTimer and TimerTwo 2.0.0 did: the namespace `xmega_timer` in `src/xmega_timer/`, Google's
names, constants and `enum class` in place of `#define` and C enums, a `Status` of its own in
place of `StdReturnType` from `StandardTypes.h`, and the template's checks in full, so that
`src/.clang-tidy` and the exceptions of `examples/.clang-tidy` go. A new major version, as the
API changes.

### 5 Run on an ATxmega32E5

XmegaTimer was never run on the chip. An example that checks itself on the board, the periods,
the PWM on PC4 and PC5 and `read()`, and its output in the README; with TCC4 as well, in a sketch
without `tone()`.
