// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later
//
// TCC4 is off unless TIMER_USE_TCC4 switches it on: using it does not compile,
// rather than failing at the link.
// Expected: this timer is switched off

// The clock a board's build defines, which this call of the compiler does not.
#define F_CPU 32000000uL

#include "Timer.h"

StdReturnType InitTcc4() { return Timer<TimerC4>::getInstance().init(1000u); }
