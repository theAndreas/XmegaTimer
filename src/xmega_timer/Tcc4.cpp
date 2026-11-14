// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

#include "xmega_timer/Tcc4.h"

#if XMEGATIMER_USE_TCC4

#include <avr/interrupt.h>

namespace xmega_timer {

// Initialized when compiling, by the constexpr constructor (CodingStyle M-5).
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): the one
// timer, which its interrupt reaches
Tcc4 tcc4;

}  // namespace xmega_timer

ISR(TCC4_OVF_vect) { xmega_timer::tcc4.HandleOverflow(); }

#endif  // XMEGATIMER_USE_TCC4
