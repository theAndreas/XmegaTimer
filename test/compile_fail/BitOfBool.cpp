// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A Bit of a bool, which has one bit and no mask but itself.
//
// Expected: Bit: Integer is an unsigned integer other than bool

#include "xmega_timer/Bit.h"

bool Case() { return xmega_timer::Bit<bool>{}.value(); }
