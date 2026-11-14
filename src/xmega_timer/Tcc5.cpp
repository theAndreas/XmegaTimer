// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

#include "xmega_timer/Tcc5.h"

#if XMEGATIMER_USE_TCC5

#include <avr/interrupt.h>

namespace xmega_timer {

// The one timer, which its interrupt reaches, made when compiling by the
// constexpr constructor (CodingStyle M-5).
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): M-5
Tcc5 tcc5;

}  // namespace xmega_timer

ISR(TCC5_OVF_vect) { xmega_timer::tcc5.HandleOverflow(); }

#endif  // XMEGATIMER_USE_TCC5
