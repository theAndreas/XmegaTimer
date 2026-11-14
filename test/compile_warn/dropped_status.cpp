// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

// A Status XMEGATIMER_NODISCARD marks, dropped: the compiler has to say so in
// every standard the library is built in, as the macro may be another
// attribute in each (FN-10).
// Expected: -Wunused-result

// The clock a board's build defines, which this call of the compiler does not.
#define F_CPU 32000000uL

#include "xmega_timer/Tcc5.h"

void DropStart() { xmega_timer::tcc5.Start(); }
